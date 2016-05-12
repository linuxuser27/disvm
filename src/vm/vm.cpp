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

using namespace disvm;
using namespace disvm::runtime;

vm_t::loaded_module_t::loaded_module_t(std::unique_ptr<runtime::vm_string_t> origin, std::shared_ptr<runtime::vm_module_t> module)
    : origin{ std::move(origin) }
    , module{ module }
{
    assert(this->origin != nullptr);
}

vm_t::loaded_module_t::loaded_module_t(loaded_module_t &&other)
    : origin{ std::move(other.origin) }
    , module{ std::move(other.module) }
{
}

vm_t::loaded_module_t::~loaded_module_t()
{
    if (origin != nullptr)
        origin->release();
}

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
    {
        const auto system_thread_count = uint32_t{ 1 };

        // The Inferno implementation defined the thread quanta as 2048 (include/interp.h)
        const auto thread_quanta = uint32_t{ 2048 };

        _scheduler = std::make_unique<default_scheduler_t>(*this, system_thread_count, thread_quanta);
    }
    else
    {
        _scheduler = create_scheduler(*this);
    }
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
}

vm_version_t vm_t::get_version() const
{
    return{ DISVM_VERSION_MAJOR, DISVM_VERSION_MINOR, DISVM_VERSION_PATCH, DISVM_VERSION_LABEL };
}

namespace
{
    const runtime::vm_thread_t& _exec(runtime::vm_scheduler_t &s, std::unique_ptr<runtime::vm_module_ref_t> &entry_module_ref)
    {
        auto thread = std::make_unique<vm_thread_t>(*entry_module_ref, vm_t::root_vm_thread_id);

        // Now that the thread has been created and has inherited ownership of the pointer, relinquish the smart pointer's ownership.
        entry_module_ref->release(); // decrement the vm alloc ref count
        entry_module_ref.release(); // release the smart pointer

        return s.schedule_thread(std::move(thread));
    }
}

const runtime::vm_thread_t& vm_t::exec(const char *path)
{
    assert(path != nullptr);
    auto entry_module = load_module(path);

    auto entry_module_ref = std::make_unique<vm_module_ref_t>(entry_module);
    return _exec(*_scheduler, entry_module_ref);
}

const runtime::vm_thread_t& vm_t::exec(std::unique_ptr<runtime::vm_module_t> entry_module)
{
    assert(entry_module != nullptr);

    auto entry_module_ref = std::make_unique<vm_module_ref_t>(std::move(entry_module));
    return _exec(*_scheduler, entry_module_ref);
}

const runtime::vm_thread_t& vm_t::fork(
    const runtime::vm_thread_t &parent,
    runtime::vm_module_ref_t &module_ref,
    const runtime::vm_frame_t &initial_frame,
    runtime::vm_pc_t initial_pc)
{
    if (initial_pc < 0 || static_cast<vm_pc_t>(module_ref.code_section.size()) <= initial_pc)
        throw vm_system_exception{ "Invalid entry program counter" };

    auto thread = std::make_unique<vm_thread_t>(module_ref, parent.get_thread_id(), initial_frame, initial_pc);
    return _scheduler->schedule_thread(std::move(thread));
}

std::shared_ptr<const runtime::vm_module_t> vm_t::load_module(const char *path)
{
    if (path == nullptr)
        throw vm_user_exception{ "Invalid module path" };

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
                iter->module = module;

                debug::log_msg(debug::component_trace_t::module, debug::log_level_t::debug, "reload: vm module: >>%s<<\n", path);
            }

            return module;
        }

        ++iter;
    }

    auto new_module = std::shared_ptr<vm_module_t>{};
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
    _modules.push_front(std::move(loaded_module_t{ std::move(path_local), new_module }));

    debug::log_msg(debug::component_trace_t::module, debug::log_level_t::debug, "load: vm module: >>%s<<\n", path);

    return new_module;
}

std::vector<std::shared_ptr<const runtime::vm_module_t>> vm_t::get_loaded_modules() const
{
    auto result = std::vector<std::shared_ptr<const runtime::vm_module_t>>{};

    std::lock_guard<std::mutex> lock{ _modules_lock };
    for (auto &m : _modules)
    {
        auto loaded_module_maybe = m.module.lock();
        if (loaded_module_maybe != nullptr)
            result.push_back(loaded_module_maybe);
    }

    return result;
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