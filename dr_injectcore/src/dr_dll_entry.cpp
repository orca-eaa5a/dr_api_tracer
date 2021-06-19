#include <iostream>
#include <fstream>
#include <iomanip>

#include "dr_api.h"
#include "dr_tools.h"
#include "drmgr.h"
#include "drreg.h"
#include "drx.h"
#include "drutil.h"
#include "droption.h"

#include "cfg/cfg_gen.h"
#include "evtcb/bb.h"

static droption_t<std::string> out(DROPTION_SCOPE_CLIENT, "-o", "", "output results to file", "");
static app_pc exe_etry;
module_data_t *exe;

void dr_exit(void){
    /*
    if(out.get_value().empty()){
        std::cout << std::setw(2) << make_cfg() << std::endl; // print at console
    }
    else{
        std::ofstream _ofstream(out.get_value());
        _ofstream << std::setw(2) << make_cfg() << std::endl; // write to file
    }
    */
    drmgr_exit();
}

void app_init(void){
    module_data_t *exe = dr_get_main_module(); //Looks up module data for the main executable.
    if (exe != NULL){
        exe_etry = exe->start;
    }
    dr_free_module_data(exe);
}

DR_EXPORT void dr_client_main(client_id_t id, int argc, const char* argv[]){
    
	//IF_DEBUG(bool, ok;)
	

	dr_set_client_name("loop_surgery", NULL);
	dr_enable_console_printing();
    dr_printf("[Dynamorio Info] Client dll attached!\n");

    if (!droption_parser_t::parse_argv(DROPTION_SCOPE_CLIENT, argc, argv, NULL, NULL)) {
        dr_printf("[Dynamorio Error] Unable to parse argv\n");
        return;
    }
    drmgr_init();
    drmgr_register_bb_instrumentation_event(NULL, bb_creation_event, NULL);
    //drmgr_register_bb_instrumentation_event(NULL, cbr_event, NULL);
    //drmgr_register_bb_instrumentation_event(NULL, cti_event, NULL);

    dr_register_exit_event(dr_exit);
}