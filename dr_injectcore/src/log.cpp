#include "log.h"

file_t outf;
void open_log_file(void)
{
	char buf[MAXIMUM_PATH];
	if (op_logdir.get_value().compare("-") == 0)
		outf = STDERR;
	else {
		outf = drx_open_unique_appid_file(op_logdir.get_value().c_str(),
			dr_get_process_id(),
			"drltrace", "log",
			DR_FILE_ALLOW_LARGE,
			buf, BUFFER_SIZE_ELEMENTS(buf));
		ASSERT(outf != INVALID_FILE, "failed to open log file");
		//VNOTIFY(0, "drltrace log file is %s" NL, buf);

	}
}