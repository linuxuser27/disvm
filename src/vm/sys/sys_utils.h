//
// Dis VM
// File: sys_utils.h
// Author: arr
//

#ifndef _DISVM_SRC_VM_SYS_SYS_UTILS_H_
#define _DISVM_SRC_VM_SYS_SYS_UTILS_H_

#include <vector>
#include <iosfwd>
#include <memory>
#include <disvm.h>
#include <utils.h>

namespace disvm
{
    namespace runtime
    {
        namespace sys
        {
            // Prints the format and arguments to the supplied buffer.
            // Returns the number of bytes written.
            word_t printf_to_buffer(
                const vm_string_t &msg_fmt,
                byte_t *msg_args,
                pointer_t base,
                const std::size_t buffer_size,
                char *buffer);

            // Prints the format and arguments to the supplied dynamic buffer.
            // Returns the number of bytes written.
            word_t printf_to_dynamic_buffer(
                const vm_string_t &msg_fmt,
                byte_t *msg_args,
                pointer_t base,
                std::vector<char> &buffer);

            enum class cfs_flags_t
            {
                none = 0,
                atomic = 1,
                ensure_create = 2,
            };

            DEFINE_ENUM_FLAG_OPERATORS(cfs_flags_t);

            std::unique_ptr<std::fstream> create_file_stream(const char *path, std::ios::openmode mode, cfs_flags_t flags = sys::cfs_flags_t::none);

            const word_t vm_invalid_fd = -1;

            // The first three FD records are optimized for lock-less access.
            word_t create_fd_record(vm_alloc_t *fd_alloc);

            vm_alloc_t * fetch_fd_record(word_t fd);

            void drop_fd_record(word_t fd);

        }
    }
}

#endif // _DISVM_SRC_VM_SYS_SYS_UTILS_H_
