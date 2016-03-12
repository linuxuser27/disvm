//
// Dis VM
// File: execution_table.h
// Author: arr
//

#ifndef _DISVM_SRC_VM_EXECUTION_TABLE_H_
#define _DISVM_SRC_VM_EXECUTION_TABLE_H_

#include "runtime.h"

namespace disvm
{
    namespace runtime
    {
        // VM instruction execution table
        extern const vm_exec_t vm_exec_table[];
    }
}

#endif // _DISVM_SRC_VM_EXECUTION_TABLE_H_