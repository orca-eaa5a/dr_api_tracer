#include "load.h"

static void module_entry(void *wrapcontext, void **user_data) {
	const char *api_name;
	const char *mod_name;
	app_pc proc;
	module_data_t* mod;
	thread_id_t tid;
	unsigned int mod_id;
	app_pc mod_start, ret_addr;
	void* drcontext;

	mod_name = NULL;
	api_name = (const char*)*user_data;
	proc = drwrap_get_func(wrapcontext);
	drcontext = drwrap_get_drcontext(wrapcontext);
	
	if (true) {
		app_pc retaddr = NULL;
		DR_TRY_EXCEPT(drcontext,
			{ //TRY
				retaddr = drwrap_get_retaddr(wrapcontext);
			}, 
			{ //EXCEPT
				retaddr = NULL;
			}
		);
		if (retaddr != NULL) {
			/*
			dr_lookup_module(app_pc)
			Looks up the module containing pc
			*/
			mod = dr_lookup_module(retaddr);
			if (mod != NULL) {
				bool is_exe = (mod->start == exe_img_base);
				dr_free_module_data(mod);
				if (!is_exe) return;
			}
		}
		else {
			/* Nearly all of these cases should be things like KiUserCallbackDispatcher
			 * or other abnormal transitions.
			 * If the user really wants to see everything they can not pass
			 * -only_from_app.
			 */
			return;
		}
	}
	
	mod = dr_lookup_module(proc);
	if (mod != NULL) {
		mod_name = dr_module_preferred_name(mod);
	}

	char module_name[256];
	memset(module_name, 0, sizeof(module_name));

	/* Temporary workaround for VC2013, which doesn't have snprintf().
	 * apparently, this was added in later releases... */
#ifdef WINDOWS
#define snprintf _snprintf
#endif
	unsigned int module_name_len = (unsigned int)snprintf(module_name, \
		sizeof(module_name) - 1, "%s%s%s", mod_name == NULL ? "" : mod_name, \
		mod_name == NULL ? "" : "!", api_name);

	/* Check if this module & function is in the whitelist. */
	bool allowed = false;
	bool tested = false;  /* True only if any white/blacklist testing below is done. */
	for (unsigned int i = 0; (allowed == false) && (i < filter_function_whitelist_len); i++) {
		tested = true;

		/* If the whitelist entry contains a wildcard, then compare only the shortest
		 * part of either string. */
		unsigned int module_name_len_compare;
		if (filter_function_whitelist[i].is_wildcard)
			module_name_len_compare = MIN(module_name_len, filter_function_whitelist[i].func_name_len);
		else
			module_name_len_compare = module_name_len;
		
		if (fast_strcmp(
				module_name, 
				module_name_len_compare,
				filter_function_whitelist[i].func_name,
				filter_function_whitelist[i].func_name_len) == 0) {
			allowed = true;
		}
	}

	/* Check the blacklist if it was specified instead of a whitelist. */
	if (!allowed && filter_function_blacklist_len > 0) {
		allowed = true;
		for (unsigned int i = 0; allowed && (i < filter_function_blacklist_len); i++) {
			tested = true;

			/* If the blacklist entry contains a wildcard, then compare only the shortest
			 * part of either string. */
			unsigned int module_name_len_compare;
			if (filter_function_blacklist[i].is_wildcard)
				module_name_len_compare = MIN(module_name_len, \
					filter_function_blacklist[i].func_name_len);
			else
				module_name_len_compare = module_name_len;

			if (fast_strcmp(module_name, module_name_len_compare, \
				filter_function_blacklist[i].func_name, \
				filter_function_blacklist[i].func_name_len) == 0) {
				allowed = false;
			}
		}
	}

	/* If whitelist/blacklist testing was performed, and it was determined
	 * this function is not to be logged... */
	if (tested && !allowed)
		return;
	dr_printf("%s\n", module_name);
	if (mod != NULL)
		dr_free_module_data(mod);
	return;
}

static void iterate_exports(const module_data_t* mod_info, bool append) {
	dr_symbol_export_iterator_t *exp_iter = dr_symbol_export_iterator_start(mod_info->handle);
	while (dr_symbol_export_iterator_hasnext(exp_iter)) {
		dr_symbol_export_t* sym = dr_symbol_export_iterator_next(exp_iter);
		app_pc api_addr = nullptr;
		if (sym->is_code)
			api_addr = sym->addr;

		if (api_addr != NULL) {
			if (append) {
				/*
					drwrap_wrap_ex(
						app_pc 	func,
						void(*)(void *wrapcxt, INOUT void **user_data) 	pre_func_cb,
						void(*)(void *wrapcxt, void *user_data) 	post_func_cb,
						void * 	user_data,
						uint 	flags
					)
				*/
				IF_DEBUG(bool ok = )
					drwrap_wrap_ex(
						api_addr,
						module_entry,
						nullptr,
						(void*)sym->name,
						false
					);
			}
			else {
				IF_DEBUG(bool ok = )
					drwrap_unwrap(
						api_addr,
						module_entry,
						nullptr
					);
			}
		}
	}
	dr_symbol_export_iterator_stop(exp_iter);
}

void cb_module_load(void* drcontext, const module_data_t* mod_info, bool reserv) {
	if (mod_info->start != exe_img_base) {
		iterate_exports(mod_info, true);
	}
}

void cb_module_unload(void* drcontext, const module_data_t* mod_info) {
	if (mod_info->start != exe_img_base) {
		iterate_exports(mod_info, false);
	}
}