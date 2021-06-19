#include "bb.h"

bool ret_check = false;
PCallNode cur_func = nullptr;

PCallNode GetNewCallNode(void* drcontext, PBasicBlockIR bb_ir) {
    PCallNode new_node = new CallNode;
    ZeroMemory(new_node, sizeof(PCallNode));
    new_node->caller = bb_ir->bb_end_addr;
    new_node->callee = instr_get_branch_target_pc(bb_ir->bb_last_inst);
    new_node->ret_addr = (app_pc)decode_next_pc(drcontext, (byte*)new_node->caller);
    new_node->parents = cur_func;

    return new_node;
}

PBasicBlockIR GetNewBB_IR(
    void* drcontext,
    void* tag, // bb address
    instrlist_t* bb,
    instr_t* bb_last_inst,
    void* user_data
) {
    PBasicBlockIR bb_ir = new BasicBlockIR;
    ZeroMemory(bb_ir, sizeof(BasicBlockIR));
    bb_ir->bb_start_addr = instr_get_app_pc(instrlist_first(bb));
    bb_ir->bb_end_addr = instr_get_app_pc(bb_last_inst); // end of bb
    bb_ir->bb_last_inst = bb_last_inst;

    return bb_ir;
}

dr_emit_flags_t bb_creation_event(
    void* drcontext,
    void* tag, // bb address
    instrlist_t* bb,
    instr_t* cti_trigger,
    bool for_trace,
    bool translating,
    void* user_data
)
{
    if((int)tag > 0x10000000)
        return DR_EMIT_DEFAULT;

    if (ret_check) {
        if (instr_get_app_pc(instrlist_first(bb)) == cur_func->ret_addr) {
            dr_mcontext_t m_ctx = { sizeof(m_ctx) };
            dr_get_mcontext(drcontext, &m_ctx);
            cur_func->ret_val = m_ctx.eax;
            cur_func = cur_func->parents;
            ret_check = false;
            if (cur_func != nullptr) {
                dr_messagebox("recover success!\ret val : %x\nparents caller : %x\nparents ret : %x\ncurrent_addr : %x",
                    m_ctx.eax,
                    cur_func->caller,
                    cur_func->ret_addr,
                    instr_get_app_pc(instrlist_first(bb))
                );
            }
        }
    }

    if (instr_is_cbr(cti_trigger) || instr_is_cti(cti_trigger)) {
        PBasicBlockIR bb_ir = GetNewBB_IR(drcontext, tag, bb, cti_trigger, user_data);
        if (instr_is_call(cti_trigger)) {
            if (instr_get_branch_target_pc(bb_ir->bb_last_inst) == 0) {
                dr_messagebox("%x", instr_get_target(cti_trigger));
            }
            dr_messagebox("caller : %x\ncallee : %x\nret : %x",
                bb_ir->bb_end_addr,
                instr_get_branch_target_pc(bb_ir->bb_last_inst),
                (app_pc)decode_next_pc(drcontext, (byte*)bb_ir->bb_end_addr));
            PCallNode new_call_node = GetNewCallNode(drcontext, bb_ir);
            cur_func = new_call_node;
        }
        else if (instr_is_return(cti_trigger)) {
            ret_check = true;
            dr_messagebox("ret triggered : %x\nret_check : %d", bb_ir->bb_end_addr, ret_check);
        }
    }
    return DR_EMIT_DEFAULT;
}