#pragma once
#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "drwrap.h"
#include "drx.h"
#include "drutil.h"

#include "common.h"
#include "utils/com_utils.h"
#include "utils/api_filter.h"

extern void cb_module_load(void* drcontext, const module_data_t* mod_info, bool reserv);
extern void cb_module_unload(void* drcontext, const module_data_t* mod_info);