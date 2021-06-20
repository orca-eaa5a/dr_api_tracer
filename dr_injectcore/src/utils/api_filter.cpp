#include "api_filter.h"

#include "dr_api.h"
#include "drutil.h"
#include "dr_tools.h"

#include <Windows.h>
#include <string>

void add_module_filter(std::vector<std::string> &module_wbl, const char *module_name) {

	for (std::vector<std::string>::const_iterator iter = module_wbl.begin(); iter != module_wbl.end(); iter++) {
		if (strcmp(module_name, iter->c_str()) == 0)
			return;
	}

	module_wbl.push_back(module_name);
}

void parse_filter(std::vector<std::string> &v_in, bool is_whitelist, std::vector<std::string> &module_wbl, wb_list **func_wbl, unsigned int *func_wbl_len) {
	
	for (std::vector<std::string>::const_iterator iter = v_in.begin(); \
		iter != v_in.end(); iter++) {
		if (iter->find('!') == std::string::npos)
			add_module_filter(module_wbl, iter->c_str());
	}

	unsigned int v_in_len = v_in.size();
	*func_wbl = (wb_list *)calloc(v_in_len, sizeof(wb_list));
	if (*func_wbl == NULL) {
		fprintf(stderr, "Failed to allocate whitelist/blacklist array.\n");
		exit(-1);
	}

	unsigned j = 0;
	for (std::vector<std::string>::const_iterator iter = v_in.begin(); \
		(iter != v_in.end()) && (j < v_in_len); iter++, j++) {

		unsigned int is_wildcard = 0;
		char *s = _strdup(iter->c_str());
		if (s == NULL) {
			fprintf(stderr, "Failed to allocate whitelist/blacklist array.\n");
			exit(-1);
		}

		if (s[strlen(s) - 1] == '*') {
			s[strlen(s) - 1] = '\0';
			is_wildcard = 1;

		}
		else if (strchr(s, '!') == NULL)
			is_wildcard = 1;

		if (is_whitelist) {
			char *module_name = _strdup(s);
			char *bang_pos = strchr(module_name, '!');
			if (bang_pos != NULL) {
				*bang_pos = '\0';
				add_module_filter(module_wbl, module_name);
			}
			free(module_name);  module_name = NULL;
		}

		wb_list *l = *func_wbl;
		l[j].func_name = s;
		l[j].is_wildcard = is_wildcard;


		l[j].func_name_len = strlen(s);
	}

	*func_wbl_len = v_in_len;
}

void parse_filter_file(void)
{
	if (op_filter_file.get_value().empty())
		return;
	std::vector<std::string> temp_whitelist;
	std::vector<std::string> temp_blacklist;

	std::ifstream filter_file(op_filter_file.get_value().c_str());

	std::string line;
	bool mode_whitelist = false, mode_blacklist = false;
	while (std::getline(filter_file, line)) {
		if (line.empty() || (line.find("#") == 0))
			continue;

		if (line == std::string("[whitelist]")) {
			mode_whitelist = true;
			mode_blacklist = false;
			continue;
		}
		else if (line == std::string("[blacklist]")) {
			mode_whitelist = false;
			mode_blacklist = true;
			continue;
		}

		if (mode_whitelist) {
			temp_whitelist.push_back(line);
		}
		else if (mode_blacklist)
			temp_blacklist.push_back(line);
	}

	if (!temp_whitelist.empty() && !temp_blacklist.empty())
		temp_blacklist.clear();

	if (!temp_whitelist.empty())
		parse_filter(temp_whitelist, true, filter_module_whitelist, &filter_function_whitelist, &filter_function_whitelist_len);
	else if (!temp_blacklist.empty())
		parse_filter(temp_blacklist, false, filter_module_blacklist, &filter_function_blacklist, &filter_function_blacklist_len);

	filter_file.close();
}

void free_wblist_array(wb_list **wbl, unsigned int wb_list_len) {
	if ((wbl == NULL) || (*wbl == NULL) || (wb_list_len == 0))
		return;
	for (unsigned int i = 0; i < wb_list_len; i++) {
		wb_list *l = *wbl;
		free(l[i].func_name);  l[i].func_name = NULL;
		l[i].func_name_len = 0;
	}

	free(*wbl);  *wbl = NULL;
}