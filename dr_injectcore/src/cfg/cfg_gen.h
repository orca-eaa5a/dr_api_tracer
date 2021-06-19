#ifndef CFG_GEN_H_
#define CFG_GEN_H_

#include "json.hpp"
void safe_insert(uintptr_t pc, uintptr_t opr);
bool branch_present(uintptr_t pc, uintptr_t opr);
nlohmann::json make_cfg();
#endif