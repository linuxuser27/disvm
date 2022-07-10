//
// Dis VM
// File: builtin_module.cpp
// Author: arr
//

#include <cassert>
#include <atomic>
#include <mutex>
#include <builtin_module.hpp>
#include <module_reader.hpp>
#include <debug.hpp>
#include <exceptions.hpp>
#include <utils.hpp>

using disvm::runtime::type_descriptor_t;
using disvm::runtime::rooted_ptr_t;
using disvm::runtime::vm_module_t;

// Declare initializers for built-in modules.
extern std::unique_ptr<vm_module_t> Sysmodinit();
extern std::unique_ptr<vm_module_t> Mathmodinit();

std::vector<rooted_ptr_t<vm_module_t>> disvm::runtime::builtin::initialize_builtin_modules()
{
    // Add built-in module initialization calls here.
    // e.g. Sysmodinit()

    std::unique_ptr<vm_module_t> mods[] =
    {
        Sysmodinit(),
        Mathmodinit()
    };

    std::vector<rooted_ptr_t<vm_module_t>> builtin_modules;
    for (auto iter = std::begin(mods); iter != std::end(mods); ++iter)
    {
#ifndef NDEBUG
        for (const auto& m : builtin_modules)
            assert(0 != vm_string_t::compare(m->module_name.get(), (*iter)->module_name.get()) && "Built-in module with matching name already exists");
#endif

        // Create a rooted allocation for the module.
        auto root = rooted_ptr_t<vm_module_t>::create(managed_ptr_t<vm_module_t>{ iter->release() });

        builtin_modules.push_back(std::move(root));
    }

    return builtin_modules;
}

std::unique_ptr<vm_module_t> disvm::runtime::builtin::create_builtin_module(const char *name, word_t table_length, const vm_runtab_t *module_runtime_table)
{
    assert(name != nullptr && (module_runtime_table != nullptr || table_length == 0) && 0 <= table_length);
    assert(name[0] == BUILTIN_MODULE_PREFIX_CHAR && "Built-in modules should have the \"" BUILTIN_MODULE_PREFIX_STR "\" prefix");

    auto new_builtin = std::make_unique<vm_module_t>();

    // Define the module header
    auto &header = new_builtin->header;
    header.magic_number = format::magic_number_constants::xmagic;
    header.Signature.length = word_t{ 0 };
    header.runtime_flag = runtime_flags_t::builtin;

    header.stack_extent = 0;
    header.code_size = table_length;
    header.data_size = 0;
    header.type_size = table_length; // 1 for each function frame type
    header.export_size = table_length;
    header.entry_pc = runtime_constants::invalid_program_counter;
    header.entry_type = -1;

    new_builtin->module_name = std::make_unique<vm_string_t>(std::strlen(name), reinterpret_cast<const uint8_t *>(name));

    // Fill out code, type, and export sections
    for (auto i = word_t{ 0 }; i < table_length; ++i)
    {
        const auto &ex = module_runtime_table[i];

        // Type
        new_builtin->type_section.push_back(managed_ptr_t<const type_descriptor_t>
        {
            new type_descriptor_t
            {
                ex.frame_size,
                ex.frame_pointer_count,
                ex.frame_pointer_map,
                type_descriptor_t::no_finalizer,
                "adt"
            }
        });

        // Export
        auto item = export_function_t{};
        item.pc = i;
        item.frame_type = i;
        item.sig = static_cast<word_t>(ex.sig);
        item.name = std::make_unique<vm_string_t>(std::strlen(ex.name), reinterpret_cast<const uint8_t *>(ex.name));

        const auto sig = item.sig;
        new_builtin->export_section.emplace(sig, std::move(item));

        // Instruction
        auto vm_instr = vm_instruction_t{};
        vm_instr.native = ex.fn;
        new_builtin->code_section.push_back(std::move(vm_instr));
    }

    return new_builtin;
}
