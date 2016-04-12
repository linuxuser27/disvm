//
// Dis VM - Shell
// File: main.cpp
// Author: arr
//

#include <cstdio>
#include <cassert>
#include <array>
#include <disvm.h>
#include <builtin_module.h>
#include <debug.h>

using namespace disvm;
using namespace disvm::runtime;

namespace
{
    // Entry frame layout
    struct vm_entry_frame_t final : public vm_frame_base_alloc_t
    {
        word_t noret;
        pointer_t command_module_ref;
        pointer_t command_stack_frame;
    };

    const auto entry_frame_pointer_map = std::vector<byte_t>{ 0x06 };

    std::shared_ptr<const runtime::vm_module_t> command_module;

    std::unique_ptr<vm_module_t> create_entry_module(disvm::vm_t &vm, int argc, char* argv[])
    {
        if (argc == 0)
            return{};

        // First argument is the module to load
        auto command_module_path = new vm_string_t{ std::strlen(argv[0]), reinterpret_cast<uint8_t *>(argv[0]) };

        // Pre-load the command module in the VM.
        command_module = vm.load_module(command_module_path->str());

        // Convert arguments to a list
        vm_list_t *args = nullptr;
        vm_list_t *last = nullptr;
        for (auto i = 1; i < argc; ++i)
        {
            auto tmp = argv[i];
            auto arg = new vm_string_t{ std::strlen(tmp), reinterpret_cast<uint8_t *>(tmp) };
            if (args == nullptr)
            {
                assert(last == nullptr);
                args = new vm_list_t{ intrinsic_type_desc::type<vm_string_t>() };
                last = args;
            }
            else
            {
                auto new_tail = new vm_list_t{ intrinsic_type_desc::type<vm_string_t>() };
                last->set_tail(new_tail);
                new_tail->release();

                last = new_tail;
            }

            pt_ref(last->value()) = arg->get_allocation();
        }

        // Create the module
        auto entry_module = std::make_unique<vm_module_t>();
        auto &e = *entry_module;

        // Header details
        e.header.entry_pc = 0;
        e.header.entry_type = 0;
        e.header.runtime_flag = (runtime_flags_t::has_import);

        // Inherit the stack extent from the command module and add the entry frame size.
        e.header.stack_extent = command_module->header.stack_extent + sizeof(vm_entry_frame_t);

        e.type_section =
        {
            type_descriptor_t::create(sizeof(vm_entry_frame_t), entry_frame_pointer_map)
        };

        // Initialize the module base register
        const auto mp_type = type_descriptor_t::create(12, { 0xe0 });
        e.original_mp.reset(vm_alloc_t::allocate(mp_type, vm_alloc_t::zero_memory));
        auto mp_base = e.original_mp->get_allocation<pointer_t>();
        mp_base[0] = command_module_path->get_allocation();

        // [TODO] Implement the UI context for the command
        mp_base[1] = nullptr;
        mp_base[2] = args->get_allocation();

        e.code_section =
        {
            { vm_exec_op_t
                {
                    opcode_t::load,
                    { address_mode_t::offset_indirect_mp, 0, 0 }, // command module path
                    { address_mode_middle_t::small_immediate, 0 },
                    { address_mode_t::offset_indirect_fp, 20, 0 } // module reference
                }
            },
            { vm_exec_op_t
                {
                    opcode_t::mframe,
                    { address_mode_t::offset_indirect_fp, 20, 0 }, // module reference
                    { address_mode_middle_t::small_immediate, 0 },
                    { address_mode_t::offset_indirect_fp, 24, 0 } // module call frame
                }
            },
            { vm_exec_op_t
                {
                    opcode_t::movp,
                    { address_mode_t::offset_indirect_mp, 4, 0 }, // UI context
                    { address_mode_middle_t::none },
                    { address_mode_t::offset_double_indirect_fp, 24, 32 } // module call frame -> UI context offset
                }
            },
            { vm_exec_op_t
                {
                    opcode_t::movp,
                    { address_mode_t::offset_indirect_mp, 8, 0 }, // Argument list
                    { address_mode_middle_t::none },
                    { address_mode_t::offset_double_indirect_fp, 24, 36 } // module call frame -> argument list offset
                }
            },
            { vm_exec_op_t
                {
                    opcode_t::mcall,
                    { address_mode_t::offset_indirect_fp, 24, 0 }, // module call frame
                    { address_mode_middle_t::small_immediate, 0 },
                    { address_mode_t::offset_indirect_fp, 20, 0 } // module reference
                }
            },
            { vm_exec_op_t{ opcode_t::ret,{ address_mode_t::none },{ address_mode_middle_t::none },{ address_mode_t::none } } },
        };

        // Create import section for Command
        // See limbo/Command.m for type details
        {
            import_function_t func{};

            // [TODO] Create mechanism to generated type signatures
            func.sig = 0x4244b354;

            std::array<uint8_t, 5> func_name = { "init" };
            func.name = std::make_unique<vm_string_t>(func_name.size() - 1, func_name.data());

            import_vm_module_t import{};
            import.functions.push_back(std::move(func));

            e.import_section.push_back(std::move(import));
        }

        return entry_module;
    }
}

int main(int argc, char* argv[])
{
    disvm::vm_t vm;

    // Ignore the process name
    auto entry = create_entry_module(vm, argc - 1, argv + 1);

    vm.exec(std::move(entry));

    vm.spin_sleep_till_idle(std::chrono::milliseconds(100));

    return 0;
}

