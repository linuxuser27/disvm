//
// Dis VM
// File: vm.cpp
// Author: arr
//

#include <cassert>
#include <memory>
#include <algorithm>
#include <sstream>
#include <disvm.h>
#include <debug.h>
#include <runtime.h>
#include <exceptions.h>
#include <builtin_module.h>
#include <vm_version.h>
#include "scheduler.h"
#include "garbage_collector.h"
#include "tool_dispatch.h"
#include "module_resolver.h"

using namespace disvm;
using namespace disvm::runtime;

namespace
{
    // Ideally this would be custom assembly based on the CPU,
    // but since this is just for the setting of the last error
    // message there is already a large cost associated with the
    // fact an error occurred so this is okay.
    struct inefficient_spin_lock_t final
    {
        inefficient_spin_lock_t(std::atomic_flag &is_lock)
            : _lf{ is_lock }
        {
            while (!_lf.test_and_set())
                ;
        }

        ~inefficient_spin_lock_t()
        {
            _lf.clear();
        }

    private:
        std::atomic_flag &_lf;
    };
}

void disvm::runtime::push_syscall_error_message(disvm::vm_t &vm, const char *msg) noexcept
{
    inefficient_spin_lock_t sl{ vm._last_syscall_error_message_lock };

    assert(msg != nullptr);
    auto bytes_to_copy = std::min(std::strlen(msg), vm._last_syscall_error_message.size() - 1);
    std::memcpy(vm._last_syscall_error_message.data(), msg, bytes_to_copy);

    // Ensure there is always a null
    vm._last_syscall_error_message[bytes_to_copy] = '\0';
}

std::string disvm::runtime::pop_syscall_error_message(disvm::vm_t &vm)
{
    inefficient_spin_lock_t sl{ vm._last_syscall_error_message_lock };

    std::string msg{ vm._last_syscall_error_message.data() };
    vm._last_syscall_error_message[0] = '\0';
    return msg;
}

// Consumed in vm_memory.cpp
thread_local vm_memory_alloc_t vm_memory_alloc;
thread_local vm_memory_free_t vm_memory_free;

namespace
{
    void internal_register_system_thread(vm_memory_allocator_t allocator) noexcept
    {
        assert(((vm_memory_alloc == nullptr && vm_memory_free == nullptr)
            || (vm_memory_alloc == allocator.alloc && vm_memory_free == allocator.free))
            && "Thread already registered with another VM");

        vm_memory_alloc = allocator.alloc;
        vm_memory_free = allocator.free;

        assert(vm_memory_alloc != nullptr && vm_memory_free != nullptr && "Invalid allocator functions supplied");
    }

    void internal_unregister_system_thread(vm_memory_allocator_t allocator) noexcept
    {
        assert(((vm_memory_alloc == nullptr && vm_memory_free == nullptr)
            || (vm_memory_alloc == allocator.alloc && vm_memory_free == allocator.free))
            && "Thread already registered with another VM");

        vm_memory_alloc = nullptr;
        vm_memory_free = nullptr;
    }
}

void disvm::runtime::register_system_thread(vm_t &vm)
{
    auto allocator = vm.get_garbage_collector().get_allocator();
    internal_register_system_thread(std::move(allocator));
}

void disvm::runtime::unregister_system_thread(vm_t &vm)
{
    auto allocator = vm.get_garbage_collector().get_allocator();
    internal_unregister_system_thread(std::move(allocator));
}

const uint32_t vm_t::root_vm_thread_id = 0;

// The Inferno implementation defined the thread quanta as 2048 (include/interp.h)
const uint32_t default_thread_quanta = 2048;
const uint32_t default_system_thread_count = 1;

vm_config_t::vm_config_t()
    : create_gc{ nullptr }
    , create_scheduler{ nullptr }
    , sys_thread_pool_size{ default_system_thread_count }
    , thread_quanta{ default_thread_quanta }
{ }

vm_config_t::vm_config_t(vm_config_t &&other)
    : additional_resolvers{ std::move(other.additional_resolvers) }
    , create_gc{ other.create_gc }
    , create_scheduler{ other.create_scheduler }
    , probing_paths{ std::move(other.probing_paths) }
    , sys_thread_pool_size{ other.sys_thread_pool_size }
    , thread_quanta{ other.thread_quanta }
{ }

vm_t::vm_t()
    : _last_syscall_error_message{}
    , _last_syscall_error_message_lock{ ATOMIC_FLAG_INIT }
{
    _gc = std::make_unique<default_garbage_collector_t>(*this);

    internal_register_system_thread(_gc->get_allocator());

    // Initialize built-in modules.
    builtin::initialize_builtin_modules();

    _scheduler = std::make_unique<default_scheduler_t>(*this, default_system_thread_count, default_thread_quanta);
    _module_resolvers.push_back(std::make_unique<default_resolver_t>(*this));
}

vm_t::vm_t(vm_config_t config)
    : _last_syscall_error_message{}
{
    if (config.create_gc == nullptr)
        _gc = std::make_unique<default_garbage_collector_t>(*this);
    else
        _gc = config.create_gc(*this);

    internal_register_system_thread(_gc->get_allocator());

    // Initialize built-in modules.
    builtin::initialize_builtin_modules();

    if (config.create_scheduler == nullptr)
        _scheduler = std::make_unique<default_scheduler_t>(*this, config.sys_thread_pool_size, config.thread_quanta);
    else
        _scheduler = config.create_scheduler(*this);

    _module_resolvers = std::move(config.additional_resolvers);
    _module_resolvers.push_back(std::make_unique<default_resolver_t>(*this, std::move(config.probing_paths)));
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
    std::unique_ptr<runtime::vm_thread_t> _create_thread_safe(std::unique_ptr<runtime::vm_module_ref_t> entry_module_ref)
    {
        auto thread = std::make_unique<vm_thread_t>(*entry_module_ref, disvm::vm_t::root_vm_thread_id);

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
    auto thread = _create_thread_safe(std::move(entry_module_ref));
    return schedule_thread(std::move(thread));
}

const runtime::vm_thread_t& vm_t::exec(std::unique_ptr<runtime::vm_module_t> entry_module)
{
    assert(entry_module != nullptr);

    auto entry_module_ref = std::make_unique<vm_module_ref_t>(std::move(entry_module));
    auto thread = _create_thread_safe(std::move(entry_module_ref));
    return schedule_thread(std::move(thread));
}

const runtime::vm_thread_t& vm_t::fork(
    const uint32_t parent_tid,
    runtime::vm_module_ref_t &module_ref,
    const runtime::vm_frame_t &initial_frame,
    runtime::vm_pc_t initial_pc)
{
    if (initial_pc < 0 || static_cast<vm_pc_t>(module_ref.code_section.size()) <= initial_pc)
        throw vm_user_exception{ "Invalid entry program counter" };

    auto thread = std::make_unique<vm_thread_t>(module_ref, parent_tid, initial_frame, initial_pc);
    return schedule_thread(std::move(thread));
}

namespace
{
    std::unique_ptr<vm_module_t> resolve_module_from_path(const char *path, const std::vector<std::unique_ptr<runtime::vm_module_resolver_t>> &resolvers)
    {
        assert(!resolvers.empty());
        std::unique_ptr<vm_module_t> resolved_module;

        // Check if the path is for the Inferno OS.
        const char *inferno_root_path = "/dis/";
        if (std::strncmp(path, inferno_root_path, sizeof(inferno_root_path)) == 0)
        {
            if (debug::is_component_tracing_enabled<debug::component_trace_t::module>())
                debug::log_msg(debug::component_trace_t::module, debug::log_level_t::debug, "load: vm module: Inferno OS path detected - >>%s<<", path);

            // Plus 1 for the next character. We know this will either be
            // null or valid character since we searched for a string
            // containing the character above.
            const char *module_name = std::strrchr(path, '/') + 1;

            if (*module_name != '\0')
            {
                for (auto &r : resolvers)
                {
                    // Since we are using a modified path, throwing an exception
                    // isn't fair. We will catch all exceptions here and rely on
                    // non-modified path resolution to trigger the actual failure.
                    try
                    {
                        if (r->try_resolve_module(module_name, resolved_module))
                            return resolved_module;
                    }
                    catch (...)
                    {
                        // No-op
                    }
                }
            }
        }

        // Attempt to resolve the path
        for (auto &r : resolvers)
        {
            if (r->try_resolve_module(path, resolved_module))
                return resolved_module;
        }

        std::stringstream ss;
        ss << "Failed to resolve path to module: " << path;
        throw vm_module_exception{ ss.str().c_str() };
    }
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
                    module = std::shared_ptr<vm_module_t>{ std::move(resolve_module_from_path(path, _module_resolvers)) };
                    module->vm_id = iter->vm_id;

                    iter->module = module;

                    if (debug::is_component_tracing_enabled<debug::component_trace_t::module>())
                        debug::log_msg(debug::component_trace_t::module, debug::log_level_t::debug, "reload: vm module: >>%s<<", path);
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
            new_module = resolve_module_from_path(path, _module_resolvers);
            assert(new_module != nullptr);
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
        debug::log_msg(debug::component_trace_t::module, debug::log_level_t::debug, "load: vm module: >>%s<<", path);

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
