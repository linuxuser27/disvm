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
#include "execution_table.h"

using namespace disvm;
using namespace disvm::runtime;

namespace
{
    // Update VM registers based on the instruction.
    void decode_address(const vm_exec_op_t &inst, vm_registers_t &reg)
    {
        switch (inst.source.mode)
        {
        case address_mode_t::offset_indirect_fp:
            reg.src = reinterpret_cast<pointer_t>(reinterpret_cast<uint8_t *>(reg.stack.peek_frame()->base()) + inst.source.register1);
            break;
        case address_mode_t::offset_indirect_mp:
            reg.src = reinterpret_cast<pointer_t>(reinterpret_cast<uint8_t *>(reg.mp_base->allocation()) + inst.source.register1);
            break;
        case address_mode_t::offset_double_indirect_fp:
        {
            const auto frame_offset = *reinterpret_cast<std::size_t *>(reinterpret_cast<uint8_t *>(reg.stack.peek_frame()->base()) + inst.source.register1);
            reg.src = reinterpret_cast<pointer_t>(frame_offset + inst.source.register2);
            break;
        }
        case address_mode_t::offset_double_indirect_mp:
        {
            const auto mp_offset = *reinterpret_cast<std::size_t *>(reinterpret_cast<uint8_t *>(reg.mp_base->allocation()) + inst.source.register1);
            reg.src = reinterpret_cast<pointer_t>(mp_offset + inst.source.register2);
            break;
        }
        case address_mode_t::immediate:
            reg.src = reinterpret_cast<pointer_t>(const_cast<word_t *>(&inst.source.register1));
            break;
        case address_mode_t::none:
            reg.src = nullptr;
            break;
        default:
            assert(false && "Unknown source addressing");
        }

        switch (inst.destination.mode)
        {
        case address_mode_t::offset_indirect_fp:
            reg.dest = reinterpret_cast<pointer_t>(reinterpret_cast<uint8_t *>(reg.stack.peek_frame()->base()) + inst.destination.register1);
            break;
        case address_mode_t::offset_indirect_mp:
            reg.dest = reinterpret_cast<pointer_t>(reinterpret_cast<uint8_t *>(reg.mp_base->allocation()) + inst.destination.register1);
            break;
        case address_mode_t::offset_double_indirect_fp:
        {
            const auto frame_offset = *reinterpret_cast<std::size_t *>(reinterpret_cast<uint8_t *>(reg.stack.peek_frame()->base()) + inst.destination.register1);
            reg.dest = reinterpret_cast<pointer_t>(frame_offset + inst.destination.register2);
            break;
        }
        case address_mode_t::offset_double_indirect_mp:
        {
            const auto mp_offset = *reinterpret_cast<std::size_t *>(reinterpret_cast<uint8_t *>(reg.mp_base->allocation()) + inst.destination.register1);
            reg.dest = reinterpret_cast<pointer_t>(mp_offset + inst.destination.register2);
            break;
        }
        case address_mode_t::immediate:
            reg.dest = reinterpret_cast<pointer_t>(const_cast<word_t *>(&inst.destination.register1));
            break;
        case address_mode_t::none:
            reg.dest = nullptr;
            break;
        default:
            assert(false && "Unknown destination addressing");
        }

        switch (inst.middle.mode)
        {
        case address_mode_middle_t::small_offset_indirect_fp:
            reg.mid = reinterpret_cast<pointer_t>(reinterpret_cast<uint8_t *>(reg.stack.peek_frame()->base()) + inst.middle.register1);
            break;
        case address_mode_middle_t::small_offset_indirect_mp:
            reg.mid = reinterpret_cast<pointer_t>(reinterpret_cast<uint8_t *>(reg.mp_base->allocation()) + inst.middle.register1);
            break;
        case address_mode_middle_t::small_immediate:
            reg.mid = reinterpret_cast<pointer_t>(const_cast<word_t *>(&inst.middle.register1));
            break;
        case address_mode_middle_t::none:
            // [SPEC] This is an undocumented expectation but required in many operations (e.g. i++).
            reg.mid = reg.dest;
            break;
        default:
            assert(false && "Unknown middle addressing");
        }

#ifndef NDEBUG
        // This is a perf critical function so logging is only available in debug builds
        if (debug::is_component_tracing_enabled<debug::component_trace_t::addressing>())
        {
            debug::log_msg(
                debug::component_trace_t::addressing,
                debug::log_level_t::debug,
                "decode: registers: %d (%#" PRIxPTR " %#" PRIxPTR " %#" PRIxPTR ")\n",
                inst.opcode,
                reg.src,
                reg.mid,
                reg.dest);
        }
#endif
    }

    std::atomic<uint32_t> thread_id_counter(1);

    uint32_t get_unique_thread_id()
    {
        return thread_id_counter.fetch_add(1);
    }
}

vm_registers_t::vm_registers_t(
    const vm_thread_t &thread,
    vm_module_ref_t &entry)
    : current_thread_state{ vm_thread_state_t::ready }
    , module_ref{ &entry }
    , mp_base{ entry.mp_base }
    , thread{ thread }
    , pc{ entry.module->header.entry_pc }
    , stack{ entry.module->header.stack_extent }
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

std::shared_ptr<const type_descriptor_t> vm_thread_t::type_desc()
{
    return intrinsic_type_desc::type<vm_thread_t>();
}

vm_thread_t::vm_thread_t(
    vm_module_ref_t &entry,
    uint32_t parent_thread_id)
    : vm_alloc_t(vm_thread_t::type_desc())
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

    debug::log_msg(debug::component_trace_t::thread, debug::log_level_t::debug, "init: vm thread: %d %d\n", _thread_id, _parent_thread_id);
}

vm_thread_t::vm_thread_t(
    vm_module_ref_t &entry,
    const vm_frame_t &initial_frame,
    vm_pc_t start_pc,
    uint32_t parent_thread_id)
    : vm_alloc_t(vm_thread_t::type_desc())
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

    debug::log_msg(debug::component_trace_t::thread, debug::log_level_t::debug, "init: vm thread: %d %d\n", _thread_id, _parent_thread_id);
}

vm_thread_t::~vm_thread_t()
{
    debug::log_msg(debug::component_trace_t::thread, debug::log_level_t::debug, "destroy: vm thread: %d %d\n", _thread_id, _parent_thread_id);
}

vm_thread_state_t vm_thread_t::execute(vm_t &virtual_machine, uint32_t quanta)
{
    _registers.current_thread_state = vm_thread_state_t::running;
    assert(_registers.pc != runtime_constants::invalid_program_counter);

    auto execution_duration = uint32_t{ 0 };

    try
    {
        for (; execution_duration < quanta; ++execution_duration)
        {
            if (_registers.stack.peek_frame() == nullptr)
            {
                debug::log_msg(debug::component_trace_t::thread, debug::log_level_t::debug, "exit: vm thread: stack exhausted: %d\n", _thread_id);
                _registers.current_thread_state = vm_thread_state_t::empty_stack;
                break;
            }

            const auto &code_section = _registers.module_ref->code_section;
            assert(static_cast<std::size_t>(_registers.pc) < code_section.size());

            const auto &inst = code_section[_registers.pc].op;
            decode_address(inst, _registers);
            _registers.pc++;
            vm_exec_table[static_cast<std::size_t>(inst.opcode)](_registers, virtual_machine);

            if (_registers.current_thread_state != vm_thread_state_t::running)
                break;
        }
    }
    catch (const unhandled_user_exception &uue)
    {
        // [TODO] Store the exception
        debug::log_msg(debug::component_trace_t::thread, debug::log_level_t::debug, "broken: vm thread: unhandled: >>%s<< %d >>%s<<\n", uue.exception_id, uue.program_counter, uue.module_name);
        _registers.current_thread_state = vm_thread_state_t::broken;
    }
    catch (const vm_user_exception &ue)
    {
        // [TODO] Store the exception
        debug::log_msg(debug::component_trace_t::thread, debug::log_level_t::debug, "broken: vm thread: >>%s<<\n", ue.what());
        _registers.current_thread_state = vm_thread_state_t::broken;
    }

    // Log the execution result
    if (debug::is_component_tracing_enabled<debug::component_trace_t::thread>())
    {
        debug::log_msg(
            debug::component_trace_t::thread,
            debug::log_level_t::debug,
            "status: vm thread: execute: %d %d %d %d\n",
            _thread_id,
            execution_duration,
            quanta,
            _registers.current_thread_state);
    }

    // If the thread is still in the running state transition it back to ready.
    if (_registers.current_thread_state == vm_thread_state_t::running)
        _registers.current_thread_state = vm_thread_state_t::ready;

    return _registers.current_thread_state;
}

vm_thread_state_t vm_thread_t::get_state() const
{
    return _registers.current_thread_state;
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