//
// Dis VM
// File: execution_table.cpp
// Author: arr
//

#include <type_traits>
#include <numeric>
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>
#include <disvm.h>
#include <vm_memory.h>
#include <opcodes.h>
#include <utils.h>
#include <exceptions.h>
#include <debug.h>
#include <builtin_module.h>
#include "tool_dispatch.h"
#include "execution_table.h"

using namespace disvm;
using namespace disvm::runtime;

namespace
{
    //
    // Private VM primitive data type definitions
    //

    using uword_t = uint32_t; // unsigned word_t
    using ubig_t = uint64_t; // unsigned big_t

#define EXEC_DECL(inst_name) void inst_name(vm_registers_t &r, vm_t& vm)

    EXEC_DECL(notimpl) { assert(false && "Instruction not implemented"); throw vm_system_exception{ "Instruction not implemented" }; }

    EXEC_DECL(invalid) { throw vm_system_exception{ "Illegal Dis VM instruction" }; }

    // [SPEC] Documented as 'Clear exception stack' and defined to be used at the discretion of the VM implementation.
    // Implemented as a 'spare slot' in Inferno
    EXEC_DECL(eclr) { assert(false && "The 'eclr' instruction is not expected to be used"); }

    //
    // Bit-wise operations
    //

    EXEC_DECL(andb) { vt_ref<byte_t>(r.dest) = vt_ref<byte_t>(r.src) & vt_ref<byte_t>(r.mid); }
    EXEC_DECL(andw) { vt_ref<word_t>(r.dest) = vt_ref<word_t>(r.src) & vt_ref<word_t>(r.mid); }
    EXEC_DECL(andl) { vt_ref<big_t>(r.dest) = vt_ref<big_t>(r.src) & vt_ref<big_t>(r.mid); }
    EXEC_DECL(orb) { vt_ref<byte_t>(r.dest) = vt_ref<byte_t>(r.src) | vt_ref<byte_t>(r.mid); }
    EXEC_DECL(orw) { vt_ref<word_t>(r.dest) = vt_ref<word_t>(r.src) | vt_ref<word_t>(r.mid); }
    EXEC_DECL(orl) { vt_ref<big_t>(r.dest) = vt_ref<big_t>(r.src) | vt_ref<big_t>(r.mid); }
    EXEC_DECL(xorb) { vt_ref<byte_t>(r.dest) = vt_ref<byte_t>(r.src) ^ vt_ref<byte_t>(r.mid); }
    EXEC_DECL(xorw) { vt_ref<word_t>(r.dest) = vt_ref<word_t>(r.src) ^ vt_ref<word_t>(r.mid); }
    EXEC_DECL(xorl) { vt_ref<big_t>(r.dest) = vt_ref<big_t>(r.src) ^ vt_ref<big_t>(r.mid); }

    //
    // Bit-shift operations
    //

    EXEC_DECL(shlb) { vt_ref<byte_t>(r.dest) = vt_ref<byte_t>(r.mid) << vt_ref<byte_t>(r.src); }
    EXEC_DECL(shlw) { vt_ref<word_t>(r.dest) = vt_ref<word_t>(r.mid) << vt_ref<word_t>(r.src); }
    EXEC_DECL(shll) { vt_ref<big_t>(r.dest) = vt_ref<big_t>(r.mid) << vt_ref<big_t>(r.src); }
    EXEC_DECL(shrb) { vt_ref<byte_t>(r.dest) = vt_ref<byte_t>(r.mid) >> vt_ref<byte_t>(r.src); }
    EXEC_DECL(shrw)
    {
        vt_ref<word_t>(r.dest) = vt_ref<word_t>(r.mid) >> vt_ref<word_t>(r.src);
        // Right shifting a negative number is implementation dependent in C++.
        // Assert the current compiler defaults to arithmetic shifting, not logical.
        assert(vt_ref<word_t>(r.mid) >= 0 || (vt_ref<word_t>(r.mid) < 0 && vt_ref<word_t>(r.dest) < 0));
    }

    EXEC_DECL(shrl)
    {
        vt_ref<big_t>(r.dest) = vt_ref<big_t>(r.mid) >> vt_ref<big_t>(r.src);
        // Right shifting a negative number is implementation dependent in C++.
        // Assert the current compiler defaults to arithmetic shifting, not logical.
        assert(vt_ref<big_t>(r.mid) >= 0 || (vt_ref<big_t>(r.mid) < 0 && vt_ref<big_t>(r.dest) < 0));
    }

    EXEC_DECL(lsrw) { vt_ref<word_t>(r.dest) = vt_ref<uword_t>(r.mid) >> vt_ref<word_t>(r.src); }
    EXEC_DECL(lsrl) { vt_ref<big_t>(r.dest) = vt_ref<ubig_t>(r.mid) >> vt_ref<big_t>(r.src); }

    //
    // Conversion operations
    //

    EXEC_DECL(cvtbw) { vt_ref<word_t>(r.dest) = vt_ref<byte_t>(r.src); }
    EXEC_DECL(cvtwb) { vt_ref<byte_t>(r.dest) = vt_ref<word_t>(r.src); }
    EXEC_DECL(cvtwl) { vt_ref<big_t>(r.dest) = vt_ref<word_t>(r.src); }
    EXEC_DECL(cvtlw) { vt_ref<word_t>(r.dest) = static_cast<word_t>(vt_ref<big_t>(r.src)); }
    EXEC_DECL(cvtrf) { vt_ref<short_real_t>(r.dest) = static_cast<short_real_t>(vt_ref<real_t>(r.src)); }
    EXEC_DECL(cvtfr) { vt_ref<real_t>(r.dest) = static_cast<real_t>(vt_ref<short_real_t>(r.src)); }
    EXEC_DECL(cvtws) { vt_ref<short_word_t>(r.dest) = vt_ref<word_t>(r.src); }
    EXEC_DECL(cvtsw) { vt_ref<word_t>(r.dest) = vt_ref<short_word_t>(r.src); }
    EXEC_DECL(cvtlf) { vt_ref<real_t>(r.dest) = static_cast<real_t>(vt_ref<big_t>(r.src)); }
    EXEC_DECL(cvtfl)
    {
        auto f = vt_ref<real_t>(r.src);
        f = f < 0 ? (f - 0.5) : (f + 0.5);
        vt_ref<big_t>(r.dest) = static_cast<big_t>(f);
    }

    EXEC_DECL(cvtxf) { vt_ref<real_t>(r.dest) = static_cast<real_t>(vt_ref<word_t>(r.src)) * vt_ref<real_t>(r.mid); }
    EXEC_DECL(cvtfx)
    {
        auto f = vt_ref<real_t>(r.src) * vt_ref<real_t>(r.mid);
        f = f < 0 ? (f - 0.5) : (f + 0.5);
        vt_ref<word_t>(r.dest) = static_cast<word_t>(f);
    }

    EXEC_DECL(cvtwf) { vt_ref<real_t>(r.dest) = static_cast<real_t>(vt_ref<word_t>(r.src)); }
    EXEC_DECL(cvtfw)
    {
        auto f = vt_ref<real_t>(r.src);
        f = f < 0 ? (f - 0.5) : (f + 0.5);
        vt_ref<word_t>(r.dest) = static_cast<word_t>(f);
    }

    // [SPEC] Op code for converting 'Fixed point' value to word_t
    // Undocumented: (http://www.vitanuova.com/inferno/papers/dis.html)
    EXEC_DECL(cvtxx)
    {
        const auto pow2_scale = vt_ref<word_t>(r.mid);
        auto res = big_t{ vt_ref<word_t>(r.src) };
        if (pow2_scale >= 0)
            res <<= pow2_scale;
        else
            res >>= (-pow2_scale);

        vt_ref<word_t>(r.dest) = static_cast<word_t>(res);
    }

    // [SPEC] Op code for converting 'Fixed point' value to word_t
    // Undocumented: (http://www.vitanuova.com/inferno/papers/dis.html)
    EXEC_DECL(cvtxx0)
    {
        auto tmp = big_t{ vt_ref<word_t>(r.src) };
        if (tmp == 0)
        {
            vt_ref<word_t>(r.dest) = 0;
            return;
        }

        const auto pow2_scale = vt_ref<word_t>(r.mid);
        if (pow2_scale >= 0)
            tmp <<= pow2_scale;
        else
            tmp >>= (-pow2_scale);

        const auto residual_scale = *reinterpret_cast<word_t *>(r.stack.peek_frame()->fixed_point_register_1());
        assert(residual_scale != 0);

        const auto result = tmp / residual_scale;
        vt_ref<word_t>(r.dest) = static_cast<word_t>(result);
    }

    EXEC_DECL(cvtca)
    {
        dec_ref_count_and_free(at_val<vm_alloc_t>(r.dest));

        auto str = at_val<vm_string_t>(r.src);
        auto new_array = new vm_array_t(str);
        pt_ref(r.dest) = new_array->get_allocation();
    }

    EXEC_DECL(cvtac)
    {
        dec_ref_count_and_free(at_val<vm_alloc_t>(r.dest));

        auto str = pointer_t{};
        auto arr = at_val<vm_array_t>(r.src);
        if (arr != nullptr)
        {
            if (arr->get_element_type() != intrinsic_type_desc::type<byte_t>())
                throw vm_user_exception{ "Invalid array element type for string conversion" };

            auto new_string = new vm_string_t{ static_cast<std::size_t>(arr->get_length()), reinterpret_cast<uint8_t *>(arr->at(0)) };
            str = new_string->get_allocation();
        }

        pt_ref(r.dest) = str;
    }

    EXEC_DECL(cvtcw)
    {
        auto result = word_t{ 0 };
        auto str = at_val<vm_string_t>(r.src);
        if (str != nullptr)
            result = static_cast<word_t>(::strtol(str->str(), nullptr, 10));

        vt_ref<word_t>(r.dest) = result;
    }

    EXEC_DECL(cvtwc)
    {
        // String represents the largest possible string from a word.
        auto buffer = std::array<char, sizeof("-2147483648")>{};

        const auto w = vt_ref<word_t>(r.src);
        const auto len = static_cast<std::size_t>(std::sprintf(buffer.data(), "%d", w));
        assert(len <= buffer.size());

        auto new_string = new vm_string_t{ len, reinterpret_cast<uint8_t *>(buffer.data()) };
        auto prev = at_val<vm_alloc_t>(r.dest);
        dec_ref_count_and_free(prev);

        pt_ref(r.dest) = new_string->get_allocation();
    }

    EXEC_DECL(cvtcf)
    {
        auto result = real_t{ 0.0 };
        auto str = at_val<vm_string_t>(r.src);
        if (str != nullptr)
            result = static_cast<real_t>(::strtod(str->str(), nullptr));

        vt_ref<real_t>(r.dest) = result;
    }

    EXEC_DECL(cvtfc)
    {
        // String represents the largest possible string from a double with scientific notation.
        auto buffer = std::array<char, sizeof("-2.2250738585072014e-308")>{};

        const auto real = vt_ref<real_t>(r.src);
        const auto len = static_cast<std::size_t>(std::sprintf(buffer.data(), "%g", real));
        assert(len <= buffer.size());

        auto new_string = new vm_string_t{ len, reinterpret_cast<uint8_t *>(buffer.data()) };

        auto prev = at_val<vm_alloc_t>(r.dest);
        dec_ref_count_and_free(prev);

        pt_ref(r.dest) = new_string->get_allocation();
    }

    EXEC_DECL(cvtcl)
    {
        auto result = big_t{ 0 };
        auto str = at_val<vm_string_t>(r.src);
        if (str != nullptr)
            result = static_cast<big_t>(::strtoll(str->str(), nullptr, 10));

        vt_ref<big_t>(r.dest) = result;
    }

    EXEC_DECL(cvtlc)
    {
        // String represents the largest possible string from a 64-bit integer.
        auto buffer = std::array<char, sizeof("-9223372036854775808")>{};

        const auto b = vt_ref<big_t>(r.src);
        const auto len = static_cast<std::size_t>(std::sprintf(buffer.data(), "%lld", b));
        assert(len <= buffer.size());

        auto new_string = new vm_string_t{ len, reinterpret_cast<uint8_t *>(buffer.data()) };

        auto prev = at_val<vm_alloc_t>(r.dest);
        dec_ref_count_and_free(prev);

        pt_ref(r.dest) = new_string->get_allocation();
    }

    //
    // Arithmetic operations
    //

    EXEC_DECL(negf) { vt_ref<real_t>(r.dest) = -vt_ref<real_t>(r.src); }
    EXEC_DECL(addb) { vt_ref<byte_t>(r.dest) = vt_ref<byte_t>(r.src) + vt_ref<byte_t>(r.mid); }
    EXEC_DECL(addw) { vt_ref<word_t>(r.dest) = vt_ref<word_t>(r.src) + vt_ref<word_t>(r.mid); }
    EXEC_DECL(addl) { vt_ref<big_t>(r.dest) = vt_ref<big_t>(r.src) + vt_ref<big_t>(r.mid); }
    EXEC_DECL(addf) { vt_ref<real_t>(r.dest) = vt_ref<real_t>(r.src) + vt_ref<real_t>(r.mid); }
    EXEC_DECL(subb) { vt_ref<byte_t>(r.dest) = vt_ref<byte_t>(r.mid) - vt_ref<byte_t>(r.src); }
    EXEC_DECL(subw) { vt_ref<word_t>(r.dest) = vt_ref<word_t>(r.mid) - vt_ref<word_t>(r.src); }
    EXEC_DECL(subl) { vt_ref<big_t>(r.dest) = vt_ref<big_t>(r.mid) - vt_ref<big_t>(r.src); }
    EXEC_DECL(subf) { vt_ref<real_t>(r.dest) = vt_ref<real_t>(r.mid) - vt_ref<real_t>(r.src); }
    EXEC_DECL(mulb) { vt_ref<byte_t>(r.dest) = vt_ref<byte_t>(r.src) * vt_ref<byte_t>(r.mid); }
    EXEC_DECL(mulw) { vt_ref<word_t>(r.dest) = vt_ref<word_t>(r.src) * vt_ref<word_t>(r.mid); }
    EXEC_DECL(mull) { vt_ref<big_t>(r.dest) = vt_ref<big_t>(r.src) * vt_ref<big_t>(r.mid); }
    EXEC_DECL(mulf) { vt_ref<real_t>(r.dest) = vt_ref<real_t>(r.src) * vt_ref<real_t>(r.mid); }

    template<typename PrimitiveType>
    void _div(vm_registers_t &r)
    {
        const auto denom = vt_ref<PrimitiveType>(r.src);
        if (denom == PrimitiveType{ 0 })
            throw divide_by_zero{};

        vt_ref<PrimitiveType>(r.dest) = vt_ref<PrimitiveType>(r.mid) / denom;
    }

    // [SPEC] The C++ defines division by 0 as undefined, however the IEEE standard does
    // define the behavior. Therefore the behavior is platform dependent and controlled
    // by the built-in Math module.
    template<>
    void _div<real_t>(vm_registers_t &r)
    {
        const auto denom = vt_ref<real_t>(r.src);
        vt_ref<real_t>(r.dest) = vt_ref<real_t>(r.mid) / denom;
    }

    EXEC_DECL(divb) { _div<byte_t>(r); }
    EXEC_DECL(divw) { _div<word_t>(r); }
    EXEC_DECL(divf) { _div<real_t>(r); }
    EXEC_DECL(divl) { _div<big_t>(r); }

    template<typename PrimitiveType>
    void _mod(vm_registers_t &r)
    {
        const auto denom = vt_ref<PrimitiveType>(r.src);
        if (denom == PrimitiveType{ 0 })
            throw divide_by_zero{};

        vt_ref<PrimitiveType>(r.dest) = vt_ref<PrimitiveType>(r.mid) % denom;
    }

    EXEC_DECL(modb) { _mod<byte_t>(r); }
    EXEC_DECL(modw) { _mod<word_t>(r); }
    EXEC_DECL(modl) { _mod<big_t>(r); }

    // [SPEC] 'Exponent' op code
    // Undocumented: (http://www.vitanuova.com/inferno/papers/dis.html)
    // Limbo reference: (http://www.vitanuova.com/inferno/papers/addendum.pdf)
    //
    // Inferno implementation: (libinterp/xec.c)
    //   src1 = power
    //   src2 = base
    //
    //   exp[w|l|f] src1 src2 dst
    //   dst = src2 'raised to' src1
    //
    // Result type matches the type of base. Power should be type 'word'.
    template<typename PrimitiveType>
    void _exp(vm_registers_t &r)
    {
        auto base = vt_ref<PrimitiveType>(r.mid);
        auto power = vt_ref<word_t>(r.src);

        auto inverse = false;
        if (power < 0)
        {
            power = (-power);
            inverse = true;
        }

        auto result = PrimitiveType{ 1 };
        for (;;)
        {
            if ((power & 1) != 0)
                result *= base;

            power >>= 1;
            if (power == 0)
                break;

            base *= base;
        }

        if (inverse)
            result = (1 / result);

        vt_ref<PrimitiveType>(r.dest) = result;
    }

    EXEC_DECL(expw) { _exp<word_t>(r); }
    EXEC_DECL(expl) { _exp<big_t>(r); }
    EXEC_DECL(expf) { _exp<real_t>(r); }

    // [SPEC] 'Fixed point' op codes
    // Undocumented: (http://www.vitanuova.com/inferno/papers/dis.html)
    // Limbo reference: (http://www.vitanuova.com/inferno/papers/addendum.pdf)

    // Fixed point identities
    //   X = s * x
    //   Y = t * y
    //   Z = u * z
    //
    // Multiplication:
    //   uz = sx * ty
    //    z = (sx/u) * (ty/u)
    //    z = (st/u) * (xy)
    //
    // Division:
    //   uz = sx/ty
    //    z = (sx/ty) * (1/u)
    //    z = (s/tu) * (x/y)

    // Multiply for fixed point types with final scaling as a power of 2.
    //  mulx src1 src2 dst
    //   src1 - integer 1 (y)
    //   src2 - integer 2 (x)
    //   fpr_2 - result scaling factor (2^p)
    //   dest - result integer (z)
    EXEC_DECL(mulx)
    {
        const auto x = big_t{ vt_ref<word_t>(r.mid) };
        const auto y = big_t{ vt_ref<word_t>(r.src) };
        auto z = x * y;

        const auto pow2_scale = *reinterpret_cast<word_t *>(r.stack.peek_frame()->fixed_point_register_2());
        if (pow2_scale >= 0)
            z <<= pow2_scale;
        else
            z >>= (-pow2_scale);

        vt_ref<word_t>(r.dest) = static_cast<word_t>(z);
    }

    // Division for fixed point types with final scaling as a power of 2.
    //  divx src1 src2 dst
    //   src1 - integer 1 (y)
    //   src2 - integer 2 (x)
    //   fpr_2 - result scaling factor (2^p)
    //   dest - result integer (z)
    EXEC_DECL(divx)
    {
        auto x = big_t{ vt_ref<word_t>(r.mid) };
        const auto pow2_scale = *reinterpret_cast<word_t *>(r.stack.peek_frame()->fixed_point_register_2());
        if (pow2_scale >= 0)
            x <<= pow2_scale;
        else
            x >>= (-pow2_scale);

        const auto y = big_t{ vt_ref<word_t>(r.src) };
        if (y == 0)
            throw divide_by_zero{};

        const auto z = x / y;
        vt_ref<word_t>(r.dest) = static_cast<word_t>(z);
    }

    //  mulx0 src1 src2 dst
    //   src1 - integer 1 (y)
    //   src2 - integer 2 (x)
    //   fpr_1 - residual factor
    //   fpr_2 - result scaling factor (2^p)
    //   dest - result integer (z)
    EXEC_DECL(mulx0)
    {
        const auto x = big_t{ vt_ref<word_t>(r.mid) };
        const auto y = big_t{ vt_ref<word_t>(r.src) };

        if (x == 0 || y == 0)
        {
            vt_ref<word_t>(r.dest) = 0;
            return;
        }

        auto tmp = x * y;
        const auto frame = r.stack.peek_frame();
        const auto pow2_scale = *reinterpret_cast<word_t *>(frame->fixed_point_register_2());
        if (pow2_scale >= 0)
            tmp <<= pow2_scale;
        else
            tmp >>= (-pow2_scale);

        const auto residual_scale = big_t{ *reinterpret_cast<word_t *>(frame->fixed_point_register_1()) };
        assert(residual_scale != 0);

        const auto z = tmp / residual_scale;
        vt_ref<word_t>(r.dest) = static_cast<word_t>(z);
    }

    //  divx0 src1 src2 dst
    //   src1 - integer 1 (y)
    //   src2 - integer 2 (x)
    //   fpr_1 - residual factor
    //   fpr_2 - result scaling factor (2^p)
    //   dest - result integer (z)
    EXEC_DECL(divx0)
    {
        const auto x = big_t{ vt_ref<word_t>(r.mid) };
        const auto y = big_t{ vt_ref<word_t>(r.src) };
        if (y == 0)
            throw divide_by_zero{};

        if (x == 0)
        {
            vt_ref<word_t>(r.dest) = 0;
            return;
        }

        const auto frame = r.stack.peek_frame();
        const auto residual_scale = big_t{ *reinterpret_cast<word_t *>(frame->fixed_point_register_1()) };

        auto tmp = x * residual_scale;
        const auto pow2_scale = *reinterpret_cast<word_t *>(frame->fixed_point_register_2());
        if (pow2_scale >= 0)
            tmp <<= pow2_scale;
        else
            tmp >>= (-pow2_scale);

        const auto z = tmp / y;
        vt_ref<word_t>(r.dest) = static_cast<word_t>(z);
    }

    //
    // Move operations
    //

    template<typename PrimitiveType>
    void _mov_(pointer_t dest, pointer_t src, const type_descriptor_t *)
    {
        vt_ref<PrimitiveType>(dest) = vt_ref<PrimitiveType>(src);
    }

    template<>
    void _mov_<vm_alloc_t>(pointer_t dest, pointer_t src, const type_descriptor_t *)
    {
        auto alloc_prev = at_val<vm_alloc_t>(dest);
        dec_ref_count_and_free(alloc_prev);

        auto alloc = at_val<vm_alloc_t>(src);
        if (alloc != nullptr)
        {
            alloc->add_ref();
            pt_ref(dest) = alloc->get_allocation();
        }
        else
        {
            pt_ref(dest) = nullptr;
        }
    }

    EXEC_DECL(movb) { _mov_<byte_t>(r.dest, r.src, nullptr); }
    EXEC_DECL(movw) { _mov_<word_t>(r.dest, r.src, nullptr); }
    EXEC_DECL(movf) { _mov_<real_t>(r.dest, r.src, nullptr); }
    EXEC_DECL(movl) { _mov_<big_t>(r.dest, r.src, nullptr); }
    EXEC_DECL(movp) { _mov_<vm_alloc_t>(r.dest, r.src, nullptr); }

    EXEC_DECL(movm)
    {
        assert(r.src != nullptr && r.dest != nullptr);

        std::memmove(r.dest, r.src, vt_ref<word_t>(r.mid));
    }

    EXEC_DECL(movmp)
    {
        auto type_id = vt_ref<word_t>(r.mid);
        const auto &types = r.module_ref->type_section;
        assert(0 <= type_id && type_id < static_cast<word_t>(types.size()));
        auto type = types[type_id];

        disvm::runtime::inc_ref_count_in_memory(*type, r.src);

        // Decrement allocated pointers in the destination.
        destroy_memory(*type, r.dest);

        assert(r.src != nullptr && r.dest != nullptr);
        std::memmove(r.dest, r.src, type->size_in_bytes);
    }

    //
    // Compare and Branch operations
    //

    EXEC_DECL(bltb) { if (vt_ref<byte_t>(r.src) < vt_ref<byte_t>(r.mid)) r.next_pc = vt_ref<vm_pc_t>(r.dest); }
    EXEC_DECL(bgtb) { if (vt_ref<byte_t>(r.src) > vt_ref<byte_t>(r.mid)) r.next_pc = vt_ref<vm_pc_t>(r.dest); }
    EXEC_DECL(bleb) { if (vt_ref<byte_t>(r.src) <= vt_ref<byte_t>(r.mid)) r.next_pc = vt_ref<vm_pc_t>(r.dest); }
    EXEC_DECL(bgeb) { if (vt_ref<byte_t>(r.src) >= vt_ref<byte_t>(r.mid)) r.next_pc = vt_ref<vm_pc_t>(r.dest); }
    EXEC_DECL(beqb) { if (vt_ref<byte_t>(r.src) == vt_ref<byte_t>(r.mid)) r.next_pc = vt_ref<vm_pc_t>(r.dest); }
    EXEC_DECL(bneb) { if (vt_ref<byte_t>(r.src) != vt_ref<byte_t>(r.mid)) r.next_pc = vt_ref<vm_pc_t>(r.dest); }

    EXEC_DECL(bltw) { if (vt_ref<word_t>(r.src) < vt_ref<word_t>(r.mid)) r.next_pc = vt_ref<vm_pc_t>(r.dest); }
    EXEC_DECL(bgtw) { if (vt_ref<word_t>(r.src) > vt_ref<word_t>(r.mid)) r.next_pc = vt_ref<vm_pc_t>(r.dest); }
    EXEC_DECL(blew) { if (vt_ref<word_t>(r.src) <= vt_ref<word_t>(r.mid)) r.next_pc = vt_ref<vm_pc_t>(r.dest); }
    EXEC_DECL(bgew) { if (vt_ref<word_t>(r.src) >= vt_ref<word_t>(r.mid)) r.next_pc = vt_ref<vm_pc_t>(r.dest); }
    EXEC_DECL(beqw) { if (vt_ref<word_t>(r.src) == vt_ref<word_t>(r.mid)) r.next_pc = vt_ref<vm_pc_t>(r.dest); }
    EXEC_DECL(bnew) { if (vt_ref<word_t>(r.src) != vt_ref<word_t>(r.mid)) r.next_pc = vt_ref<vm_pc_t>(r.dest); }

    EXEC_DECL(bltf) { if (vt_ref<real_t>(r.src) < vt_ref<real_t>(r.mid)) r.next_pc = vt_ref<vm_pc_t>(r.dest); }
    EXEC_DECL(bgtf) { if (vt_ref<real_t>(r.src) > vt_ref<real_t>(r.mid)) r.next_pc = vt_ref<vm_pc_t>(r.dest); }
    EXEC_DECL(blef) { if (vt_ref<real_t>(r.src) <= vt_ref<real_t>(r.mid)) r.next_pc = vt_ref<vm_pc_t>(r.dest); }
    EXEC_DECL(bgef) { if (vt_ref<real_t>(r.src) >= vt_ref<real_t>(r.mid)) r.next_pc = vt_ref<vm_pc_t>(r.dest); }
    EXEC_DECL(beqf) { if (vt_ref<real_t>(r.src) == vt_ref<real_t>(r.mid)) r.next_pc = vt_ref<vm_pc_t>(r.dest); }
    EXEC_DECL(bnef) { if (vt_ref<real_t>(r.src) != vt_ref<real_t>(r.mid)) r.next_pc = vt_ref<vm_pc_t>(r.dest); }

    EXEC_DECL(bltl) { if (vt_ref<big_t>(r.src) < vt_ref<big_t>(r.mid)) r.next_pc = vt_ref<vm_pc_t>(r.dest); }
    EXEC_DECL(bgtl) { if (vt_ref<big_t>(r.src) > vt_ref<big_t>(r.mid)) r.next_pc = vt_ref<vm_pc_t>(r.dest); }
    EXEC_DECL(blel) { if (vt_ref<big_t>(r.src) <= vt_ref<big_t>(r.mid)) r.next_pc = vt_ref<vm_pc_t>(r.dest); }
    EXEC_DECL(bgel) { if (vt_ref<big_t>(r.src) >= vt_ref<big_t>(r.mid)) r.next_pc = vt_ref<vm_pc_t>(r.dest); }
    EXEC_DECL(beql) { if (vt_ref<big_t>(r.src) == vt_ref<big_t>(r.mid)) r.next_pc = vt_ref<vm_pc_t>(r.dest); }
    EXEC_DECL(bnel) { if (vt_ref<big_t>(r.src) != vt_ref<big_t>(r.mid)) r.next_pc = vt_ref<vm_pc_t>(r.dest); }

    // Define case table constants
    // [SPEC]Table format is partially defined in Dis VM specification
    //          [element count] ([low value (inc)][high value (exc)][program counter])* [fallback program counter]
    namespace case_op_constants
    {
        const auto table_length_index = std::size_t{ 0 };
        const auto first_entry_index = std::size_t{ 1 };
        const auto entry_low_index = std::size_t{ 0 };
        const auto entry_high_index = std::size_t{ 1 };
        const auto entry_pc_index = std::size_t{ 2 };
        const auto entry_length = std::size_t{ 3 };
    }

    template<typename PrimitiveType>
    void _case(vm_registers_t &r)
    {
        const auto value = vt_ref<PrimitiveType>(r.src);

        assert(r.dest != nullptr);
        const auto table = reinterpret_cast<const PrimitiveType *>(r.dest);
        auto entry_count = table[case_op_constants::table_length_index];
        assert(entry_count < std::numeric_limits<word_t>::max());

        // Initialize the target PC with the fallback value.
        // This value is located after all entries in the table.
        auto target_pc = table[case_op_constants::first_entry_index + (entry_count * case_op_constants::entry_length)];

        // Move to first entry
        auto current_entry = table + case_op_constants::first_entry_index;

        // Find the matching table entry
        for (; 0 < entry_count; --entry_count)
        {
            // Check if value is in range
            if (value < current_entry[case_op_constants::entry_low_index] || current_entry[case_op_constants::entry_high_index] <= value)
            {
                current_entry += case_op_constants::entry_length;
                continue;
            }

            target_pc = current_entry[case_op_constants::entry_pc_index];
            break;
        }

        assert(0 <= static_cast<std::size_t>(target_pc) && static_cast<std::size_t>(target_pc) < r.module_ref->code_section.size());
        r.next_pc = static_cast<vm_pc_t>(target_pc);
    }

    EXEC_DECL(casew) { _case<word_t>(r); }
    EXEC_DECL(casel) { _case<big_t>(r); }

    // [SPEC] The documentation for the 'casec' instruction is a bit sparse
    // The full semantics are the case table should have string cases lexically
    // ordered (i.e. e[n] < e[n + 1]).
    EXEC_DECL(casec)
    {
        const auto value = at_val<vm_string_t>(r.src);

        assert(r.dest != nullptr);
        auto table = reinterpret_cast<word_t *>(r.dest);
        const auto entry_count = table[case_op_constants::table_length_index];

        // Initialize the target PC with the fallback value. 
        // This value is located after all entries in the table.
        auto target_pc = table[case_op_constants::first_entry_index + (entry_count * case_op_constants::entry_length)];

        // Move to first entry
        auto current_entry = table + case_op_constants::first_entry_index;

        // Find the matching table entry
        for (auto entry = word_t{ 0 }; entry < entry_count; ++entry)
        {
            auto string_low = at_val<vm_string_t>(reinterpret_cast<pointer_t>(current_entry + case_op_constants::entry_low_index));

            // Compare the value against the low string
            int result = vm_string_t::compare(value, string_low);
            if (result != 0)
            {
                // If the value compares less than the low string then skip the high string.
                if (result < 0)
                {
                    current_entry += case_op_constants::entry_length;
                    continue;
                }

                // Compare the value against the high string
                auto string_high = at_val<vm_string_t>(reinterpret_cast<pointer_t>(current_entry + case_op_constants::entry_high_index));
                if (string_high == nullptr || vm_string_t::compare(value, string_high) != 0)
                {
                    current_entry += case_op_constants::entry_length;
                    continue;
                }
            }

            target_pc = current_entry[case_op_constants::entry_pc_index];
            break;
        }

        assert(0 <= static_cast<std::size_t>(target_pc) && static_cast<std::size_t>(target_pc) < r.module_ref->code_section.size());
        r.next_pc = target_pc;
    }

    //
    // String operations
    //

    EXEC_DECL(bltc) { if (vm_string_t::compare(at_val<vm_string_t>(r.src), at_val<vm_string_t>(r.mid)) < 0) r.next_pc = vt_ref<vm_pc_t>(r.dest); }
    EXEC_DECL(bgtc) { if (vm_string_t::compare(at_val<vm_string_t>(r.src), at_val<vm_string_t>(r.mid)) > 0) r.next_pc = vt_ref<vm_pc_t>(r.dest); }
    EXEC_DECL(blec) { if (vm_string_t::compare(at_val<vm_string_t>(r.src), at_val<vm_string_t>(r.mid)) <= 0) r.next_pc = vt_ref<vm_pc_t>(r.dest); }
    EXEC_DECL(bgec) { if (vm_string_t::compare(at_val<vm_string_t>(r.src), at_val<vm_string_t>(r.mid)) >= 0) r.next_pc = vt_ref<vm_pc_t>(r.dest); }
    EXEC_DECL(beqc) { if (vm_string_t::compare(at_val<vm_string_t>(r.src), at_val<vm_string_t>(r.mid)) == 0) r.next_pc = vt_ref<vm_pc_t>(r.dest); }
    EXEC_DECL(bnec) { if (vm_string_t::compare(at_val<vm_string_t>(r.src), at_val<vm_string_t>(r.mid)) != 0) r.next_pc = vt_ref<vm_pc_t>(r.dest); }

    EXEC_DECL(lenc)
    {
        auto len = word_t{ 0 };
        const auto str = at_val<vm_string_t>(r.src);
        if (str != nullptr)
            len = str->get_length();

        vt_ref<word_t>(r.dest) = len;
    }

    EXEC_DECL(indc)
    {
        auto str = at_val<vm_string_t>(r.src);
        if (str == nullptr)
            throw dereference_nil{ "Index string" };

        const auto index = vt_ref<word_t>(r.mid);
        const auto rune = str->get_rune(index);

        vt_ref<runtime::rune_t>(r.dest) = rune;
    }

    // [SPEC] 'Insert Character' op code
    // Documented: (http://www.vitanuova.com/inferno/papers/dis.html)
    //   insc src1 src2 dst
    //   src1[src2] = dst
    //
    // Inferno implementation: (libinterp/string.c)
    //   insc src1 src2 dst
    //   dst[src2] = src1
    EXEC_DECL(insc)
    {
        auto rune = vt_ref<runtime::rune_t>(r.src);
        auto index = vt_ref<word_t>(r.mid);
        auto str = at_val<vm_string_t>(r.dest);
        if (str == nullptr)
            str = new vm_string_t{};
        else if (str->get_ref_count() > 1)
            str = new vm_string_t{ *str, 0, str->get_length() };

        // Set the rune at the supplied index.
        str->set_rune(index, rune);

        auto old_str = at_val<vm_string_t>(r.dest);
        if (str != old_str)
        {
            pt_ref(r.dest) = str->get_allocation();
            dec_ref_count_and_free(old_str);
        }
    }

    // Returns a string instance only if it is new
    vm_string_t *concat_string(vm_string_t *s1, const vm_string_t *s2, bool try_append_to_s1)
    {
        if (s1 == nullptr)
        {
            if (s2 == nullptr)
                return new vm_string_t{};

            return new vm_string_t{ *s2, 0, s2->get_length() };
        }

        // If there are other references to the string, it is immutable.
        if (s1->get_ref_count() > 1)
            try_append_to_s1 = false;

        if (s2 == nullptr)
        {
            if (try_append_to_s1)
                return nullptr;

            return new vm_string_t{ *s1, 0, s1->get_length() };
        }

        if (!try_append_to_s1)
            return new vm_string_t{ *s1, *s2 };

        s1->append(*s2);
        return nullptr;
    }

    // [SPEC] 'Concat strings' op code
    // Documented: (http://www.vitanuova.com/inferno/papers/dis.html)
    //   addc src1 src2 dst
    //   dst = src1 + src2
    //
    // Inferno implementation: (libinterp/string.c)
    //   addc src1 src2 dst
    //   dst = src2 + src1
    EXEC_DECL(addc)
    {
        const auto try_append_to_s1 = r.mid == r.dest;
        auto s1 = at_val<vm_string_t>(r.mid);
        const auto s2 = at_val<vm_string_t>(r.src);

        // If the source is the same as the destination, try to append the string during concatenation.
        const auto new_str_maybe = concat_string(s1, s2, try_append_to_s1);
        if (new_str_maybe != nullptr)
        {
            auto dest_maybe = at_val<vm_alloc_t>(r.dest);
            dec_ref_count_and_free(dest_maybe);

            pt_ref(r.dest) = new_str_maybe->get_allocation();
        }
    }

    EXEC_DECL(slicec)
    {
        const auto start = vt_ref<word_t>(r.src);
        const auto end = vt_ref<word_t>(r.mid);
        auto str = at_val<vm_string_t>(r.dest);

        if (str != nullptr)
        {
            auto new_string = new vm_string_t{ *str, start, end };
            pt_ref(r.dest) = new_string->get_allocation();
        }
        else if (start == 0 && end == 0)
        {
            pt_ref(r.dest) = nullptr;
        }
        else
        {
            throw dereference_nil{ "Slice a nil string" };
        }

        dec_ref_count_and_free(str);
    }

    //
    // Array operations
    //

    EXEC_DECL(lena)
    {
        auto len = word_t{ 0 };
        const auto arr = at_val<vm_array_t>(r.src);
        if (arr != nullptr)
            len = arr->get_length();

        vt_ref<word_t>(r.dest) = len;
    }

    EXEC_DECL(slicea)
    {
        const auto begin_index = vt_ref<word_t>(r.src);
        const auto end_index = vt_ref<word_t>(r.mid);
        const auto length = end_index - begin_index;

        auto arr = at_val<vm_array_t>(r.dest);
        if (arr == nullptr)
        {
            if (length == 0)
                return;

            throw vm_user_exception{ "Slice of empty array is invalid" };
        }
        else if (begin_index < 0 || arr->get_length() < end_index || length < 0)
        {
            throw out_of_range_memory{};
        }

        auto new_array = new vm_array_t(arr, begin_index, length);

        if (arr->alloc_type->map_in_bytes > 0)
            vm.get_garbage_collector().track_allocation(new_array);

        pt_ref(r.dest) = new_array->get_allocation();
    }

    EXEC_DECL(slicela)
    {
        auto src_arr = at_val<vm_array_t>(r.src);
        if (src_arr == nullptr)
            return;

        auto dest_arr = at_val<vm_array_t>(r.dest);
        if (dest_arr == nullptr)
            throw dereference_nil{ "Slice assignment" };

        const auto begin_index = vt_ref<word_t>(r.mid);
        dest_arr->copy_from(*src_arr, begin_index);
    }

    void _index_in(vm_registers_t &r)
    {
        const auto arr = at_val<vm_array_t>(r.src);
        if (arr == nullptr)
            throw dereference_nil{ "Indexing into array" };

        auto index = vt_ref<word_t>(r.dest);
        if (index < 0 || arr->get_length() <= index)
            throw index_out_of_range_memory{ 0, arr->get_length(), index };

        // Return the address, not the value
        pt_ref(r.mid) = arr->at(index);
    }

    EXEC_DECL(indb) { _index_in(r); }
    EXEC_DECL(indw) { _index_in(r); }
    EXEC_DECL(indf) { _index_in(r); }
    EXEC_DECL(indl) { _index_in(r); }
    EXEC_DECL(indx) { _index_in(r); }

    //
    // List operations
    //

    EXEC_DECL(lenl)
    {
        auto len = word_t{ 0 };
        const auto list = at_val<vm_list_t>(r.src);
        if (list != nullptr)
        {
            assert(list->alloc_type == vm_list_t::type_desc());
            len = list->get_length();
        }

        vt_ref<word_t>(r.dest) = len;
    }

    template<typename PrimitiveType>
    void _cons(vm_registers_t &r)
    {
        auto tail_maybe = at_val<vm_list_t>(r.dest);
        auto new_list = new vm_list_t{ intrinsic_type_desc::type<PrimitiveType>(), tail_maybe };

        if (tail_maybe != nullptr)
            tail_maybe->release();

        auto value = vt_ref<PrimitiveType>(r.src);
        vt_ref<PrimitiveType>(new_list->value()) = value;

        pt_ref(r.dest) = new_list->get_allocation();
    }

    EXEC_DECL(consb) { _cons<byte_t>(r); }
    EXEC_DECL(consw) { _cons<word_t>(r); }
    EXEC_DECL(consf) { _cons<real_t>(r); }
    EXEC_DECL(consl) { _cons<big_t>(r); }

    EXEC_DECL(consp)
    {
        auto tail_maybe = at_val<vm_list_t>(r.dest);

        // Create a new list
        auto new_list = new vm_list_t{ intrinsic_type_desc::type<pointer_t>(), tail_maybe };
        if (tail_maybe != nullptr)
            tail_maybe->release();

        auto alloc = at_val<vm_alloc_t>(r.src);
        if (alloc != nullptr)
        {
            alloc->add_ref(); 
            pt_ref(new_list->value()) = alloc->get_allocation();

            vm.get_garbage_collector().track_allocation(new_list);
        }
        else
        {
            pt_ref(new_list->value()) = nullptr;
        }

        // Return the list
        pt_ref(r.dest) = new_list->get_allocation();
    }

    EXEC_DECL(consmp)
    {
        auto type_id = vt_ref<word_t>(r.mid);
        const auto &types = r.module_ref->type_section;
        assert(0 <= type_id && type_id < static_cast<word_t>(types.size()));
        auto type = types[type_id];

        // Increment the ref count for element content
        inc_ref_count_in_memory(*type, r.src);

        auto tail_maybe = at_val<vm_list_t>(r.dest);

        // Create a new list
        auto new_list = new vm_list_t{ type, tail_maybe };
        if (tail_maybe != nullptr)
            tail_maybe->release();

        auto destination = new_list->value();
        std::memmove(destination, r.src, type->size_in_bytes);

        if (type->map_in_bytes > 0)
            vm.get_garbage_collector().track_allocation(new_list);

        // Return the list
        pt_ref(r.dest) = new_list->get_allocation();
    }

    template<typename PrimitiveType>
    void _head(vm_registers_t &r)
    {
        const auto head = at_val<vm_list_t>(r.src);
        if (head == nullptr)
            throw dereference_nil{ "Head of list" };

        assert(head->get_element_type() == intrinsic_type_desc::type<PrimitiveType>());
        auto head_value = head->value();
        vt_ref<PrimitiveType>(r.dest) = vt_ref<PrimitiveType>(head_value);
    }

    EXEC_DECL(headb) { _head<byte_t>(r); }
    EXEC_DECL(headw) { _head<word_t>(r); }
    EXEC_DECL(headf) { _head<real_t>(r); }
    EXEC_DECL(headl) { _head<big_t>(r); }

    EXEC_DECL(headp)
    {
        const auto list = at_val<vm_list_t>(r.src);
        if (list == nullptr)
            throw dereference_nil{ "Head of list" };

        r.src = list->value();
        movp(r, vm);
    }

    EXEC_DECL(headmp)
    {
        const auto list = at_val<vm_list_t>(r.src);
        if (list == nullptr)
            throw dereference_nil{ "Head of list" };

        r.src = list->value();
        movmp(r, vm);
    }

    EXEC_DECL(tail)
    {
        const auto list = at_val<vm_list_t>(r.src);
        if (list == nullptr)
            throw dereference_nil{ "Tail of list" };

        assert(list->alloc_type == vm_list_t::type_desc());
        auto tail = pointer_t{};

        auto tail_maybe = list->get_tail();
        if (tail_maybe != nullptr)
            tail = tail_maybe->get_allocation();

        // Use the address of the local variable.
        r.src = reinterpret_cast<pointer_t>(&tail);
        movp(r, vm);
    }

    //
    // Channel operations
    //

    namespace
    {
        void _channel_movm(pointer_t dest, pointer_t src, const type_descriptor_t *type)
        {
            assert(type != nullptr);
            std::memmove(dest, src, type->size_in_bytes);
        }

        void _channel_movmp(pointer_t dest, pointer_t src, const type_descriptor_t *type)
        {
            assert(type != nullptr);
            destroy_memory(*type, dest);

            inc_ref_count_in_memory(*type, src);
            std::memmove(dest, src, type->size_in_bytes);
        }

        // [SPEC] 'Create new channel' op codes
        // Documented: (http://www.vitanuova.com/inferno/papers/dis.html)
        // As documented the src2 register is not used, however the Inferno
        // implementation does use the src2 register. It is checked against
        // the dst register and if they are not equal the src2 register
        // contains the internal buffer size of the channel.
        //
        // See Inferno implementation: (libinterp/xec.c)
        void _newc_(
            vm_registers_t &r,
            const vm_t &vm,
            std::shared_ptr<const type_descriptor_t> type_desc,
            vm_channel_t::data_transfer_func_t transfer)
        {
            auto buffer_len = word_t{ 0 };
            if (r.mid != r.dest)
            {
                buffer_len = vt_ref<word_t>(r.mid);
                if (buffer_len < 0)
                    throw out_of_range_memory{};
            }

            dec_ref_count_and_free(at_val<vm_alloc_t>(r.dest));
            auto new_channel = new vm_channel_t{ type_desc, transfer, buffer_len };

            if (type_desc->map_in_bytes > 0)
                vm.get_garbage_collector().track_allocation(new_channel);

            pt_ref(r.dest) = new_channel->get_allocation();
        }
    }

    EXEC_DECL(newcb) { _newc_(r, vm, intrinsic_type_desc::type<byte_t>(), _mov_<byte_t>); }
    EXEC_DECL(newcw) { _newc_(r, vm, intrinsic_type_desc::type<word_t>(), _mov_<word_t>); }
    EXEC_DECL(newcf) { _newc_(r, vm, intrinsic_type_desc::type<real_t>(), _mov_<real_t>); }
    EXEC_DECL(newcl) { _newc_(r, vm, intrinsic_type_desc::type<big_t>(), _mov_<big_t>); }
    EXEC_DECL(newcp) { _newc_(r, vm, intrinsic_type_desc::type<pointer_t>(), _mov_<vm_alloc_t>); }

    EXEC_DECL(newcm)
    {
        auto memory_size = vt_ref<word_t>(r.src); 
        auto channel_data_type = type_descriptor_t::create(memory_size); 

        _newc_(r, vm, channel_data_type, _channel_movm);
    }

    EXEC_DECL(newcmp)
    {
        auto type_id = vt_ref<word_t>(r.src);
        const auto &types = r.module_ref->type_section;
        assert(0 <= type_id && type_id < static_cast<word_t>(types.size()));
        auto type = types[type_id];

        _newc_(r, vm, type, _channel_movmp);
    }

    EXEC_DECL(send)
    {
        auto channel = at_val<vm_channel_t>(r.dest);
        if (channel == nullptr)
            throw dereference_nil{ "Send to channel" };

        const auto current_thread_id = r.thread.get_thread_id();

        assert(!r.request_mutex.pending_request && "There should be no pending request for the thread");
        r.request_mutex.pending_request = true;

        assert(r.current_thread_state == vm_thread_state_t::running);
        r.current_thread_state = vm_thread_state_t::blocked_sending;

        // Setup the request
        auto request = vm_channel_request_t{ current_thread_id, r.request_mutex };
        request.data = r.src;
        request.request_handled_callback = [&vm, &r, current_thread_id](const vm_channel_t &)
        {
            assert(!r.request_mutex.pending_request && r.current_thread_state == vm_thread_state_t::blocked_sending);
            vm.get_scheduler_control().enqueue_blocked_thread(current_thread_id);
        };

        if (channel->send_data(request))
            r.current_thread_state = vm_thread_state_t::running;
    }

    EXEC_DECL(recv)
    {
        auto channel = at_val<vm_channel_t>(r.src);
        if (channel == nullptr)
            throw dereference_nil{ "Receive from channel" };

        const auto current_thread_id = r.thread.get_thread_id();

        assert(!r.request_mutex.pending_request && "There should be no pending request for the thread");
        r.request_mutex.pending_request = true;

        assert(r.current_thread_state == vm_thread_state_t::running);
        r.current_thread_state = vm_thread_state_t::blocked_receiving;

        // Setup the request
        auto request = vm_channel_request_t { current_thread_id, r.request_mutex };
        request.data = r.dest;
        request.request_handled_callback = [&vm, &r, current_thread_id](const vm_channel_t &)
        {
            assert(!r.request_mutex.pending_request && r.current_thread_state == vm_thread_state_t::blocked_receiving);
            vm.get_scheduler_control().enqueue_blocked_thread(current_thread_id);
        };

        if (channel->receive_data(request))
            r.current_thread_state = vm_thread_state_t::running;
    }

    namespace
    {
        std::minstd_rand alt_channel_selector;

        word_t cancel_outstanding_channel_requests(uint32_t request_thread_id, vm_alt_stack_layout_t *alt, const vm_channel_t &handled_channel)
        {
            auto alloc = handled_channel.get_allocation();

            auto handled_index = word_t{ -1 };
            const auto channel_count = (alt->send_count + alt->receive_count);
            for (auto i = word_t{ 0 }; i < channel_count; ++i)
            {
                auto ch_pointer = reinterpret_cast<pointer_t>(alt->channels[i].alloc);
                if (ch_pointer == alloc)
                {
                    assert(handled_index == -1 && "Index should only be set once");
                    handled_index = i;
                    continue;
                }

                auto ch = at_val<vm_channel_t>(reinterpret_cast<pointer_t>(&ch_pointer));
                ch->cancel_request(request_thread_id);
            }

            assert(handled_index != -1 && "Index should be set");
            return handled_index;
        }

        void exec_alt(vm_registers_t &r, const vm_t &vm, const bool blocking)
        {
            auto alt = reinterpret_cast<vm_alt_stack_layout_t *>(r.src);
            assert(alt != nullptr);

            const auto current_thread_id = r.thread.get_thread_id();

            assert(!r.request_mutex.pending_request && "There should be no pending request for the thread");
            r.request_mutex.pending_request = true;

            assert(r.current_thread_state == vm_thread_state_t::running);
            r.current_thread_state = vm_thread_state_t::blocked_in_alt;

            // Create request callback
            auto alt_request_handled_callback = [&vm, &r, alt, current_thread_id](const vm_channel_t &handled_channel)
            {
                // Cancel all non-handled requests and determine the
                // index of the channel that handled this request.
                auto handled_index = cancel_outstanding_channel_requests(current_thread_id, alt, handled_channel);

                // Set the index of the channel that handled the request
                vt_ref<word_t>(r.dest) = handled_index;

                assert(!r.request_mutex.pending_request && r.current_thread_state == vm_thread_state_t::blocked_in_alt);
                vm.get_scheduler_control().enqueue_blocked_thread(current_thread_id);
            };

            const auto channel_count = (alt->send_count + alt->receive_count);
            assert(channel_count > 0);

            // Generate a random channel index sequence.
            auto random_idx_sequence = std::vector<word_t>(channel_count);
            std::iota(std::begin(random_idx_sequence), std::end(random_idx_sequence), 0);
            std::shuffle(std::begin(random_idx_sequence), std::end(random_idx_sequence), alt_channel_selector);

            auto requests_queued = uint32_t{ 0 };
            auto handled = bool{};
            for (auto rnd_idx : random_idx_sequence)
            {
                auto &channel_stack_layout = alt->channels[rnd_idx];
                if (channel_stack_layout.alloc == runtime_constants::nil)
                    throw dereference_nil{ "Alt channel" };

                auto channel = at_val<vm_channel_t>(reinterpret_cast<pointer_t>(&channel_stack_layout.alloc));

                auto request = vm_channel_request_t{ current_thread_id, r.request_mutex };
                request.data = reinterpret_cast<pointer_t>(channel_stack_layout.data);
                request.request_handled_callback = alt_request_handled_callback;

                if (rnd_idx < alt->send_count)
                    handled = channel->send_data(request);
                else
                    handled = channel->receive_data(request);

                if (handled)
                {
                    // Check if requests need to be handled
                    if (requests_queued > 0)
                    {
                        auto handled_index = cancel_outstanding_channel_requests(current_thread_id, alt, *channel);
                        assert(handled_index == rnd_idx);
                    }

                    vt_ref<word_t>(r.dest) = rnd_idx;
                    r.current_thread_state = vm_thread_state_t::running;
                    return;
                }
                else
                {
                    requests_queued++;
                }
            }

            // If this is a non-blocking alt request and we are here no channels
            // were immediately able to handle the request. Try to lock the request
            // so we can cancel it in all channels.
            if (!blocking && r.request_mutex.pending_request_lock.try_lock())
            {
                std::lock_guard<std::mutex> lock{ r.request_mutex.pending_request_lock, std::adopt_lock };
                // Check if a channel handled the request between the initial queueing and now.
                if (r.request_mutex.pending_request)
                {
                    r.request_mutex.pending_request = false;

                    if (debug::is_component_tracing_enabled<debug::component_trace_t::channel>())
                        debug::log_msg(debug::component_trace_t::channel, debug::log_level_t::debug, "alt: request: non-blocking");

                    // Cancel request in all channels.
                    for (auto i = word_t{ 0 }; i < channel_count; ++i)
                    {
                        auto ch_pointer = reinterpret_cast<pointer_t>(alt->channels[i].alloc);
                        auto ch = at_val<vm_channel_t>(reinterpret_cast<pointer_t>(&ch_pointer));
                        ch->cancel_request(current_thread_id);
                    }

                    // Index of a non-blocking request is the final index of the alt.
                    vt_ref<word_t>(r.dest) = channel_count;
                    r.current_thread_state = vm_thread_state_t::running;
                }
            }
        }
    }

    EXEC_DECL(alt)
    {
        exec_alt(r, vm, true /* blocking */);
    }

    EXEC_DECL(nbalt)
    {
        exec_alt(r, vm, false /* blocking */);
    }

    //
    // Dynamic allocation operations
    //

    namespace
    {
        void new_instance(vm_registers_t &r, const vm_t &vm)
        {
            const auto type_id = vt_ref<word_t>(r.src);
            const auto &types = r.module_ref->type_section;
            assert(0 <= type_id && type_id < static_cast<word_t>(types.size()));

            dec_ref_count_and_free(at_val<vm_alloc_t>(r.dest));

            auto type_desc = types[type_id];
            auto new_alloc = vm_alloc_t::allocate(type_desc);

            if (type_desc->map_in_bytes > 0)
                vm.get_garbage_collector().track_allocation(new_alloc);

            pt_ref(r.dest) = new_alloc->get_allocation();
        }
    }

    EXEC_DECL(new_)
    {
        new_instance(r, vm);
    }

    EXEC_DECL(newz)
    {
        // See allocation implementation for guarantee about
        // always using zeroed memory.
        new_instance(r, vm);
    }

    EXEC_DECL(mnewz)
    {
        auto target_module = at_val<vm_module_ref_t>(r.src);
        if (target_module == nullptr)
            throw vm_user_exception{ "Module not loaded" };

        const auto type_id = vt_ref<word_t>(r.mid);
        const auto &types = target_module->type_section;
        assert(0 <= type_id && type_id < static_cast<word_t>(types.size()));

        dec_ref_count_and_free(at_val<vm_alloc_t>(r.dest));

        auto type_desc = types[type_id];
        auto new_alloc = vm_alloc_t::allocate(type_desc);

        if (type_desc->map_in_bytes > 0)
            vm.get_garbage_collector().track_allocation(new_alloc);

        pt_ref(r.dest) = new_alloc->get_allocation();
    }

    namespace
    {
        void new_array(vm_registers_t &r, const vm_t &vm)
        {
            assert(r.module_ref != nullptr);
            const auto &types = r.module_ref->type_section;

            const auto type_id = vt_ref<word_t>(r.mid);
            assert(0 <= type_id && type_id < static_cast<word_t>(types.size()));
            auto type_desc = types[type_id];

            dec_ref_count_and_free(at_val<vm_alloc_t>(r.dest));

            const auto array_size = vt_ref<word_t>(r.src);
            auto new_array = new vm_array_t(type_desc, array_size);

            if (type_desc->map_in_bytes > 0)
                vm.get_garbage_collector().track_allocation(new_array);

            pt_ref(r.dest) = new_array->get_allocation();
        }
    }

    EXEC_DECL(newa)
    {
        new_array(r, vm);
    }

    EXEC_DECL(newaz)
    {
        // See allocation implementation for guarantee about
        // always using zeroed memory.
        new_array(r, vm);
    }

    EXEC_DECL(tcmp)
    {
        const auto s = at_val<vm_alloc_t>(r.src);
        const auto d = at_val<vm_alloc_t>(r.dest);

        if (s == nullptr)
            return;

        if (d == nullptr || !s->alloc_type->is_equal(d->alloc_type.get()))
            throw type_violation{};
    }

    //
    // Stack operations
    //

    // [SPEC] 'runt' op code
    // Undocumented: (http://www.vitanuova.com/inferno/papers/dis.html)
    //
    // Inferno implementation: (libinterp/xec.c)
    //   No operands and appears to be a type of NOP instruction.
    EXEC_DECL(runt) { /* NOP */ }

    EXEC_DECL(lea) { pt_ref(r.dest) = r.src; }

    EXEC_DECL(frame)
    {
        const auto &types = r.module_ref->type_section;

        const auto frame_type_id = vt_ref<word_t>(r.src);
        assert(0 <= frame_type_id && frame_type_id < static_cast<word_t>(types.size()));
        const auto frame_type = types[frame_type_id];

        pt_ref(r.dest) = r.stack.alloc_frame(frame_type)->base();
    }

    EXEC_DECL(mframe)
    {
        const auto target_module = at_val<vm_module_ref_t>(r.src);
        if (target_module == nullptr)
            throw vm_user_exception{ "Module not loaded" };

        const auto &types = target_module->type_section;

        // Get the function ref index for this module.
        const auto function_id = vt_ref<word_t>(r.mid);
        const auto &function_ref = target_module->get_function_ref(function_id);

        const auto frame_type_id = function_ref.frame_type;
        assert(0 <= frame_type_id && frame_type_id < static_cast<word_t>(types.size()));
        const auto frame_type = types[frame_type_id];

        pt_ref(r.dest) = r.stack.alloc_frame(frame_type)->base();
    }

    EXEC_DECL(ret)
    {
        const auto current_top_frame = r.stack.peek_frame();
        assert(current_top_frame != nullptr);
        r.next_pc = current_top_frame->prev_pc();

        if (current_top_frame->prev_module_ref() != nullptr)
        {
            dec_ref_count_and_free(r.module_ref);
            r.module_ref = current_top_frame->prev_module_ref();
            current_top_frame->prev_module_ref() = nullptr;

            dec_ref_count_and_free(r.mp_base);
            r.mp_base = r.module_ref->mp_base;
        }

        auto new_frame = r.stack.pop_frame();
        if (new_frame == nullptr)
            r.current_thread_state = vm_thread_state_t::empty_stack;

        if (debug::is_component_tracing_enabled<debug::component_trace_t::stack>())
            debug::log_msg(debug::component_trace_t::stack, debug::log_level_t::debug, "exit: function");
    }

    EXEC_DECL(call)
    {
        auto top_frame = r.stack.push_frame();
        top_frame->prev_pc() = r.next_pc;

#ifndef NDEBUG
        // Validate the stack state
        const auto frame_base_pointer = pt_ref(r.src);
        assert(frame_base_pointer != nullptr);
        assert(top_frame->base() == frame_base_pointer);
#endif

        r.next_pc = vt_ref<vm_pc_t>(r.dest);

        debug::log_msg(debug::component_trace_t::stack, debug::log_level_t::debug, "enter: function");
    }

    EXEC_DECL(mcall)
    {
        auto target_module = at_val<vm_module_ref_t>(r.dest);
        if (target_module == nullptr)
            throw vm_user_exception{ "Module not loaded" };

        // Get the pc for the function call into this module
        const auto export_id = vt_ref<word_t>(r.mid);
        const auto &function_ref = target_module->get_function_ref(export_id);
        const auto function_pc = function_ref.entry_pc;
        assert(static_cast<std::size_t>(function_pc) < target_module->code_section.size());

        // Push the next frame
        auto top_frame = r.stack.push_frame();

#ifndef NDEBUG
        // Validate the stack state
        const auto frame_base_pointer = pt_ref(r.src);
        assert(frame_base_pointer != nullptr);
        assert(top_frame->base() == frame_base_pointer);
#endif

        // Store current state
        top_frame->prev_pc() = r.next_pc;
        top_frame->prev_module_ref() = r.module_ref;

        if (debug::is_component_tracing_enabled<debug::component_trace_t::stack>())
        {
            debug::log_msg(
                debug::component_trace_t::stack,
                debug::log_level_t::debug,
                "enter: function: inter-module: %d >>%s<< %d %s",
                export_id,
                target_module->module->module_name->str(),
                function_pc,
                (target_module->is_builtin_module() ? "true" : "false"));
        }

        // Set registers for execution
        r.module_ref = target_module;
        r.module_ref->add_ref();
        r.mp_base = r.module_ref->mp_base;
        r.next_pc = function_pc;

        if (r.module_ref->is_builtin_module())
        {
            const auto &inst = r.module_ref->code_section[function_pc];
            assert(r.mp_base == nullptr && "Built-in modules shouldn't have module data (MP register)");

            r.current_thread_state = vm_thread_state_t::release;
            inst.native(r, vm);
            ret(r, vm);

            // Only reset the thread if it is in the state set prior to native call.
            if (r.current_thread_state == vm_thread_state_t::release)
                r.current_thread_state = vm_thread_state_t::running;
        }
        else
        {
            r.mp_base->add_ref();
        }
    }

    EXEC_DECL(jmp) { r.next_pc = vt_ref<vm_pc_t>(r.dest); }

    EXEC_DECL(goto_)
    {
        const auto pc_index = vt_ref<word_t>(r.src);

        assert(r.dest != nullptr);
        const auto pc_table = reinterpret_cast<vm_pc_t *>(r.dest);

        r.next_pc = pc_table[pc_index];
    }

    //
    // Thread operations
    //

    EXEC_DECL(spawn)
    {
        // Arguments to the spawned thread are passed on the current
        // thread's stack in a new frame.
        auto spawned_frame = r.stack.push_frame();

#ifndef NDEBUG
        // Validate the stack state
        const auto frame_base_pointer = pt_ref(r.src);
        assert(frame_base_pointer != nullptr);
        assert(spawned_frame->base() == frame_base_pointer);
#endif

        const auto starting_pc = vt_ref<vm_pc_t>(r.dest);

        assert(r.module_ref != nullptr);
        vm.fork(r.thread.get_thread_id(), *r.module_ref, *spawned_frame, starting_pc);

        // After the new thread is forked pop off the argument passing frame.
        auto new_frame = r.stack.pop_frame();
        if (new_frame == nullptr)
            r.current_thread_state = vm_thread_state_t::empty_stack;
    }

    EXEC_DECL(mspawn)
    {
        // Arguments to the spawned thread are passed on the current
        // thread's stack in a new frame.
        auto spawned_frame = r.stack.push_frame();

#ifndef NDEBUG
        // Validate the stack state
        const auto frame_base_pointer = pt_ref(r.src);
        assert(frame_base_pointer != nullptr);
        assert(spawned_frame->base() == frame_base_pointer);
#endif

        const auto import_function_id = vt_ref<word_t>(r.mid);
        auto target_module_ref = at_val<vm_module_ref_t>(r.dest);
        assert(target_module_ref != nullptr);

        const auto &function_ref = target_module_ref->get_function_ref(import_function_id);

        // Spawning a built-in module is not permitted
        if (target_module_ref->is_builtin_module())
            throw vm_user_exception{ "Spawning a built-in module is not permitted" };

        vm.fork(r.thread.get_thread_id(), *target_module_ref, *spawned_frame, function_ref.entry_pc);

        // After the new thread is forked pop off the argument passing frame.
        auto new_frame = r.stack.pop_frame();
        if (new_frame == nullptr)
            r.current_thread_state = vm_thread_state_t::empty_stack;
    }

    EXEC_DECL(exit)
    {
        r.current_thread_state = vm_thread_state_t::exiting;
    }

    //
    // Exception operations
    //

    EXEC_DECL(raise)
    {
        auto e = at_val<vm_alloc_t>(r.src);
        if (e == nullptr)
            throw dereference_nil{ "Raise exception" };

        const vm_string_t *exception_id;

        // Determine if a 'string' or 'exception' type was raised.
        if (e->alloc_type == vm_string_t::type_desc())
        {
            // String
            exception_id = static_cast<vm_string_t *>(e);

            if (debug::is_component_tracing_enabled<debug::component_trace_t::exception>())
                debug::log_msg(debug::component_trace_t::exception, debug::log_level_t::debug, "raise: string: >>%s<<", exception_id->str());
        }
        else
        {
            // [SPEC] The exception format is not formally specified.
            // The first pointer in the exception is a string type
            // that is used to uniquely identify the exception type.
            // The remainder of the type is user-defined.
            auto content = e->get_allocation();
            exception_id = at_val<vm_string_t>(content);
            assert(exception_id != nullptr);

            if (debug::is_component_tracing_enabled<debug::component_trace_t::exception>())
                debug::log_msg(debug::component_trace_t::exception, debug::log_level_t::debug, "raise: exception: >>%s<<", exception_id->str());
        }

        // Check if the tool dispatcher has been supplied
        auto tool_dispatch = r.tool_dispatch.load();
        if (tool_dispatch != nullptr)
            tool_dispatch->on_exception_raised(r, *exception_id, *e);

        const handler_t *handler;
        vm_frame_t *target_frame;
        vm_pc_t handler_pc;

        const auto faulting_pc = r.pc;

        assert(r.module_ref != nullptr);
        std::tie(handler, target_frame, handler_pc) = find_exception_handler(r.stack, *r.module_ref, faulting_pc, *exception_id);
        if (handler == nullptr)
        {
            if (tool_dispatch != nullptr)
                tool_dispatch->on_exception_unhandled(r, *exception_id, *e);

            auto faulting_module_name = r.module_ref->module->module_name->str();
            throw unhandled_user_exception{ exception_id->str(), faulting_module_name, faulting_pc };
        }

        assert(target_frame != nullptr && handler_pc != runtime_constants::invalid_program_counter);

        // Now that we know the exception has a handler, take a reference.
        e->add_ref();

        if (target_frame != r.stack.peek_frame())
        {
            while (target_frame != r.stack.pop_frame())
            {
                // Remove frames from the stack until the target frame is found.
            }
        }

        // Re-initialize the current frame
        if (handler->type_desc != nullptr)
        {
            destroy_memory(*handler->type_desc, target_frame->base());
            init_memory(*handler->type_desc, target_frame->base());
        }

        // Set the exception at the appropriate frame offset
        auto frame_pointer = reinterpret_cast<uint8_t *>(target_frame->base());
        auto exception_destination = reinterpret_cast<pointer_t>(frame_pointer + handler->exception_offset);

        // Destroy any data at this location.
        auto current_value = at_val<vm_alloc_t>(exception_destination);
        dec_ref_count_and_free(current_value);

        // Set the exception destination.
        pt_ref(exception_destination) = e->get_allocation();

        r.next_pc = handler_pc;
    }

    // [SPEC] Added to support debugger.
    // The actual opcode for execution can be retrieved from the tool dispatch interface.
    // The current register state is for the real opcode, the breakpoint opcode has no inputs/outputs.
    EXEC_DECL(brkpt)
    {
        auto tool_dispatch = r.tool_dispatch.load();
        if (tool_dispatch == nullptr)
        {
            assert(false && "Breakpoint set but no tool dispatcher found");
            throw vm_system_exception{ "Breakpoint set but no tool(s) loaded" };
        }

        const auto original_op = tool_dispatch->on_breakpoint(r);

        // Continue execution
        vm_exec_table[static_cast<std::size_t>(original_op)](r, vm);
    }

    //
    // Module operations
    //

    EXEC_DECL(load)
    {
        // Validate module loading module.
        auto importing_module = r.module_ref->module;
        if (!util::has_flag(importing_module->header.runtime_flag, runtime_flags_t::has_import))
            throw vm_user_exception{ "Invalid importing module" };

        auto str = at_val<vm_string_t>(r.src);
        assert(str != nullptr);

        dec_ref_count_and_free(at_val<vm_alloc_t>(r.dest));
        pt_ref(r.dest) = nullptr;

        auto imported_module = std::shared_ptr<vm_module_t>{};

        // Handle self loading module optimization.
        if (std::strcmp(str->str(), "$self") == 0)
        {
            // Remove 'const' from the module type in the shared_ptr<>
            imported_module = std::const_pointer_cast<vm_module_t>(r.module_ref->module);
        }
        else
        {
            try
            {
                imported_module = vm.load_module(str->str());
            }
            catch (const vm_module_exception &e)
            {
                debug::log_msg(debug::component_trace_t::module, debug::log_level_t::debug, "exception: load: >>%s<<", e.what());
                return;
            }
        }

        auto module_import_index = vt_ref<uint32_t>(r.mid);
        const auto &imports = importing_module->import_section;

        assert(module_import_index < imports.size());
        const auto &function_imports = imports[module_import_index];

        auto entry_module_ref = new vm_module_ref_t{ imported_module, function_imports };
        pt_ref(r.dest) = entry_module_ref->get_allocation();

        // Check if the tool dispatcher has been supplied
        auto tool_dispatch = r.tool_dispatch.load();
        if (tool_dispatch != nullptr)
            tool_dispatch->on_module_thread_load(r, imported_module);
    }
}

const vm_exec_t disvm::runtime::vm_exec_table[static_cast<std::size_t>(opcode_t::last_opcode) + 1] =
{
    invalid,
    alt,
    nbalt,
    goto_,
    call,
    frame,
    spawn,
    runt,
    load,
    mcall,
    mspawn,
    mframe,
    ret,
    jmp,
    casew,
    exit,
    new_,
    newa,
    newcb,
    newcw,
    newcf,
    newcp,
    newcm,
    newcmp,
    send,
    recv,
    consb,
    consw,
    consp,
    consf,
    notimpl, // consm,
    consmp,
    headb,
    headw,
    headp,
    headf,
    notimpl, // headm,
    headmp,
    tail,
    lea,
    indx,
    movp,
    movm,
    movmp,
    movb,
    movw,
    movf,
    cvtbw,
    cvtwb,
    cvtfw,
    cvtwf,
    cvtca,
    cvtac,
    cvtwc,
    cvtcw,
    cvtfc,
    cvtcf,
    addb,
    addw,
    addf,
    subb,
    subw,
    subf,
    mulb,
    mulw,
    mulf,
    divb,
    divw,
    divf,
    modw,
    modb,
    andb,
    andw,
    orb,
    orw,
    xorb,
    xorw,
    shlb,
    shlw,
    shrb,
    shrw,
    insc,
    indc,
    addc,
    lenc,
    lena,
    lenl,
    beqb,
    bneb,
    bltb,
    bleb,
    bgtb,
    bgeb,
    beqw,
    bnew,
    bltw,
    blew,
    bgtw,
    bgew,
    beqf,
    bnef,
    bltf,
    blef,
    bgtf,
    bgef,
    beqc,
    bnec,
    bltc,
    blec,
    bgtc,
    bgec,
    slicea,
    slicela,
    slicec,
    indw,
    indf,
    indb,
    negf,
    movl,
    addl,
    subl,
    divl,
    modl,
    mull,
    andl,
    orl,
    xorl,
    shll,
    shrl,
    bnel,
    bltl,
    blel,
    bgtl,
    bgel,
    beql,
    cvtlf,
    cvtfl,
    cvtlw,
    cvtwl,
    cvtlc,
    cvtcl,
    headl,
    consl,
    newcl,
    casec,
    indl,
    notimpl, // movpc,
    tcmp,
    mnewz,
    cvtrf,
    cvtfr,
    cvtws,
    cvtsw,
    lsrw,
    lsrl,
    eclr,
    newz,
    newaz,
    raise,
    casel,
    mulx,
    divx,
    cvtxx,
    mulx0,
    divx0,
    cvtxx0,
    notimpl, // mulx1,
    notimpl, // divx1,
    notimpl, // cvtxx1,
    cvtfx,
    cvtxf,
    expw,
    expl,
    expf,
    notimpl, // self,
    brkpt,
};
