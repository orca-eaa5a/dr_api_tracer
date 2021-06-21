/****************************************************************************
 * Arguments printing
 */
#include "sym_util.h"
#include "log.h"

void print_simple_value(drltrace_arg_t *arg, bool leading_zeroes)
{
	bool pointer = !TEST(DRSYS_PARAM_INLINED, arg->mode);
	dr_fprintf(outf, pointer ? PFX : (leading_zeroes ? PFX : PIFX), arg->value);
	if (pointer && ((arg->pre && TEST(DRSYS_PARAM_IN, arg->mode)) ||
		(!arg->pre && TEST(DRSYS_PARAM_OUT, arg->mode)))) {
		ptr_uint_t deref = 0;
		ASSERT(arg->size <= sizeof(deref), "too-big simple type");
		/* We assume little-endian */
		if (dr_safe_read((void *)arg->value, arg->size, &deref, NULL))
			dr_fprintf(outf, (leading_zeroes ? " => " PFX : " => " PIFX), deref);
	}
}

void print_string(void *drcontext, void *pointer_str, bool is_wide)
{
	if (pointer_str == NULL)
		dr_fprintf(outf, "<null>");
	else {
		DR_TRY_EXCEPT(drcontext, {
			dr_fprintf(outf, is_wide ? "%S" : "%s", pointer_str);
			}, {
				dr_fprintf(outf, "<invalid memory>");
			});
	}
}

void print_arg(void *drcontext, drltrace_arg_t *arg)
{
	if (arg->pre && (TEST(DRSYS_PARAM_OUT, arg->mode) && !TEST(DRSYS_PARAM_IN, arg->mode)))
		return;
	dr_fprintf(outf, "%s%d: ", "\n    arg ", arg->ordinal);
	switch (arg->type) {
	case DRSYS_TYPE_VOID:         print_simple_value(arg, true); break;
	case DRSYS_TYPE_POINTER:      print_simple_value(arg, true); break;
	case DRSYS_TYPE_BOOL:         print_simple_value(arg, false); break;
	case DRSYS_TYPE_INT:          print_simple_value(arg, false); break;
	case DRSYS_TYPE_SIGNED_INT:   print_simple_value(arg, false); break;
	case DRSYS_TYPE_UNSIGNED_INT: print_simple_value(arg, false); break;
	case DRSYS_TYPE_HANDLE:       print_simple_value(arg, false); break;
	case DRSYS_TYPE_NTSTATUS:     print_simple_value(arg, false); break;
	case DRSYS_TYPE_ATOM:         print_simple_value(arg, false); break;
#ifdef WINDOWS
	case DRSYS_TYPE_LCID:         print_simple_value(arg, false); break;
	case DRSYS_TYPE_LPARAM:       print_simple_value(arg, false); break;
	case DRSYS_TYPE_SIZE_T:       print_simple_value(arg, false); break;
	case DRSYS_TYPE_HMODULE:      print_simple_value(arg, false); break;
#endif
	case DRSYS_TYPE_CSTRING:
		print_string(drcontext, (void *)arg->value, false);
		break;
	case DRSYS_TYPE_CWSTRING:
		print_string(drcontext, (void *)arg->value, true);
		break;
	default: {
		if (arg->value == 0)
			dr_fprintf(outf, "<null>");
		else
			dr_fprintf(outf, PFX, arg->value);
	}
	}

	dr_fprintf(outf, " (%s%s%stype=%s%s, size=" PIFX ")",
		(arg->arg_name == NULL) ? "" : "name=",
		(arg->arg_name == NULL) ? "" : arg->arg_name,
		(arg->arg_name == NULL) ? "" : ", ",
		(arg->type_name == NULL) ? "\"\"" : arg->type_name,
		(arg->type_name == NULL ||
			TESTANY(DRSYS_PARAM_INLINED | DRSYS_PARAM_RETVAL, arg->mode)) ? "" : "*",
		arg->size);
}

bool drlib_iter_arg_cb(drltrace_arg_t *arg, void *wrapcxt)
{
	if (arg->ordinal == -1)
		return true;
	if (arg->ordinal >= op_max_args.get_value())
		return false; /* limit number of arguments to be printed */

	arg->value = (ptr_uint_t)drwrap_get_arg(wrapcxt, arg->ordinal);

	print_arg(drwrap_get_drcontext(wrapcxt), arg);
	return true; /* keep going */
}

void print_args_unknown_call(app_pc func, void *wrapcxt)
{
	uint i;
	void *drcontext = drwrap_get_drcontext(wrapcxt);
	char *prefix = "\n    arg ";
	char *suffix = "";
	DR_TRY_EXCEPT(drcontext, {
		for (i = 0; i < op_unknown_args.get_value(); i++) {
			dr_fprintf(outf, "%s%d: " PFX, prefix, i,
					   drwrap_get_arg(wrapcxt, i));
			if (*suffix != '\0')
				dr_fprintf(outf, suffix);
		}
		}, {
			dr_fprintf(outf, "<invalid memory>");
		/* Just keep going */
		});
	/* all args have been sucessfully printed */
	dr_fprintf(outf, op_print_ret_addr.get_value() ? "\n   " : "");
}

bool print_libcall_args(std::vector<drltrace_arg_t*> *args_vec, void *wrapcxt)
{
	if (args_vec == NULL || args_vec->size() <= 0)
		return false;

	std::vector<drltrace_arg_t*>::iterator it;
	for (it = args_vec->begin(); it != args_vec->end(); ++it) {
		if (!drlib_iter_arg_cb(*it, wrapcxt))
			break;
	}
	return true;
}

void print_symbolic_args(const char *name, void *wrapcxt, app_pc func)
{
	std::vector<drltrace_arg_t *> *args_vec;

	if (op_max_args.get_value() == 0)
		return;

	if (op_use_config.get_value()) {
		/* looking for libcall in libcalls hashtable */
		args_vec = libcalls_search(name);
		if (print_libcall_args(args_vec, wrapcxt)) {
			dr_fprintf(outf, op_print_ret_addr.get_value() ? "\n   " : "");
			return; /* we found libcall and sucessfully printed all arguments */
		}
	}
	/* use standard type-blind scheme */
	if (op_unknown_args.get_value() > 0)
		print_args_unknown_call(func, wrapcxt);
}