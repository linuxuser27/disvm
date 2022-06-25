//
// Dis VM
// File: Mathmod.cpp
// Author: arr
//

#ifdef _MSC_VER
// Pragma for floating point support - https://msdn.microsoft.com/en-us/library/bfwa91s0.aspx
#pragma fenv_access (on)
#else
// Pragma for floating point support - http://en.cppreference.com/w/cpp/numeric/fenv
#pragma STDC FENV_ACCESS ON
#endif

#include <cfenv>
#include <algorithm>

#include <debug.hpp>
#include "Mathmod.hpp"

using disvm::vm_t;

using disvm::debug::component_trace_t;
using disvm::debug::log_level_t;

using disvm::runtime::dereference_nil;
using disvm::runtime::intrinsic_type_desc;
using disvm::runtime::out_of_range_memory;
using disvm::runtime::real_t;
using disvm::runtime::vm_alloc_t;
using disvm::runtime::vm_array_t;
using disvm::runtime::vm_module_t;
using disvm::runtime::vm_user_exception;
using disvm::runtime::vm_system_exception;
using disvm::runtime::word_t;

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

    // Convert from std floating point round to limbo
    word_t convert_to_limbo_fp_round(int r)
    {
        switch (r & FE_ROUND_MASK)
        {
        default:
            assert(false && "Invalid floating point rounding flag");
        case FE_TONEAREST:
            return Math_RND_NR;
        case FE_DOWNWARD:
            return Math_RND_NINF;
        case FE_UPWARD:
            return Math_RND_PINF;
        case FE_TOWARDZERO:
            return Math_RND_Z;
        }
    }

    // Convert from std floating point exception to limbo
    word_t convert_to_limbo_fp_exception(std::fexcept_t e)
    {
        word_t e_flags{};
        if (e & FE_INEXACT)   e_flags |= Math_INEX;
        if (e & FE_UNDERFLOW) e_flags |= Math_UNFL;
        if (e & FE_OVERFLOW)  e_flags |= Math_OVFL;
        if (e & FE_DIVBYZERO) e_flags |= Math_ZDIV;
        if (e & FE_INVALID)   e_flags |= Math_INVAL;

        return e_flags;
    }

    // Convert from limbo floating point round to std
    int convert_to_std_fp_round(word_t r)
    {
        switch (r & Math_RND_MASK)
        {
        default:
            assert(false && "Invalid floating point rounding flag");
        case Math_RND_NR:
            return FE_TONEAREST;
        case Math_RND_NINF:
            return FE_DOWNWARD;
        case Math_RND_PINF:
            return FE_UPWARD;
        case Math_RND_Z:
            return FE_TOWARDZERO;
        }
    }

    // Convert from limbo floating point exception to std
    std::fexcept_t convert_to_std_fp_exception(word_t e)
    {
        std::fexcept_t e_flags{};
        if (e & Math_INEX)  e_flags |= FE_INEXACT;
        if (e & Math_UNFL)  e_flags |= FE_UNDERFLOW;
        if (e & Math_OVFL)  e_flags |= FE_OVERFLOW;
        if (e & Math_ZDIV)  e_flags |= FE_DIVBYZERO;
        if (e & Math_INVAL) e_flags |= FE_INVALID;

        return e_flags;
    }

    // Get floating point exception flags
    word_t get_fp_exception_flags()
    {
        std::fexcept_t e_flags;
        int ec = std::fegetexceptflag(&e_flags, FE_ALL_EXCEPT);
        if (ec != 0)
        {
            assert(false && "Failed to get except flag");
            return 0;
        }

        return convert_to_limbo_fp_exception(e_flags);
    }

    // Get all floating point flags
    word_t get_all_fp_flags()
    {
        const auto rnd_flag = std::fegetround();
        const auto e_flags = get_fp_exception_flags();
        return (convert_to_limbo_fp_round(rnd_flag) | e_flags);
    }

    // Set floating point control flags
    void set_fp_control_flags(std::fexcept_t e_flags, std::fexcept_t e_mask, int rnd_flag, int rnd_mask)
    {
        int ec;

        // [SPEC] No current mechanism to set floating point exception control flags

        if (rnd_mask != 0)
        {
            ec = std::fesetround(rnd_flag & rnd_mask);
            assert(ec == 0);
            (void)ec;
        }
    }
}

std::unique_ptr<vm_module_t> Mathmodinit()
{
    auto mod = disvm::runtime::builtin::create_builtin_module(Math_PATH, Mathmodlen, Mathmodtab);

    // Inferno floating point flags (http://www.vitanuova.com/inferno/man/2/math-fp.html)
    set_fp_control_flags(
        FE_INEXACT,  // Inexact is fatal
        FE_ALL_EXCEPT,
        FE_TONEAREST,  // Round to nearest even
        FE_ROUND_MASK);

    return mod;
}

void
Math_FPcontrol(vm_registers_t &r, vm_t &)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_FPcontrol>();

    const auto prev_fp_flags = get_all_fp_flags();

    const auto e_flags = convert_to_std_fp_exception(fp.r & fp.mask);
    const auto rnd_flag = convert_to_std_fp_round(fp.r & fp.mask);

    set_fp_control_flags(
        e_flags,
        fp.mask & FE_ALL_EXCEPT,
        rnd_flag,
        fp.mask & FE_ROUND_MASK);

    *fp.ret = (prev_fp_flags & fp.mask);
}

void
Math_FPstatus(vm_registers_t &r, vm_t &)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_FPstatus>();

    const auto prev_e_flags = get_fp_exception_flags();

    // Clear all exceptions
    int ec = std::feclearexcept(FE_ALL_EXCEPT);
    assert(ec == 0);
    (void)ec;

    const auto e_flags = convert_to_std_fp_exception(fp.r);
    const auto e_mask = convert_to_std_fp_exception(fp.mask);

    // Set exception flags
    ec = std::fesetexceptflag(&e_flags, static_cast<const int>(e_mask));
    assert(ec == 0);
    (void)ec;

    *fp.ret = (prev_e_flags & e_mask);
}

void
Math_acos(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_acos>();
    *fp.ret = std::acos(fp.x);
}

void
Math_acosh(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_acosh>();
    *fp.ret = std::acosh(fp.x);
}

void
Math_asin(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_asin>();
    *fp.ret = std::asin(fp.x);
}

void
Math_asinh(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_asinh>();
    *fp.ret = std::asinh(fp.x);
}

void
Math_atan(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_atan>();
    *fp.ret = std::atan(fp.x);
}

void
Math_atan2(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_atan2>();
    *fp.ret = std::atan2(fp.x, fp.y);
}

void
Math_atanh(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_atanh>();
    *fp.ret = std::atanh(fp.x);
}

void
Math_bits32real(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_bits32real>();

    auto x = fp.b;
    float t;
    std::memcpy(&t, &x, sizeof(x));

    *fp.ret = real_t{ t };
}

void
Math_bits64real(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_bits64real>();

    auto x = fp.b;
    real_t t;
    std::memcpy(&t, &x, sizeof(x));
    static_assert(sizeof(t) == sizeof(x), "Real should be 64-bits");

    *fp.ret = t;
}

void
Math_cbrt(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_cbrt>();
    *fp.ret = std::cbrt(fp.x);
}

void
Math_ceil(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_ceil>();
    *fp.ret = std::ceil(fp.x);
}

void
Math_copysign(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_copysign>();
    *fp.ret = std::copysign(fp.x, fp.s);
}

void
Math_cos(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_cos>();
    *fp.ret = std::cos(fp.x);
}

void
Math_cosh(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_cosh>();
    *fp.ret = std::cosh(fp.x);
}

void
Math_dot(vm_registers_t &r, vm_t &vm)
{
    // Defined as: dot(x, y) = sum(x[i] * y[i])
    auto &fp = r.stack.peek_frame()->base<F_Math_dot>();

    auto xs = vm_alloc_t::from_allocation<vm_array_t>(fp.x);
    auto ys = vm_alloc_t::from_allocation<vm_array_t>(fp.y);
    if (xs == nullptr || ys == nullptr)
        throw dereference_nil{ "dot product" };

    assert(xs->get_element_type()->is_equal(intrinsic_type_desc::type<real_t>()));
    assert(ys->get_element_type()->is_equal(intrinsic_type_desc::type<real_t>()));
    const auto x_len = xs->get_length();
    if (x_len != ys->get_length())
        throw vm_user_exception{ "Array lengths must be equal" };

    auto x_raw = reinterpret_cast<real_t *>(xs->at(0));
    auto y_raw = reinterpret_cast<real_t *>(ys->at(0));

    auto result = real_t{ 0 };
    for (auto i = 0; i < x_len; ++i)
        result += (x_raw[i] * y_raw[i]);

    *fp.ret = result;
}

void
Math_erf(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_erf>();
    *fp.ret = std::erf(fp.x);
}

void
Math_erfc(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_erfc>();
    *fp.ret = std::erfc(fp.x);
}

void
Math_exp(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_exp>();
    *fp.ret = std::exp(fp.x);
}

void
Math_expm1(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_expm1>();
    *fp.ret = std::expm1(fp.x);
}

void
Math_export_int(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_export_int>();

    auto buffer = vm_alloc_t::from_allocation<vm_array_t>(fp.b);
    assert(buffer->get_element_type()->is_equal(intrinsic_type_desc::type<byte_t>()));

    auto values = vm_alloc_t::from_allocation<vm_array_t>(fp.x);
    assert(values->get_element_type()->is_equal(intrinsic_type_desc::type<word_t>()));
    const auto values_len = values->get_length();

    if (static_cast<std::size_t>(buffer->get_length()) != (sizeof(word_t) * values_len))
        throw vm_user_exception{ "Invalid buffer size based on conversion" };

    // Insert as big endian
    for (int i = 0; i < values_len; ++i)
    {
        auto x = values->at<word_t>(i);

        auto base = reinterpret_cast<byte_t *>(buffer->at(i * sizeof(x)));
        base[0] = static_cast<byte_t>(x >> 24);
        base[1] = static_cast<byte_t>(x >> 16);
        base[2] = static_cast<byte_t>(x >> 8);
        base[3] = static_cast<byte_t>(x);
    }
}

void
Math_export_real(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_export_real>();

    auto buffer = vm_alloc_t::from_allocation<vm_array_t>(fp.b);
    assert(buffer->get_element_type()->is_equal(intrinsic_type_desc::type<byte_t>()));

    auto values = vm_alloc_t::from_allocation<vm_array_t>(fp.x);
    assert(values->get_element_type()->is_equal(intrinsic_type_desc::type<real_t>()));
    const auto values_len = values->get_length();

    if (static_cast<std::size_t>(buffer->get_length()) != (sizeof(real_t) * values_len))
        throw vm_user_exception{ "Invalid buffer size based on conversion" };

    // Insert as big endian
    for (int i = 0; i < values_len; ++i)
    {
        auto x = values->at<real_t>(i);
        uint64_t t;
        std::memcpy(&t, &x, sizeof(x));
        static_assert(sizeof(t) == sizeof(x), "Real should be 64-bits");

        auto base = reinterpret_cast<byte_t *>(buffer->at(i * sizeof(x)));
        base[0] = static_cast<byte_t>(t >> 56);
        base[1] = static_cast<byte_t>(t >> 48);
        base[2] = static_cast<byte_t>(t >> 40);
        base[3] = static_cast<byte_t>(t >> 32);
        base[4] = static_cast<byte_t>(t >> 24);
        base[5] = static_cast<byte_t>(t >> 16);
        base[6] = static_cast<byte_t>(t >> 8);
        base[7] = static_cast<byte_t>(t);
    }
}

void
Math_export_real32(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_export_real32>();

    auto buffer = vm_alloc_t::from_allocation<vm_array_t>(fp.b);
    assert(buffer->get_element_type()->is_equal(intrinsic_type_desc::type<byte_t>()));

    auto values = vm_alloc_t::from_allocation<vm_array_t>(fp.x);
    assert(values->get_element_type()->is_equal(intrinsic_type_desc::type<real_t>()));
    const auto values_len = values->get_length();

    if (static_cast<std::size_t>(buffer->get_length()) != (sizeof(float) * values_len))
        throw vm_user_exception{ "Invalid buffer size based on conversion" };

    // Insert as big endian
    for (int i = 0; i < values_len; ++i)
    {
        auto x_r = values->at<real_t>(i);
        auto x = static_cast<float>(x_r);
        uint32_t t;
        std::memcpy(&t, &x, sizeof(x));
        static_assert(sizeof(t) == sizeof(x), "Short-real should be 32-bits");

        auto base = reinterpret_cast<byte_t *>(buffer->at(i * sizeof(x)));
        base[0] = static_cast<byte_t>(t >> 24);
        base[1] = static_cast<byte_t>(t >> 16);
        base[2] = static_cast<byte_t>(t >> 8);
        base[3] = static_cast<byte_t>(t);
    }
}

void
Math_fabs(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_fabs>();
    *fp.ret = std::fabs(fp.x);
}

void
Math_fdim(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_fdim>();
    *fp.ret = std::fdim(fp.x, fp.y);
}

void
Math_finite(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_finite>();
    *fp.ret = std::isfinite(fp.x) ? 1 : 0;
}

void
Math_floor(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_floor>();
    *fp.ret = std::floor(fp.x);
}

void
Math_fmax(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_fmax>();
    *fp.ret = std::fmax(fp.x, fp.y);
}

void
Math_fmin(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_fmin>();
    *fp.ret = std::fmin(fp.x, fp.y);
}

void
Math_fmod(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_fmod>();
    *fp.ret = std::fmod(fp.x, fp.y);
}

// Forward declaration
extern "C" int dgemm_(
    char *transa, char *transb,
    int *m, int *n, int *k,
    double *alpha,
    double *a, int *lda,
    double *b, int *ldb,
    double *beta,
    double *c, int *ldc,
    int a_len, int b_len, int c_len);

void
Math_gemm(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_gemm>();

    char t_a[2] = {};
    t_a[0] = static_cast<char>(fp.transa);

    char t_b[2] = {};
    t_b[0] = static_cast<char>(fp.transb);

    auto m = fp.m;
    auto n = fp.n;
    auto k = fp.k;

    auto alpha = fp.alpha;
    auto beta = fp.beta;

    auto lda = fp.lda;
    auto a_v = vm_alloc_t::from_allocation<vm_array_t>(fp.a);
    if (a_v == nullptr)
        throw dereference_nil{ "gemm - a" };

    auto ldb = fp.ldb;
    auto b_v = vm_alloc_t::from_allocation<vm_array_t>(fp.b);
    if (b_v == nullptr)
        throw dereference_nil{ "gemm - b" };

    auto ldc = fp.ldc;
    auto c_v = vm_alloc_t::from_allocation<vm_array_t>(fp.c);
    if (c_v == nullptr)
        throw dereference_nil{ "gemm - c" };

    assert(a_v->get_element_type()->is_equal(intrinsic_type_desc::type<real_t>()));
    assert(b_v->get_element_type()->is_equal(intrinsic_type_desc::type<real_t>()));
    assert(c_v->get_element_type()->is_equal(intrinsic_type_desc::type<real_t>()));

    const auto a_len = a_v->get_length();
    const auto b_len = b_v->get_length();
    const auto c_len = c_v->get_length();

    // Call BLAS version of function
    const auto result = dgemm_(
        t_a, t_b,
        &m, &n, &k,
        &alpha,
        reinterpret_cast<double *>(a_v->at(0)), &lda,
        reinterpret_cast<double *>(b_v->at(0)), &ldb,
        &beta,
        reinterpret_cast<double *>(c_v->at(0)), &ldc,
        a_len, b_len, c_len);

    if (result != 0)
        throw vm_user_exception{ "gemm invalid argument" };
}

void
Math_getFPcontrol(vm_registers_t &r, vm_t &)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_getFPcontrol>();
    *fp.ret = get_all_fp_flags();
}

void
Math_getFPstatus(vm_registers_t &r, vm_t &)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_getFPstatus>();
    *fp.ret = get_fp_exception_flags();
}

void
Math_hypot(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_hypot>();
    *fp.ret = std::hypot(fp.x, fp.y);
}

void
Math_iamax(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_iamax>();

    // [SPEC] According to the Inferno implementation (libmath/blas.c), 0 is returned
    // if the array is empty. This means there is no indication the supplied array was
    // empty or the first index was max. The spec does not describe this condition,
    // therefore this implementation will indicate with -1.
    *fp.ret = -1;

    auto result = vm_alloc_t::from_allocation<vm_array_t>(fp.x);
    assert(result->get_element_type()->is_equal(intrinsic_type_desc::type<real_t>()));
    if (result->get_length() <= 0)
        return;

    auto r_raw = reinterpret_cast<real_t *>(result->at(0));

    auto max_idx = 0;
    auto r_len = result->get_length();
    auto curr_max = std::fabs(r_raw[max_idx]);
    for (auto i = word_t{ 1 }; i < r_len; ++i)
    {
        auto t = std::fabs(r_raw[i]);
        if (curr_max < t)
        {
            max_idx = i;
            curr_max = t;
        }
    }

    *fp.ret = max_idx;
}

void
Math_ilogb(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_ilogb>();
    *fp.ret = std::ilogb(fp.x);
}

void
Math_import_int(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_import_int>();

    auto buffer = vm_alloc_t::from_allocation<vm_array_t>(fp.b);
    assert(buffer->get_element_type()->is_equal(intrinsic_type_desc::type<byte_t>()));

    auto result = vm_alloc_t::from_allocation<vm_array_t>(fp.x);
    assert(result->get_element_type()->is_equal(intrinsic_type_desc::type<word_t>()));
    const auto result_len = result->get_length();

    if (static_cast<std::size_t>(buffer->get_length()) != (sizeof(word_t) * result_len))
        throw vm_user_exception{ "Invalid buffer size based on conversion" };

    // Process as big endian
    for (int i = 0; i < result_len; ++i)
    {
        uint32_t t = buffer->at<byte_t>(0);
        for (int j = 1; j < sizeof(word_t); ++j)
            t = (t << 8) | buffer->at<byte_t>(j);

        auto x = static_cast<word_t>(t);
        result->at<word_t>(i) = x;
    }
}

void
Math_import_real(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_import_real>();

    auto buffer = vm_alloc_t::from_allocation<vm_array_t>(fp.b);
    assert(buffer->get_element_type()->is_equal(intrinsic_type_desc::type<byte_t>()));

    auto result = vm_alloc_t::from_allocation<vm_array_t>(fp.x);
    assert(result->get_element_type()->is_equal(intrinsic_type_desc::type<real_t>()));
    const auto result_len = result->get_length();

    if (static_cast<std::size_t>(buffer->get_length()) != (sizeof(real_t) * result_len))
        throw vm_user_exception{ "Invalid buffer size based on conversion" };

    // Process as big endian
    for (int i = 0; i < result_len; ++i)
    {
        uint64_t t = buffer->at<byte_t>(0);
        for (int j = 1; j < sizeof(real_t); ++j)
            t = (t << 8) | buffer->at<byte_t>(j);

        real_t x;
        std::memcpy(&x, &t, sizeof(t));
        static_assert(sizeof(t) == sizeof(x), "Real should be 64-bits");

        result->at<real_t>(i) = x;
    }
}

void
Math_import_real32(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_import_real32>();

    auto buffer = vm_alloc_t::from_allocation<vm_array_t>(fp.b);
    assert(buffer->get_element_type()->is_equal(intrinsic_type_desc::type<byte_t>()));

    auto result = vm_alloc_t::from_allocation<vm_array_t>(fp.x);
    assert(result->get_element_type()->is_equal(intrinsic_type_desc::type<real_t>()));
    const auto result_len = result->get_length();

    if (static_cast<std::size_t>(buffer->get_length()) != (sizeof(float) * result_len))
        throw vm_user_exception{ "Invalid buffer size based on conversion" };

    // Process as big endian
    for (int i = 0; i < result_len; ++i)
    {
        uint32_t t = buffer->at<byte_t>(0);
        for (int j = 1; j < sizeof(float); ++j)
            t = (t << 8) | buffer->at<byte_t>(j);

        float x_f;
        std::memcpy(&x_f, &t, sizeof(t));
        static_assert(sizeof(t) == sizeof(x_f), "Short-real should be 32-bits");

        auto x = real_t{ x_f };
        result->at<real_t>(i) = x;
    }
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

    // [PAL] Consume platform API
    throw vm_system_exception{ "Function not implemented" };
}

void
Math_j1(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_j1>();

    // [PAL] Consume platform API
    throw vm_system_exception{ "Function not implemented" };
}

void
Math_jn(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_jn>();

    // [PAL] Consume platform API
    throw vm_system_exception{ "Function not implemented" };
}

void
Math_lgamma(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_lgamma>();
    throw vm_system_exception{ "Function not implemented" };
}

void
Math_log(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_log>();
    *fp.ret = std::log(fp.x);
}

void
Math_log10(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_log10>();
    *fp.ret = std::log10(fp.x);
}

void
Math_log1p(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_log1p>();
    *fp.ret = std::log1p(fp.x);
}

void
Math_modf(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_modf>();

    real_t integral_out;
    const auto fractional_part = std::modf(fp.x, &integral_out);
    const auto integral_part = static_cast<word_t>(integral_out);

    fp.ret->t0 = integral_part;
    fp.ret->t1 = fractional_part;
}

void
Math_nextafter(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_nextafter>();
    *fp.ret = std::nextafter(fp.x, fp.y);
}

void
Math_norm1(vm_registers_t &r, vm_t &vm)
{
    // Defined as: norm1(x) = sum(fabs(x[i]))
    auto &fp = r.stack.peek_frame()->base<F_Math_norm1>();

    auto xs = vm_alloc_t::from_allocation<vm_array_t>(fp.x);
    if (xs == nullptr)
        throw dereference_nil{ "norm1" };

    assert(xs->get_element_type()->is_equal(intrinsic_type_desc::type<real_t>()));

    const auto x_len = xs->get_length();
    auto x_raw = reinterpret_cast<real_t *>(xs->at(0));

    auto result = real_t{ 0 };
    for (auto i = 0; i < x_len; ++i)
        result += std::fabs(x_raw[i]);

    *fp.ret = result;
}

void
Math_norm2(vm_registers_t &r, vm_t &vm)
{
    // Defined as: norm2(x) = sqrt(dot(x, x))
    auto &fp = r.stack.peek_frame()->base<F_Math_norm2>();

    auto xs = vm_alloc_t::from_allocation<vm_array_t>(fp.x);
    if (xs == nullptr)
        throw dereference_nil{ "norm2" };

    assert(xs->get_element_type()->is_equal(intrinsic_type_desc::type<real_t>()));

    const auto x_len = xs->get_length();
    auto x_raw = reinterpret_cast<real_t *>(xs->at(0));

    auto result = real_t{ 0 };
    for (auto i = 0; i < x_len; ++i)
    {
        const auto x_l = x_raw[i];
        result += (x_l * x_l);
    }

    *fp.ret = std::sqrt(result);
}

void
Math_pow(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_pow>();
    *fp.ret = std::pow(fp.x, fp.y);
}

void
Math_pow10(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_pow10>();
    *fp.ret = std::pow(10., static_cast<real_t>(fp.p));
}

void
Math_realbits32(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_realbits32>();

    auto x = static_cast<float>(fp.x);
    word_t t;
    std::memcpy(&t, &x, sizeof(x));

    *fp.ret = t;
}

void
Math_realbits64(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_realbits64>();

    auto x = fp.x;
    big_t t;
    std::memcpy(&t, &x, sizeof(x));
    static_assert(sizeof(t) == sizeof(x), "Real should be 64-bits");

    *fp.ret = t;
}

void
Math_remainder(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_remainder>();
    *fp.ret = std::remainder(fp.x, fp.p);
}

void
Math_rint(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_rint>();
    *fp.ret = std::rint(fp.x);
}

void
Math_scalbn(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_scalbn>();
    *fp.ret = std::scalbln(fp.x, fp.n);
}

void
Math_sin(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_sin>();
    *fp.ret = std::sin(fp.x);
}

void
Math_sinh(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_sinh>();
    *fp.ret = std::sinh(fp.x);
}

void
Math_sort(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_sort>();

    auto reals = vm_alloc_t::from_allocation<vm_array_t>(fp.x);
    auto words = vm_alloc_t::from_allocation<vm_array_t>(fp.pi);
    if (reals == nullptr || words == nullptr)
        throw dereference_nil{ "Sort reals" };

    assert(reals->get_element_type()->is_equal(intrinsic_type_desc::type<real_t>()));
    assert(words->get_element_type()->is_equal(intrinsic_type_desc::type<word_t>()));

    const auto words_len = words->get_length();
    if (reals->get_length() != words_len)
        throw vm_user_exception{ "Array lengths must be equal" };

    // Handle trivial sort cases
    if (words_len <= 1)
        return;

    auto begin_words = reinterpret_cast<word_t *>(words->at(0));
    auto end_words = begin_words + words_len;

    // Validate array of indices
    for (auto iter = begin_words; iter != end_words; ++iter)
    {
        if (*iter < 0 || words_len <= *iter)
            throw vm_user_exception{ "Invalid sort index values" };
    }

    const auto reals_raw = reinterpret_cast<real_t *>(reals->at(0));
    std::sort(begin_words, end_words, [reals_raw](word_t l, word_t r) { return reals_raw[l] < reals_raw[r]; });
}

void
Math_sqrt(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_sqrt>();
    *fp.ret = std::sqrt(fp.x);
}

void
Math_tan(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_tan>();
    *fp.ret = std::tan(fp.x);
}

void
Math_tanh(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_tanh>();
    *fp.ret = std::tanh(fp.x);
}

void
Math_y0(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_y0>();

    // [PAL] Consume platform API
    throw vm_system_exception{ "Function not implemented" };
}

void
Math_y1(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_y1>();

    // [PAL] Consume platform API
    throw vm_system_exception{ "Function not implemented" };
}

void
Math_yn(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Math_yn>();

    // [PAL] Consume platform API
    throw vm_system_exception{ "Function not implemented" };
}
