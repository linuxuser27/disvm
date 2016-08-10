//
// Dis VM
// File: disvm.h
// Author: arr
//

#ifndef _DISVM_SRC_INCLUDE_DISVM_H_
#define _DISVM_SRC_INCLUDE_DISVM_H_

#include "runtime.h"
#include <iosfwd>
#include <memory>
#include <functional>
#include <mutex>
#include <forward_list>

namespace disvm
{
    // Read in the module at the supplied path
    std::unique_ptr<runtime::vm_module_t> read_module(const char *path);

    // Read in a module from the supplied stream
    std::unique_ptr<runtime::vm_module_t> read_module(std::istream &data);

    template <typename C>
    using create_vm_interface_callback_t = std::unique_ptr<C>(*)(vm_t &);

    // VM version structure
    // Adheres to the semantic versioning scheme found at http://semver.org/
    struct vm_version_t final
    {
        const uint32_t major;
        const uint32_t minor;
        const uint32_t patch;
        const char * const label; // Optional
    };

    // Loaded module reference
    class loaded_vm_module_t final
    {
    public:
        std::size_t load_id;

        // Origin of module
        std::unique_ptr<runtime::vm_string_t> origin;

        // Weak reference to a loaded module
        std::weak_ptr<runtime::vm_module_t> module;
    };

    // Callback for enumerating loaded modules - return 'false' to stop enumeration, otherwise 'true'
    using loaded_vm_module_callback_t = std::function<bool(const loaded_vm_module_t&)>;

    // Virtual machine
    class vm_t final
    {
    public: // static
        static const auto root_vm_thread_id = uint32_t{ 0 };

        static const auto default_system_thread_count = uint32_t{ 1 };

        // The Inferno implementation defined the thread quanta as 2048 (include/interp.h)
        static const auto default_thread_quanta = uint32_t{ 2048 };

    public:
        explicit vm_t(
            create_vm_interface_callback_t<runtime::vm_scheduler_t> create_scheduler = nullptr,
            create_vm_interface_callback_t<runtime::vm_garbage_collector_t> create_gc = nullptr);

        explicit vm_t(uint32_t thread_pool_size, uint32_t thread_quanta);

        ~vm_t();

    public:
        // Get the VM version.
        // All fields on the output structure will be set.
        vm_version_t get_version() const;

        // Begin execution of the module at the supplied path
        const runtime::vm_thread_t& exec(const char *path);

        // Begin execution of the supplied module
        const runtime::vm_thread_t& exec(std::unique_ptr<runtime::vm_module_t> entry_module);

        // Begin execution of a child thread in the supplied module at the indicated program counter
        const runtime::vm_thread_t& fork(
            const uint32_t parent_tid,
            runtime::vm_module_ref_t &module_ref,
            const runtime::vm_frame_t &initial_frame,
            runtime::vm_pc_t initial_pc);

        // Load the module at the supplied path into the virtual machine
        std::shared_ptr<runtime::vm_module_t> load_module(const char *path);

        // Enumerate all loaded modules - this can include references to modules that
        // have been unloaded due to no longer being used. If the module instance is null, the
        // module is no longer loaded.
        // Note this will only enumerate modules that have been loaded by a call to vm_t::load_module.
        void enum_loaded_modules(loaded_vm_module_callback_t callback) const;

        // Access the scheduler control for this VM.
        runtime::vm_scheduler_control_t &get_scheduler_control() const;

        // Access the garbage collector for this VM.
        runtime::vm_garbage_collector_t &get_garbage_collector() const;

        // Spin and sleep until the VM is idle, then return.
        // Idle is defined as no vm threads executing, scheduled to be executed, or blocked.
        void spin_sleep_till_idle(std::chrono::milliseconds sleep_interval) const;

        // Loads a tool into the VM
        std::size_t load_tool(std::shared_ptr<runtime::vm_tool_t> tool);

        // Unloads the tool associated with the supplied ID
        void unload_tool(std::size_t tool_id);

    private:
        const runtime::vm_thread_t &schedule_thread(std::unique_ptr<runtime::vm_thread_t> thread);

        using loaded_modules_t = std::forward_list<loaded_vm_module_t>;
        mutable std::mutex _modules_lock;
        loaded_modules_t _modules;

        std::unique_ptr<runtime::vm_scheduler_t> _scheduler;
        std::unique_ptr<runtime::vm_garbage_collector_t> _gc;
        std::unique_ptr<runtime::vm_tool_dispatch_t> _tool_dispatch;
        std::mutex _tool_dispatch_lock;
    };
}

#endif // _DISVM_SRC_INCLUDE_DISVM_H_