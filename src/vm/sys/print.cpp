//
// Dis VM
// File: print.cpp
// Author: arr
//

#include <cinttypes>
#include <array>
#include <utf8.h>
#include "sys_utils.h"

using disvm::runtime::byte_t;
using disvm::runtime::pointer_t;
using disvm::runtime::vm_array_t;
using disvm::runtime::word_t;

namespace
{
    // Align the supplied pointer for the template type relative to base.
    template<typename T>
    byte_t *align_for(byte_t *p, pointer_t base)
    {
        for (;;)
        {
            const auto d = reinterpret_cast<std::uintptr_t>(p) - reinterpret_cast<std::uintptr_t>(base);
            if (d % sizeof(T) == 0)
                return p;

            p++;
        }
    }
}

// Shamelessly adopted and altered from Inferno implementation (xprint - libinterp/runt.c)
// [TODO] Handle UTF-8, binary format 'B'
word_t disvm::runtime::sys::printf_to_buffer(
    disvm::vm_t &vm,
    const disvm::runtime::vm_string_t &msg_fmt,
    disvm::runtime::byte_t *msg_args,
    disvm::runtime::pointer_t base,
    const std::size_t buffer_size,
    char *buffer)
{
    auto b_curr = buffer;
    const auto b_end = buffer + (buffer_size - 1);
    const auto fmt_len = msg_fmt.get_length();
    auto i = word_t{ 0 };

    auto format = std::vector<char>{};
    format.reserve(32); // Default size in Inferno.

    // Loop over the entire format string
    while (i < fmt_len)
    {
        if (b_curr >= b_end)
            return -1;

        auto c = msg_fmt.get_rune(i++);
        if (c != '%')
        {
            b_curr = reinterpret_cast<char *>(utf8::encode(c, reinterpret_cast<uint8_t *>(b_curr)));
            continue;
        }

        format.clear();
        format.push_back('%');
        auto wb = int{ 0 };
        auto big_flag = false;
        auto unsigned_flag = false;

        // Decode the format specifier
        while (i < fmt_len)
        {
            c = msg_fmt.get_rune(i++);
            switch (c)
            {
                // Format flags
            default:
                format.push_back(static_cast<char>(c));
                continue;
            case 'b': // Print big type (64-bit)
                format.push_back('l');
                format.push_back('l');
                big_flag = true;
                continue;
            case '*': // Placeholder for word argument
            {
                const auto w = *reinterpret_cast<word_t *>(msg_args);
                std::array<char, sizeof("-2147483648")> local_buffer;
                const auto len = std::sprintf(local_buffer.data(), "%d", w);
                if (len < 0)
                    return -1;

                format.insert(std::end(format), local_buffer.data(), local_buffer.data() + len);
                msg_args += sizeof(w);
                continue;
            }
            case 'u':
                unsigned_flag = true;
                continue;

                // Format verbs
            case '%': // Escaped specifier
                wb = std::snprintf(b_curr, (b_end - b_curr), "%%%%");
                break;
            case 'f': // floating point verbs
            case 'e':
            case 'E':
            case 'g':
            case 'G':
            {
                format.push_back(static_cast<char>(c));
                format.push_back('\0');

                msg_args = align_for<real_t>(msg_args, base);

                const auto r = *reinterpret_cast<real_t *>(msg_args);
                wb = std::snprintf(b_curr, (b_end - b_curr), format.data(), r);
                msg_args += sizeof(r);
                break;
            }
            case 'd': // decimal
                if (unsigned_flag)
                    c = 'u';
            case 'o': // octal
            case 'x': // hexadecimal
            case 'X': // Upper-case hexadecimal
            case 'c': // Unicode
                format.push_back(static_cast<char>(c));
                format.push_back('\0');
                if (big_flag)
                {
                    msg_args = align_for<big_t>(msg_args, base);

                    const auto b = *reinterpret_cast<big_t *>(msg_args);
                    wb = std::snprintf(b_curr, (b_end - b_curr), format.data(), b);
                    msg_args += sizeof(b);
                }
                else
                {
                    const auto w = *reinterpret_cast<word_t *>(msg_args);
                    wb = std::snprintf(b_curr, (b_end - b_curr), format.data(), w);
                    msg_args += sizeof(w);
                }
                break;
            case 'q':
                assert(false && "'q' format not implemented");
            case 's': // string
            {
                auto p = *reinterpret_cast<pointer_t *>(msg_args);
                auto str = vm_alloc_t::from_allocation<vm_string_t>(p);
                if (str != nullptr)
                    wb = std::snprintf(b_curr, (b_end - b_curr), "%s", str->str());

                msg_args += sizeof(p);
                break;
            }
            case 'r': // Last system error message
            {
                auto err_msg = pop_syscall_error_message(vm);
                wb = std::snprintf(b_curr, (b_end - b_curr), "%s", err_msg.c_str());
                break;
            }
            // [SPEC] Undocumented format verb for debugging VM objects.
            case 'H':
            {
                auto p = *reinterpret_cast<pointer_t *>(msg_args);
                auto alloc = vm_alloc_t::from_allocation(p);
                auto rc = static_cast<std::size_t>(-1);
                const void *type_alloc = nullptr;
                if (alloc != nullptr)
                {
                    rc = alloc->get_ref_count();
                    type_alloc = alloc->alloc_type.get();
                }

                wb = std::snprintf(b_curr, (b_end - b_curr), "%" PRIuPTR ".%#08" PRIxPTR, static_cast<std::uintptr_t>(rc), reinterpret_cast<std::uintptr_t>(type_alloc));
                msg_args += sizeof(p);
                break;
            }
            }

            break;
        }

        if (wb < 0)
            return -1;

        b_curr += wb;
    }

    assert(*b_curr == '\0' && "The buffer should be null-terminated");
    return (b_curr - buffer);
}

word_t disvm::runtime::sys::printf_to_dynamic_buffer(
    disvm::vm_t &vm,
    const disvm::runtime::vm_string_t &msg_fmt,
    disvm::runtime::byte_t *msg_args,
    disvm::runtime::pointer_t base,
    std::vector<char> &buffer)
{
    auto result = word_t{ 0 };
    while (-1 == (result = printf_to_buffer(vm, msg_fmt, msg_args, base, buffer.size(), buffer.data())))
        buffer.resize(buffer.size() * 2);

    return result;
}
