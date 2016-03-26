//
// Dis VM
// File: vm_exception_handler.cpp
// Author: arr
//

#include <cstdint>
#include <runtime.h>
#include <vm_memory.h>
#include <exceptions.h>
#include <debug.h>

using namespace disvm;
using namespace disvm::runtime;

std::tuple<const handler_t *, vm_frame_t *, vm_pc_t> disvm::runtime::find_exception_handler(
    const vm_stack_t &stack,
    const vm_module_ref_t &faulting_module,
    const vm_pc_t faulting_pc,
    const vm_string_t &exception_id)
{
    auto unwind_depth = uint32_t{ 0 };
    auto current_module = &faulting_module;
    auto current_pc = faulting_pc;
    auto current_frame = stack.peek_frame();

    while (current_frame != nullptr)
    {
        debug::log_msg(debug::component_trace_t::exception, debug::log_level_t::debug, "begin: handler: %d\n", unwind_depth);

        auto &handler_section = current_module->module->handler_section;

        for (auto &handler : handler_section)
        {
            // Outside pc range of this handler
            if (current_pc < handler.begin_pc || handler.end_pc <= current_pc)
                continue;

            // Find the appropriate exception case
            for (const auto &exception : handler.exception_table)
            {
                auto exception_name = exception.name.get();
                if (exception_id.compare_to(exception_name) == 0)
                {
                    debug::log_msg(debug::component_trace_t::exception, debug::log_level_t::debug, "end: handler: match: %d\n", unwind_depth);
                    return std::make_tuple(&handler, current_frame, exception.pc);
                }
                else if (exception_name == nullptr && exception.pc != runtime_constants::invalid_program_counter)
                {
                    debug::log_msg(debug::component_trace_t::exception, debug::log_level_t::debug, "end: handler: fallback: %d\n", unwind_depth);
                    return std::make_tuple(&handler, current_frame, exception.pc);
                }
            }
        }

        // Decrement the frame program counter. See raise instruction for details.
        current_pc = static_cast<word_t>(current_frame->prev_pc() - 1);

        // Update the current module if it changes walking up the stack.
        auto prev_mod_ref = current_frame->prev_module_ref();
        if (prev_mod_ref != nullptr)
            current_module = prev_mod_ref;

        // Move up the stack.
        current_frame = current_frame->prev_frame();
        ++unwind_depth;
    }

    auto faulting_module_name = faulting_module.module->module_name->str();
    throw unhandled_user_exception{ exception_id.str(), faulting_module_name, faulting_pc };
}