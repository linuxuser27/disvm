//
// Dis VM
// File: fd_types.hpp
// Author: arr
//

#ifndef _DISVM_SRC_VM_SYS_FD_TYPES_HPP_
#define _DISVM_SRC_VM_SYS_FD_TYPES_HPP_

#include <memory>

#include <runtime.hpp>
#include <utils.hpp>

namespace disvm
{
    namespace runtime
    {
        namespace sys
        {
            enum class seek_start_t
            {
                start,
                relative,
                end,
            };

            enum class open_mode_t
            {
                none = 0,
                read = (1 << 0),
                write = (1 << 1),
                truncate = (1 << 2),
                atomic = (1 << 3),
                ensure_create = (1 << 4), // Mutually exclusive with 'ensure_exists'
                ensure_exists = (1 << 5), // Mutually exclusive with 'ensure_create'
                delete_on_close = (1 << 6),
                disable_seek = (1 << 7),
            };

            DEFINE_ENUM_FLAG_OPERATORS(open_mode_t);

            // VM file descriptor
            class vm_fd_t : public vm_alloc_t
            {
            public: // static
                static managed_ptr_t<const type_descriptor_t> type_desc();

            public:
                // Read from the file descriptor into the supplied buffer
                virtual word_t read(vm_t &vm, const word_t buffer_size_in_bytes, void *buffer) = 0;

                // Write to the file descriptor from the supplied buffer
                virtual void write(vm_t &vm, const word_t buffer_size_in_bytes, void *buffer) = 0;

                // Seek to the supplied offset in the file descriptor
                virtual big_t seek(vm_t &vm, const seek_start_t seek_start, const big_t offset) = 0;

            protected:
                vm_fd_t(managed_ptr_t<const type_descriptor_t> td);
            };

            // Create a file descriptor based on a file system path
            vm_fd_t *create_from_file_path(vm_string_t *path, open_mode_t mode);

            struct std_streams final
            {
                vm_fd_t *input;
                vm_fd_t *output;
                vm_fd_t *error;
            };

            std_streams get_std_streams();
        }
    }
}

#endif // _DISVM_SRC_VM_SYS_FD_TYPES_HPP_
