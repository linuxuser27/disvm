//
// Dis VM
// File: disvm.h
// Author: arr
//

#ifndef _DISVM_SRC_INCLUDE_DISVM_H_
#define _DISVM_SRC_INCLUDE_DISVM_H_

#include <array>
#include <iosfwd>
#include <memory>
#include <functional>
#include <mutex>
#include <forward_list>
#include "runtime.h"
#include "exceptions.h"

namespace disvm
{
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
        runtime::module_id_t vm_id;

        // Origin of module
        std::unique_ptr<runtime::vm_string_t> origin;

        // Weak reference to a loaded module
        std::weak_ptr<runtime::vm_module_t> module;
    };

    // Callback for enumerating loaded modules - return 'false' to stop enumeration, otherwise 'true'
    using loaded_vm_module_callback_t = std::function<bool(const loaded_vm_module_t&)>;

    // Configuration for the created VM
    class vm_config_t final
    {
    public:
        vm_config_t();
        vm_config_t(vm_config_t &&);

        // Settings for the default scheduler. Only used if create_scheduler is not set.
        // Both of these values are initialized to valid default values on construction.
        uint32_t sys_thread_pool_size;
        uint32_t thread_quanta;

        create_vm_interface_callback_t<runtime::vm_scheduler_t> create_scheduler;
        create_vm_interface_callback_t<runtime::vm_garbage_collector_t> create_gc;

        // Prefix paths used by the default module resolver if the path supplied by
        // the user isn't found. Note these will be concatenated with the supplied
        // path, not other manipulation will be performed.
        std::vector<std::string> probing_paths;
        std::vector<std::unique_ptr<runtime::vm_module_resolver_t>> additional_resolvers;
    };

    // Virtual machine
    class vm_t final
    {
    public: // static
        static const uint32_t root_vm_thread_id;

    public:
        explicit vm_t();
        explicit vm_t(vm_config_t config);

        ~vm_t();

    public:
        // Get the VM version.
        // All fields on the output structure will be set.
        vm_version_t get_version() const;

        // Begin execution of the module at the supplied path and return the ID of
        // the executing thread.
        uint32_t exec(const char *path);

        // Begin execution of the supplied module and return the ID of the executing thread.
        uint32_t exec(std::unique_ptr<runtime::vm_module_t> entry_module);

        // Begin execution of a child thread in the supplied module at the indicated
        // program counter and return the ID of the executing thread.
        uint32_t fork(
            const uint32_t parent_tid,
            runtime::vm_module_ref_t &module_ref,
            const runtime::vm_frame_t &initial_frame,
            runtime::vm_pc_t initial_pc);

        // Load the module at the supplied path into the virtual machine.
        std::shared_ptr<runtime::vm_module_t> load_module(const char *path);

        // Enumerate all loaded modules - this can include references to modules that
        // have been unloaded due to no longer being used. If the module instance is null, the
        // module is no longer loaded.
        // Note this will only enumerate modules that have been loaded via a path.
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
        void schedule_thread(std::unique_ptr<runtime::vm_thread_t> thread);

        using loaded_modules_t = std::forward_list<loaded_vm_module_t>;
        mutable std::mutex _modules_lock;
        loaded_modules_t _modules;

        std::vector<std::unique_ptr<runtime::vm_module_resolver_t>> _module_resolvers;
        std::unique_ptr<runtime::vm_scheduler_t> _scheduler;
        std::unique_ptr<runtime::vm_garbage_collector_t> _gc;
        std::unique_ptr<runtime::vm_tool_dispatch_t> _tool_dispatch;
        std::mutex _tool_dispatch_lock;

        std::atomic_flag _last_syscall_error_message_lock;
        std::array<char, 128> _last_syscall_error_message;

        friend void disvm::runtime::push_syscall_error_message(disvm::vm_t &, const char *) noexcept;
        friend std::string disvm::runtime::pop_syscall_error_message(disvm::vm_t &);
    };
}

#endif // _DISVM_SRC_INCLUDE_DISVM_H_
