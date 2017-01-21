//
// Dis VM
// File: Mathmod.cpp
// Author: arr
//

#include "Mathmod.h"

using namespace disvm;
using namespace disvm::runtime;

namespace
{
    disvm::runtime::builtin::vm_runtab_t Mathmodtab[] = {
     "FPcontrol",0x6584767b,Math_FPcontrol,40,0,{0},
     "FPstatus",0x6584767b,Math_FPstatus,40,0,{0},
     "acos",0xa1d1fe48,Math_acos,40,0,{0},
     "acosh",0xa1d1fe48,Math_acosh,40,0,{0},
     "asin",0xa1d1fe48,Math_asin,40,0,{0},
     "asinh",0xa1d1fe48,Math_asinh,40,0,{0},
     "atan",0xa1d1fe48,Math_atan,40,0,{0},
     "atan2",0xf293f6cf,Math_atan2,48,0,{0},
     "atanh",0xa1d1fe48,Math_atanh,40,0,{0},
     "bits32real",0x40a58596,Math_bits32real,40,0,{0},
     "bits64real",0x5463c72c,Math_bits64real,40,0,{0},
     "cbrt",0xa1d1fe48,Math_cbrt,40,0,{0},
     "ceil",0xa1d1fe48,Math_ceil,40,0,{0},
     "copysign",0xf293f6cf,Math_copysign,48,0,{0},
     "cos",0xa1d1fe48,Math_cos,40,0,{0},
     "cosh",0xa1d1fe48,Math_cosh,40,0,{0},
     "dot",0xfeca4db6,Math_dot,40,2,{0x0,0xc0,},
     "erf",0xa1d1fe48,Math_erf,40,0,{0},
     "erfc",0xa1d1fe48,Math_erfc,40,0,{0},
     "exp",0xa1d1fe48,Math_exp,40,0,{0},
     "expm1",0xa1d1fe48,Math_expm1,40,0,{0},
     "export_int",0x3f1d94ee,Math_export_int,40,2,{0x0,0xc0,},
     "export_real",0xd1fb0c8c,Math_export_real,40,2,{0x0,0xc0,},
     "export_real32",0xd1fb0c8c,Math_export_real32,40,2,{0x0,0xc0,},
     "fabs",0xa1d1fe48,Math_fabs,40,0,{0},
     "fdim",0xf293f6cf,Math_fdim,48,0,{0},
     "finite",0x7b2e5f52,Math_finite,40,0,{0},
     "floor",0xa1d1fe48,Math_floor,40,0,{0},
     "fmax",0xf293f6cf,Math_fmax,48,0,{0},
     "fmin",0xf293f6cf,Math_fmin,48,0,{0},
     "fmod",0xf293f6cf,Math_fmod,48,0,{0},
     "gemm",0x53b8cd34,Math_gemm,96,3,{0x0,0x0,0xa2,},
     "getFPcontrol",0x616977e8,Math_getFPcontrol,32,0,{0},
     "getFPstatus",0x616977e8,Math_getFPstatus,32,0,{0},
     "hypot",0xf293f6cf,Math_hypot,48,0,{0},
     "iamax",0xa5fc2e4d,Math_iamax,40,2,{0x0,0x80,},
     "ilogb",0x7b2e5f52,Math_ilogb,40,0,{0},
     "import_int",0x3f1d94ee,Math_import_int,40,2,{0x0,0xc0,},
     "import_real",0xd1fb0c8c,Math_import_real,40,2,{0x0,0xc0,},
     "import_real32",0xd1fb0c8c,Math_import_real32,40,2,{0x0,0xc0,},
     "isnan",0x7b2e5f52,Math_isnan,40,0,{0},
     "j0",0xa1d1fe48,Math_j0,40,0,{0},
     "j1",0xa1d1fe48,Math_j1,40,0,{0},
     "jn",0xb61ffc5b,Math_jn,48,0,{0},
     "lgamma",0x6e97c646,Math_lgamma,40,0,{0},
     "log",0xa1d1fe48,Math_log,40,0,{0},
     "log10",0xa1d1fe48,Math_log10,40,0,{0},
     "log1p",0xa1d1fe48,Math_log1p,40,0,{0},
     "modf",0x6e97c646,Math_modf,40,0,{0},
     "nextafter",0xf293f6cf,Math_nextafter,48,0,{0},
     "norm1",0x1629207e,Math_norm1,40,2,{0x0,0x80,},
     "norm2",0x1629207e,Math_norm2,40,2,{0x0,0x80,},
     "pow",0xf293f6cf,Math_pow,48,0,{0},
     "pow10",0x40a58596,Math_pow10,40,0,{0},
     "realbits32",0x7b2e5f52,Math_realbits32,40,0,{0},
     "realbits64",0x9589d476,Math_realbits64,40,0,{0},
     "remainder",0xf293f6cf,Math_remainder,48,0,{0},
     "rint",0xa1d1fe48,Math_rint,40,0,{0},
     "scalbn",0x64fa84ba,Math_scalbn,48,0,{0},
     "sin",0xa1d1fe48,Math_sin,40,0,{0},
     "sinh",0xa1d1fe48,Math_sinh,40,0,{0},
     "sort",0xfc7c7b8,Math_sort,40,2,{0x0,0xc0,},
     "sqrt",0xa1d1fe48,Math_sqrt,40,0,{0},
     "tan",0xa1d1fe48,Math_tan,40,0,{0},
     "tanh",0xa1d1fe48,Math_tanh,40,0,{0},
     "y0",0xa1d1fe48,Math_y0,40,0,{0},
     "y1",0xa1d1fe48,Math_y1,40,0,{0},
     "yn",0xb61ffc5b,Math_yn,48,0,{0},
        0
    };

    const word_t Mathmodlen = 68;
}

void
Mathmodinit(void)
{
    builtin::register_module_exports(Math_PATH, Mathmodlen, Mathmodtab);
}

void
Math_FPcontrol(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_FPcontrol>();

    // [TODO] Properly implment floating point control
    *fp.ret = fp.mask;
}

void
Math_FPstatus(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_FPstatus>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_acos(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_acos>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_acosh(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_acosh>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_asin(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_asin>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_asinh(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_asinh>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_atan(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_atan>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_atan2(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_atan2>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_atanh(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_atanh>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_bits32real(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_bits32real>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_bits64real(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_bits64real>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_cbrt(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_cbrt>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_ceil(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_ceil>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_copysign(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_copysign>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_cos(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_cos>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_cosh(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_cosh>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_dot(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_dot>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_erf(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_erf>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_erfc(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_erfc>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_exp(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_exp>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_expm1(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_expm1>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_export_int(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_export_int>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_export_real(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_export_real>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_export_real32(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_export_real32>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_fabs(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_fabs>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_fdim(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_fdim>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_finite(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_finite>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_floor(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_floor>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_fmax(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_fmax>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_fmin(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_fmin>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_fmod(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_fmod>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_gemm(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_gemm>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_getFPcontrol(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_getFPcontrol>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_getFPstatus(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_getFPstatus>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_hypot(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_hypot>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_iamax(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_iamax>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_ilogb(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_ilogb>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_import_int(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_import_int>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_import_real(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_import_real>();

    auto buffer = vm_alloc_t::from_allocation<vm_array_t>(fp.b);
    assert(buffer->get_element_type()->size_in_bytes == intrinsic_type_desc::type<byte_t>()->size_in_bytes);

    auto result = vm_alloc_t::from_allocation<vm_array_t>(fp.x);
    assert(result->get_element_type()->size_in_bytes == intrinsic_type_desc::type<real_t>()->size_in_bytes);
    const auto result_len = result->get_length();

    if (buffer->get_length() != (8 * result_len))
        throw vm_user_exception{ "Invalid buffer size based on conversion" };

    // Process as big endian
    for (int i = 0; i < result_len; ++i)
    {
        uint64_t t = buffer->at<byte_t>(0);
        for (int j = 1; j < 8; ++j)
            t = (t << 8) | buffer->at<byte_t>(j);

        auto r = real_t{};
        std::memcpy(&r, &t, sizeof(t));
        static_assert(sizeof(t) == sizeof(r), "Real should be 64-bits");

        result->at<real_t>(i) = r;
    }
}

void
Math_import_real32(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_import_real32>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_isnan(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_isnan>();
    *fp.ret = std::isnan(fp.x) ? 1 : 0;
}

void
Math_j0(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_j0>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_j1(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_j1>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_jn(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_jn>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_lgamma(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_lgamma>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_log(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_log>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_log10(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_log10>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_log1p(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_log1p>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_modf(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_modf>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_nextafter(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_nextafter>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_norm1(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_norm1>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_norm2(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_norm2>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_pow(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_pow>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_pow10(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_pow10>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_realbits32(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_realbits32>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_realbits64(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_realbits64>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_remainder(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_remainder>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_rint(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_rint>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_scalbn(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_scalbn>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_sin(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_sin>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_sinh(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_sinh>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_sort(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_sort>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_sqrt(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_sqrt>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_tan(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_tan>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_tanh(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_tanh>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_y0(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_y0>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_y1(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_y1>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Math_yn(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_yn>();
    throw vm_system_exception{ "Instruction not implemented" };
}
