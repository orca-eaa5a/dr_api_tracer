#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#ifdef WINDOWS
#define UNICODE
#define _UNICODE
# define DIRSEP '\\'
# define ALT_DIRSEP '/'
# define NL "\r\n"
#endif

#define MAX_DR_CMDLINE (MAXIMUM_PATH*6)

#include "dr_api.h"
#include "dr_inject.h"
#include "dr_frontend.h"
#include "droption.h"

#include "tools_utils.h"

static void
check_input_files(const char *target_app_full_name, char *dr_root, char *drltrace_path) {
	bool result = false;

	/* check that the target application exists */
	if (target_app_full_name[0] == '\0')
		DRLTRACE_ERROR("target application is not specified");

	if (drfront_access(target_app_full_name, DRFRONT_READ, &result) != DRFRONT_SUCCESS)
		DRLTRACE_ERROR("cannot find target application at %s", target_app_full_name);
	if (!result) {
		DRLTRACE_ERROR("cannot open target application for read at %s",
			target_app_full_name);
	}

	/* check that dynamorio's root dir exists and is accessible */
	if (drfront_access(dr_root, DRFRONT_READ, &result) != DRFRONT_SUCCESS)
		DRLTRACE_ERROR("cannot find DynamoRIO's root dir at %s", dr_root);
	if (!result)
		DRLTRACE_ERROR("cannot open DynamoRIO's root dir for read at %s", dr_root);

	/* check that target libary exist */
	if (drfront_access(drltrace_path, DRFRONT_READ, &result) != DRFRONT_SUCCESS)
		DRLTRACE_ERROR("cannot find drltracelib at %s", drltrace_path);
	if (!result)
		DRLTRACE_ERROR("cannot open drltracelib for read at %s", drltrace_path);
}


static void
configure_application(
	char *app_name, 
	char **app_argv, 
	void **inject_data,
	const char *dr_root, 
	const char *lib_path, 
	const char *log_dir,
	const char *config_dir
)
{
	bool is_debug = false;
#ifdef DEBUG
	is_debug = true;
#endif
	int errcode;
	ssize_t len;
	size_t sofar = 0;
	char *process;
	process_id_t pid;
	char dr_option[MAX_DR_CMDLINE];
	char drltrace_option[MAX_DR_CMDLINE];
	dr_option[0] = '\0';

	errcode = dr_inject_process_create(app_name, (const char **)app_argv, inject_data);
	if (errcode != 0 && errcode != WARN_IMAGE_MACHINE_TYPE_MISMATCH_EXE) {
		std::string msg =
			std::string("failed to create process for \"") + app_name + "\"";
		char buf[MAXIMUM_PATH];
		int sofar = dr_snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%s", msg.c_str());
		NULL_TERMINATE_BUFFER(buf);
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, errcode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR)buf + sofar,
			BUFFER_SIZE_ELEMENTS(buf) - sofar * sizeof(char), NULL);
		DRLTRACE_ERROR("%s", msg.c_str());
	}

	pid = dr_inject_get_process_id(*inject_data);

	process = dr_inject_get_image_name(*inject_data);
	if (dr_register_process(process, pid,
		false, dr_root,
		DR_MODE_CODE_MANIPULATION,
		is_debug, DR_PLATFORM_DEFAULT,
		dr_option) != DR_SUCCESS) {
		DRLTRACE_ERROR("failed to register DynamoRIO configuration");
	}

	if (dr_register_client(process, pid, false, DR_PLATFORM_DEFAULT, 0, 0, lib_path,
		drltrace_option) != DR_SUCCESS) {
		DRLTRACE_ERROR("failed to register DynamoRIO client configuration");
	}
}


int wmain(int argc, const TCHAR *targv[])
{
	char drlibpath[MAXIMUM_PATH];
	static const char *libname = "dr_injectcore.dll";

	void *inject_data;
	int exitcode;
	char **argv;
	char *tmp;
	const char *target_app_name;

	char full_target_app_path[MAXIMUM_PATH];
	char tmp_path[MAXIMUM_PATH];
	char full_frontend_path[MAXIMUM_PATH];
	char full_dr_root_path[MAXIMUM_PATH];
	char full_drlibtrace_path[MAXIMUM_PATH];
	char logdir[MAXIMUM_PATH];
	char config_dir[MAXIMUM_PATH];

	int last_index;
	std::string parse_err;
	drfront_status_t sc;

	dr_snprintf(drlibpath, BUFFER_SIZE_ELEMENTS(drlibpath), "%s", libname); NULL_TERMINATE_BUFFER(drlibpath);

#if defined(WINDOWS) && !defined(_UNICODE)
# error _UNICODE must be defined
#else
	/* Convert to UTF-8 if necessary */
	sc = drfront_convert_args((const TCHAR **)targv, &argv, argc);
	if (sc != DRFRONT_SUCCESS)
		DRLTRACE_ERROR("failed to process args, error code = %d\n", sc);
#endif

	if (!droption_parser_t::parse_argv(DROPTION_SCOPE_FRONTEND, argc, (const char **)argv,
		&parse_err, &last_index) || argc < 2) {
		DRLTRACE_ERROR("Usage error: %s\n Usage:\n%s\n", parse_err.c_str(),
			droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
	}
	/*
	if (op_help.get_value()) {
		printf("Usage:\n%s", droption_parser_t::usage_long(DROPTION_SCOPE_ALL).c_str());
		return 0;
	}
	*/
	dr_enable_console_printing();

	target_app_name = argv[last_index];
	if (target_app_name == NULL) {
		DRLTRACE_ERROR("Usage error, target application is not specified.\n Usage:\n%s\n",
			droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
	}
	sc = drfront_get_app_full_path(target_app_name, full_target_app_path,
		BUFFER_SIZE_ELEMENTS(full_target_app_path));
	if (sc != DRFRONT_SUCCESS) {
		DRLTRACE_ERROR("drfront_get_app_full_path failed on %s, error code = %d\n",
			target_app_name, sc);
	}

	/* get DR's root directory and drltrace.exe full path */
	sc = drfront_get_app_full_path(argv[0], full_frontend_path,
		BUFFER_SIZE_ELEMENTS(full_frontend_path));
	if (sc != DRFRONT_SUCCESS) {
		DRLTRACE_ERROR("drfront_get_app_full_path failed on %s, error code = %d\n",
			argv[0], sc);
	}

	tmp = full_frontend_path + strlen(full_frontend_path) - 1;

	/* we assume that default root for our executable is <root>/bin/drltrace.exe */
	while (*tmp != DIRSEP && *tmp != ALT_DIRSEP && tmp > full_frontend_path)
		tmp--;
	*(tmp + 1) = '\0';

	/* in case of default config option, we use drltrace's frontend path */

	NULL_TERMINATE_BUFFER(config_dir);

	dr_snprintf(tmp_path, BUFFER_SIZE_ELEMENTS(tmp_path), "%s../dynamorio",
		full_frontend_path);
	NULL_TERMINATE_BUFFER(tmp_path);

	sc = drfront_get_absolute_path(tmp_path, full_dr_root_path,
		BUFFER_SIZE_ELEMENTS(full_dr_root_path));
	NULL_TERMINATE_BUFFER(full_dr_root_path);

	if (sc != DRFRONT_SUCCESS)
		DRLTRACE_ERROR("drfront_get_absolute_path failed, error code = %d\n", sc);

	dr_snprintf(full_drlibtrace_path, BUFFER_SIZE_ELEMENTS(full_drlibtrace_path),
		"%s%s", full_frontend_path, drlibpath);
	NULL_TERMINATE_BUFFER(full_drlibtrace_path);

	check_input_files(full_target_app_path, full_dr_root_path, full_drlibtrace_path);

	dr_standalone_init();

	configure_application(full_target_app_path, &argv[last_index],
		&inject_data, full_dr_root_path, full_drlibtrace_path, logdir,
		config_dir);

	if (!dr_inject_process_inject(inject_data, false/*!force*/, NULL))
		DRLTRACE_ERROR("unable to inject");

	if (!dr_inject_process_run(inject_data))
		DRLTRACE_ERROR("unable to execute target application");

	//DRLTRACE_INFO(1, "%s sucessfully started, waiting app for exit", full_target_app_path);

	dr_inject_wait_for_child(inject_data, 0/*wait forever*/);

	exitcode = dr_inject_process_exit(inject_data, false);

	sc = drfront_cleanup_args(argv, argc);
	if (sc != DRFRONT_SUCCESS)
		DRLTRACE_ERROR("drfront_cleanup_args error, error code = %d", sc);

	return exitcode;
}
