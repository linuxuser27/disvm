//
// Dis VM
// File: print_to_stream.cpp
// Author: arr
//

#include <ostream>
#include <vm_asm.hpp>

std::ostream& disvm::runtime::operator<<(std::ostream &ss, const inst_data_generic_t &m)
{
    switch (m.mode)
    {
    case address_mode_t::offset_indirect_mp:
        ss << m.register1 << "(mp)";
        break;
    case address_mode_t::offset_indirect_fp:
        ss << m.register1 << "(fp)";
        break;
    case address_mode_t::immediate:
        ss << "$" << m.register1;
        break;
    case address_mode_t::none:
        ss << "_";
        break;
    case address_mode_t::offset_double_indirect_mp:
        ss << m.register2 << "(" << m.register1 << "(mp))";
        break;
    case address_mode_t::offset_double_indirect_fp:
        ss << m.register2 << "(" << m.register1 << "(fp))";
        break;

    case address_mode_t::reserved_1:
    case address_mode_t::reserved_2:
    default:
        ss << "??";
    }

    return ss;
}

std::ostream& disvm::runtime::operator<<(std::ostream &ss, const middle_data_t &m)
{
    switch (m.mode)
    {
    case address_mode_middle_t::none:
        ss << "_";
        break;
    case address_mode_middle_t::small_immediate:
        ss << "$" << m.register1;
        break;
    case address_mode_middle_t::small_offset_indirect_fp:
        ss << m.register1 << "(fp)";
        break;
    case address_mode_middle_t::small_offset_indirect_mp:
        ss << m.register1 << "(mp)";
        break;
    default:
        ss << "??";
    }

    return ss;
}

std::ostream& disvm::runtime::operator<<(std::ostream &ss, const vm_exec_op_t &m)
{
    const auto print_d = m.destination.mode != address_mode_t::none;
    const auto print_m = print_d || m.middle.mode != address_mode_middle_t::none;
    const auto print_s = print_m || m.source.mode != address_mode_t::none;

    ss << disvm::assembly::opcode_to_token(m.opcode);

    if (print_s)
        ss << " " << m.source;

    if (print_m)
        ss << " " << m.middle;

    if (print_d)
        ss << " " << m.destination;

    return ss;
}