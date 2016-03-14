//
// Dis VM
// File: loader.cpp
// Author: arr
//

#include <cassert>
#include <limits>
#include <memory>
#include <iostream>
#include <fstream>
#include <array>
#include <tuple>
#include <disvm.h>
#include <exceptions.h>
#include <vm_memory.h>
#include <utils.h>
#include <module_reader.h>
#include "buffered_reader.h"

using namespace disvm;
using namespace disvm::runtime;
using namespace disvm::format;

namespace
{
    // constants used in module processing
    struct module_constants
    {
        static const auto max_type_pointer_count = operand_t{ 128 * 1024 };
        static const auto no_error_handle_type_desc = operand_t{ -1 };
        static const auto no_entry_pc = vm_pc_t{ -1 };
        static const auto vm_module_type_desc_number = std::size_t{ 0 };
        static const auto max_module_name_bytes = std::size_t{ 128 }; // Including null
        static const auto array_address_stack_size = std::size_t{ 4 };
    };

    // enumeration for types in the data section
    enum class datum_type_t
    {
        value_bit8 = 1,
        value_bit32 = 2,
        utf_string = 3,
        value_real64 = 4,
        array = 5,
        set_array = 6,
        restore_load_address = 7,
        value_bit64 = 8
    };

    // Read the next operand as defined in the Dis VM specification.
    std::tuple<bool, operand_t> read_next_operand(util::buffered_reader_t &reader)
    {
        auto result = operand_t{ 0 };

        // Converting stack allocation to buffer since max size of operand is a machine word
        byte_t buffer[sizeof(result)];

        if (!reader.get_next_bytes(1, buffer))
            return std::make_tuple(false, 0);

        switch (buffer[0] & 0xc0)
        {
        case 0x00:
            // 1 byte operand
            result = buffer[0];
            break;

        case 0x40:
            // 1 byte operand - Preserve sign
            result = buffer[0] | ~0x7f;
            break;

        case 0x80:
            // 2 byte operand
            if (!reader.get_next_bytes(1, buffer + 1))
                return std::make_tuple(false, 0);

            // Preserve sign
            if ((buffer[0] & 0x20) != 0)
                buffer[0] |= ~0x3f;
            else
                buffer[0] &= 0x3f;

            result = (buffer[0] << 8);
            result |= buffer[1];
            break;

        case 0xc0:
            // 4 byte operand
            if (!reader.get_next_bytes(3, buffer + 1))
                return std::make_tuple(false, 0);

            // Preserve sign
            if ((buffer[0] & 0x20) != 0)
                buffer[0] |= ~0x3f;
            else
                buffer[0] &= 0x3f;

            result = (buffer[0] << 24);
            result |= (buffer[1] << 16);
            result |= (buffer[2] << 8);
            result |= buffer[3];
            break;

        default:
            assert(false && "Should not be possible");
        }

        return std::make_tuple(true, result);
    }

    // Read the next word as defined in the Dis VM specification.
    std::tuple<bool, word_t> read_next_word(util::buffered_reader_t &reader)
    {
        auto result = word_t{};

        // Converting stack allocation to buffer since max size of operand is a machine word
        byte_t buffer[sizeof(result)];

        if (!reader.get_next_bytes(sizeof(result), buffer))
            return std::make_tuple(false, 0);

        result = (buffer[0] << 24);
        result |= (buffer[1] << 16);
        result |= (buffer[2] << 8);
        result |= buffer[3];

        return std::make_tuple(true, result);
    }

    real_t convert_to_real(const operand_t f, const operand_t s)
    {
        auto u64 = static_cast<uint64_t>(f);
        u64 = (u64 << 32) | (static_cast<uint64_t>(s) & 0xffffffff);

        auto r = real_t{};
        std::memcpy(&r, &u64, sizeof(u64));

        static_assert(sizeof(u64) == sizeof(r), "Real should be 64-bits");
        return r;
    }

    bool read_next_string(util::buffered_reader_t &reader, std::vector<uint8_t> &char_buffer)
    {
        assert(char_buffer.size() == 0);
        char_buffer.reserve(1024);

        auto success = bool{};
        auto value = uint8_t{};
        std::tie(success, value) = reader.get_next_byte();
        while (success && value != 0)
        {
            char_buffer.push_back(value);
            std::tie(success, value) = reader.get_next_byte();
        }

        return success;
    }

    byte_t *read_8byte_segments(util::buffered_reader_t &reader, datum_type_t type, uint32_t segment_count, byte_t *dest)
    {
        assert(type == datum_type_t::value_real64 || type == datum_type_t::value_bit64);

        auto success = bool{};
        auto first = word_t{};
        auto second = word_t{};
        for (auto idx = uint32_t{ 0 }; idx < segment_count; ++idx)
        {
            std::tie(success, first) = read_next_word(reader);
            if (!success) throw module_reader_exception{ "Failed to read first word" };

            std::tie(success, second) = read_next_word(reader);
            if (!success) throw module_reader_exception{ "Failed to read second word" };

            if (type == datum_type_t::value_real64)
            {
                auto r = convert_to_real(first, second);
                *reinterpret_cast<real_t *>(dest) = r;
                dest += sizeof(r);
            }
            else
            {
                auto b = static_cast<big_t>(first);
                b = (b << 32) | (static_cast<big_t>(second) & 0xffffffff);
                *reinterpret_cast<big_t *>(dest) = b;
                dest += sizeof(b);
            }
        }

        return dest;
    }

    std::tuple<address_mode_middle_t, address_mode_t, address_mode_t> convert_to_address_mode(uint8_t instr_addr_mode)
    {
        // Taken from Dis VM specification
        // bit  7  6  5  4  3  2  1  0
        //     m1 m0 s2 s1 s0 d2 d1 d0

        static const auto mid_mask = uint8_t{ 0xc0 };
        static const auto src_mask = uint8_t{ 0x38 };
        static const auto dest_mask = uint8_t{ 0x07 };
        assert((mid_mask | src_mask | dest_mask) == std::numeric_limits<uint8_t>::max());

        const auto mid = static_cast<address_mode_middle_t>((instr_addr_mode & mid_mask) >> 6);
        const auto src = static_cast<address_mode_t>((instr_addr_mode & src_mask) >> 3);
        const auto dest = static_cast<address_mode_t>(instr_addr_mode & dest_mask);

        return std::make_tuple(mid, src, dest);
    }

    // See header format definition in Dis VM specification (http://www.vitanuova.com/inferno/man/6/dis.html)
    void read_header(util::buffered_reader_t &reader, vm_module_t &modobj)
    {
        auto success = bool{};
        auto &header = modobj.header;

        std::tie(success, header.magic_number) = read_next_operand(reader);
        if (!success)
            throw module_reader_exception{ "Failed to read magic number" };

        assert(header.magic_number == magic_number_constants::xmagic || header.magic_number == magic_number_constants::smagic);

        // If the object file is signed, read in the signature.
        if (header.magic_number == magic_number_constants::smagic)
        {
            std::tie(success, header.Signature.length) = read_next_operand(reader);
            if (!success) throw module_reader_exception{ "Failed to read signature length" };

            header.Signature.signature.reset(new byte_t[header.Signature.length]);
            const auto bytesRead = reader.get_next_bytes(header.Signature.length, header.Signature.signature.get());
            if (bytesRead != header.Signature.length) throw module_reader_exception{ "Failed to read full signature" };
        }
        else
        {
            header.Signature.length = word_t{ 0 };
        }

        auto runtime_flags = operand_t{};
        std::tie(success, runtime_flags) = read_next_operand(reader);
        if (!success) throw module_reader_exception{ "Failed to read runtime flag" };
        header.runtime_flag = static_cast<runtime_flags_t>(runtime_flags);

        std::tie(success, header.stack_extent) = read_next_operand(reader);
        if (!success) throw module_reader_exception{ "Failed to read stack extent" };

        std::tie(success, header.code_size) = read_next_operand(reader);
        if (!success) throw module_reader_exception{ "Failed to read code size" };

        std::tie(success, header.data_size) = read_next_operand(reader);
        if (!success) throw module_reader_exception{ "Failed to read data size" };

        std::tie(success, header.type_size) = read_next_operand(reader);
        if (!success) throw module_reader_exception{ "Failed to read type size" };

        std::tie(success, header.export_size) = read_next_operand(reader);
        if (!success) throw module_reader_exception{ "Failed to read export size" };

        // [SPEC] If a module does not have an entry program counter it is set to '-1'.
        std::tie(success, header.entry_pc) = read_next_operand(reader);
        if (!success) throw module_reader_exception{ "Failed to entry pc" };

        // [SPEC] If a module does not have an entry program counter the entry type index is set to '-1'.
        std::tie(success, header.entry_type) = read_next_operand(reader);
        if (!success) throw module_reader_exception{ "Failed to read entry type" };
    }

    // See code section layout in Dis VM specification (http://www.vitanuova.com/inferno/man/6/dis.html)
    void read_code_section(util::buffered_reader_t &reader, vm_module_t &modobj)
    {
        auto success = bool{};
        const auto instruction_count = modobj.header.code_size;

        // Allocate memory for the instructions
        modobj.code_section.resize(instruction_count);

        for (auto c = word_t{ 0 }; c < instruction_count; ++c)
        {
            // Read in a single instruction
            byte_t op_and_addrmode[2] = {};
            const auto bytesRead = reader.get_next_bytes(sizeof(op_and_addrmode), op_and_addrmode);
            if (bytesRead != sizeof(op_and_addrmode)) throw module_reader_exception{ "Failed to read op code and address mode" };

            auto vm_instr = vm_instruction_t{};
            auto &instr = vm_instr.op;

            instr.opcode = static_cast<opcode_t>(op_and_addrmode[0]);
            assert(opcode_t::first_opcode <= instr.opcode && instr.opcode <= opcode_t::last_opcode);

            std::tie(instr.middle.mode, instr.source.mode, instr.destination.mode) = convert_to_address_mode(op_and_addrmode[1]);

            if (instr.middle.mode != address_mode_middle_t::none)
            {
                std::tie(success, instr.middle.register1) = read_next_operand(reader);
                if (!success) throw module_reader_exception{ "Failed to read middle register" };
            }

            if (instr.source.mode != address_mode_t::none)
            {
                std::tie(success, instr.source.register1) = read_next_operand(reader);
                if (!success) throw module_reader_exception{ "Failed to read source register 1" };

                if (instr.source.mode == address_mode_t::offset_double_indirect_fp ||
                    instr.source.mode == address_mode_t::offset_double_indirect_mp)
                {
                    std::tie(success, instr.source.register2) = read_next_operand(reader);
                    if (!success) throw module_reader_exception{ "Failed to read source register 2" };

                    // Double indirect offsets need to be < 2^16-1.
                    assert(instr.source.register1 <= std::numeric_limits<uint16_t>::max());
                    assert(instr.source.register2 <= std::numeric_limits<uint16_t>::max());
                }
            }

            if (instr.destination.mode != address_mode_t::none)
            {
                std::tie(success, instr.destination.register1) = read_next_operand(reader);
                if (!success) throw module_reader_exception{ "Failed to read destination register 1" };

                if (instr.destination.mode == address_mode_t::offset_double_indirect_fp ||
                    instr.destination.mode == address_mode_t::offset_double_indirect_mp)
                {
                    std::tie(success, instr.destination.register2) = read_next_operand(reader);
                    if (!success) throw module_reader_exception{ "Failed to destination register 2" };

                    // Double indirect offsets need to be < 2^16-1.
                    assert(instr.destination.register1 <= std::numeric_limits<uint16_t>::max());
                    assert(instr.destination.register2 <= std::numeric_limits<uint16_t>::max());
                }
            }

            modobj.code_section[c] = std::move(vm_instr);
        }

        if (modobj.header.entry_pc != module_constants::no_entry_pc && static_cast<std::size_t>(modobj.header.entry_pc) >= modobj.code_section.size())
            throw module_reader_exception{ "Invalid initial program counter value for code section" };
    }

    // See type section layout in Dis VM specification (http://www.vitanuova.com/inferno/man/6/dis.html)
    void read_type_section(util::buffered_reader_t &reader, vm_module_t &modobj)
    {
        auto success = bool{};
        const auto type_count = modobj.header.type_size;

        // Allocate memory for the types
        modobj.type_section.resize(type_count);

        for (auto c = word_t{ 0 }; c < type_count; ++c)
        {
            // Read in single type
            operand_t desc_number;
            std::tie(success, desc_number) = read_next_operand(reader);
            if (!success) throw module_reader_exception{ "Failed to read type descriptor number" };

            operand_t size;
            std::tie(success, size) = read_next_operand(reader);
            if (!success) throw module_reader_exception{ "Failed to read type size" };

            operand_t map_in_bytes;
            std::tie(success, map_in_bytes) = read_next_operand(reader);
            if (!success) throw module_reader_exception{ "Failed to read type pointer count" };
            if (map_in_bytes > module_constants::max_type_pointer_count) throw module_reader_exception{ "Invalid Limbo type" };

            auto pointer_map = std::vector<uint8_t>(map_in_bytes);
            if (map_in_bytes != 0)
            {
                const auto bytesRead = reader.get_next_bytes(map_in_bytes, pointer_map.data());
                if (bytesRead != map_in_bytes) throw module_reader_exception{ "Failed to read type pointer map" };
            }

            modobj.type_section[desc_number] = std::move(type_descriptor_t::create(size, pointer_map));
        }
    }

    // See data section layout in Dis VM specification (http://www.vitanuova.com/inferno/man/6/dis.html)
    void read_data_section(util::buffered_reader_t &reader, vm_module_t &modobj)
    {
        if (modobj.header.data_size != 0)
        {
            auto vm_module_type = modobj.type_section[module_constants::vm_module_type_desc_number];
            if (vm_module_type->size_in_bytes != modobj.header.data_size) throw module_reader_exception{ "Invalid type desc for MP" };

            // The Dis VM spec does not guarantee the MP segment (i.e. data segment) of the loaded
            // module will be initialized to zero, but it is the C/C++ approach so defer to that.
            modobj.original_mp.reset(vm_alloc_t::allocate(vm_module_type, vm_alloc_t::zero_memory));
        }

        auto array_stack = std::array<byte_t *, module_constants::array_address_stack_size>{};
        auto array_stack_pointer = std::size_t{ 0 };
        auto success = bool{};
        auto code = byte_t{};
        auto base = reinterpret_cast<byte_t *>(modobj.original_mp->get_allocation());
        for (;;)
        {
            auto bytesRead = reader.get_next_bytes(sizeof(code), &code);
            if (bytesRead != sizeof(code)) throw module_reader_exception{ "Failed to read data item code" };
            if (code == 0)
                break;

            const auto type = static_cast<datum_type_t>((code & 0xf0) >> 4);
            auto count = static_cast<uint32_t>(code & 0x0f);
            if (count == 0)
            {
                std::tie(success, count) = read_next_operand(reader);
                if (!success) throw module_reader_exception{ "Failed to read long item datum count" };
            }

            auto byte_offset = operand_t{};
            std::tie(success, byte_offset) = read_next_operand(reader);
            if (!success) throw module_reader_exception{ "Failed to read offset" };

            // Compute the next data destination.
            auto data_dest = base + byte_offset;

            switch (type)
            {
            case datum_type_t::value_bit8:
            {
                auto byte = uint8_t{};
                for (auto idx = uint32_t{ 0 }; idx < count; ++idx)
                {
                    std::tie(success, byte) = reader.get_next_byte();
                    if (!success) throw module_reader_exception{ "Failed to read 1 byte" };

                    *reinterpret_cast<byte_t *>(data_dest) = byte;
                    data_dest += sizeof(byte);
                }
                break;
            }
            case datum_type_t::value_bit32:
            {
                auto word = word_t{};
                for (auto idx = uint32_t{ 0 }; idx < count; ++idx)
                {
                    word = word_t{ 0 };
                    std::tie(success, word) = read_next_word(reader);
                    if (!success) throw module_reader_exception{ "Failed to read word" };

                    *reinterpret_cast<word_t *>(data_dest) = word;
                    data_dest += sizeof(word);
                }
                break;
            }
            case datum_type_t::utf_string:
            {
                auto str_buffer = std::vector<uint8_t>(count);
                auto bytesRead = reader.get_next_bytes(count, str_buffer.data());
                if (bytesRead != count)
                    throw module_reader_exception{ "Failed to read string data" };

                auto new_string = new vm_string_t(str_buffer.size(), str_buffer.data());
                *reinterpret_cast<pointer_t *>(data_dest) = new_string->get_allocation();
                break;
            }
            case datum_type_t::value_real64:
                data_dest = read_8byte_segments(reader, datum_type_t::value_real64, count, data_dest);
                break;
            case datum_type_t::array:
            {
                auto element_type = word_t{};
                std::tie(success, element_type) = read_next_word(reader);
                if (!success) throw module_reader_exception{ "Failed to read word" };

                if (element_type < 0 || modobj.header.type_size < element_type)
                    if (!success) throw module_reader_exception{ "Invalid array element type" };

                auto element_count = word_t{};
                std::tie(success, element_count) = read_next_word(reader);
                if (!success) throw module_reader_exception{ "Failed to read word" };

                auto new_array = new vm_array_t(modobj.type_section[element_type], element_count);
                *reinterpret_cast<pointer_t *>(data_dest) = new_array->get_allocation();
                break;
            }
            case datum_type_t::set_array:
            {
                if (data_dest == nullptr) throw module_reader_exception{ "Invalid data type processing" };
                auto arr_maybe = vm_alloc_t::from_allocation(*reinterpret_cast<pointer_t *>(data_dest));
                assert(arr_maybe != nullptr);

                if (arr_maybe->alloc_type != vm_array_t::type_desc())
                    throw module_reader_exception{ "Data index not an array type" };

                auto arr = dynamic_cast<vm_array_t *>(arr_maybe);

                auto array_index = word_t{};
                std::tie(success, array_index) = read_next_word(reader);
                if (!success) throw module_reader_exception{ "Failed to read word" };

                if (array_index < 0 || arr->get_length() <= array_index || array_stack_pointer >= module_constants::array_address_stack_size)
                    throw module_reader_exception{ "Invalid array stack pointer" };

                array_stack[array_stack_pointer++] = base;
                base = reinterpret_cast<byte_t *>(arr->at(0)) + (array_index * arr->alloc_type->size_in_bytes);

                break;
            }
            case datum_type_t::restore_load_address:
                if (array_stack_pointer == 0) throw module_reader_exception{ "Invalid array stack pointer" };

                base = array_stack[--array_stack_pointer];
                assert(base != nullptr);

#ifndef NDEBUG
                array_stack[array_stack_pointer] = nullptr;
#endif

                break;
            case datum_type_t::value_bit64:
                data_dest = read_8byte_segments(reader, datum_type_t::value_bit64, count, data_dest);
                break;
            default:
                throw module_reader_exception{ "Unknown item data type" };
            }
        }
    }

    // See module name in Dis VM specification (http://www.vitanuova.com/inferno/man/6/dis.html)
    void read_module_name(util::buffered_reader_t &reader, vm_module_t &modobj)
    {
        auto string_buffer = std::vector<uint8_t>{};
        if (!read_next_string(reader, string_buffer))
            throw module_reader_exception{ "Failed to read module name" };

        modobj.module_name = std::make_unique<vm_string_t>(string_buffer.size(), string_buffer.data());
        assert(modobj.module_name->get_length() <= module_constants::max_module_name_bytes);
    }

    // See link section in Dis VM specification (http://www.vitanuova.com/inferno/man/6/dis.html)
    void read_link_section(util::buffered_reader_t &reader, vm_module_t &modobj)
    {
        auto success = bool{};
        const auto export_count = modobj.header.export_size;

        // Allocate memory for the export entries
        modobj.export_section.reserve(export_count);

        for (auto c = word_t{ 0 }; c < export_count; ++c)
        {
            // Read in single export item
            auto item = export_function_t{};

            std::tie(success, item.pc) = read_next_operand(reader);
            if (!success) throw module_reader_exception{ "Failed to read linkage pc" };

            std::tie(success, item.frame_type) = read_next_operand(reader);
            if (!success) throw module_reader_exception{ "Failed to read linkage frame type index" };

            std::tie(success, item.sig) = read_next_word(reader);
            if (!success) throw module_reader_exception{ "Failed to read linkage signature" };

            auto string_buffer = std::vector<uint8_t>{};
            if (!read_next_string(reader, string_buffer))
                throw module_reader_exception{ "Failed to read linkage name" };

            item.name = std::make_unique<vm_string_t>(string_buffer.size(), string_buffer.data());

            const auto sig = item.sig;
            modobj.export_section.emplace(sig, std::move(item));
        }
    }

    // See import section in Dis VM specification (http://www.vitanuova.com/inferno/man/6/dis.html)
    // [SPEC] The specification does not seem to match what is observed in an actual module. 
    //        The function is modeled after the disassembler (dis.b and libinterp/load.c) instead.
    void read_import_section(util::buffered_reader_t &reader, vm_module_t &modobj)
    {
        auto success = bool{};
        auto module_import_count = operand_t{};
        std::tie(success, module_import_count) = read_next_operand(reader);
        if (!success) throw module_reader_exception{ "Failed to read module import count" };

        // Allocate memory for module imports
        modobj.import_section.reserve(module_import_count);

        for (auto c = operand_t{ 0 }; c < module_import_count; ++c)
        {
            // Read in single module import
            import_vm_module_t module{};

            auto func_import_count = operand_t{};
            std::tie(success, func_import_count) = read_next_operand(reader);
            if (!success) throw module_reader_exception{ "Failed to read function import count" };

            // Allocate memory for function imports
            module.functions.reserve(func_import_count);

            for (auto k = operand_t{ 0 }; k < func_import_count; ++k)
            {
                // Read in single function import
                import_function_t func{};

                std::tie(success, func.sig) = read_next_word(reader);
                if (!success) throw module_reader_exception{ "Failed to read import function signature" };

                auto string_buffer = std::vector<uint8_t>{};
                if (!read_next_string(reader, string_buffer))
                    throw module_reader_exception{ "Failed to read import function name" };

                func.name = std::make_unique<vm_string_t>(string_buffer.size(), string_buffer.data());

                module.functions.push_back(std::move(func));
            }

            modobj.import_section.push_back(std::move(module));
        }

        byte_t value;
        std::tie(success, value) = reader.get_next_byte();
        if (!success || value != 0)
            throw module_reader_exception{ "Failed to read final byte from import section" };
    }

    // See handler section in Dis VM specification (http://www.vitanuova.com/inferno/man/6/dis.html)
    void read_handler_section(util::buffered_reader_t &reader, vm_module_t &modobj)
    {
        auto success = bool{};
        auto handler_count = operand_t{};
        std::tie(success, handler_count) = read_next_operand(reader);

        // Allocate memory for handlers
        modobj.handler_section.reserve(handler_count);

        for (auto c = operand_t{ 0 }; c < handler_count; ++c)
        {
            // Read in single handler
            handler_t handler{};

            std::tie(success, handler.exception_offset) = read_next_operand(reader);
            if (!success) throw module_reader_exception{ "Failed to read exception offset" };

            std::tie(success, handler.begin_pc) = read_next_operand(reader);
            if (!success) throw module_reader_exception{ "Failed to read handler begin program counter" };

            std::tie(success, handler.end_pc) = read_next_operand(reader);
            if (!success) throw module_reader_exception{ "Failed to read handler end program counter" };

            auto type_desc_number = operand_t{};
            std::tie(success, type_desc_number) = read_next_operand(reader);
            if (!success) throw module_reader_exception{ "Failed to read handler type description ID" };

            if (type_desc_number != module_constants::no_error_handle_type_desc)
            {
                assert(0 <= type_desc_number && type_desc_number < static_cast<operand_t>(modobj.type_section.size()));
                handler.type_desc = modobj.type_section[type_desc_number];
                assert(handler.type_desc != nullptr);
            }

            auto handler_cases = operand_t{};
            std::tie(success, handler_cases) = read_next_operand(reader);

            // [SPEC] The operand read above contains two pieces of data.
            // The higher 16-bits represent the number of exception cases that catch exception types.
            // The lower 16-bits represent the total number of handled cases.
            // In the limbo language strings and exceptions can be caught in an exception handler.
            // Based on Inferno implementation libinterp/load.c.
            handler.exception_type_count = word_t{ handler_cases >> 16 };
            const auto total_count = (handler_cases & 0xffff);

            // Allocate memory for exceptions plus the wild card.
            handler.exception_table.reserve(total_count + 1);

            for (auto k = operand_t{ 0 }; k < total_count; ++k)
            {
                // Read in single exception
                exception_t except{};

                auto string_buffer = std::vector<uint8_t>{};
                if (!read_next_string(reader, string_buffer))
                    throw module_reader_exception{ "Failed to read exception name" };

                except.name = std::make_unique<vm_string_t>(string_buffer.size(), string_buffer.data());

                std::tie(success, except.pc) = read_next_operand(reader);
                if (!success) throw module_reader_exception{ "Failed to read exception pc" };

                handler.exception_table.push_back(std::move(except));
            }

            // Read in wildcard exception
            exception_t wildcard_exception{};
            std::tie(success, wildcard_exception.pc) = read_next_operand(reader);
            if (!success) throw module_reader_exception{ "Failed to read wildcard exception pc" };

            handler.exception_table.push_back(std::move(wildcard_exception));

            modobj.handler_section.push_back(std::move(handler));
        }

        byte_t value;
        std::tie(success, value) = reader.get_next_byte();
        if (!success || value != 0)
            throw module_reader_exception{ "Failed to read final byte from handler section" };
    }
}

export_function_t::export_function_t(export_function_t &&other)
    : pc{ other.pc }
    , frame_type{ other.frame_type }
    , sig{ other.sig }
    , name{ std::move(other.name) }
{
}

export_function_t::~export_function_t()
{
    if (name != nullptr)
        name->release();
}

import_function_t::import_function_t(import_function_t &&other)
    : sig{ other.sig }
    , name{ std::move(other.name) }
{
}

import_function_t::~import_function_t()
{
    if (name != nullptr)
        name->release();
}

import_vm_module_t::import_vm_module_t(import_vm_module_t &&other)
    : functions{ std::move(other.functions) }
{
}

exception_t::exception_t(exception_t &&other)
    : pc{ other.pc }
    , name{ std::move(other.name) }
{
}

exception_t::~exception_t()
{
    if (name != nullptr)
        name->release();
}

handler_t::handler_t(handler_t &&other)
    : exception_offset{ other.exception_offset }
    , begin_pc{ other.begin_pc }
    , end_pc{ other.end_pc }
    , type_desc{ std::move(other.type_desc) }
    , exception_table{ std::move(other.exception_table) }
    , exception_type_count{ other.exception_type_count }
{
}

vm_module_t::~vm_module_t()
{
    if (module_name != nullptr)
        module_name->release();

    if (original_mp != nullptr)
        original_mp->release();
}

std::unique_ptr<vm_module_t> disvm::read_module(const char *path)
{
    assert(path != nullptr);

    auto module_file = std::ifstream{ path, std::ifstream::binary };
    if (!module_file.is_open())
    {
        assert(false && "Failed to open module");
        throw vm_user_exception{ "Failed to open module" };
    }

    return read_module(module_file);
}

std::unique_ptr<runtime::vm_module_t> disvm::read_module(std::istream &data)
{
    util::buffered_reader_t reader{ data };

    auto modobj = std::make_unique<vm_module_t>();

    read_header(reader, *modobj);

    if (util::has_flag(modobj->header.runtime_flag, runtime_flags_t::has_import_deprecated))
        throw module_reader_exception{ "Obsolete module" };

    read_code_section(reader, *modobj);
    read_type_section(reader, *modobj);
    read_data_section(reader, *modobj);
    read_module_name(reader, *modobj);
    read_link_section(reader, *modobj);

    if (util::has_flag(modobj->header.runtime_flag, runtime_flags_t::has_import))
        read_import_section(reader, *modobj);

    if (util::has_flag(modobj->header.runtime_flag, runtime_flags_t::has_handler))
        read_handler_section(reader, *modobj);

    // [TODO] Native code generation - runtime_flags_t::must_compile
    assert(!util::has_flag(modobj->header.runtime_flag, runtime_flags_t::must_compile));

    return modobj;
}