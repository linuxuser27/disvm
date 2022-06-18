//
// Dis VM
// File: exceptions.hpp
// Author: arr
//

#ifndef _DISVM_SRC_INCLUDE_EXCEPTIONS_HPP_
#define _DISVM_SRC_INCLUDE_EXCEPTIONS_HPP_

#include <cassert>
#include <string>
#include <stdexcept>
#include "runtime.hpp"

namespace disvm
{
    namespace runtime
    {
        // Functions for managing syscall exception messages
        void push_syscall_error_message(disvm::vm_t &, const char *) noexcept;
        std::string pop_syscall_error_message(disvm::vm_t &);

        class vm_user_exception : public std::runtime_error
        {
        public:
            explicit vm_user_exception(const char *message)
                : std::runtime_error(message)
            { }
        };

        class vm_system_exception : public std::runtime_error
        {
        public:
            explicit vm_system_exception(const char *message)
                : std::runtime_error(message)
            { }
        };

        class vm_syscall_exception final : public vm_system_exception
        {
        public:
            explicit vm_syscall_exception(disvm::vm_t &vm, const char *message)
                : vm_system_exception(message)
            {
                push_syscall_error_message(vm, message);
            }
        };

        class vm_module_exception : public vm_system_exception
        {
        public:
            explicit vm_module_exception(const char *message)
                : vm_system_exception(message)
            { }
        };

        class module_reader_exception final : public vm_module_exception
        {
        public:
            explicit module_reader_exception(const char *message)
                : vm_module_exception(message)
            { }
        };

        class divide_by_zero final : public vm_user_exception
        {
        public:
            explicit divide_by_zero()
                : vm_user_exception("Divide by 0")
            { }
        };

        class invalid_utf8 final : public vm_user_exception
        {
        public:
            explicit invalid_utf8()
                : vm_user_exception("Invalid UTF-8 string")
            { }
        };

        class dereference_nil final : public vm_user_exception
        {
        public:
            explicit dereference_nil()
                : vm_user_exception("Dereference of nil value")
                , operation{ nullptr }
            { }

            explicit dereference_nil(const char *operation)
                : vm_user_exception("Dereference of nil value")
                , operation{ operation }
            { }

            const char *operation;
        };

        class type_violation final : public vm_user_exception
        {
        public:
            explicit type_violation()
                : vm_user_exception("Inconsistent types in operation")
            { }
        };

        class out_of_range_memory : public vm_user_exception
        {
        public:
            explicit out_of_range_memory()
                : vm_user_exception("Out of range access")
            { }
        };

        class index_out_of_range_memory final : public out_of_range_memory
        {
        public:
            explicit index_out_of_range_memory(word_t valid_min, word_t valid_max, word_t invalid_value)
                : invalid_value{ invalid_value }
                , valid_max{ valid_max }
                , valid_min{ valid_min }
            { }

            const word_t valid_min;
            const word_t valid_max;
            const word_t invalid_value;
        };

        // [TODO] Allow non-string exceptions to be marshalled.
        class marshallable_user_exception final : public vm_user_exception
        {
        public:
            explicit marshallable_user_exception(const char *message)
                : vm_user_exception(message)
            { }
        };

        class unhandled_user_exception final : public vm_user_exception
        {
        public:
            explicit unhandled_user_exception(const char *exception_id, const char *module_name, vm_pc_t current_pc)
                : vm_user_exception("Unhandled exception")
                , exception_id{ exception_id }
                , module_name{ module_name }
                , program_counter{ current_pc }
            { }

            const char *exception_id;
            const char *module_name;
            const vm_pc_t program_counter;
        };

        class vm_term_request final : public vm_user_exception
        {
        public:
            explicit vm_term_request()
                : vm_user_exception("VM termination requested")
            { }

            explicit vm_term_request(const char *msg)
                : vm_user_exception(msg)
            { }
        };
    }
}

#endif // _DISVM_SRC_INCLUDE_EXCEPTIONS_HPP_
