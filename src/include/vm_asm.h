//
// Dis VM
// File: vm_asm.h
// Author: arr
//

#ifndef _DISVM_SRC_INCLUDE_VM_ASM_H_
#define _DISVM_SRC_INCLUDE_VM_ASM_H_

#include <iosfwd>
#include <cstdint>
#include "opcodes.h"
#include "runtime.h"
#include "utils.h"

namespace disvm
{
    namespace assembly
    {
        // Construct a single address code from the addressing enumerations
        constexpr disvm::runtime::addr_code_t construct_address_code(
            disvm::runtime::address_mode_t src,
            disvm::runtime::address_mode_middle_t mid,
            disvm::runtime::address_mode_t dest)
        {
            return static_cast<uint8_t>(mid) << 6 | static_cast<uint8_t>(src) << 3 | static_cast<uint8_t>(dest);
        }

        // Get string repsentation of opcode
        const char *opcode_to_token(disvm::opcode_t);

        // Get the opcode from this string.
        // If the supplied string cannot be resolved or if it is null, the 'invalid' opcode is returned.
        disvm::opcode_t token_to_opcode(const char *);
    }

    namespace symbol
    {
        // Type class enum for symbols is unrelated to signature types enum.
        // The historical reason for this isn't recorded but is found in
        // the official Limbo compiler.
        enum class type_class_t : uint8_t
        {
            type_index = '@',
            adt = 'a',
            adt_pick = 'p',
            tuple = 't',
            module = 'm',
            function = 'F',
            array = 'A',
            channel = 'C',
            list = 'L',
            ref = 'R',
            none = 'n',
            nil = 'N',
            big = 'B',
            byte = 'b',
            integer = 'i',
            real = 'f',
            string = 's',
            poly = 'P',
        };

        // Reference into source code
        struct source_ref_t
        {
            int32_t source_id;
            int32_t begin_line;
            int32_t begin_column;
            int32_t end_line;
            int32_t end_column;
        };

        // Enumeration for formatting a function name
        enum class function_name_format_t
        {
            name = 1,
            return_type = (1 << 1),
            arguments = (1 << 2),

            declaration = name | arguments | return_type,
        };

        DEFINE_ENUM_FLAG_OPERATORS(function_name_format_t);

        // Enumeration for advancing the pc
        enum class advance_pc_t
        {
            current, // Return the current pc
            next_pc,
            next_debug_statement, // Advance to the next debug statement (i.e. source language statement)
        };

        class symbol_pc_iter_t
        {
        public:
            // Set the current pc
            virtual void set_current_pc(disvm::runtime::vm_pc_t) = 0;

            // Advance the current pc.
            // Returns 'true' if the function was successful, otherwise 'false'.
            virtual bool try_advance_pc(advance_pc_t, disvm::runtime::vm_pc_t *pc_after_advance = nullptr) = 0;

            // Return the name of the function containing the current pc.
            // Optionally format the returned string.
            // This function will throw a 'std::runtime_error' if a function doesn't exist at the current pc.
            virtual std::string current_function_name(function_name_format_t f) const = 0;

            // Return the source location for the current pc
            virtual source_ref_t current_source_location() const = 0;

            // Get source file name based on ID
            virtual const std::string &get_source_by_id(int32_t source_id) const = 0;
        };

        struct function_data_t
        {
            disvm::runtime::vm_pc_t entry_pc;
            disvm::runtime::vm_pc_t limit_pc;
            std::string name;
        };

        class symbol_data_t
        {
        public:
            // Get name of the module
            virtual const std::string& get_module_name() const = 0;

            // Get instruction count in the symbol data
            virtual size_t get_instruction_count() const = 0;

            // Get list of functions in module.
            // Supply a formatting enum for the function name.
            virtual std::vector<function_data_t> get_functions(function_name_format_t f) const = 0;

            // Get pc iterator interface over symbol data
            virtual std::unique_ptr<symbol_pc_iter_t> get_pc_iter() const = 0;
        };

        // Read in symbol data from the supplied stream
        std::unique_ptr<symbol_data_t> read(std::istream &);
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
