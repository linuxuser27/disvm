//
// Dis VM
// File: exceptions.h
// Author: arr
//

#ifndef _DISVM_SRC_INCLUDE_EXCEPTIONS_H_
#define _DISVM_SRC_INCLUDE_EXCEPTIONS_H_

#include <cassert>
#include <string>
#include <stdexcept>
#include "runtime.h"

namespace disvm
{
    namespace runtime
    {
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
            explicit module_reader_exception()
                : vm_module_exception("Invalid module")
            { }

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

        class invalid_utf8 : public vm_user_exception
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

        class type_violation : public vm_user_exception
        {
        public:
            explicit type_violation()
                : vm_user_exception("Inconsistent types in operation")
            { }
        };

        class out_of_range_memory final : public vm_user_exception
        {
        public:
            explicit out_of_range_memory()
                : vm_user_exception("Out of range access")
                , invalid_value{ 0 }
                , range_defined{ false }
                , valid_max{ 0 }
                , valid_min{ 0 }
            { }

            explicit out_of_range_memory(word_t valid_min, word_t valid_max, word_t invalid_valid)
                : vm_user_exception("Out of range access")
                , invalid_value{ invalid_valid }
                , range_defined{ true }
                , valid_max{ valid_max }
                , valid_min{ valid_min }
            { }

            const bool range_defined;
            const word_t valid_min;
            const word_t valid_max;
            const word_t invalid_value;
        };

        class unhandled_user_exception final : public vm_user_exception
        {
        public:
            explicit unhandled_user_exception(const char *exception_id, const char *module_name, vm_pc_t current_pc)
                : vm_user_exception("Unhandled user exception")
                , exception_id{ exception_id }
                , module_name{ module_name }
                , program_counter{ current_pc }
            { }

            const char *exception_id;
            const char *module_name;
            const vm_pc_t program_counter;
        };
    }
}

#endif // _DISVM_SRC_INCLUDE_EXCEPTIONS_H_