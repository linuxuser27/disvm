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
    break_on_enter = (1 << 0),
    break_on_module_load = (1 << 1),
    break_on_exception = (1 << 2),
    disable_symbol_loading = (1 << 3),
};

DEFINE_ENUM_FLAG_OPERATORS(debugger_options);

std::ostream& operator<<(std::ostream &ss, const debugger_options o);

class debugger final : public disvm::runtime::vm_tool_t
{
public:
    debugger(const debugger_options);

    std::string resolve_function_source_line(std::shared_ptr<const disvm::runtime::vm_module_t> m, disvm::runtime::vm_pc_t pc);

public: // vm_tool_t
    void on_load(disvm::runtime::vm_tool_controller_t &, std::size_t tool_id) override;

    void on_unload() override;

public:
    disvm::runtime::vm_tool_controller_t *controller;
    std::unordered_set<disvm::runtime::cookie_t> breakpoint_cookies;

private:
    void load_symbols(const disvm::loaded_vm_module_t &);

    debugger_options _options;
    std::size_t _tool_id;
    std::unordered_set<disvm::runtime::cookie_t> _event_cookies;
    std::unordered_map<std::size_t, std::unique_ptr<disvm::symbol::symbol_debug_t>> _vm_id_to_symbol_debug;
};

#endif // _DISVM_SRC_EXEC_EXEC_H_