//
// Dis VM
// File: vm.cpp
// Author: arr
//

#include <cassert>
#include <memory>
#include <algorithm>
#include <disvm.h>
#include <debug.h>
#include <runtime.h>
#include <exceptions.h>
#include <builtin_module.h>
#include <vm_version.h>
#include "scheduler.h"
#include "garbage_collector.h"
#include "tool_dispatch.h"

using namespace disvm;
using namespace disvm::runtime;

vm_t::vm_t(
    create_vm_interface_callback_t<runtime::vm_scheduler_t> create_scheduler,
    create_vm_interface_callback_t<runtime::vm_garbage_collector_t> create_gc)
{
    // Initialize built-in modules.
    builtin::initialize_builtin_modules();

    if (create_gc == nullptr)
        _gc = std::make_unique<default_garbage_collector_t>(*this);
    else
        _gc = create_gc(*this);

    if (create_scheduler == nullptr)
        _scheduler = std::make_unique<default_scheduler_t>(*this, default_system_thread_count, default_thread_quanta);
    else
        _scheduler = create_scheduler(*this);
}

vm_t::vm_t(uint32_t system_thread_count, uint32_t thread_quanta)
{
    // Initialize built-in modules.
    builtin::initialize_builtin_modules();

    _gc = std::make_unique<default_garbage_collector_t>(*this);
    _scheduler = std::make_unique<default_scheduler_t>(*this, system_thread_count, thread_quanta);
}

vm_t::~vm_t()
{
    // Release the scheduler first since it could be using the gc instance.
    _scheduler.reset();
    _gc.reset();

    std::lock_guard<std::mutex> lock{ _modules_lock };
    for (auto &m : _modules)
    {
        if (m.origin != nullptr)
            m.origin->release();
    }
}

vm_version_t vm_t::get_version() const
{
    return{ DISVM_VERSION_MAJOR, DISVM_VERSION_MINOR, DISVM_VERSION_PATCH, DISVM_VERSION_LABEL };
}

namespace
{
    std::unique_ptr<runtime::vm_thread_t> _create_thread_safe(std::unique_ptr<runtime::vm_module_ref_t> &entry_module_ref)
    {
        auto thread = std::make_unique<vm_thread_t>(*entry_module_ref, vm_t::root_vm_thread_id);

        // Now that the thread has been created and has inherited ownership of the pointer, relinquish the smart pointer's ownership.
        entry_module_ref->release(); // decrement the vm alloc ref count
        entry_module_ref.release(); // release the smart pointer

        return thread;
    }
}

const runtime::vm_thread_t& vm_t::exec(const char *path)
{
    assert(path != nullptr);
    auto entry_module = load_module(path);

    auto entry_module_ref = std::make_unique<vm_module_ref_t>(entry_module);
    auto thread = _create_thread_safe(entry_module_ref);
    return schedule_thread(std::move(thread));
}

const runtime::vm_thread_t& vm_t::exec(std::unique_ptr<runtime::vm_module_t> entry_module)
{
    assert(entry_module != nullptr);

    auto entry_module_ref = std::make_unique<vm_module_ref_t>(std::move(entry_module));
    auto thread = _create_thread_safe(entry_module_ref);
    return schedule_thread(std::move(thread));
}

const runtime::vm_thread_t& vm_t::fork(
    const uint32_t parent_tid,
    runtime::vm_module_ref_t &module_ref,
    const runtime::vm_frame_t &initial_frame,
    runtime::vm_pc_t initial_pc)
{
    if (initial_pc < 0 || static_cast<vm_pc_t>(module_ref.code_section.size()) <= initial_pc)
        throw vm_system_exception{ "Invalid entry program counter" };

    auto thread = std::make_unique<vm_thread_t>(module_ref, parent_tid, initial_frame, initial_pc);
    return schedule_thread(std::move(thread));
}

std::shared_ptr<runtime::vm_module_t> vm_t::load_module(const char *path)
{
    if (path == nullptr)
        throw vm_user_exception{ "Invalid module path" };

    auto new_module_iter = loaded_modules_t::const_iterator{};
    auto new_module = std::shared_ptr<vm_module_t>{};
    {
        std::lock_guard<std::mutex> lock{ _modules_lock };

        // Check if the module is already loaded.
        auto iter = std::begin(_modules);
        while (iter != std::end(_modules))
        {
            if (0 == ::strcmp(iter->origin->str(), path))
            {
                auto module = iter->module.lock();

                // If an entry exists but is null, the module was loaded before. Read the module again and return.
                if (module == nullptr)
                {
                    module = std::shared_ptr<vm_module_t>{ std::move(read_module(path)) };
                    module->vm_id = iter->vm_id;

                    iter->module = module;

                    debug::log_msg(debug::component_trace_t::module, debug::log_level_t::debug, "reload: vm module: >>%s<<\n", path);
                }

                return module;
            }

            ++iter;
        }

        if (path[0] == BUILTIN_MODULE_PREFIX_CHAR)
        {
            new_module = std::move(builtin::get_builtin_module(path));
        }
        else
        {
            // Read in the module
            new_module = std::move(read_module(path));
        }

        auto path_local = std::make_unique<vm_string_t>(std::strlen(path), reinterpret_cast<const uint8_t *>(path));

        auto vm_id_next = std::size_t{ 1 };
        if (!_modules.empty())
            vm_id_next = (_modules.front().vm_id + 1);

        new_module->vm_id = vm_id_next;
        _modules.push_front(std::move(loaded_vm_module_t{ vm_id_next, std::move(path_local), new_module }));
        new_module_iter = _modules.begin();
    }

    if (_tool_dispatch != nullptr)
    {
        std::lock_guard<std::mutex> lock{ _tool_dispatch_lock };
        if (_tool_dispatch != nullptr)
            _tool_dispatch->on_module_vm_load(*new_module_iter);
    }

    if (debug::is_component_tracing_enabled<debug::component_trace_t::module>())
        debug::log_msg(debug::component_trace_t::module, debug::log_level_t::debug, "load: vm module: >>%s<<\n", path);

    return new_module;
}

void vm_t::enum_loaded_modules(loaded_vm_module_callback_t callback) const
{
    if (callback == nullptr)
        throw vm_system_exception{ "Callback should not be null" };

    std::lock_guard<std::mutex> lock{ _modules_lock };
    for (auto &m : _modules)
    {
        if (!callback(m))
            return;
    }
}

runtime::vm_scheduler_control_t &vm_t::get_scheduler_control() const
{
    return _scheduler->get_controller();
}

runtime::vm_garbage_collector_t &vm_t::get_garbage_collector() const
{
    return *_gc;
}

void vm_t::spin_sleep_till_idle(std::chrono::milliseconds sleep_interval) const
{
    do
        std::this_thread::sleep_for(sleep_interval);
    while (!_scheduler->is_idle());
}

std::size_t vm_t::load_tool(std::shared_ptr<runtime::vm_tool_t> tool)
{
    std::lock_guard<std::mutex> lock{ _tool_dispatch_lock };

    if (_tool_dispatch == nullptr)
    {
        _tool_dispatch = std::make_unique<vm_tool_dispatch_t>(*this);

        // Attach to all threads
        _scheduler->set_tool_dispatch_on_all_threads(_tool_dispatch.get());
    }

    return _tool_dispatch->load_tool(std::move(tool));
}

void vm_t::unload_tool(std::size_t tool_id)
{
    std::lock_guard<std::mutex> lock{ _tool_dispatch_lock };

    if (_tool_dispatch == nullptr)
        return;

    const auto tool_count = _tool_dispatch->unload_tool(tool_id);
    if (tool_count == 0)
    {
        // Detach from all threads
        _scheduler->set_tool_dispatch_on_all_threads(nullptr);

        // Note that the tool dispatcher instance is _not_ freed.
        // This is done to ease scheduler implementations that are
        // unable to fully guarantee no vm threads have a handle to
        // the dispatcher instance.
    }
}

const runtime::vm_thread_t &vm_t::schedule_thread(std::unique_ptr<runtime::vm_thread_t> thread)
{
    assert(thread != nullptr);

    if (_tool_dispatch != nullptr)
    {
        std::lock_guard<std::mutex> lock{ _tool_dispatch_lock };
        if (_tool_dispatch != nullptr)
        {
            _tool_dispatch->on_thread_begin(*thread);
            thread->set_tool_dispatch(_tool_dispatch.get());
        }
    }

    return _scheduler->schedule_thread(std::move(thread));
}
