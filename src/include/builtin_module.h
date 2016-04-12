//
// Dis VM
// File: builtin_module.h
// Author: arr
//

#ifndef _DISVM_SRC_INCLUDE_BUILTIN_MODULE_H_
#define _DISVM_SRC_INCLUDE_BUILTIN_MODULE_H_

#include <cstdint>
#include <memory>
#include "runtime.h"

#define BUILTIN_MODULE_PREFIX_STR "$"
#define BUILTIN_MODULE_PREFIX_CHAR '$'

namespace disvm
{
    namespace runtime
    {
        namespace builtin
        {
            // Called to initialize built-in modules.
            // This will be called once per process hosting a VM.
            void initialize_builtin_modules();

            // Defined as 'Runtab' in Inferno (limbo/stubs.c)
            struct vm_runtab_t
            {
                // Function identification
                char *name;
                uint32_t sig;

                // Function pointer
                vm_exec_t fn;

                // Function frame type
                word_t frame_size;
                word_t frame_pointer_count;
                byte_t frame_pointer_map[16];
            };

            // Called by the built-in module during initialization
            void register_module_exports(const char *name, word_t table_length, const vm_runtab_t *module_runtime_table);

            // Get the supplied built-in module.
            std::shared_ptr<vm_module_t> get_builtin_module(const char *name);
        }

        // [SPEC] All allocated frames have the following base layout.
        // See 'struct Frame' in Inferno (include/interp.h)
        // The base frame does not include the return register since that
        // is typed on the generated function frame type.
        struct vm_frame_base_alloc_t
        {
            vm_pc_t p_pc; // Previous PC
            vm_frame_t *p_fp; // Previous frame pointer
            vm_module_ref_t *p_mr; // Previous module ref
            pointer_t reserved; // Reserved for VM
        };

        struct vm_frame_constants
        {
            static const auto register_size_in_bytes = sizeof(word_t);

            // Includes registers on base frame plus the implied return register
            static const auto default_register_count = (sizeof(vm_frame_base_alloc_t) / register_size_in_bytes) + 1;

            // See VM frame definition
            static const auto fixed_point_register_1_frame_offset = default_register_count * register_size_in_bytes;
            static const auto fixed_point_register_2_frame_offset = fixed_point_register_1_frame_offset + (2 * register_size_in_bytes);
        };

        //
        // Helpers for accessing VM types
        //

        // Get value-type reference
        template<typename ValueType>
        ValueType& vt_ref(pointer_t vt)
        {
            assert(vt != nullptr);
            return *reinterpret_cast<ValueType *>(vt);
        }

        // Get allocation type value
        template<typename AllocType>
        typename std::enable_if<std::is_base_of<vm_alloc_t, AllocType>::value, AllocType *>::type
            at_val(pointer_t at)
        {
            assert(at != nullptr);
            return vm_alloc_t::from_allocation<AllocType>(*reinterpret_cast<pointer_t *>(at));
        }

        // Get allocation type value - base alloc type
        template<>
        inline vm_alloc_t *at_val<vm_alloc_t>(pointer_t at)
        {
            assert(at != nullptr);
            return vm_alloc_t::from_allocation(*reinterpret_cast<pointer_t *>(at));
        }

        // Get pointer type reference
        inline pointer_t &pt_ref(pointer_t pt)
        {
            assert(pt != nullptr);
            return *reinterpret_cast<pointer_t *>(pt);
        }
    }
}

#endif // _DISVM_SRC_INCLUDE_BUILTIN_MODULE_H_