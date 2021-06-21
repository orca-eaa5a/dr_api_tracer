#include <fstream>
#include <vector>
#include <mutex>
#include "drltrace.h"
#include "drltrace_utils.h"
#include "sym/sym_util.h"
#include "log.h"
/* Where to write the trace */

/* Avoid exe exports, as on Linux many apps have a ton of global symbols. */
static app_pc exe_start;
static app_pc exe_img_base;
PBasicBlockIR pCur_bb_ir;
PAPIInfo cur_api_info;
std::mutex mtx;

/****************************************************************************
 * Library entry wrapping
 */


/****************************************************************************
 * Init and exit
 */

static void
event_exit(void)
{
    if (op_use_config.get_value())
        libcalls_hashtable_delete();

    if (outf != STDERR) {
        if (op_print_ret_addr.get_value())
            drmodtrack_dump(outf);
        dr_close_file(outf);
    }

    drx_exit();
    drwrap_exit();
    drmgr_exit();
    if (op_print_ret_addr.get_value())
        drmodtrack_exit();
}


static bool is_api_call(app_pc target_pc) {
	module_data_t *mod = dr_lookup_module(dr_fragment_app_pc((app_pc)target_pc));
	if (mod->start != exe_img_base) {
		dr_free_module_data(mod);
		return true;
	}
	dr_free_module_data(mod);
	return false;
}

static void cb_pre_apicall(void *wrapcontext, void **user_data) {
	BasicBlockIR* udata = (BasicBlockIR*)*user_data;
	const char *name = udata->pAPIinfo->api_name.c_str();
	const char *modname = NULL;
	app_pc func = drwrap_get_func(wrapcontext);
	module_data_t *mod;
	thread_id_t tid;
	uint mod_id;
	app_pc mod_start, ret_addr;
	drcovlib_status_t res;

	uintptr_t targ = udata->pAPIinfo->address;
	mod = dr_lookup_module(dr_fragment_app_pc((app_pc)targ));
	if (mod != NULL)
		modname = dr_module_preferred_name(mod);
	if (modname == NULL) modname = "";

	dr_symbol_export_iterator_t *exp_iter = dr_symbol_export_iterator_start(mod->handle);
	while (dr_symbol_export_iterator_hasnext(exp_iter)) {
		dr_symbol_export_t* sym = dr_symbol_export_iterator_next(exp_iter);
		app_pc api_addr = nullptr;
		if (sym->is_code)
			api_addr = sym->addr;
		if (api_addr == (app_pc)targ) {
			udata->pAPIinfo->api_name = std::string(sym->name);
			break;
		}
	}
	dr_symbol_export_iterator_stop(exp_iter);
	dr_fprintf(outf, "%s!%s:[0x%x]", modname, udata->pAPIinfo->api_name.c_str(), udata->pAPIinfo->address);
	print_symbolic_args(name, wrapcontext, func);
	dr_fprintf(outf, "\n");
}

static void cb_post_apicall(void *wrapcontext, void *user_data) {
	BasicBlockIR* udata = (BasicBlockIR*)user_data;

	udata->exe_cnt += 1;
	DR_TRY_EXCEPT(
		drwrap_get_drcontext(wrapcontext), {
			udata->pAPIinfo->ret_val = (unsigned int)drwrap_get_retval(wrapcontext);
		}, {
			dr_messagebox("???%x", drwrap_get_retval(wrapcontext));
			udata->pAPIinfo->ret_val = 0;
		}
		);
	udata->pAPIinfo->ret_val = (unsigned int)drwrap_get_retval(wrapcontext);
	dr_fprintf(outf, "    ret : 0x%x\n\n", udata->pAPIinfo->ret_val);
	DR_TRY_EXCEPT(
		drwrap_get_drcontext(wrapcontext), {
			drwrap_unwrap(
				(app_pc)udata->pAPIinfo->address,
				cb_pre_apicall,
				cb_post_apicall
			);
		}, {
			dr_messagebox("drwrap_unwrap error");
		}
	);
}

static void set_api_instrument(uintptr_t src, uintptr_t targ)
{
	if (is_api_call((app_pc)targ)) {
		cur_api_info = new APIInfo;
		pCur_bb_ir->pAPIinfo = cur_api_info;
		pCur_bb_ir->pAPIinfo->address = targ;
		drwrap_wrap_ex(
			(app_pc)targ,
			cb_pre_apicall,
			cb_post_apicall,
			(void*)pCur_bb_ir,
			false
		);
	}
}

PBasicBlockIR create_new_bb_ir(
	void* drcontext,
	void* tag, // bb address
	instrlist_t* bb,
	instr_t* bb_last_inst,
	void* user_data
) {
	PBasicBlockIR bb_ir = new BasicBlockIR;
	ZeroMemory(bb_ir, sizeof(BasicBlockIR));
	bb_ir->block_id = *(int*)tag;
	bb_ir->bb_start_addr = instr_get_app_pc(instrlist_first(bb));
	bb_ir->bb_end_addr = instr_get_app_pc(bb_last_inst); // end of bb
	bb_ir->bb_last_inst = bb_last_inst;
	bb_ir->exe_cnt += 1;

	return bb_ir;
}

dr_emit_flags_t bb_creation_event(
	void* drcontext,
	void* tag, // bb address
	instrlist_t* bb,
	instr_t* last_instr,
	bool for_trace,
	bool translating,
	void* user_data
)
{
	module_data_t *mod = dr_lookup_module(dr_fragment_app_pc(tag));
	if (mod != NULL) {
		bool is_exe = (mod->start == exe_img_base);
		dr_free_module_data(mod);
		if (!is_exe) {
			return DR_EMIT_DEFAULT;
		}
	}
	PBasicBlockIR bb_ir = create_new_bb_ir(drcontext, tag, bb, last_instr, user_data);
	pCur_bb_ir = bb_ir;
	if (instr_is_call(last_instr)) {
		if (instr_is_call_indirect(last_instr)) {
			opnd_t target_opnd = instr_get_target(last_instr);
			if (opnd_is_memory_reference(target_opnd) | opnd_is_reg(target_opnd)) {
				dr_insert_clean_call(drcontext, bb, last_instr, (void *)set_api_instrument, false, 2, OPND_CREATE_INTPTR(instr_get_app_pc(last_instr)), target_opnd);
			}
		}
	}
	return DR_EMIT_DEFAULT;
}


DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    module_data_t *exe;
    IF_DEBUG(bool ok;)

    dr_set_client_name("dr_api_tracer", "???");

	if (!droption_parser_t::parse_argv(DROPTION_SCOPE_CLIENT, argc, argv,
		NULL, NULL)) {
		ASSERT(false, "unable to parse options specified for drltracelib");
	}

    IF_DEBUG(ok = )
        drmgr_init();
    ASSERT(ok, "drmgr failed to initialize");
    IF_DEBUG(ok = )
        drwrap_init();
    ASSERT(ok, "drwrap failed to initialize");
    IF_DEBUG(ok = )
        drx_init();
    ASSERT(ok, "drx failed to initialize");
    if (op_print_ret_addr.get_value()) {
        IF_DEBUG(ok = )
            drmodtrack_init();
        ASSERT(ok == DRCOVLIB_SUCCESS, "drmodtrack failed to initialize");
    }

    exe = dr_get_main_module();
	if (exe != NULL) {
		exe_start = exe->start;
		exe_img_base = exe_start;
	}
	dr_printf("dll client attached!!");
    dr_free_module_data(exe);

    drwrap_set_global_flags((drwrap_global_flags_t)
                            (DRWRAP_NO_FRILLS | DRWRAP_FAST_CLEANCALLS));

    dr_register_exit_event(event_exit);
	drmgr_register_bb_instrumentation_event(NULL, bb_creation_event, NULL);
    dr_enable_console_printing();
    if (op_max_args.get_value() > 0)
        parse_config();

    open_log_file();
}
