//
// Dis VM
// File: utf8.h
// Author: arr
//

#ifndef _DISVM_SRC_INCLUDE_UTF8_H_
#define _DISVM_SRC_INCLUDE_UTF8_H_

#include <cstdint>
#include "runtime.h"

namespace disvm
{
    namespace runtime
    {
        namespace utf8
        {
            // As defined in http://tools.ietf.org/html/rfc3629
            namespace constants
            {
                const disvm::runtime::rune_t multi_byte_value_mask = 0x3f;

                const disvm::runtime::rune_t max_codepoint_ascii = 0x7f;
                const disvm::runtime::rune_t max_codepoint_2_byte = 0x7ff;
                const disvm::runtime::rune_t max_codepoint_3_byte = 0xffff;
                const disvm::runtime::rune_t max_codepoint_4_byte = 0x10ffff;

                const disvm::runtime::rune_t max_supported_codepoint = max_codepoint_4_byte;
            };

            // Compute the number of code points in the supplied string
            // See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.
            std::size_t count_codepoints(const uint8_t *str, const std::size_t max_len);

            enum class decode_state_t
            {
                accept = 0,
                reject = 12
            };

            // Decode a byte at a time to form a codepoint.
            // See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.
            decode_state_t decode_step(decode_state_t &state, rune_t &codepoint, uint8_t byte);

            // Decode the next codepoint in the supplied string.
            // Returns number of characters in string consumed.
            std::size_t decode(const uint8_t *str, rune_t &codepoint);

            // Encode the rune into the supplied buffer.
            // Returns updated pointer in the buffer or null on error.
            uint8_t *encode(const rune_t rune, uint8_t *str);
        }
    }
}

#endif // _DISVM_SRC_INCLUDE_UTF8_H_
