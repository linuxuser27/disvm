//
// Dis VM - exec program
// File: exec.h
// Author: arr
//

#ifndef _DISVM_SRC_EXEC_EXEC_H_
#define _DISVM_SRC_EXEC_EXEC_H_

#include <cstdint>
#include <unordered_set>
#include <disvm.h>
#include <vm_tools.h>
#include <utils.h>

enum class debugger_options
{
    none = 0,
    break_on_enter = 1,
    break_on_module_load = 2,
    break_on_exception = 4,
};

DEFINE_ENUM_FLAG_OPERATORS(debugger_options);

std::ostream& operator<<(std::ostream &ss, const debugger_options o);

class debugger final : public disvm::runtime::vm_tool_t
{
public:
    debugger(const debugger_options);

public: // vm_tool_t
    void on_load(disvm::runtime::vm_tool_controller_t &, std::size_t tool_id) override;

    void on_unload() override;

public:
    disvm::runtime::vm_tool_controller_t *controller;
    std::unordered_set<disvm::runtime::cookie_t> breakpoint_cookies;

private:
    debugger_options _options;
    std::size_t _tool_id;
    std::unordered_set<disvm::runtime::cookie_t> _event_cookies;
};

#endif // _DISVM_SRC_EXEC_EXEC_H_