#include "exit.h"
#include "common.h"

void cb_exit(void) {
	free_wblist_array(&filter_function_whitelist, filter_function_whitelist_len);
	free_wblist_array(&filter_function_blacklist, filter_function_blacklist_len);
	drx_exit();
	drwrap_exit();
	drmgr_exit();
}