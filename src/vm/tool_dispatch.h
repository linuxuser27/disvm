//
// Dis VM
// File: tool_dispatch.h
// Author: arr
//

#ifndef _DISVM_SRC_VM_TOOL_DISPATCH_H_
#define _DISVM_SRC_VM_TOOL_DISPATCH_H_

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <runtime.h>
#include <tuple>
#include <vm_tools.h>

namespace disvm
{
    namespace runtime
    {
        class vm_tool_dispatch_t final : private vm_tool_controller_t
        {
        public:
            vm_tool_dispatch_t(vm_t &vm);

            virtual ~vm_tool_dispatch_t();

            // Loads a tool for dispatch and returns the tool's ID
            std::size_t load_tool(std::shared_ptr<runtime::vm_tool_t> tool);

            // Unloads the tool associated with the supplied ID and returns the current tool count
            std::size_t unload_tool(std::size_t tool_id);

            // Callback on a breakpoint event.
            // Returns opcode that was replaced by breakpoint opcode.
            opcode_t on_breakpoint(vm_registers_t &r);

            // Callback on a raised exception
            void on_exception_raised(vm_registers_t &r, const vm_string_t &id, vm_alloc_t &e);

            // Callback on an unhandled exception
            void on_exception_unhandled(vm_registers_t &r, const vm_string_t &id, vm_alloc_t &e);

            // Callback for when the VM loads a module from disk
            void on_module_vm_load(const loaded_vm_module_t &loaded_module);

            // Callback for when a thread loads a module into its scope.
            void on_module_thread_load(vm_registers_t &r, std::shared_ptr<vm_module_t> &m);

            // Callback when thread lifetime begins.
            // The thread has not been scheduled and no opcodes have been executed.
            void on_thread_begin(vm_thread_t &t);

            // Callback when thread lifetime ends without error.
            void on_thread_end(vm_thread_t &t);

            // Callback when thread lifetime is terminated by error.
            void on_thread_broken(vm_thread_t &t);

            // Function called by system threads to determine if the associated vm thread is
            // currently suspended.
            void block_if_thread_suspended(uint32_t thread_id);

        public: // vm_tool_controller_t
            vm_t& get_vm_instance() const override;

            cookie_t subscribe_event(vm_event_t evt, vm_event_callback_t cb) override;
            void unsubscribe_event(cookie_t cookie_id) override;

            cookie_t set_breakpoint(std::shared_ptr<vm_module_t> module, vm_pc_t pc) override;
            breakpoint_details_t get_breakpoint_details(cookie_t) const override;
            void clear_breakpoint(cookie_t cookie_id) override;

            void suspend_all_threads() override;
            void resume_all_threads() override;

        private:
            std::tuple<opcode_t, cookie_t> get_original_opcode_and_cookie(const vm_registers_t &r);

        private:
            vm_t &_vm;

            std::atomic_bool _threads_suspended;
            std::atomic<int32_t> _threads_suspended_count;
            std::mutex _threads_suspend_lock;
            std::condition_variable _threads_resume;
            std::mutex _threads_resume_lock;

            using tool_collection_t = std::unordered_map<std::size_t, std::shared_ptr<vm_tool_t>>;
            tool_collection_t _tools;
            std::mutex _tools_lock;

            struct events_t
            {
                void fire_event_callbacks(vm_event_t, vm_event_context_t &);

                using cookie_to_callback_map_t = std::unordered_map<cookie_t, vm_event_callback_t>;
                using event_to_callbacks_map_t = std::unordered_map<vm_event_t, cookie_to_callback_map_t>;
                event_to_callbacks_map_t callbacks;

                using cookie_to_event_map_t = std::unordered_map<cookie_t, vm_event_t>;
                cookie_to_event_map_t cookie_to_event;

                mutable std::mutex event_lock;
                cookie_t cookie_counter;
            } _events;

            struct
            {
                using cookie_to_details_t = std::unordered_map<cookie_t, breakpoint_details_t>;
                cookie_to_details_t cookie_to_details;

                using pc_to_opcode_cookie_map_t = std::unordered_map<vm_pc_t, std::pair<opcode_t, cookie_t>>;
                using module_to_pc_map_t = std::unordered_map<std::uintptr_t, pc_to_opcode_cookie_map_t>;
                module_to_pc_map_t original_opcodes;

                mutable std::mutex lock;
                cookie_t cookie_counter;
            } _breakpoints;
        };
    }
}

#endif // _DISVM_SRC_VM_TOOL_DISPATCH_H_
