#include <iostream>
#include <fstream>
#include <iomanip>

#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "drwrap.h"
#include "drx.h"
#include "drcovlib.h"
#include "drutil.h"
#include "droption.h"

#include "common.h"

#include "cfg/cfg_gen.h"
#include "utils/api_filter.h"
#include "utils/com_utils.h"
#include "evtcb/bb.h"
#include "evtcb/load.h"
#include "evtcb/exit.h"

module_data_t *exe;
unsigned char* exe_img_base = NULL;
droption_t<std::string> op_filter_file
(DROPTION_SCOPE_CLIENT, "filter", "filter.config", "The path of the whitelist/blacklist file.",
	"");

wb_list *filter_function_whitelist = NULL;
unsigned int filter_function_whitelist_len = 0;

wb_list *filter_function_blacklist = NULL;
unsigned int filter_function_blacklist_len = 0;

/* Vectors to hold modules in the whitelist/blacklist. */
std::vector<std::string> filter_module_whitelist;
std::vector<std::string> filter_module_blacklist;

void dr_module_init(void) {
	IF_DEBUG(bool ok;)
	dr_set_client_name("Garbage Loop Finder", NULL);
	dr_printf("[Dynamorio Info] Client dll attached!\n");

	IF_DEBUG(ok = )
		drmgr_init();
	IF_DEBUG(ok = )
		drwrap_init();
	IF_DEBUG(ok = )
		drx_init();

	exe = dr_get_main_module(); //Looks up module data for the main executable.
	if (exe != NULL) {
		exe_img_base = exe->start;
		dr_printf("target img base : %x\n", exe_img_base);
	}
	dr_free_module_data(exe);
}

DR_EXPORT void dr_client_main(client_id_t id, int argc, const char* argv[]){
    if (!droption_parser_t::parse_argv(DROPTION_SCOPE_CLIENT, argc, argv, NULL, NULL)) {
        dr_printf("[Dynamorio Error] Unable to parse argv\n");
        return;
    }

    //drmodtrack_init();
	dr_module_init();

	drwrap_set_global_flags((drwrap_global_flags_t)(DRWRAP_NO_FRILLS | DRWRAP_FAST_CLEANCALLS));
    
	dr_register_exit_event(cb_exit);

    //drmgr_register_bb_instrumentation_event(NULL, bb_creation_event, NULL);
    
	//drmgr_register_bb_instrumentation_event(NULL, cbr_event, NULL);
    //drmgr_register_bb_instrumentation_event(NULL, cti_event, NULL);

    drmgr_register_module_load_event(cb_module_load);
    drmgr_register_module_unload_event(cb_module_unload);

	parse_filter_file();
	
}