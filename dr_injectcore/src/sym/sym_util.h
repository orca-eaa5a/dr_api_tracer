#pragma once

#include "drltrace.h"

void print_simple_value(drltrace_arg_t *arg, bool leading_zeroes);
void print_string(void *drcontext, void *pointer_str, bool is_wide);
void print_arg(void *drcontext, drltrace_arg_t *arg);
bool drlib_iter_arg_cb(drltrace_arg_t *arg, void *wrapcxt);
void print_args_unknown_call(app_pc func, void *wrapcxt);
bool print_libcall_args(std::vector<drltrace_arg_t*> *args_vec, void *wrapcxt);
void print_symbolic_args(const char *name, void *wrapcxt, app_pc func);
