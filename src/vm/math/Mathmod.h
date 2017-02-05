//
// Dis VM
// File: Mathmod.h
// Author: arr
//

#ifndef _DISVM_SRC_VM_MATH_MATHMOD_H_
#define _DISVM_SRC_VM_MATH_MATHMOD_H_

#include <cmath>
#include <disvm.h>
#include <builtin_module.h>

using disvm::vm_t;

using disvm::runtime::byte_t;
using disvm::runtime::word_t;
using disvm::runtime::big_t;
using disvm::runtime::real_t;
using disvm::runtime::pointer_t;
using disvm::runtime::vm_registers_t;
using disvm::runtime::vm_frame_base_alloc_t;

void Math_FPcontrol(vm_registers_t &r, vm_t &vm);
struct F_Math_FPcontrol : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
    word_t r;
    word_t mask;
};
void Math_FPstatus(vm_registers_t &r, vm_t &vm);
struct F_Math_FPstatus : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
    word_t r;
    word_t mask;
};
void Math_acos(vm_registers_t &r, vm_t &vm);
struct F_Math_acos : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_acosh(vm_registers_t &r, vm_t &vm);
struct F_Math_acosh : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_asin(vm_registers_t &r, vm_t &vm);
struct F_Math_asin : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_asinh(vm_registers_t &r, vm_t &vm);
struct F_Math_asinh : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_atan(vm_registers_t &r, vm_t &vm);
struct F_Math_atan : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_atan2(vm_registers_t &r, vm_t &vm);
struct F_Math_atan2 : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t y;
    real_t x;
};
void Math_atanh(vm_registers_t &r, vm_t &vm);
struct F_Math_atanh : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_bits32real(vm_registers_t &r, vm_t &vm);
struct F_Math_bits32real : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    word_t b;
};
void Math_bits64real(vm_registers_t &r, vm_t &vm);
struct F_Math_bits64real : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    big_t b;
};
void Math_cbrt(vm_registers_t &r, vm_t &vm);
struct F_Math_cbrt : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_ceil(vm_registers_t &r, vm_t &vm);
struct F_Math_ceil : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_copysign(vm_registers_t &r, vm_t &vm);
struct F_Math_copysign : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
    real_t s;
};
void Math_cos(vm_registers_t &r, vm_t &vm);
struct F_Math_cos : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_cosh(vm_registers_t &r, vm_t &vm);
struct F_Math_cosh : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_dot(vm_registers_t &r, vm_t &vm);
struct F_Math_dot : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    /* vm_array_t */ pointer_t x;
    /* vm_array_t */ pointer_t y;
};
void Math_erf(vm_registers_t &r, vm_t &vm);
struct F_Math_erf : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_erfc(vm_registers_t &r, vm_t &vm);
struct F_Math_erfc : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_exp(vm_registers_t &r, vm_t &vm);
struct F_Math_exp : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_expm1(vm_registers_t &r, vm_t &vm);
struct F_Math_expm1 : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_export_int(vm_registers_t &r, vm_t &vm);
struct F_Math_export_int : public vm_frame_base_alloc_t
{
    word_t noret;
    byte_t temps[12];
    /* vm_array_t */ pointer_t b;
    /* vm_array_t */ pointer_t x;
};
void Math_export_real(vm_registers_t &r, vm_t &vm);
struct F_Math_export_real : public vm_frame_base_alloc_t
{
    word_t noret;
    byte_t temps[12];
    /* vm_array_t */ pointer_t b;
    /* vm_array_t */ pointer_t x;
};
void Math_export_real32(vm_registers_t &r, vm_t &vm);
struct F_Math_export_real32 : public vm_frame_base_alloc_t
{
    word_t noret;
    byte_t temps[12];
    /* vm_array_t */ pointer_t b;
    /* vm_array_t */ pointer_t x;
};
void Math_fabs(vm_registers_t &r, vm_t &vm);
struct F_Math_fabs : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_fdim(vm_registers_t &r, vm_t &vm);
struct F_Math_fdim : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
    real_t y;
};
void Math_finite(vm_registers_t &r, vm_t &vm);
struct F_Math_finite : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_floor(vm_registers_t &r, vm_t &vm);
struct F_Math_floor : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_fmax(vm_registers_t &r, vm_t &vm);
struct F_Math_fmax : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
    real_t y;
};
void Math_fmin(vm_registers_t &r, vm_t &vm);
struct F_Math_fmin : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
    real_t y;
};
void Math_fmod(vm_registers_t &r, vm_t &vm);
struct F_Math_fmod : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
    real_t y;
};
void Math_gemm(vm_registers_t &r, vm_t &vm);
struct F_Math_gemm : public vm_frame_base_alloc_t
{
    word_t noret;
    byte_t temps[12];
    word_t transa;
    word_t transb;
    word_t m;
    word_t n;
    word_t k;
    byte_t _pad52[4];
    real_t alpha;
    /* vm_array_t */ pointer_t a;
    word_t lda;
    /* vm_array_t */ pointer_t b;
    word_t ldb;
    real_t beta;
    /* vm_array_t */ pointer_t c;
    word_t ldc;
};
void Math_getFPcontrol(vm_registers_t &r, vm_t &vm);
struct F_Math_getFPcontrol : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
};
void Math_getFPstatus(vm_registers_t &r, vm_t &vm);
struct F_Math_getFPstatus : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
};
void Math_hypot(vm_registers_t &r, vm_t &vm);
struct F_Math_hypot : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
    real_t y;
};
void Math_iamax(vm_registers_t &r, vm_t &vm);
struct F_Math_iamax : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
    /* vm_array_t */ pointer_t x;
};
void Math_ilogb(vm_registers_t &r, vm_t &vm);
struct F_Math_ilogb : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_import_int(vm_registers_t &r, vm_t &vm);
struct F_Math_import_int : public vm_frame_base_alloc_t
{
    word_t noret;
    byte_t temps[12];
    /* vm_array_t */ pointer_t b;
    /* vm_array_t */ pointer_t x;
};
void Math_import_real(vm_registers_t &r, vm_t &vm);
struct F_Math_import_real : public vm_frame_base_alloc_t
{
    word_t noret;
    byte_t temps[12];
    /* vm_array_t */ pointer_t b;
    /* vm_array_t */ pointer_t x;
};
void Math_import_real32(vm_registers_t &r, vm_t &vm);
struct F_Math_import_real32 : public vm_frame_base_alloc_t
{
    word_t noret;
    byte_t temps[12];
    /* vm_array_t */ pointer_t b;
    /* vm_array_t */ pointer_t x;
};
void Math_isnan(vm_registers_t &r, vm_t &vm);
struct F_Math_isnan : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_j0(vm_registers_t &r, vm_t &vm);
struct F_Math_j0 : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_j1(vm_registers_t &r, vm_t &vm);
struct F_Math_j1 : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_jn(vm_registers_t &r, vm_t &vm);
struct F_Math_jn : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    word_t n;
    byte_t _pad36[4];
    real_t x;
};
void Math_lgamma(vm_registers_t &r, vm_t &vm);
struct F_Math_lgamma : public vm_frame_base_alloc_t
{
    struct { word_t t0; byte_t _pad4[4]; real_t t1; }*ret;
    byte_t temps[12];
    real_t x;
};
void Math_log(vm_registers_t &r, vm_t &vm);
struct F_Math_log : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_log10(vm_registers_t &r, vm_t &vm);
struct F_Math_log10 : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_log1p(vm_registers_t &r, vm_t &vm);
struct F_Math_log1p : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_modf(vm_registers_t &r, vm_t &vm);
struct F_Math_modf : public vm_frame_base_alloc_t
{
    struct { word_t t0; byte_t _pad4[4]; real_t t1; }*ret;
    byte_t temps[12];
    real_t x;
};
void Math_nextafter(vm_registers_t &r, vm_t &vm);
struct F_Math_nextafter : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
    real_t y;
};
void Math_norm1(vm_registers_t &r, vm_t &vm);
struct F_Math_norm1 : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    /* vm_array_t */ pointer_t x;
};
void Math_norm2(vm_registers_t &r, vm_t &vm);
struct F_Math_norm2 : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    /* vm_array_t */ pointer_t x;
};
void Math_pow(vm_registers_t &r, vm_t &vm);
struct F_Math_pow : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
    real_t y;
};
void Math_pow10(vm_registers_t &r, vm_t &vm);
struct F_Math_pow10 : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    word_t p;
};
void Math_realbits32(vm_registers_t &r, vm_t &vm);
struct F_Math_realbits32 : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_realbits64(vm_registers_t &r, vm_t &vm);
struct F_Math_realbits64 : public vm_frame_base_alloc_t
{
    big_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_remainder(vm_registers_t &r, vm_t &vm);
struct F_Math_remainder : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
    real_t p;
};
void Math_rint(vm_registers_t &r, vm_t &vm);
struct F_Math_rint : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_scalbn(vm_registers_t &r, vm_t &vm);
struct F_Math_scalbn : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
    word_t n;
};
void Math_sin(vm_registers_t &r, vm_t &vm);
struct F_Math_sin : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_sinh(vm_registers_t &r, vm_t &vm);
struct F_Math_sinh : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_sort(vm_registers_t &r, vm_t &vm);
struct F_Math_sort : public vm_frame_base_alloc_t
{
    word_t noret;
    byte_t temps[12];
    /* vm_array_t */ pointer_t x;
    /* vm_array_t */ pointer_t pi;
};
void Math_sqrt(vm_registers_t &r, vm_t &vm);
struct F_Math_sqrt : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_tan(vm_registers_t &r, vm_t &vm);
struct F_Math_tan : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_tanh(vm_registers_t &r, vm_t &vm);
struct F_Math_tanh : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_y0(vm_registers_t &r, vm_t &vm);
struct F_Math_y0 : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_y1(vm_registers_t &r, vm_t &vm);
struct F_Math_y1 : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    real_t x;
};
void Math_yn(vm_registers_t &r, vm_t &vm);
struct F_Math_yn : public vm_frame_base_alloc_t
{
    real_t* ret;
    byte_t temps[12];
    word_t n;
    byte_t _pad36[4];
    real_t x;
};
const char *Math_PATH = "$Math";
const real_t Math_Infinity = INFINITY;
const real_t Math_NaN = NAN;

static_assert(sizeof(real_t) == 8, "Recalculate EPS if size of real_t is changing");
const real_t Math_MachEps = 2.220446049250313e-16; // 64-bit double
const real_t Math_Pi = 3.141592653589793;
const real_t Math_Degree = .017453292519943295;
const word_t Math_INVAL = 0x1;
const word_t Math_ZDIV = 0x2;
const word_t Math_OVFL = 0x4;
const word_t Math_UNFL = 0x8;
const word_t Math_INEX = 0x10;
const word_t Math_RND_NR = 0x0;
const word_t Math_RND_NINF = 0x100;
const word_t Math_RND_PINF = 0x200;
const word_t Math_RND_Z = 0x300;
const word_t Math_RND_MASK = 0x300;

#endif // _DISVM_SRC_VM_MATH_MATHMOD_H_
