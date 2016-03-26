//
// Dis VM
// File: vm_memory.h
// Author: arr
//

#ifndef _DISVM_SRC_INCLUDE_VM_MEMORY_H_
#define _DISVM_SRC_INCLUDE_VM_MEMORY_H_

#include <cstdint>
#include <cassert>
#include <mutex>
#include <memory>
#include <bitset>
#include <functional>
#include "runtime.h"

namespace disvm
{
    namespace runtime
    {
        class global_alloc_lock_t final
        {
        public:
            global_alloc_lock_t(const global_alloc_lock_t &) = delete;
            global_alloc_lock_t& operator=(const global_alloc_lock_t &) = delete;

            global_alloc_lock_t(global_alloc_lock_t &&);

            ~global_alloc_lock_t();

        private:
            bool _owner;
            global_alloc_lock_t(bool owner);
            friend global_alloc_lock_t get_global_alloc_lock();
        };

        // Take the global allocation lock for the VM heap.
        // While this lock is taken all further allocations will block.
        global_alloc_lock_t get_global_alloc_lock();

        // Allocate memory from the VM heap
        void *alloc_memory(std::size_t amount_in_bytes);

        template<typename T>
        T *alloc_memory(std::size_t amount_in_bytes = sizeof(T))
        {
            return reinterpret_cast<T *>(alloc_memory(amount_in_bytes));
        }

        // Allocate memory from the VM heap and initialize to 0.
        void *calloc_memory(std::size_t amount_in_bytes);

        template<typename T>
        T *calloc_memory(std::size_t amount_in_bytes = sizeof(T))
        {
            return reinterpret_cast<T *>(calloc_memory(amount_in_bytes));
        }

        // Free memory on the VM heap
        void free_memory(void *memory);

        // Initialize supplied memory based on the type descriptor
        void init_memory(const type_descriptor_t &type_desc, void *data);

        // Destroy supplied memory based on the type descriptor
        void destroy_memory(const type_descriptor_t &type_desc, void *data);

        // Increment the ref count of all points in the supplied memory allocation
        void inc_ref_count_in_memory(const type_descriptor_t &type_desc, void *data);

        // Decrement the ref count and if 0 free the allocation.
        void dec_ref_count_and_free(vm_alloc_t *alloc);

        using pointer_field_callback_t = std::function<void(pointer_t)>;

        // The callback will be called on all non-null pointer fields.
        inline void enum_pointer_fields(const type_descriptor_t &type_desc, void *data, pointer_field_callback_t callback)
        {
            assert(data != nullptr && callback != nullptr);
            if (type_desc.size_in_bytes == 0)
                return;

            auto memory = reinterpret_cast<word_t *>(data);
            for (auto i = word_t{ 0 }; i < type_desc.map_in_bytes; ++i, memory += 8)
            {
                const auto words8 = type_desc.pointer_map[i];
                if (words8 != 0)
                {
                    const auto flags = std::bitset<sizeof(words8) * 8>{ words8 };

                    // Enumerating the flags in reverse order so memory access is linear.
                    if (flags[7] && (memory[0] != runtime_constants::nil)) callback(reinterpret_cast<pointer_t>(memory[0]));
                    if (flags[6] && (memory[1] != runtime_constants::nil)) callback(reinterpret_cast<pointer_t>(memory[1]));
                    if (flags[5] && (memory[2] != runtime_constants::nil)) callback(reinterpret_cast<pointer_t>(memory[2]));
                    if (flags[4] && (memory[3] != runtime_constants::nil)) callback(reinterpret_cast<pointer_t>(memory[3]));
                    if (flags[3] && (memory[4] != runtime_constants::nil)) callback(reinterpret_cast<pointer_t>(memory[4]));
                    if (flags[2] && (memory[5] != runtime_constants::nil)) callback(reinterpret_cast<pointer_t>(memory[5]));
                    if (flags[1] && (memory[6] != runtime_constants::nil)) callback(reinterpret_cast<pointer_t>(memory[6]));
                    if (flags[0] && (memory[7] != runtime_constants::nil)) callback(reinterpret_cast<pointer_t>(memory[7]));
                }
            }
        }
    }
}

#endif // _DISVM_SRC_INCLUDE_VM_MEMORY_H_