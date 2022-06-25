//
// Dis VM
// File: vm_memory.hpp
// Author: arr
//

#ifndef _DISVM_SRC_INCLUDE_VM_MEMORY_HPP_
#define _DISVM_SRC_INCLUDE_VM_MEMORY_HPP_

#include <cstdint>
#include <cassert>
#include <limits>
#include <mutex>
#include <memory>
#include <bitset>
#include <functional>
#include "runtime.hpp"

namespace disvm
{
    namespace runtime
    {
        // Allocate memory from the VM heap that is initialize to 0.
        // [SPEC] All memory in this implementation of the Dis VM spec
        // will be 0 initialized.
        void *alloc_memory(std::size_t amount_in_bytes);

        template<typename T>
        T *alloc_memory(std::size_t amount_in_bytes = sizeof(T))
        {
            return reinterpret_cast<T *>(alloc_memory(amount_in_bytes));
        }

        // Free memory on the VM heap
        void free_memory(void *memory);

        // Initialize supplied memory based on the type descriptor
        void init_memory(const type_descriptor_t &type_desc, void *data);

        // Destroy supplied memory based on the type descriptor
        void destroy_memory(const type_descriptor_t &type_desc, void *data);

        // Increment the ref count of all pointers in the supplied memory allocation
        void inc_ref_count_in_memory(const type_descriptor_t &type_desc, void *data);

        // Decrement the ref count of all pointers in the supplied memory allocation
        void dec_ref_count_in_memory(const type_descriptor_t &type_desc, void *data);

        // Decrement the ref count and if 0 free the allocation
        void dec_ref_count_and_free(vm_alloc_t *alloc);

        // Function to enumerate pointer fields in an object. The function is defined
        // as a template to permit raw function pointers or lambdas. The signature
        // of the callback must be 'void(pointer_t *)' and is supplied a pointer to the
        // non-null pointer field. The offset of the field is relative to the supplied
        // 'data' pointer.
        template<typename CB>
        void enum_pointer_fields(const type_descriptor_t &type_desc, void *data, CB callback)
        {
            assert(data != nullptr);

            auto memory = reinterpret_cast<word_t *>(data);
            auto map = type_desc.get_map();
            for (auto i = word_t{ 0 }; i < type_desc.map_in_bytes; ++i, memory += 8)
            {
                const auto words8 = map[i];
                assert(sizeof(words8) == 1);
                if (words8 == 0)
                    continue;

                const auto flags = std::bitset<sizeof(words8) * 8>{ words8 };

                // Enumerating the flags in reverse order so memory access is sequential.
                if (flags[7] && (memory[0] != runtime_constants::nil)) callback(reinterpret_cast<pointer_t *>(memory));
                if (flags[6] && (memory[1] != runtime_constants::nil)) callback(reinterpret_cast<pointer_t *>(memory) + 1);
                if (flags[5] && (memory[2] != runtime_constants::nil)) callback(reinterpret_cast<pointer_t *>(memory) + 2);
                if (flags[4] && (memory[3] != runtime_constants::nil)) callback(reinterpret_cast<pointer_t *>(memory) + 3);
                if (flags[3] && (memory[4] != runtime_constants::nil)) callback(reinterpret_cast<pointer_t *>(memory) + 4);
                if (flags[2] && (memory[5] != runtime_constants::nil)) callback(reinterpret_cast<pointer_t *>(memory) + 5);
                if (flags[1] && (memory[6] != runtime_constants::nil)) callback(reinterpret_cast<pointer_t *>(memory) + 6);
                if (flags[0] && (memory[7] != runtime_constants::nil)) callback(reinterpret_cast<pointer_t *>(memory) + 7);
            }
        }

        // Check if a specific pointer-sized offset in a type represents a pointer
        bool is_offset_pointer(const type_descriptor_t &type_desc, std::size_t offset);
    }
}

#endif // _DISVM_SRC_INCLUDE_VM_MEMORY_HPP_
