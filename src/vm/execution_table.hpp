//
// Dis VM
// File: execution_table.hpp
// Author: arr
//

#ifndef _DISVM_SRC_VM_EXECUTION_TABLE_HPP_
#define _DISVM_SRC_VM_EXECUTION_TABLE_HPP_

#include "runtime.hpp"

namespace disvm
{
    namespace runtime
    {
        // VM instruction execution table
        extern const vm_exec_t vm_exec_table[];
    }
}

#endif // _DISVM_SRC_VM_EXECUTION_TABLE_HPP_
