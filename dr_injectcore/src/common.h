#pragma once

#include <vector>
#include "droption.h"

extern unsigned char* exe_img_base;
extern droption_t<std::string> op_filter_file;
struct _wblist {
	char *func_name;
	size_t func_name_len;
	unsigned int is_wildcard;  /* Set to 1 when this is a wildcard, otherwise 0. */
};
typedef struct _wblist wb_list; /* Stands for white/black list. */

/* Arrays to hold functions in the whitelist/blacklist.  Used instead
 * of vectors due to speed requirements. */
extern wb_list *filter_function_whitelist;
extern unsigned int filter_function_whitelist_len;

extern wb_list *filter_function_blacklist;
extern unsigned int filter_function_blacklist_len;

/* Vectors to hold modules in the whitelist/blacklist. */
extern std::vector<std::string> filter_module_whitelist;
extern std::vector<std::string> filter_module_blacklist;