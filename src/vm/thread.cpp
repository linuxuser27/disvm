//
// Dis VM
// File: thread.cpp
// Author: arr
//

#include <cinttypes>
#include <cassert>
#include <memory>
#include <atomic>
#include <disvm.h>
#include <exceptions.h>
#include <vm_memory.h>
#include <debug.h>
#include <utils.h>
#include "execution_table.h"
#include "tool_dispatch.h"

using namespace disvm;
using namespace disvm::runtime;

namespace
{
#include "address_decoding.inc"

    // Update VM registers based on the instruction.
    void decode_address(const vm_exec_op_t &inst, vm_registers_t &reg)
    {
        decode_table[inst.addr_code](inst, reg);

#ifndef NDEBUG
        // This is a perf critical function so logging is only available in debug builds
        if (debug::is_component_tracing_enabled<debug::component_trace_t::addressing>())
        {
            debug::log_msg(
                debug::component_trace_t::addressing,
                debug::log_level_t::debug,
                "decode: registers: %d (%#" PRIxPTR " %#" PRIxPTR " %#" PRIxPTR ")",
                inst.opcode,
                reg.src,
                reg.mid,
                reg.dest);
        }
#endif
    }

    uint32_t get_unique_thread_id()
    {
        static std::atomic<uint32_t> thread_id_counter{ 1 };
        return thread_id_counter.fetch_add(1);
    }
}

vm_registers_t::vm_registers_t(
    vm_thread_t &thread,
    vm_module_ref_t &entry)
    : current_thread_state{ vm_thread_state_t::ready }
    , current_thread_quanta{ 0 }
    , trap_flags{ vm_trap_flags_t::none }
    , module_ref{ &entry }
    , mp_base{ entry.mp_base }
    , thread{ thread }
    , pc{ entry.module->header.entry_pc }
    , next_pc { entry.module->header.entry_pc }
    , stack{ static_cast<std::size_t>(entry.module->header.stack_extent) }
    , tool_dispatch{ nullptr }
    , src{ nullptr }
    , mid{ nullptr }
    , dest{ nullptr }
{
    // Add a reference to the module ref and MP register
    if (mp_base != nullptr)
        mp_base->add_ref();

    module_ref->add_ref();
}

vm_registers_t::~vm_registers_t()
{
    dec_ref_count_and_free(mp_base);
    debug::assign_debug_pointer(&mp_base);

    dec_ref_count_and_free(module_ref);
    debug::assign_debug_pointer(&module_ref);
}

void disvm::runtime::walk_stack(const vm_registers_t &r, vm_stack_walk_callback_t callback)
{
    assert(callback != nullptr);

    auto current_pc = r.pc;
    auto current_module = r.module_ref;
    auto current_frame = r.stack.peek_frame();
    while (current_frame != nullptr)
    {
        if (!callback(current_frame->base(), current_pc, *current_module))
            break;

        current_pc = current_frame->prev_pc();
        auto prev_module_maybe = current_frame->prev_module_ref();
        if (prev_module_maybe != nullptr)
            current_module = prev_module_maybe;

        current_frame = current_frame->prev_frame();
    }
}

std::shared_ptr<const type_descriptor_t> vm_thread_t::type_desc()
{
    return intrinsic_type_desc::type<vm_thread_t>();
}

vm_thread_t::vm_thread_t(
    vm_module_ref_t &entry,
    uint32_t parent_thread_id)
    : vm_alloc_t(vm_thread_t::type_desc())
    , _error_message{ nullptr }
    , _parent_thread_id{ parent_thread_id }
    , _registers{ *this, entry }
    , _thread_id{ get_unique_thread_id() }
{
    assert(_parent_thread_id != _thread_id);
    assert(static_cast<std::size_t>(_registers.pc) < entry.code_section.size());

    // Set up the stack
    const auto entry_type = entry.module->header.entry_type;
    auto frame_type = entry.type_section[entry_type];
    _registers.stack.alloc_frame(frame_type);

    // Pushing the initial frame sets the FP register
    _registers.stack.push_frame();

    debug::log_msg(debug::component_trace_t::thread, debug::log_level_t::debug, "init: vm thread: %d %d", _thread_id, _parent_thread_id);
}

vm_thread_t::vm_thread_t(
    vm_module_ref_t &entry,
    uint32_t parent_thread_id,
    const vm_frame_t &initial_frame,
    vm_pc_t start_pc)
    : vm_alloc_t(vm_thread_t::type_desc())
    , _error_message{ nullptr }
    , _parent_thread_id{ parent_thread_id }
    , _registers{ *this, entry }
    , _thread_id{ get_unique_thread_id() }
{
    _registers.pc = start_pc;

    assert(_parent_thread_id != _thread_id);
    assert(static_cast<std::size_t>(_registers.pc) < entry.code_section.size());

    // Set the stack
    auto frame_type = initial_frame.frame_type;
    _registers.stack.alloc_frame(frame_type);

    // Pushing the initial frame sets the FP register
    auto current_frame = _registers.stack.push_frame();

    //  Copy over the frame into this thread - pass arguments to the thread
    current_frame->copy_frame_contents(initial_frame);

    if (debug::is_component_tracing_enabled<debug::component_trace_t::thread>())
        debug::log_msg(debug::component_trace_t::thread, debug::log_level_t::debug, "init: vm thread: %d %d", _thread_id, _parent_thread_id);
}

vm_thread_t::~vm_thread_t()
{
    if (debug::is_component_tracing_enabled<debug::component_trace_t::thread>())
        debug::log_msg(debug::component_trace_t::thread, debug::log_level_t::debug, "destroy: vm thread: %d %d", _thread_id, _parent_thread_id);

    free_memory(_error_message);
    debug::assign_debug_pointer(&_error_message);
}

namespace
{
    struct execute_with_tool_t final
    {
        static void begin_exec_loop(vm_registers_t &r, vm_t &)
        {
            auto tool_dispatch = r.tool_dispatch.load();
            assert(tool_dispatch != nullptr);

            tool_dispatch->block_if_thread_suspended(r.thread.get_thread_id());
        }

        static void after_exec(vm_registers_t &r, vm_t &)
        {
            if (r.trap_flags == vm_trap_flags_t::none
                || !util::has_flag(r.trap_flags, vm_trap_flags_t::instruction))
                return;

            auto tool_dispatch = r.tool_dispatch.load();
            assert(tool_dispatch != nullptr);

            tool_dispatch->on_trap(r, vm_trap_flags_t::instruction);
        }
    };

    struct execute_normal_t final
    {
        static void begin_exec_loop(vm_registers_t &, vm_t &) { }
        static void after_exec(vm_registers_t &, vm_t &) { }
    };

    template<typename EXEC_DETOUR>
    void execute_impl(vm_registers_t &r, vm_t &vm)
    {
        for (; 0 != r.current_thread_quanta; --r.current_thread_quanta)
        {
            EXEC_DETOUR::begin_exec_loop(r, vm);
            assert(r.stack.peek_frame() != nullptr && "Thread state should not be running with empty stack");

            const auto &code_section = r.module_ref->code_section;
            assert(static_cast<std::size_t>(r.pc) < code_section.size());

            assert(!r.module_ref->is_builtin_module() && "Interpreter thread is unable to execute native instructions");
            const auto &inst = code_section[r.pc].op;
            const auto opcode = static_cast<std::size_t>(inst.opcode);
            if (static_cast<std::size_t>(opcode_t::last_opcode) < opcode)
                throw vm_system_exception{ "Unknown op-code" };

            // Instruction is valid, decode and update registers for operation
            decode_address(inst, r);
            r.next_pc = (r.pc + 1);
            vm_exec_table[opcode](r, vm);
            r.pc = r.next_pc;

            EXEC_DETOUR::after_exec(r, vm);

            if (r.current_thread_state != vm_thread_state_t::running)
                break;
        }
    }
}

vm_thread_state_t vm_thread_t::execute(vm_t &vm, const uint32_t quanta)
{
    const auto max_error_message = std::size_t{ 1024 };
    _registers.current_thread_state = vm_thread_state_t::running;
    _registers.current_thread_quanta = quanta;
    assert(_registers.pc != runtime_constants::invalid_program_counter);
    auto tool_dispatch = _registers.tool_dispatch.load();

    try
    {
        if (tool_dispatch == nullptr)
            execute_impl<execute_normal_t>(_registers, vm);
        else
            execute_impl<execute_with_tool_t>(_registers, vm);
    }
    catch (const vm_term_request &)
    {
        throw;
    }
    catch (const unhandled_user_exception &uue)
    {
        _error_message = calloc_memory<char>(max_error_message);
        const int ec = std::snprintf(_error_message, max_error_message, "%s in %s @%d\n  %s", uue.what(), uue.module_name, uue.program_counter, uue.exception_id);
        assert(ec > 0 && ec < max_error_message);
        (void)ec;
        _registers.current_thread_state = vm_thread_state_t::broken;
    }
    catch (const index_out_of_range_memory &ioor)
    {
        _error_message = calloc_memory<char>(max_error_message);
        const int ec = std::snprintf(_error_message, max_error_message, "%s - %d [%d,%d]", ioor.what(), ioor.invalid_value, ioor.valid_min, ioor.valid_max);
        assert(ec > 0 && ec < max_error_message);
        (void)ec;
        _registers.current_thread_state = vm_thread_state_t::broken;
    }
    catch (const vm_user_exception &ue)
    {
        _error_message = calloc_memory<char>(max_error_message);
        const int ec = std::snprintf(_error_message, max_error_message, "%s", ue.what());
        assert(ec > 0 && ec < max_error_message);
        (void)ec;
        _registers.current_thread_state = vm_thread_state_t::broken;
    }

    // Log the execution result
    if (debug::is_component_tracing_enabled<debug::component_trace_t::thread>())
    {
        debug::log_msg(
            debug::component_trace_t::thread,
            debug::log_level_t::debug,
            "status: vm thread: execute: %d %d %d %d",
            _thread_id,
            (quanta - _registers.current_thread_quanta),
            quanta,
            _registers.current_thread_state);
    }

    switch (_registers.current_thread_state)
    {
    case vm_thread_state_t::running:
        _registers.current_thread_state = (tool_dispatch == nullptr) ? vm_thread_state_t::ready : vm_thread_state_t::debug;
        break;

    case vm_thread_state_t::empty_stack:
    case vm_thread_state_t::exiting:
        if (tool_dispatch != nullptr)
            tool_dispatch->on_thread_end(*this);
        break;

    case vm_thread_state_t::broken:
        if (tool_dispatch != nullptr)
            tool_dispatch->on_thread_broken(*this);
        break;

    default:
        // Let other thread states fall through
        break;
    }

    return _registers.current_thread_state;
}

void vm_thread_t::set_tool_dispatch(vm_tool_dispatch_t *dispatch)
{
    assert((_registers.tool_dispatch.load() == nullptr) != (dispatch == nullptr) && "Dispatch value should toggle between null and non-null");
    _registers.tool_dispatch = dispatch;
}

const char *vm_thread_t::get_error_message() const
{
    return _error_message;
}

uint32_t vm_thread_t::get_thread_id() const
{
    return _thread_id;
}

uint32_t vm_thread_t::get_parent_thread_id() const
{
    return _parent_thread_id;
}

const vm_registers_t& vm_thread_t::get_registers() const
{
    return _registers;
}
