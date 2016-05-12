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
        union vm_context_value_t
        {
            std::size_t i;
            vm_t *vm;
            vm_module_t *module;
            vm_registers_t *registers;
            vm_alloc_t *alloc;
        };

        struct vm_event_context_t
        {
            vm_context_value_t value1;
            vm_context_value_t value2;
        };

        enum class vm_event_t
        {
            module_load,         // value1: vm_module_t
            module_unload,       // value1: vm_module_t
            thread_begin,        // value1: vm_registers_t
            thread_end,          // value1: vm_registers_t
            exception_raised,    // value1: vm_registers_t  value2: vm_alloc_t
            exception_unhandled, // value1: vm_registers_t  value2: vm_alloc_t
            breakpoint,          // value1: vm_registers_t  value2: std::size_t
        };

        using vm_event_callback_t = std::function<void(vm_event_t, vm_event_context_t &)>;

        // VM tool controller
        class vm_tool_controller_t
        {
        public:
            // Call the supplied callback when the specified event occurs
            virtual std::size_t subscribe_event(vm_event_t, vm_event_callback_t) = 0;
            virtual void unsubscribe_event(std::size_t) = 0;

            // Set a breakpoint in the supplied module at the specified PC location
            virtual std::size_t set_breakpoint(std::shared_ptr<const vm_module_t>, vm_pc_t) = 0;
            virtual void clear_breakpoint(std::size_t) = 0;
        };
    }
}

#endif // _DISVM_SRC_INCLUDE_VM_TOOLS_H_