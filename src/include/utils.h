//
// Dis VM
// File: utils.h
// Author: arr
//

#ifndef _DISVM_SRC_INCLUDE_UTILS_H_
#define _DISVM_SRC_INCLUDE_UTILS_H_

#include <cstdint>
#include <type_traits>

namespace disvm
{
    namespace util
    {
        template<typename Flags>
        inline bool has_flag(const Flags flags, const Flags flag)
        {
            return (static_cast<std::underlying_type_t<Flags>>(flags) & static_cast<std::underlying_type_t<Flags>>(flag)) != 0;
        }

        template <typename T1, typename T2>
        inline bool are_equal(const T1 &l, const T2 &r)
        {
            return (l == r);
        }

        template <typename T1, typename T2, typename... Others>
        inline bool are_equal(const T1 &l, const T2 &r, const Others &... args)
        {
            return (l == r) && are_equal(r, args...);
        }
    }
}

// Adopted from 'winnt.h' in the Windows SDK
#define DEFINE_ENUM_FLAG_OPERATORS(TYPE) \
inline TYPE operator | (TYPE a, TYPE b) { return static_cast<TYPE>(static_cast<std::underlying_type_t<TYPE>>(a) | static_cast<std::underlying_type_t<TYPE>>(b)); } \
inline TYPE &operator |= (TYPE &a, TYPE b) { a = a | b; return a; } \
inline TYPE operator & (TYPE a, TYPE b) { return static_cast<TYPE>(static_cast<std::underlying_type_t<TYPE>>(a) & static_cast<std::underlying_type_t<TYPE>>(b)); } \
inline TYPE &operator &= (TYPE &a, TYPE b) { a = a & b; return a; } \
inline TYPE operator ^ (TYPE a, TYPE b) { return static_cast<TYPE>(static_cast<std::underlying_type_t<TYPE>>(a) ^ static_cast<std::underlying_type_t<TYPE>>(b)); } \
inline TYPE &operator ^= (TYPE &a, TYPE b) { a = a ^ b; return a; } \
inline TYPE operator ~ (TYPE a) { return static_cast<TYPE>(~static_cast<std::underlying_type_t<TYPE>>(a)); }

#endif // _DISVM_SRC_INCLUDE_UTILS_H_