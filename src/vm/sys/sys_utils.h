//
// Dis VM
// File: sys_utils.h
// Author: arr
//

#ifndef _DISVM_SRC_VM_SYS_SYS_UTILS_H_
#define _DISVM_SRC_VM_SYS_SYS_UTILS_H_

#include <vector>
#include <ios>
#include <memory>
#include <disvm.h>
#include <utils.h>
#include "fd_types.h"

namespace disvm
{
    namespace runtime
    {
        namespace sys
        {
            // Prints the format and arguments to the supplied buffer.
            // Returns the number of bytes written.
            word_t printf_to_buffer(
                disvm::vm_t &vm,
                const vm_string_t &msg_fmt,
                byte_t *msg_args,
                pointer_t base,
                const std::size_t buffer_size,
                char *buffer);

            // Prints the format and arguments to the supplied dynamic buffer.
            // Returns the number of bytes written.
            word_t printf_to_dynamic_buffer(
                disvm::vm_t &vm,
                const vm_string_t &msg_fmt,
                byte_t *msg_args,
                pointer_t base,
                std::vector<char> &buffer);

            const word_t vm_invalid_fd = -1;

            // No reference count updates are performed on supplied vm_fd_t instance.
            word_t create_fd_record(vm_fd_t *vm_fd);

            // Returns 'false' if the supplied file descriptor is invalid, otherwise 'true'.
            // No reference count updates are performed on supplied vm_fd_t instance.
            bool try_update_fd_record(word_t fd, vm_fd_t *vm_fd);

            // Returns null if the record does not exist.
            // If the record does exist, the reference count is incremented prior
            // to the instance being returned.
            vm_fd_t * fetch_fd_record(word_t fd);

            // The associated vm_fd_t instance reference count is decremented
            // and will be deleted once all references are released.
            void drop_fd_record(word_t fd);
        }
    }
}

#endif // _DISVM_SRC_VM_SYS_SYS_UTILS_H_
