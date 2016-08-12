//
// Dis VM
// File: address_decoding.inc
// Author: arr
//

// This file should only be used by the thread class

// Decode source register
template<address_mode_t M>
void dec_src(const vm_exec_op_t &inst, vm_registers_t &reg)
{
    assert(false && "Unknown source addressing");
}

template<>
void dec_src<address_mode_t::offset_indirect_fp>(const vm_exec_op_t &inst, vm_registers_t &reg)
{
    assert(inst.source.mode == address_mode_t::offset_indirect_fp);
    reg.src = reinterpret_cast<pointer_t>(reinterpret_cast<uint8_t *>(reg.stack.peek_frame()->base()) + inst.source.register1);
}

template<>
void dec_src<address_mode_t::offset_indirect_mp>(const vm_exec_op_t &inst, vm_registers_t &reg)
{
    assert(inst.source.mode == address_mode_t::offset_indirect_mp);
    reg.src = reinterpret_cast<pointer_t>(reinterpret_cast<uint8_t *>(reg.mp_base->get_allocation()) + inst.source.register1);
}

template<>
void dec_src<address_mode_t::offset_double_indirect_fp>(const vm_exec_op_t &inst, vm_registers_t &reg)
{
    assert(inst.source.mode == address_mode_t::offset_double_indirect_fp);
    const auto frame_offset = *reinterpret_cast<std::size_t *>(reinterpret_cast<uint8_t *>(reg.stack.peek_frame()->base()) + inst.source.register1);
    reg.src = reinterpret_cast<pointer_t>(frame_offset + inst.source.register2);
}

template<>
void dec_src<address_mode_t::offset_double_indirect_mp>(const vm_exec_op_t &inst, vm_registers_t &reg)
{
    assert(inst.source.mode == address_mode_t::offset_double_indirect_mp);
    const auto mp_offset = *reinterpret_cast<std::size_t *>(reinterpret_cast<uint8_t *>(reg.mp_base->get_allocation()) + inst.source.register1);
    reg.src = reinterpret_cast<pointer_t>(mp_offset + inst.source.register2);
}

template<>
void dec_src<address_mode_t::immediate>(const vm_exec_op_t &inst, vm_registers_t &reg)
{
    assert(inst.source.mode == address_mode_t::immediate);
    reg.src = reinterpret_cast<pointer_t>(const_cast<word_t *>(&inst.source.register1));
}

template<>
void dec_src<address_mode_t::none>(const vm_exec_op_t &inst, vm_registers_t &reg)
{
    assert(inst.source.mode == address_mode_t::none);
    reg.src = nullptr;
}

// Decode destination register
template<address_mode_t M>
void dec_dest(const vm_exec_op_t &inst, vm_registers_t &reg)
{
    assert(false && "Unknown destination addressing");
}

template<>
void dec_dest<address_mode_t::offset_indirect_fp>(const vm_exec_op_t &inst, vm_registers_t &reg)
{
    assert(inst.destination.mode == address_mode_t::offset_indirect_fp);
    reg.dest = reinterpret_cast<pointer_t>(reinterpret_cast<uint8_t *>(reg.stack.peek_frame()->base()) + inst.destination.register1);
}

template<>
void dec_dest<address_mode_t::offset_indirect_mp>(const vm_exec_op_t &inst, vm_registers_t &reg)
{
    assert(inst.destination.mode == address_mode_t::offset_indirect_mp);
    reg.dest = reinterpret_cast<pointer_t>(reinterpret_cast<uint8_t *>(reg.mp_base->get_allocation()) + inst.destination.register1);
}

template<>
void dec_dest<address_mode_t::offset_double_indirect_fp>(const vm_exec_op_t &inst, vm_registers_t &reg)
{
    assert(inst.destination.mode == address_mode_t::offset_double_indirect_fp);
    const auto frame_offset = *reinterpret_cast<std::size_t *>(reinterpret_cast<uint8_t *>(reg.stack.peek_frame()->base()) + inst.destination.register1);
    reg.dest = reinterpret_cast<pointer_t>(frame_offset + inst.destination.register2);
}

template<>
void dec_dest<address_mode_t::offset_double_indirect_mp>(const vm_exec_op_t &inst, vm_registers_t &reg)
{
    assert(inst.destination.mode == address_mode_t::offset_double_indirect_mp);
    const auto mp_offset = *reinterpret_cast<std::size_t *>(reinterpret_cast<uint8_t *>(reg.mp_base->get_allocation()) + inst.destination.register1);
    reg.dest = reinterpret_cast<pointer_t>(mp_offset + inst.destination.register2);
}

template<>
void dec_dest<address_mode_t::immediate>(const vm_exec_op_t &inst, vm_registers_t &reg)
{
    assert(inst.destination.mode == address_mode_t::immediate);
    reg.dest = reinterpret_cast<pointer_t>(const_cast<word_t *>(&inst.destination.register1));
}

template<>
void dec_dest<address_mode_t::none>(const vm_exec_op_t &inst, vm_registers_t &reg)
{
    assert(inst.destination.mode == address_mode_t::none);
    reg.dest = nullptr;
}

// Decode middle register
template<address_mode_middle_t M>
void dec_mid(const vm_exec_op_t &inst, vm_registers_t &reg);

template<>
void dec_mid<address_mode_middle_t::small_offset_indirect_fp>(const vm_exec_op_t &inst, vm_registers_t &reg)
{
    assert(inst.middle.mode == address_mode_middle_t::small_offset_indirect_fp);
    reg.mid = reinterpret_cast<pointer_t>(reinterpret_cast<uint8_t *>(reg.stack.peek_frame()->base()) + inst.middle.register1);
}

template<>
void dec_mid<address_mode_middle_t::small_offset_indirect_mp>(const vm_exec_op_t &inst, vm_registers_t &reg)
{
    assert(inst.middle.mode == address_mode_middle_t::small_offset_indirect_mp);
    reg.mid = reinterpret_cast<pointer_t>(reinterpret_cast<uint8_t *>(reg.mp_base->get_allocation()) + inst.middle.register1);
}

template<>
void dec_mid<address_mode_middle_t::small_immediate>(const vm_exec_op_t &inst, vm_registers_t &reg)
{
    assert(inst.middle.mode == address_mode_middle_t::small_immediate);
    reg.mid = reinterpret_cast<pointer_t>(const_cast<word_t *>(&inst.middle.register1));
}

template<>
void dec_mid<address_mode_middle_t::none>(const vm_exec_op_t &inst, vm_registers_t &reg)
{
    assert(inst.middle.mode == address_mode_middle_t::none);
    // [SPEC] This is an undocumented expectation but required in many operations (e.g. i++).
    reg.mid = reg.dest;
}

template<address_mode_middle_t MM, address_mode_t SM, address_mode_t DM>
void da(const vm_exec_op_t &inst, vm_registers_t &reg)
{
    dec_src<SM>(inst, reg);
    dec_dest<DM>(inst, reg);
    dec_mid<MM>(inst, reg);
}

//
// The elements' of the decode table were printed out using the following function.
//
//void print_decode_table()
//{
//#define PRINT_CASE(c) case c: ::printf(#c); break;
//
//    static const auto mid_mask = uint8_t{ 0xc0 };
//    static const auto src_mask = uint8_t{ 0x38 };
//    static const auto dest_mask = uint8_t{ 0x07 };
//
//    for (auto i = uint32_t{ 0 }; i <= uint8_t(~0); ++i)
//    {
//        ::printf("da<");
//        const uint8_t mid = (i & mid_mask) >> 6;
//        switch (static_cast<address_mode_middle_t>(mid))
//        {
//            PRINT_CASE(address_mode_middle_t::none)
//            PRINT_CASE(address_mode_middle_t::small_immediate)
//            PRINT_CASE(address_mode_middle_t::small_offset_indirect_fp)
//            PRINT_CASE(address_mode_middle_t::small_offset_indirect_mp)
//        }
//        ::printf(",");
//        const uint8_t src = (i & src_mask) >> 3;
//        switch (static_cast<address_mode_t>(src))
//        {
//            PRINT_CASE(address_mode_t::offset_indirect_mp)
//            PRINT_CASE(address_mode_t::offset_indirect_fp)
//            PRINT_CASE(address_mode_t::immediate)
//            PRINT_CASE(address_mode_t::none)
//            PRINT_CASE(address_mode_t::offset_double_indirect_mp)
//            PRINT_CASE(address_mode_t::offset_double_indirect_fp)
//            PRINT_CASE(address_mode_t::reserved_1)
//            PRINT_CASE(address_mode_t::reserved_2)
//        }
//        ::printf(",");
//        const uint8_t dest = (i & dest_mask);
//        switch (static_cast<address_mode_t>(dest))
//        {
//            PRINT_CASE(address_mode_t::offset_indirect_mp)
//            PRINT_CASE(address_mode_t::offset_indirect_fp)
//            PRINT_CASE(address_mode_t::immediate)
//            PRINT_CASE(address_mode_t::none)
//            PRINT_CASE(address_mode_t::offset_double_indirect_mp)
//            PRINT_CASE(address_mode_t::offset_double_indirect_fp)
//            PRINT_CASE(address_mode_t::reserved_1)
//            PRINT_CASE(address_mode_t::reserved_2)
//        }
//        ::printf(">,\n");
//    }
//
//#undef PRINT_CASE
//}

void(*decode_table[])(const vm_exec_op_t &, vm_registers_t &) =
{
    da<address_mode_middle_t::none, address_mode_t::offset_indirect_mp, address_mode_t::offset_indirect_mp>,
    da<address_mode_middle_t::none, address_mode_t::offset_indirect_mp, address_mode_t::offset_indirect_fp>,
    da<address_mode_middle_t::none, address_mode_t::offset_indirect_mp, address_mode_t::immediate>,
    da<address_mode_middle_t::none, address_mode_t::offset_indirect_mp, address_mode_t::none>,
    da<address_mode_middle_t::none, address_mode_t::offset_indirect_mp, address_mode_t::offset_double_indirect_mp>,
    da<address_mode_middle_t::none, address_mode_t::offset_indirect_mp, address_mode_t::offset_double_indirect_fp>,
    da<address_mode_middle_t::none, address_mode_t::offset_indirect_mp, address_mode_t::reserved_1>,
    da<address_mode_middle_t::none, address_mode_t::offset_indirect_mp, address_mode_t::reserved_2>,
    da<address_mode_middle_t::none, address_mode_t::offset_indirect_fp, address_mode_t::offset_indirect_mp>,
    da<address_mode_middle_t::none, address_mode_t::offset_indirect_fp, address_mode_t::offset_indirect_fp>,
    da<address_mode_middle_t::none, address_mode_t::offset_indirect_fp, address_mode_t::immediate>,
    da<address_mode_middle_t::none, address_mode_t::offset_indirect_fp, address_mode_t::none>,
    da<address_mode_middle_t::none, address_mode_t::offset_indirect_fp, address_mode_t::offset_double_indirect_mp>,
    da<address_mode_middle_t::none, address_mode_t::offset_indirect_fp, address_mode_t::offset_double_indirect_fp>,
    da<address_mode_middle_t::none, address_mode_t::offset_indirect_fp, address_mode_t::reserved_1>,
    da<address_mode_middle_t::none, address_mode_t::offset_indirect_fp, address_mode_t::reserved_2>,
    da<address_mode_middle_t::none, address_mode_t::immediate, address_mode_t::offset_indirect_mp>,
    da<address_mode_middle_t::none, address_mode_t::immediate, address_mode_t::offset_indirect_fp>,
    da<address_mode_middle_t::none, address_mode_t::immediate, address_mode_t::immediate>,
    da<address_mode_middle_t::none, address_mode_t::immediate, address_mode_t::none>,
    da<address_mode_middle_t::none, address_mode_t::immediate, address_mode_t::offset_double_indirect_mp>,
    da<address_mode_middle_t::none, address_mode_t::immediate, address_mode_t::offset_double_indirect_fp>,
    da<address_mode_middle_t::none, address_mode_t::immediate, address_mode_t::reserved_1>,
    da<address_mode_middle_t::none, address_mode_t::immediate, address_mode_t::reserved_2>,
    da<address_mode_middle_t::none, address_mode_t::none, address_mode_t::offset_indirect_mp>,
    da<address_mode_middle_t::none, address_mode_t::none, address_mode_t::offset_indirect_fp>,
    da<address_mode_middle_t::none, address_mode_t::none, address_mode_t::immediate>,
    da<address_mode_middle_t::none, address_mode_t::none, address_mode_t::none>,
    da<address_mode_middle_t::none, address_mode_t::none, address_mode_t::offset_double_indirect_mp>,
    da<address_mode_middle_t::none, address_mode_t::none, address_mode_t::offset_double_indirect_fp>,
    da<address_mode_middle_t::none, address_mode_t::none, address_mode_t::reserved_1>,
    da<address_mode_middle_t::none, address_mode_t::none, address_mode_t::reserved_2>,
    da<address_mode_middle_t::none, address_mode_t::offset_double_indirect_mp, address_mode_t::offset_indirect_mp>,
    da<address_mode_middle_t::none, address_mode_t::offset_double_indirect_mp, address_mode_t::offset_indirect_fp>,
    da<address_mode_middle_t::none, address_mode_t::offset_double_indirect_mp, address_mode_t::immediate>,
    da<address_mode_middle_t::none, address_mode_t::offset_double_indirect_mp, address_mode_t::none>,
    da<address_mode_middle_t::none, address_mode_t::offset_double_indirect_mp, address_mode_t::offset_double_indirect_mp>,
    da<address_mode_middle_t::none, address_mode_t::offset_double_indirect_mp, address_mode_t::offset_double_indirect_fp>,
    da<address_mode_middle_t::none, address_mode_t::offset_double_indirect_mp, address_mode_t::reserved_1>,
    da<address_mode_middle_t::none, address_mode_t::offset_double_indirect_mp, address_mode_t::reserved_2>,
    da<address_mode_middle_t::none, address_mode_t::offset_double_indirect_fp, address_mode_t::offset_indirect_mp>,
    da<address_mode_middle_t::none, address_mode_t::offset_double_indirect_fp, address_mode_t::offset_indirect_fp>,
    da<address_mode_middle_t::none, address_mode_t::offset_double_indirect_fp, address_mode_t::immediate>,
    da<address_mode_middle_t::none, address_mode_t::offset_double_indirect_fp, address_mode_t::none>,
    da<address_mode_middle_t::none, address_mode_t::offset_double_indirect_fp, address_mode_t::offset_double_indirect_mp>,
    da<address_mode_middle_t::none, address_mode_t::offset_double_indirect_fp, address_mode_t::offset_double_indirect_fp>,
    da<address_mode_middle_t::none, address_mode_t::offset_double_indirect_fp, address_mode_t::reserved_1>,
    da<address_mode_middle_t::none, address_mode_t::offset_double_indirect_fp, address_mode_t::reserved_2>,
    da<address_mode_middle_t::none, address_mode_t::reserved_1, address_mode_t::offset_indirect_mp>,
    da<address_mode_middle_t::none, address_mode_t::reserved_1, address_mode_t::offset_indirect_fp>,
    da<address_mode_middle_t::none, address_mode_t::reserved_1, address_mode_t::immediate>,
    da<address_mode_middle_t::none, address_mode_t::reserved_1, address_mode_t::none>,
    da<address_mode_middle_t::none, address_mode_t::reserved_1, address_mode_t::offset_double_indirect_mp>,
    da<address_mode_middle_t::none, address_mode_t::reserved_1, address_mode_t::offset_double_indirect_fp>,
    da<address_mode_middle_t::none, address_mode_t::reserved_1, address_mode_t::reserved_1>,
    da<address_mode_middle_t::none, address_mode_t::reserved_1, address_mode_t::reserved_2>,
    da<address_mode_middle_t::none, address_mode_t::reserved_2, address_mode_t::offset_indirect_mp>,
    da<address_mode_middle_t::none, address_mode_t::reserved_2, address_mode_t::offset_indirect_fp>,
    da<address_mode_middle_t::none, address_mode_t::reserved_2, address_mode_t::immediate>,
    da<address_mode_middle_t::none, address_mode_t::reserved_2, address_mode_t::none>,
    da<address_mode_middle_t::none, address_mode_t::reserved_2, address_mode_t::offset_double_indirect_mp>,
    da<address_mode_middle_t::none, address_mode_t::reserved_2, address_mode_t::offset_double_indirect_fp>,
    da<address_mode_middle_t::none, address_mode_t::reserved_2, address_mode_t::reserved_1>,
    da<address_mode_middle_t::none, address_mode_t::reserved_2, address_mode_t::reserved_2>,
    da<address_mode_middle_t::small_immediate, address_mode_t::offset_indirect_mp, address_mode_t::offset_indirect_mp>,
    da<address_mode_middle_t::small_immediate, address_mode_t::offset_indirect_mp, address_mode_t::offset_indirect_fp>,
    da<address_mode_middle_t::small_immediate, address_mode_t::offset_indirect_mp, address_mode_t::immediate>,
    da<address_mode_middle_t::small_immediate, address_mode_t::offset_indirect_mp, address_mode_t::none>,
    da<address_mode_middle_t::small_immediate, address_mode_t::offset_indirect_mp, address_mode_t::offset_double_indirect_mp>,
    da<address_mode_middle_t::small_immediate, address_mode_t::offset_indirect_mp, address_mode_t::offset_double_indirect_fp>,
    da<address_mode_middle_t::small_immediate, address_mode_t::offset_indirect_mp, address_mode_t::reserved_1>,
    da<address_mode_middle_t::small_immediate, address_mode_t::offset_indirect_mp, address_mode_t::reserved_2>,
    da<address_mode_middle_t::small_immediate, address_mode_t::offset_indirect_fp, address_mode_t::offset_indirect_mp>,
    da<address_mode_middle_t::small_immediate, address_mode_t::offset_indirect_fp, address_mode_t::offset_indirect_fp>,
    da<address_mode_middle_t::small_immediate, address_mode_t::offset_indirect_fp, address_mode_t::immediate>,
    da<address_mode_middle_t::small_immediate, address_mode_t::offset_indirect_fp, address_mode_t::none>,
    da<address_mode_middle_t::small_immediate, address_mode_t::offset_indirect_fp, address_mode_t::offset_double_indirect_mp>,
    da<address_mode_middle_t::small_immediate, address_mode_t::offset_indirect_fp, address_mode_t::offset_double_indirect_fp>,
    da<address_mode_middle_t::small_immediate, address_mode_t::offset_indirect_fp, address_mode_t::reserved_1>,
    da<address_mode_middle_t::small_immediate, address_mode_t::offset_indirect_fp, address_mode_t::reserved_2>,
    da<address_mode_middle_t::small_immediate, address_mode_t::immediate, address_mode_t::offset_indirect_mp>,
    da<address_mode_middle_t::small_immediate, address_mode_t::immediate, address_mode_t::offset_indirect_fp>,
    da<address_mode_middle_t::small_immediate, address_mode_t::immediate, address_mode_t::immediate>,
    da<address_mode_middle_t::small_immediate, address_mode_t::immediate, address_mode_t::none>,
    da<address_mode_middle_t::small_immediate, address_mode_t::immediate, address_mode_t::offset_double_indirect_mp>,
    da<address_mode_middle_t::small_immediate, address_mode_t::immediate, address_mode_t::offset_double_indirect_fp>,
    da<address_mode_middle_t::small_immediate, address_mode_t::immediate, address_mode_t::reserved_1>,
    da<address_mode_middle_t::small_immediate, address_mode_t::immediate, address_mode_t::reserved_2>,
    da<address_mode_middle_t::small_immediate, address_mode_t::none, address_mode_t::offset_indirect_mp>,
    da<address_mode_middle_t::small_immediate, address_mode_t::none, address_mode_t::offset_indirect_fp>,
    da<address_mode_middle_t::small_immediate, address_mode_t::none, address_mode_t::immediate>,
    da<address_mode_middle_t::small_immediate, address_mode_t::none, address_mode_t::none>,
    da<address_mode_middle_t::small_immediate, address_mode_t::none, address_mode_t::offset_double_indirect_mp>,
    da<address_mode_middle_t::small_immediate, address_mode_t::none, address_mode_t::offset_double_indirect_fp>,
    da<address_mode_middle_t::small_immediate, address_mode_t::none, address_mode_t::reserved_1>,
    da<address_mode_middle_t::small_immediate, address_mode_t::none, address_mode_t::reserved_2>,
    da<address_mode_middle_t::small_immediate, address_mode_t::offset_double_indirect_mp, address_mode_t::offset_indirect_mp>,
    da<address_mode_middle_t::small_immediate, address_mode_t::offset_double_indirect_mp, address_mode_t::offset_indirect_fp>,
    da<address_mode_middle_t::small_immediate, address_mode_t::offset_double_indirect_mp, address_mode_t::immediate>,
    da<address_mode_middle_t::small_immediate, address_mode_t::offset_double_indirect_mp, address_mode_t::none>,
    da<address_mode_middle_t::small_immediate, address_mode_t::offset_double_indirect_mp, address_mode_t::offset_double_indirect_mp>,
    da<address_mode_middle_t::small_immediate, address_mode_t::offset_double_indirect_mp, address_mode_t::offset_double_indirect_fp>,
    da<address_mode_middle_t::small_immediate, address_mode_t::offset_double_indirect_mp, address_mode_t::reserved_1>,
    da<address_mode_middle_t::small_immediate, address_mode_t::offset_double_indirect_mp, address_mode_t::reserved_2>,
    da<address_mode_middle_t::small_immediate, address_mode_t::offset_double_indirect_fp, address_mode_t::offset_indirect_mp>,
    da<address_mode_middle_t::small_immediate, address_mode_t::offset_double_indirect_fp, address_mode_t::offset_indirect_fp>,
    da<address_mode_middle_t::small_immediate, address_mode_t::offset_double_indirect_fp, address_mode_t::immediate>,
    da<address_mode_middle_t::small_immediate, address_mode_t::offset_double_indirect_fp, address_mode_t::none>,
    da<address_mode_middle_t::small_immediate, address_mode_t::offset_double_indirect_fp, address_mode_t::offset_double_indirect_mp>,
    da<address_mode_middle_t::small_immediate, address_mode_t::offset_double_indirect_fp, address_mode_t::offset_double_indirect_fp>,
    da<address_mode_middle_t::small_immediate, address_mode_t::offset_double_indirect_fp, address_mode_t::reserved_1>,
    da<address_mode_middle_t::small_immediate, address_mode_t::offset_double_indirect_fp, address_mode_t::reserved_2>,
    da<address_mode_middle_t::small_immediate, address_mode_t::reserved_1, address_mode_t::offset_indirect_mp>,
    da<address_mode_middle_t::small_immediate, address_mode_t::reserved_1, address_mode_t::offset_indirect_fp>,
    da<address_mode_middle_t::small_immediate, address_mode_t::reserved_1, address_mode_t::immediate>,
    da<address_mode_middle_t::small_immediate, address_mode_t::reserved_1, address_mode_t::none>,
    da<address_mode_middle_t::small_immediate, address_mode_t::reserved_1, address_mode_t::offset_double_indirect_mp>,
    da<address_mode_middle_t::small_immediate, address_mode_t::reserved_1, address_mode_t::offset_double_indirect_fp>,
    da<address_mode_middle_t::small_immediate, address_mode_t::reserved_1, address_mode_t::reserved_1>,
    da<address_mode_middle_t::small_immediate, address_mode_t::reserved_1, address_mode_t::reserved_2>,
    da<address_mode_middle_t::small_immediate, address_mode_t::reserved_2, address_mode_t::offset_indirect_mp>,
    da<address_mode_middle_t::small_immediate, address_mode_t::reserved_2, address_mode_t::offset_indirect_fp>,
    da<address_mode_middle_t::small_immediate, address_mode_t::reserved_2, address_mode_t::immediate>,
    da<address_mode_middle_t::small_immediate, address_mode_t::reserved_2, address_mode_t::none>,
    da<address_mode_middle_t::small_immediate, address_mode_t::reserved_2, address_mode_t::offset_double_indirect_mp>,
    da<address_mode_middle_t::small_immediate, address_mode_t::reserved_2, address_mode_t::offset_double_indirect_fp>,
    da<address_mode_middle_t::small_immediate, address_mode_t::reserved_2, address_mode_t::reserved_1>,
    da<address_mode_middle_t::small_immediate, address_mode_t::reserved_2, address_mode_t::reserved_2>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::offset_indirect_mp, address_mode_t::offset_indirect_mp>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::offset_indirect_mp, address_mode_t::offset_indirect_fp>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::offset_indirect_mp, address_mode_t::immediate>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::offset_indirect_mp, address_mode_t::none>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::offset_indirect_mp, address_mode_t::offset_double_indirect_mp>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::offset_indirect_mp, address_mode_t::offset_double_indirect_fp>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::offset_indirect_mp, address_mode_t::reserved_1>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::offset_indirect_mp, address_mode_t::reserved_2>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::offset_indirect_fp, address_mode_t::offset_indirect_mp>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::offset_indirect_fp, address_mode_t::offset_indirect_fp>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::offset_indirect_fp, address_mode_t::immediate>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::offset_indirect_fp, address_mode_t::none>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::offset_indirect_fp, address_mode_t::offset_double_indirect_mp>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::offset_indirect_fp, address_mode_t::offset_double_indirect_fp>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::offset_indirect_fp, address_mode_t::reserved_1>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::offset_indirect_fp, address_mode_t::reserved_2>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::immediate, address_mode_t::offset_indirect_mp>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::immediate, address_mode_t::offset_indirect_fp>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::immediate, address_mode_t::immediate>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::immediate, address_mode_t::none>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::immediate, address_mode_t::offset_double_indirect_mp>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::immediate, address_mode_t::offset_double_indirect_fp>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::immediate, address_mode_t::reserved_1>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::immediate, address_mode_t::reserved_2>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::none, address_mode_t::offset_indirect_mp>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::none, address_mode_t::offset_indirect_fp>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::none, address_mode_t::immediate>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::none, address_mode_t::none>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::none, address_mode_t::offset_double_indirect_mp>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::none, address_mode_t::offset_double_indirect_fp>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::none, address_mode_t::reserved_1>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::none, address_mode_t::reserved_2>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::offset_double_indirect_mp, address_mode_t::offset_indirect_mp>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::offset_double_indirect_mp, address_mode_t::offset_indirect_fp>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::offset_double_indirect_mp, address_mode_t::immediate>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::offset_double_indirect_mp, address_mode_t::none>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::offset_double_indirect_mp, address_mode_t::offset_double_indirect_mp>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::offset_double_indirect_mp, address_mode_t::offset_double_indirect_fp>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::offset_double_indirect_mp, address_mode_t::reserved_1>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::offset_double_indirect_mp, address_mode_t::reserved_2>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::offset_double_indirect_fp, address_mode_t::offset_indirect_mp>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::offset_double_indirect_fp, address_mode_t::offset_indirect_fp>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::offset_double_indirect_fp, address_mode_t::immediate>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::offset_double_indirect_fp, address_mode_t::none>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::offset_double_indirect_fp, address_mode_t::offset_double_indirect_mp>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::offset_double_indirect_fp, address_mode_t::offset_double_indirect_fp>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::offset_double_indirect_fp, address_mode_t::reserved_1>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::offset_double_indirect_fp, address_mode_t::reserved_2>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::reserved_1, address_mode_t::offset_indirect_mp>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::reserved_1, address_mode_t::offset_indirect_fp>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::reserved_1, address_mode_t::immediate>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::reserved_1, address_mode_t::none>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::reserved_1, address_mode_t::offset_double_indirect_mp>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::reserved_1, address_mode_t::offset_double_indirect_fp>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::reserved_1, address_mode_t::reserved_1>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::reserved_1, address_mode_t::reserved_2>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::reserved_2, address_mode_t::offset_indirect_mp>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::reserved_2, address_mode_t::offset_indirect_fp>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::reserved_2, address_mode_t::immediate>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::reserved_2, address_mode_t::none>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::reserved_2, address_mode_t::offset_double_indirect_mp>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::reserved_2, address_mode_t::offset_double_indirect_fp>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::reserved_2, address_mode_t::reserved_1>,
    da<address_mode_middle_t::small_offset_indirect_fp, address_mode_t::reserved_2, address_mode_t::reserved_2>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::offset_indirect_mp, address_mode_t::offset_indirect_mp>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::offset_indirect_mp, address_mode_t::offset_indirect_fp>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::offset_indirect_mp, address_mode_t::immediate>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::offset_indirect_mp, address_mode_t::none>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::offset_indirect_mp, address_mode_t::offset_double_indirect_mp>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::offset_indirect_mp, address_mode_t::offset_double_indirect_fp>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::offset_indirect_mp, address_mode_t::reserved_1>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::offset_indirect_mp, address_mode_t::reserved_2>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::offset_indirect_fp, address_mode_t::offset_indirect_mp>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::offset_indirect_fp, address_mode_t::offset_indirect_fp>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::offset_indirect_fp, address_mode_t::immediate>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::offset_indirect_fp, address_mode_t::none>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::offset_indirect_fp, address_mode_t::offset_double_indirect_mp>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::offset_indirect_fp, address_mode_t::offset_double_indirect_fp>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::offset_indirect_fp, address_mode_t::reserved_1>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::offset_indirect_fp, address_mode_t::reserved_2>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::immediate, address_mode_t::offset_indirect_mp>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::immediate, address_mode_t::offset_indirect_fp>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::immediate, address_mode_t::immediate>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::immediate, address_mode_t::none>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::immediate, address_mode_t::offset_double_indirect_mp>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::immediate, address_mode_t::offset_double_indirect_fp>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::immediate, address_mode_t::reserved_1>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::immediate, address_mode_t::reserved_2>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::none, address_mode_t::offset_indirect_mp>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::none, address_mode_t::offset_indirect_fp>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::none, address_mode_t::immediate>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::none, address_mode_t::none>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::none, address_mode_t::offset_double_indirect_mp>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::none, address_mode_t::offset_double_indirect_fp>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::none, address_mode_t::reserved_1>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::none, address_mode_t::reserved_2>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::offset_double_indirect_mp, address_mode_t::offset_indirect_mp>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::offset_double_indirect_mp, address_mode_t::offset_indirect_fp>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::offset_double_indirect_mp, address_mode_t::immediate>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::offset_double_indirect_mp, address_mode_t::none>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::offset_double_indirect_mp, address_mode_t::offset_double_indirect_mp>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::offset_double_indirect_mp, address_mode_t::offset_double_indirect_fp>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::offset_double_indirect_mp, address_mode_t::reserved_1>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::offset_double_indirect_mp, address_mode_t::reserved_2>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::offset_double_indirect_fp, address_mode_t::offset_indirect_mp>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::offset_double_indirect_fp, address_mode_t::offset_indirect_fp>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::offset_double_indirect_fp, address_mode_t::immediate>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::offset_double_indirect_fp, address_mode_t::none>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::offset_double_indirect_fp, address_mode_t::offset_double_indirect_mp>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::offset_double_indirect_fp, address_mode_t::offset_double_indirect_fp>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::offset_double_indirect_fp, address_mode_t::reserved_1>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::offset_double_indirect_fp, address_mode_t::reserved_2>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::reserved_1, address_mode_t::offset_indirect_mp>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::reserved_1, address_mode_t::offset_indirect_fp>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::reserved_1, address_mode_t::immediate>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::reserved_1, address_mode_t::none>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::reserved_1, address_mode_t::offset_double_indirect_mp>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::reserved_1, address_mode_t::offset_double_indirect_fp>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::reserved_1, address_mode_t::reserved_1>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::reserved_1, address_mode_t::reserved_2>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::reserved_2, address_mode_t::offset_indirect_mp>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::reserved_2, address_mode_t::offset_indirect_fp>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::reserved_2, address_mode_t::immediate>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::reserved_2, address_mode_t::none>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::reserved_2, address_mode_t::offset_double_indirect_mp>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::reserved_2, address_mode_t::offset_double_indirect_fp>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::reserved_2, address_mode_t::reserved_1>,
    da<address_mode_middle_t::small_offset_indirect_mp, address_mode_t::reserved_2, address_mode_t::reserved_2>,
};
