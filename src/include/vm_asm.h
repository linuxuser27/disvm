//
// Dis VM
// File: vm_asm.h
// Author: arr
//

#ifndef _DISVM_SRC_INCLUDE_VM_ASM_H_
#define _DISVM_SRC_INCLUDE_VM_ASM_H_

#include <iosfwd>
#include "opcodes.h"

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
}

#endif // _DISVM_SRC_INCLUDE_VM_ASM_H_