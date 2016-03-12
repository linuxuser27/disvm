//
// Dis VM
// File: sys_utils.h
// Author: arr
//

#ifndef _DISVM_SRC_VM_SYS_UTILS_H_
#define _DISVM_SRC_VM_SYS_UTILS_H_

#include <vector>
#include <disvm.h>

// Prints the format and arguments to the supplied buffer.
// Returns the number of bytes written.
disvm::runtime::word_t printf_to_buffer(const disvm::runtime::vm_string_t &msg_fmt, disvm::runtime::byte_t *msg_args, std::size_t buffer_size, char *buffer);

// Prints the format and arguments to the supplied dynamic buffer.
// Returns the number of bytes written.
disvm::runtime::word_t printf_to_dynamic_buffer(const disvm::runtime::vm_string_t &msg_fmt, disvm::runtime::byte_t *msg_args, std::vector<char> &buffer);

#endif // _DISVM_SRC_VM_SYS_UTILS_H_
