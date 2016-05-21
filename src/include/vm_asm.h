//
// Dis VM
// File: vm_asm.h
// Author: arr
//

#ifndef _DISVM_SRC_INCLUDE_VM_ASM_H_
#define _DISVM_SRC_INCLUDE_VM_ASM_H_

#include <iosfwd>
#include "opcodes.h"
#include "runtime.h"

namespace disvm
{
    namespace assembly
    {
        // Get string repsentation of opcode
        const char *opcode_to_token(disvm::opcode_t);

        // Get the opcode from this string.
        // If the supplied string cannot be resolved or if it is null, the 'invalid' opcode is returned.
        disvm::opcode_t token_to_opcode(const char *);
    }

    namespace runtime
    {
        // Operators for printing bytecode instructions
        std::ostream& operator<<(std::ostream &, const disvm::runtime::inst_data_generic_t &);
        std::ostream& operator<<(std::ostream &, const disvm::runtime::middle_data_t &);
        std::ostream& operator<<(std::ostream &, const disvm::runtime::vm_exec_op_t &);
    }
}

#endif // _DISVM_SRC_INCLUDE_VM_ASM_H_