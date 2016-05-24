//
// Dis VM - exec program
// File: exec.h
// Author: arr
//

#ifndef _DISVM_SRC_EXEC_EXEC_H_
#define _DISVM_SRC_EXEC_EXEC_H_

#include <cstdint>
#include <vector>
#include <disvm.h>

class debugger final : public disvm::runtime::vm_tool_t
{
public: // vm_tool_t
    void on_load(disvm::vm_t &vm, disvm::runtime::vm_tool_controller_t &controller, std::size_t tool_id);

    void on_unload();

private:
    disvm::vm_t *_vm;
    std::size_t _tool_id;
    disvm::runtime::vm_tool_controller_t *_controller;
    std::vector<std::size_t> _event_cookies;
    std::vector<std::size_t> _breakpoint_cookies;
};

#endif // _DISVM_SRC_EXEC_EXEC_H_