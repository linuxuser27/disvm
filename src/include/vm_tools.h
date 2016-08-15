//
// Dis VM
// File: vm_tools.h
// Author: arr
//

#ifndef _DISVM_SRC_INCLUDE_VM_TOOLS_H_
#define _DISVM_SRC_INCLUDE_VM_TOOLS_H_

#include <cstdint>
#include <vector>
#include <memory>
#include <functional>
#include "runtime.h"

namespace disvm
{
    namespace runtime
    {
        using cookie_t = std::size_t;

        // Value in event context
        union vm_context_value_t
        {
            cookie_t cookie;
            std::shared_ptr<vm_module_t> *module; // Note this is a pointer to std::shared_ptr<>
            vm_registers_t *registers;
            const vm_thread_t *thread;
            const vm_string_t *str;
            const vm_alloc_t *alloc;
        };

        // Event context
        struct vm_event_context_t
        {
            vm_context_value_t value1;
            vm_context_value_t value2;
            vm_context_value_t value3;
        };

        // List of possible events
        enum class vm_event_t
        {
            module_vm_load,      // value1: std::shared_ptr<vm_module_t>
                                 // value2: const vm_string_t - path on disk

            module_thread_load,  // value1: vm_registers_t
                                 // value2: std::shared_ptr<vm_module_t>

            thread_begin,        // value1: const vm_thread_t
            thread_end,          // value1: const vm_thread_t

            exception_raised,    // value1: vm_registers_t
                                 // value2: const vm_string_t - exception ID
                                 // value3: vm_alloc_t - object raised

            exception_unhandled, // value1: vm_registers_t
                                 // value2: const vm_string_t - exception ID
                                 // value3: const vm_alloc_t - object raised

            breakpoint,          // value1: vm_registers_t
                                 // value2: cookie_t - cookie of breakpoint. '0' indicates the breakpoint was inserted by the VM or compiler.
        };

        using vm_event_callback_t = std::function<void(vm_event_t, vm_event_context_t &)>;

        struct breakpoint_details_t final
        {
            std::shared_ptr<vm_module_t> module;
            vm_pc_t pc;
        };

        // VM tool controller
        class vm_tool_controller_t
        {
        public:
            // The current vm instance
            virtual vm_t& get_vm_instance() const = 0;

            // Call the supplied callback when the specified event occurs
            virtual cookie_t subscribe_event(vm_event_t, vm_event_callback_t) = 0;
            virtual void unsubscribe_event(cookie_t) = 0;

            // Set a breakpoint in the supplied module at the specified PC location
            virtual cookie_t set_breakpoint(std::shared_ptr<vm_module_t>, vm_pc_t) = 0;
            virtual breakpoint_details_t get_breakpoint_details(cookie_t) const = 0;
            virtual void clear_breakpoint(cookie_t) = 0;

            // Manipulate thread execution
            virtual void suspend_all_threads() = 0;
            virtual void resume_all_threads() = 0;
        };
    }
}

#endif // _DISVM_SRC_INCLUDE_VM_TOOLS_H_