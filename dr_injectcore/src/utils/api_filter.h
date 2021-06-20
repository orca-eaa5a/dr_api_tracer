#pragma once
#include <vector>
#include <iomanip>
#include <fstream>
#include <string>
#include "common.h"

void add_module_filter(std::vector<std::string> &module_wbl, const char *module_name);
void parse_filter(std::vector<std::string> &v_in, bool is_whitelist, std::vector<std::string> &module_wbl, wb_list **func_wbl, unsigned int *func_wbl_len);
void parse_filter_file(void);
void free_wblist_array(wb_list **wbl, unsigned int wb_list_len);