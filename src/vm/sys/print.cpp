//
// Dis VM
// File: print.cpp
// Author: arr
//

#include <array>
#include <cinttypes>
#include <utf8.h>
#include "sys_utils.h"

using namespace disvm::runtime;
using namespace disvm::runtime::utf8;

// Shamelessly adopted and altered from Inferno implementation (xprint - libinterp/runt.c)
// [TODO] Handle UTF-8
word_t printf_to_buffer(const vm_string_t &msg_fmt, byte_t *msg_args, std::size_t buffer_size, char *buffer)
{
    auto b_curr = buffer;
    const auto b_end = buffer + (buffer_size - 1);
    const auto fmt_len = msg_fmt.get_length();
    auto i = word_t{ 0 };

    // Loop over the entire format string
    while (i < fmt_len)
    {
        if (b_curr >= b_end)
            return -1;

        auto c = msg_fmt.get_rune(i);
        ++i;
        if (c != '%')
        {
            b_curr = reinterpret_cast<char *>(utf8::encode(c, reinterpret_cast<uint8_t *>(b_curr)));
            continue;
        }

        auto format = std::vector<char>{};
        format.push_back('%');
        auto wb = int{ 0 };
        auto big_flag = false;

        // Decode the format specifier
        while (i < fmt_len)
        {
            c = msg_fmt.get_rune(i);
            ++i;
            switch (c)
            {
                // Format flags
            default:
                continue;
            case 'b': // Print big type (64-bit)
                format.push_back('l');
                format.push_back('l');
                big_flag = true;
                continue;

                // Format verbs
            case '%': // Escaped specifier
                wb = std::snprintf(b_curr, (b_end - b_curr), "%%%%");
                break;
            case 'o': // octal
            case 'd': // decimal
            case 'x': // hexidecimal
            case 'X': // Upper-case hexidecimal
                format.push_back(static_cast<char>(c));
                format.push_back('\0');
                if (big_flag)
                {
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
            case 's': // string
            {
                auto s = vm_alloc_t::from_allocation<vm_string_t>(*reinterpret_cast<pointer_t *>(msg_args));
                if (s != nullptr)
                    wb = std::snprintf(b_curr, (b_end - b_curr), "%s", s->str());

                msg_args += sizeof(pointer_t);
                break;
            }
            // [SPEC] Undocumented format verb for debugging VM objects.
            case 'H':
            {
                auto alloc = vm_alloc_t::from_allocation(*reinterpret_cast<pointer_t *>(msg_args));
                auto rc = static_cast<std::size_t>(-1);
                const void *type_alloc = nullptr;
                if (alloc != nullptr)
                {
                    rc = alloc->get_ref_count();
                    type_alloc = alloc->alloc_type.get();
                }

                wb = std::snprintf(b_curr, (b_end - b_curr), "%" PRIdPTR ".%#08" PRIxPTR, rc, reinterpret_cast<std::size_t>(type_alloc));
                msg_args += sizeof(pointer_t);
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

word_t printf_to_dynamic_buffer(const vm_string_t &msg_fmt, byte_t *msg_args, std::vector<char> &buffer)
{
    auto result = word_t{ 0 };
    while (-1 == (result = printf_to_buffer(msg_fmt, msg_args, buffer.size(), buffer.data())))
    {
        buffer.resize(buffer.size() * 2);
    }

    return result;
}
