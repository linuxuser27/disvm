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
        struct vm_frame_base_alloc_t
        {
            vm_pc_t p_pc; // Previous PC
            vm_frame_t *p_fp; // Previous frame pointer
            vm_module_ref_t *p_mr; // Previous module ref
            pointer_t reserved; // Reserved for VM
        };

        static_assert(
            sizeof(vm_frame_base_alloc_t) == runtime_constants::default_register_count * runtime_constants::default_register_size_bytes,
            "Default register allocation should match base allocation type");
    }
}

#endif // _DISVM_SRC_INCLUDE_BUILTIN_MODULE_H_