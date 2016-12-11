//
// Dis VM - exec program
// File: exec.h
// Author: arr
//

#ifndef _DISVM_SRC_EXEC_EXEC_H_
#define _DISVM_SRC_EXEC_EXEC_H_

#include <cstdint>
#include <unordered_set>
#include <vector>
#include <string>
#include <memory>
#include <disvm.h>
#include <vm_asm.h>
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

    // Resolve all PCs in the range from begin and end (inclusive)
    std::vector<std::string> resolve_to_function_source_line(
        disvm::runtime::module_id_t vm_id,
        disvm::runtime::vm_pc_t begin_pc,
        disvm::runtime::vm_pc_t end_pc,
        disvm::symbol::function_name_format_t fmt) const;

    // Return all functions for the supplied module ID.
    // Optionally match a function against the supplied string.
    std::vector<std::string> get_module_functions(disvm::runtime::module_id_t vm_id, const char *match) const;

    // Check if the supplied module ID has symbols loaded
    bool symbols_exist(disvm::runtime::module_id_t vm_id) const;

    // Get/set debugger option
    std::string get_option(const std::string &option) const;
    void set_option(const std::string &option, std::string value);

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

    struct symbol_info_t
    {
        std::unique_ptr<disvm::symbol::symbol_data_t> data;
        std::unique_ptr<disvm::symbol::symbol_pc_iter_t> iter;
    };

    std::unordered_map<std::size_t, symbol_info_t> _vm_id_to_symbol_info;
    std::unordered_map<std::string, std::string> _debugger_options;
};

#endif // _DISVM_SRC_EXEC_EXEC_H_
