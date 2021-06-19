#pragma once
#ifndef BB_H_
#define BB_H_

#include "dr_api.h"
#include "drmgr.h"

dr_emit_flags_t bb_creation_event(void* drcontext, void* tag, instrlist_t* bb, instr_t* instr,
    bool for_trace, bool translating, void* user_data);

typedef struct _CallNode {
    app_pc caller;
    app_pc callee;
    app_pc ret_addr;
    int ret_val;
    _CallNode* parents;
}CallNode, *PCallNode;

typedef struct _BasicBlockIR {
    app_pc bb_start_addr;
    app_pc bb_end_addr;
    instr_t* bb_last_inst;
    unsigned int exec_count;
    _BasicBlockIR* next_bb;
}BasicBlockIR, *PBasicBlockIR;

extern bool ret_check;
extern PCallNode cur_func;

#endif