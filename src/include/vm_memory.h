//
// Dis VM
// File: vm_memory.h
// Author: arr
//

#ifndef _DISVM_SRC_INCLUDE_VM_MEMORY_H_
#define _DISVM_SRC_INCLUDE_VM_MEMORY_H_

#include <cstdint>
#include <cassert>
#include <limits>
#include <mutex>
#include <memory>
#include <bitset>
#include <functional>
#include "runtime.h"

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

        // Callback for a pointer field supplying the pointer
        using pointer_field_callback_t = std::function<void(pointer_t)>;

        // The callback will be called on all non-null pointer fields.
        void enum_pointer_fields(const type_descriptor_t &type_desc, void *data, pointer_field_callback_t callback);

        // Callback for a pointer field supplying the pointer and byte offset
        using pointer_field_offset_callback_t = std::function<void(pointer_t, std::size_t)>;

        // The callback will be called on all non-null pointer fields.
        void enum_pointer_fields_with_offset(const type_descriptor_t &type_desc, void *data, pointer_field_offset_callback_t callback);

        // Check if a specific pointer-sized offset in a type represents a pointer
        bool is_offset_pointer(const type_descriptor_t &type_desc, std::size_t offset);
    }
}

#endif // _DISVM_SRC_INCLUDE_VM_MEMORY_H_
