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

            // Handle a breakpoint event.
            // Returns opcode that was replaced by breakpoint opcode.
            opcode_t on_breakpoint(vm_registers_t &r, vm_t &vm);

        public: // vm_tool_controller_t
            std::size_t subscribe_event(vm_event_t evt, vm_event_callback_t cb) override final;
            void unsubscribe_event(std::size_t cookie_id) override final;

            std::size_t set_breakpoint(std::shared_ptr<const vm_module_t> module, vm_pc_t pc) override final;
            void clear_breakpoint(std::size_t cookie_id) override final;

        private:
            opcode_t get_original_opcode(const vm_module_t *module, vm_pc_t pc);

        private:
            vm_t &_vm;

            using tool_collection_t = std::unordered_map<std::size_t, std::shared_ptr<vm_tool_t>>;
            tool_collection_t _tools;
            std::mutex _tools_lock;

            struct
            {
                using cookie_to_callback_map_t = std::unordered_map<std::size_t, vm_event_callback_t>;
                using event_to_callbacks_map_t = std::unordered_map<vm_event_t, cookie_to_callback_map_t>;
                event_to_callbacks_map_t callbacks;

                using cookie_to_event_map_t = std::unordered_map<std::size_t, vm_event_t>;
                cookie_to_event_map_t cookie_to_event;

                std::mutex lock;
                std::size_t cookie_counter;
            } _events;

            struct
            {
                using cookie_to_modulepc_t = std::unordered_map<vm_pc_t, std::pair<std::shared_ptr<const vm_module_t>, vm_pc_t>>;
                cookie_to_modulepc_t cookie_to_modulepc;

                using pc_to_opcode_map_t = std::unordered_map<vm_pc_t, opcode_t>;
                using module_to_pc_map_t = std::unordered_map<std::uintptr_t, pc_to_opcode_map_t>;
                module_to_pc_map_t original_opcodes;

                std::mutex lock;
                std::size_t cookie_counter;
            } _breakpoints;
        };
    }
}

#endif // _DISVM_SRC_VM_TOOL_DISPATCH_H_