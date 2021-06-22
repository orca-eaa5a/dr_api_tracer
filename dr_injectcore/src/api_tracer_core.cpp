#include <fstream>
#include <vector>
#include <mutex>
#include "drltrace.h"
#include "drltrace_utils.h"
#include "sym/sym_util.h"
#include "log.h"
/* Where to write the trace */


static app_pc exe_start;
static app_pc exe_img_base;
void* dr_mtx = dr_mutex_create();

static void event_exit_cb(void)
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
	
	dr_printf("application launch finished\n");
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

static void pre_apicall_cb(void *wrapcontext, void **user_data) {
	uintptr_t targ = *(uintptr_t*)user_data;
	const char *name = NULL;
	const char *modname = NULL;
	app_pc func = drwrap_get_func(wrapcontext);
	module_data_t *mod;

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
			name = sym->name;
			break;
		}
	}
	dr_mutex_lock(dr_mtx);
	dr_symbol_export_iterator_stop(exp_iter);
	dr_fprintf(outf, "%s!%s:[0x%x]", modname, name, targ);
	print_symbolic_args(name, wrapcontext, func);
	dr_fprintf(outf, "\n");
	dr_mutex_unlock(dr_mtx);
}

static void post_apicall_cb(void *wrapcontext, void *user_data) {
	uintptr_t targ = (uintptr_t)user_data;
	DR_TRY_EXCEPT(
		drwrap_get_drcontext(wrapcontext), {
			dr_mutex_lock(dr_mtx);
			dr_fprintf(outf, "    ret : 0x%x\n\n", (unsigned int)drwrap_get_retval(wrapcontext));
			dr_mutex_unlock(dr_mtx);
		}, {
			dr_messagebox("???%x", drwrap_get_retval(wrapcontext));
		}
	);
}

static void set_api_instrument(uintptr_t src, uintptr_t targ)
{
	
	if (is_api_call((app_pc)targ)) {
		if (!drwrap_is_wrapped(
				(app_pc)targ,
				pre_apicall_cb,
				post_apicall_cb
			)){
			drwrap_wrap_ex(
				(app_pc)targ,
				pre_apicall_cb,
				post_apicall_cb,
				(void*)targ,
				false
			);
		}
	}
	
}

dr_emit_flags_t bb_creation_event_cb(
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

static void
thread_init_cb(void *drcontext)
{
	uintptr_t tid = dr_get_thread_id(drcontext);
	dr_printf("[%d] new thread init\n",tid);
}

static void
thread_exit_cb(void *drcontext)
{
	uintptr_t tid = dr_get_thread_id(drcontext);
	dr_printf("[%d] thread exit\n", tid);
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    module_data_t *exe;
    IF_DEBUG(bool ok;)
	
	dr_enable_console_printing();
    dr_set_client_name("Dr_api_tracer", "");
	
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
	dr_free_module_data(exe);
	
	dr_printf("dll client attached!!\n");

    drwrap_set_global_flags((drwrap_global_flags_t) (DRWRAP_NO_FRILLS | DRWRAP_FAST_CLEANCALLS));
	
    dr_register_exit_event(event_exit_cb);
	drmgr_register_bb_instrumentation_event(NULL, bb_creation_event_cb, NULL);
	drmgr_register_thread_init_event(thread_init_cb);
	drmgr_register_thread_exit_event(thread_exit_cb);
    if (op_max_args.get_value() > 0)
        parse_config();

    open_log_file();
}
