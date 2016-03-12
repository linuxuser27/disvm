//
// Dis VM
// File: module_reader.h
// Author: arr
//

#ifndef _DISVM_SRC_INCLUDE_MODULE_READER_H_
#define _DISVM_SRC_INCLUDE_MODULE_READER_H_

#include <cstdint>
#include "runtime.h"

namespace disvm
{
    namespace format
    {
        using operand_t = int32_t;
        static_assert(sizeof(operand_t) == sizeof(runtime::word_t), "Operand and word should be the same size");
        static_assert(sizeof(operand_t) == sizeof(runtime::vm_pc_t), "Operand and VM program counter type should be the same size");

        struct magic_number_constants final
        {
            static const auto xmagic = operand_t{ 819248 }; // Normal module (0xC8030)
            static const auto smagic = operand_t{ 923426 }; // Signed module (0xE1722)
        };
    }
}

#endif // _DISVM_SRC_INCLUDE_MODULE_READER_H_