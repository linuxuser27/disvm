//
// Dis VM
// File: tool_dispatch.cpp
// Author: arr
//

#include <cassert>
#include <thread>
#include <vector>
#include <debug.hpp>
#include <disvm.hpp>
#include <vm_tools.hpp>
#include <exceptions.hpp>
#include <utils.hpp>
#include "tool_dispatch.hpp"

using disvm::vm_t;
using disvm::opcode_t;
using disvm::loaded_vm_module_t;

using disvm::debug::component_trace_t;
using disvm::debug::log_level_t;

using disvm::runtime::cookie_t;
using disvm::runtime::vm_pc_t;
using disvm::runtime::vm_module_t;
using disvm::runtime::vm_thread_t;
using disvm::runtime::vm_registers_t;
using disvm::runtime::vm_alloc_t;
using disvm::runtime::vm_string_t;
using disvm::runtime::vm_tool_t;
using disvm::runtime::vm_tool_dispatch_t;
using disvm::runtime::vm_trap_flags_t;
using disvm::runtime::vm_event_t;
using disvm::runtime::vm_event_context_t;
using disvm::runtime::vm_event_callback_t;
using disvm::runtime::breakpoint_details_t;

// Empty destructor for vm tool 'interface'
vm_tool_t::~vm_tool_t()
{
}

vm_tool_dispatch_t::vm_tool_dispatch_t(vm_t &vm)
    : _threads_suspended{ false }
    , _threads_suspended_count{ 0 }
    , _vm { vm }
{
    _events.cookie_counter = 1;
    _breakpoints.cookie_counter = 1;
}

vm_tool_dispatch_t::~vm_tool_dispatch_t()
{
    std::lock_guard<std::mutex> lock{ _tools_lock };
    for (auto &t : _tools)
        t.second->on_unload();
}

std::size_t vm_tool_dispatch_t::load_tool(std::shared_ptr<vm_tool_t> tool)
{
    static std::size_t _tool_id_counter{ 1 };

    if (tool == nullptr)
        throw vm_system_exception{ "Tool cannot be null" };

    // Keep the lock for the collection until the tool has been completed being loaded.
    std::lock_guard<std::mutex> lock{ _tools_lock };
    const auto current_id = _tool_id_counter++;
    _tools[current_id] = tool;

    try
    {
        // [TODO] For debug builds, it might be useful to wrap the controller in a proxy that records
        // the subscribed callbacks and asserts they are all properly unsubscribed during agent unload.
        tool->on_load(*this, current_id);
    }
    catch (...)
    {
        if (disvm::debug::is_component_tracing_enabled<component_trace_t::tool>())
            disvm::debug::log_msg(component_trace_t::tool, log_level_t::warning, "load: tool: failure");

        // The ID is tainted and will not be re-used
        _tools.erase(current_id);
        throw;
    }

    if (disvm::debug::is_component_tracing_enabled<component_trace_t::tool>())
        disvm::debug::log_msg(component_trace_t::tool, log_level_t::debug, "load: tool: %d", current_id);

    return current_id;
}

std::size_t vm_tool_dispatch_t::unload_tool(std::size_t tool_id)
{
    // Keep the lock for the collection until the tool has been completed being unloaded.
    std::lock_guard<std::mutex> lock{ _tools_lock };
    auto iter = _tools.find(tool_id);
    if (iter == _tools.cend())
        return _tools.size();

    auto tool = iter->second;
    _tools.erase(iter);

    if (disvm::debug::is_component_tracing_enabled<component_trace_t::tool>())
        disvm::debug::log_msg(component_trace_t::tool, log_level_t::debug, "unload: tool: %d", tool_id);

    assert(tool != nullptr);
    tool->on_unload();

    return _tools.size();
}

namespace
{
    DEFINE_ENUM_FLAG_OPERATORS(vm_trap_flags_t);
}

void vm_tool_dispatch_t::on_trap(vm_registers_t &r, vm_trap_flags_t f)
{
    vm_event_context_t cxt{};
    cxt.value1.registers = &r;
    cxt.value2.trap = f;

    // Clear the trap flag
    r.trap_flags &= ~f;
    _events.fire_event_callbacks(vm_event_t::trap, cxt);
}

opcode_t vm_tool_dispatch_t::on_breakpoint(vm_registers_t &r)
{
    opcode_t original_op;
    cookie_t cookie_id;
    std::tie(original_op, cookie_id) = get_original_opcode_and_cookie(r);

    vm_event_context_t cxt{};
    cxt.value1.registers = &r;
    cxt.value2.cookie = cookie_id;

    _events.fire_event_callbacks(vm_event_t::breakpoint, cxt);

    return original_op;
}

void vm_tool_dispatch_t::on_exception_raised(vm_registers_t &r, const vm_string_t &id, vm_alloc_t &e)
{
    vm_event_context_t cxt{};
    cxt.value1.registers = &r;
    cxt.value2.str = &id;
    cxt.value3.alloc = &e;

    _events.fire_event_callbacks(vm_event_t::exception_raised, cxt);
}

void vm_tool_dispatch_t::on_exception_unhandled(vm_registers_t &r, const vm_string_t &id, vm_alloc_t &e)
{
    vm_event_context_t cxt{};
    cxt.value1.registers = &r;
    cxt.value2.str = &id;
    cxt.value3.alloc = &e;

    _events.fire_event_callbacks(vm_event_t::exception_unhandled, cxt);
}

void vm_tool_dispatch_t::on_module_vm_load(const loaded_vm_module_t &m)
{
    vm_event_context_t cxt{};
    cxt.value1.loaded_mod = &m;

    _events.fire_event_callbacks(vm_event_t::module_vm_load, cxt);
}

void vm_tool_dispatch_t::on_module_thread_load(vm_registers_t &r, std::shared_ptr<vm_module_t> &m)
{
    assert(m != nullptr);

    vm_event_context_t cxt{};
    cxt.value1.registers = &r;
    cxt.value2.module = &m;

    _events.fire_event_callbacks(vm_event_t::module_thread_load, cxt);
}

void vm_tool_dispatch_t::on_thread_begin(vm_thread_t &t)
{
    vm_event_context_t cxt{};
    cxt.value1.thread = &t;

    _events.fire_event_callbacks(vm_event_t::thread_begin, cxt);
}

void vm_tool_dispatch_t::on_thread_end(vm_thread_t &t)
{
    vm_event_context_t cxt{};
    cxt.value1.thread = &t;

    _events.fire_event_callbacks(vm_event_t::thread_end, cxt);
}

void vm_tool_dispatch_t::on_thread_broken(vm_thread_t &t)
{
    vm_event_context_t cxt{};
    cxt.value1.thread = &t;

    _events.fire_event_callbacks(vm_event_t::thread_broken, cxt);
}

void vm_tool_dispatch_t::block_if_thread_suspended(uint32_t thread_id)
{
    if (!_threads_suspended)
        return;

    std::unique_lock<std::mutex> resume_lock{ _threads_resume_lock };

    _threads_suspended_count++;

    while (_threads_suspended)
        _threads_resume.wait(resume_lock);

    _threads_suspended_count--;
}

vm_t& vm_tool_dispatch_t::get_vm_instance() const
{
    return _vm;
}

cookie_t vm_tool_dispatch_t::subscribe_event(vm_event_t evt, vm_event_callback_t cb)
{
    if (!cb)
        throw vm_system_exception{ "Invalid callback for event" };

    std::lock_guard<std::mutex> lock{ _events.event_lock };

    const auto cookie_id = _events.cookie_counter++;

    // Store the event callback
    auto &event_callbacks = _events.callbacks[evt];
    event_callbacks[cookie_id] = cb;

    // Store the cookie with the associate event
    _events.cookie_to_event[cookie_id] = evt;

    if (disvm::debug::is_component_tracing_enabled<component_trace_t::tool>())
        disvm::debug::log_msg(component_trace_t::tool, log_level_t::debug, "subscribe: event: %d %d", evt, cookie_id);

    return cookie_id;
}

void vm_tool_dispatch_t::unsubscribe_event(cookie_t cookie_id)
{
    std::lock_guard<std::mutex> lock{ _events.event_lock };

    auto iter = _events.cookie_to_event.find(cookie_id);
    if (iter == _events.cookie_to_event.cend())
        return;

    // Erase cookie and event
    const auto evt = iter->second;
    auto &event_callbacks = _events.callbacks[evt];
    event_callbacks.erase(cookie_id);
    _events.cookie_to_event.erase(iter);

    if (disvm::debug::is_component_tracing_enabled<component_trace_t::tool>())
        disvm::debug::log_msg(component_trace_t::tool, log_level_t::debug, "unsubscribe: event: %d %d", evt, cookie_id);
}

cookie_t vm_tool_dispatch_t::set_breakpoint(std::shared_ptr<vm_module_t> module, vm_pc_t pc)
{
    if (module == nullptr || util::has_flag(module->header.runtime_flag, runtime_flags_t::builtin))
        throw vm_system_exception{ "Unable to set breakpoint in supplied module" };

    auto &code_section = module->code_section;
    if (pc >= static_cast<vm_pc_t>(code_section.size()))
        throw vm_system_exception{ "Invalid PC for module" };

    const auto real_opcode = code_section[pc].op.opcode;

    {
        std::lock_guard<std::mutex> lock{ _breakpoints.lock };
        auto &pc_map = _breakpoints.original_opcodes[reinterpret_cast<std::uintptr_t>(module.get())];

        // If the real opcode is already a breakpoint and it is known, return the current cookie.
        if (real_opcode == opcode_t::brkpt)
        {
            auto iter = pc_map.find(pc);
            if (iter != pc_map.cend())
                return iter->second.second;
        }

        const auto cookie_id = _breakpoints.cookie_counter++;

        // Map the cookie to the module/pc pair.
        _breakpoints.cookie_to_details[cookie_id] = { module, pc, real_opcode };

        // Record the original opcode
        pc_map[pc] = std::make_pair(real_opcode, cookie_id);

        // Replace the current opcode with breakpoint
        code_section[pc].op.opcode = opcode_t::brkpt;

        if (disvm::debug::is_component_tracing_enabled<component_trace_t::tool>())
            disvm::debug::log_msg(component_trace_t::tool, log_level_t::debug, "breakpoint: set: %d %d >>%s<<", cookie_id, pc, module->module_name->str());

        return cookie_id;
    }
}

breakpoint_details_t vm_tool_dispatch_t::get_breakpoint_details(cookie_t cookie_id) const
{
    std::lock_guard<std::mutex> lock{ _breakpoints.lock };

    auto iter_cookie = _breakpoints.cookie_to_details.find(cookie_id);
    if (iter_cookie == _breakpoints.cookie_to_details.cend())
        throw vm_system_exception{ "Unknown breakpoint cookie" };

    return iter_cookie->second;
}

void vm_tool_dispatch_t::clear_breakpoint(cookie_t cookie_id)
{
    std::lock_guard<std::mutex> lock{ _breakpoints.lock };

    auto iter_cookie = _breakpoints.cookie_to_details.find(cookie_id);
    if (iter_cookie == _breakpoints.cookie_to_details.cend())
        throw vm_system_exception{ "Unknown breakpoint cookie" };

    auto details = iter_cookie->second;
    const auto target_pc = details.pc;

    // Determine the original opcode
    auto iter_orig = _breakpoints.original_opcodes.find(reinterpret_cast<std::uintptr_t>(details.module.get()));
    assert(iter_orig != _breakpoints.original_opcodes.cend());

    auto &pc_map = iter_orig->second;
    auto iter_pc = pc_map.find(target_pc);
    assert(iter_pc != pc_map.cend());

    auto original_opcode = iter_pc->second.first;

    // Replace the breakpoint opcode with the original
    auto &code_section = const_cast<code_section_t &>(details.module->code_section);
    assert(code_section[target_pc].op.opcode == opcode_t::brkpt);
    code_section[target_pc].op.opcode = original_opcode;

    if (disvm::debug::is_component_tracing_enabled<component_trace_t::tool>())
        disvm::debug::log_msg(component_trace_t::tool, log_level_t::debug, "breakpoint: unset: %d %d >>%s<<", cookie_id, target_pc, details.module->module_name->str());

    // Clean-up the PC mapping to opcode
    pc_map.erase(iter_pc);

    // If the PC map is empty, erase the module mapping as well
    if (pc_map.size() == 0)
        _breakpoints.original_opcodes.erase(iter_orig);

    // Remove the cookie map
    _breakpoints.cookie_to_details.erase(iter_cookie);
}

void vm_tool_dispatch_t::suspend_all_threads()
{
    std::lock(_threads_suspend_lock, _threads_resume_lock);
    std::lock_guard<std::mutex> suspend_lock{ _threads_suspend_lock, std::adopt_lock };
    std::lock_guard<std::mutex> resume_lock{ _threads_resume_lock, std::adopt_lock };

    _threads_suspended = true;

    const auto &sched = _vm.get_scheduler_control();

    const auto sys_thread_count = static_cast<int32_t>(sched.get_system_thread_count());
    assert(sys_thread_count != 0);

    // Sleep and check until we account for all system threads associated with the VM.
    auto suspended = _threads_suspended_count.load();
    while ((suspended + 1) < sys_thread_count)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{ 20 });
        suspended = _threads_suspended_count.load();
    }

    // If the suspended count is ever equal to the system thread count, then a non-vm thread
    // is inside the tool dispatcher which may be okay, but it indicates a type of
    // non-invasive debugging that was not originally incorporated into this
    // dispatcher design.
    assert(suspended < sys_thread_count);
}

void vm_tool_dispatch_t::resume_all_threads()
{
    std::lock(_threads_suspend_lock, _threads_resume_lock);
    std::lock_guard<std::mutex> suspend_lock{ _threads_suspend_lock, std::adopt_lock };

    _threads_suspended = false;

    _threads_resume_lock.unlock();
    _threads_resume.notify_all();
}

void vm_tool_dispatch_t::set_thread_trap_flag(const vm_registers_t &r, vm_trap_flags_t flag)
{
    std::lock_guard<std::mutex> suspend_lock{ _threads_resume_lock };
    if (!_threads_suspended)
        throw vm_system_exception{ "Unable to set thread trap flag while threads are not suspended" };

    const_cast<vm_trap_flags_t &>(r.trap_flags) |= flag;
}

std::tuple<opcode_t, cookie_t> vm_tool_dispatch_t::get_original_opcode_and_cookie(const vm_registers_t &r)
{
    const auto module = r.module_ref->module.get();

    if (module == nullptr || util::has_flag(module->header.runtime_flag, runtime_flags_t::builtin))
        throw vm_system_exception{ "Unable to determine original opcode in supplied module" };

    std::lock_guard<std::mutex> lock{ _breakpoints.lock };

    auto iter_orig = _breakpoints.original_opcodes.find(reinterpret_cast<std::uintptr_t>(module));
    if (iter_orig != _breakpoints.original_opcodes.cend())
    {
        auto &pc_map = iter_orig->second;
        auto iter_pc = pc_map.find(r.pc);
        if (iter_pc != pc_map.cend())
            return iter_pc->second;
    }

    assert(false && "Why are we missing this information?");
    return std::make_tuple(opcode_t::runt, 0); // Return NOP instruction?
}

void vm_tool_dispatch_t::events_t::fire_event_callbacks(vm_event_t event_type, vm_event_context_t &cxt)
{
    auto event_callbacks = std::vector<vm_event_callback_t>{};

    {
        std::lock_guard<std::mutex> lock_local{ event_lock };

        auto &event_callbacks_tmp = callbacks[event_type];
        for (auto &c : event_callbacks_tmp)
        {
            assert(static_cast<bool>(c.second) && "Why is callback empty?");
            event_callbacks.push_back(c.second);
        }
    }

    for (auto &c : event_callbacks)
        c(event_type, cxt);
}
