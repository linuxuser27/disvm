//
// Dis VM
// File: module_ref.cpp
// Author: arr
//

#include <runtime.h>
#include <exceptions.h>
#include <vm_memory.h>
#include <utils.h>
#include <debug.h>

using disvm::debug::component_trace_t;
using disvm::debug::log_level_t;

using disvm::runtime::type_descriptor_t;
using disvm::runtime::intrinsic_type_desc;
using disvm::runtime::byte_t;
using disvm::runtime::word_t;
using disvm::runtime::vm_array_t;
using disvm::runtime::vm_string_t;
using disvm::runtime::vm_module_t;
using disvm::runtime::vm_module_ref_t;
using disvm::runtime::export_section_t;
using disvm::runtime::import_vm_module_t;
using disvm::runtime::vm_module_function_ref_t;
using disvm::runtime::vm_system_exception;
using disvm::runtime::vm_user_exception;

namespace
{
    std::unique_ptr<vm_array_t> resolve_imports(const export_section_t &exports, const import_vm_module_t &imports)
    {
        static auto function_ref_type_desc = type_descriptor_t::create(sizeof(vm_module_function_ref_t), std::vector<byte_t>{});
        auto refs = std::make_unique<vm_array_t>(function_ref_type_desc, imports.functions.size());

        for (auto index = std::size_t{ 0 }; index < imports.functions.size(); ++index)
        {
            const auto &import = imports.functions[index];

            const auto exports_with_sig = exports.equal_range(import.sig);
            if (exports_with_sig.first == exports.cend() && exports_with_sig.second == exports.cend())
                throw vm_user_exception{ "Module does not export a function with expected signature" };

            auto export_match = exports_with_sig.first;
            while (export_match != exports_with_sig.second)
            {
                // Look for the matching function name in those with matching signature
                if (0 == vm_string_t::compare(export_match->second.name.get(), import.name.get()))
                    break;

                ++export_match;
            }

            if (export_match == exports_with_sig.second)
                throw vm_user_exception{ "Type check failure for exported function" };

            const auto &export_entry = export_match->second;

            auto &ref = refs->at<vm_module_function_ref_t>(index);
            ref.entry_pc = export_entry.pc;
            ref.frame_type = export_entry.frame_type;

            if (disvm::debug::is_component_tracing_enabled<component_trace_t::module>())
            {
                disvm::debug::log_msg(
                    component_trace_t::module,
                    log_level_t::debug,
                    "export: %d %#08x >>%s<<",
                    index,
                    export_entry.sig,
                    export_entry.name->str());
            }
        }

        return refs;
    }
}

std::shared_ptr<const type_descriptor_t> vm_module_ref_t::type_desc()
{
    return intrinsic_type_desc::type<vm_module_ref_t>();
}

vm_module_ref_t::vm_module_ref_t(std::shared_ptr<const vm_module_t> module)
    : vm_alloc_t(vm_module_ref_t::type_desc())
    , code_section{ module->code_section }
    , module{ module }
    , mp_base{ nullptr }
    , type_section{ module->type_section }
    , _builtin_module{ util::has_flag(module->header.runtime_flag, runtime_flags_t::builtin) }
{
    if (module->original_mp != nullptr)
        mp_base = vm_alloc_t::copy(*module->original_mp);

    if (disvm::debug::is_component_tracing_enabled<component_trace_t::memory>())
        disvm::debug::log_msg(component_trace_t::memory, log_level_t::debug, "init: vm module ref");
}

vm_module_ref_t::vm_module_ref_t(std::shared_ptr<const vm_module_t> module, const import_vm_module_t &imports)
    : vm_alloc_t(vm_module_ref_t::type_desc())
    , code_section{ module->code_section }
    , module{ module }
    , mp_base{ nullptr }
    , type_section{ module->type_section }
    , _function_refs{ resolve_imports(module->export_section, imports) }
    , _builtin_module{ util::has_flag(module->header.runtime_flag, runtime_flags_t::builtin) }
{
    assert(module->header.data_size == 0 || module->original_mp != nullptr);

    if (module->original_mp != nullptr)
        mp_base = vm_alloc_t::copy(*module->original_mp);

    if (disvm::debug::is_component_tracing_enabled<component_trace_t::memory>())
        disvm::debug::log_msg(component_trace_t::memory, log_level_t::debug, "init: vm module ref: exported %d", _function_refs->get_length());
}

vm_module_ref_t::~vm_module_ref_t()
{
    dec_ref_count_and_free(mp_base);

    if (_function_refs != nullptr)
        _function_refs->release();

    if (disvm::debug::is_component_tracing_enabled<component_trace_t::memory>())
        disvm::debug::log_msg(component_trace_t::memory, log_level_t::debug, "destroy: vm module ref");
}

bool vm_module_ref_t::is_builtin_module() const
{
    return _builtin_module;
}

const vm_module_function_ref_t& vm_module_ref_t::get_function_ref(word_t index) const
{
    assert(_function_refs != nullptr);

    if (index < 0 || _function_refs->get_length() <= index)
        throw vm_system_exception{ "Invalid function reference index into module reference functions" };

    return _function_refs->at<vm_module_function_ref_t>(index);
}

