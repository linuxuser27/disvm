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
#include <vm_tools.h>

struct debugger_options
{
    bool break_on_enter;
    bool break_on_module_load;
    bool break_on_exception;
};

std::ostream& operator<<(std::ostream &ss, const debugger_options o);

class debugger final : public disvm::runtime::vm_tool_t
{
public:
    debugger(const debugger_options &);

public: // vm_tool_t
    void on_load(disvm::runtime::vm_tool_controller_t &, std::size_t tool_id) override;

    void on_unload() override;

public:
    disvm::runtime::vm_tool_controller_t *controller;
    std::vector<disvm::runtime::cookie_t> breakpoint_cookies;

private:
    debugger_options _options;
    std::size_t _tool_id;
    std::vector<disvm::runtime::cookie_t> _event_cookies;
};

#endif // _DISVM_SRC_EXEC_EXEC_H_