//
// Dis VM
// File: utf8.cpp
// Author: arr
//

// Copyright (c) 2008-2010 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.
// Most code in this file falls under the above copyright. It was augmented only to be more C++ friendly.

#include <cinttypes>
#include <utf8.h>
#include <exceptions.h>

using namespace disvm::runtime;

namespace
{
    const uint8_t utf8d[] =
    {
        // The first part of the table maps bytes to character classes that
        // to reduce the size of the transition table and create bitmasks.
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        8, 8, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
        10, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 3, 3, 11, 6, 6, 6, 5, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,

        // The second part is a transition table that maps a combination
        // of a state of the automaton and a character class to a state.
        0, 12, 24, 36, 60, 96, 84, 12, 12, 12, 48, 72, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
        12, 0, 12, 12, 12, 12, 12, 0, 12, 0, 12, 12, 12, 24, 12, 12, 12, 12, 12, 24, 12, 24, 12, 12,
        12, 12, 12, 12, 12, 12, 12, 24, 12, 12, 12, 12, 12, 24, 12, 12, 12, 12, 12, 12, 12, 24, 12, 12,
        12, 12, 12, 12, 12, 12, 12, 36, 12, 36, 12, 12, 12, 36, 12, 12, 12, 12, 12, 36, 12, 36, 12, 12,
        12, 36, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    };
}

// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.
utf8::decode_state disvm::runtime::utf8::decode_step(utf8::decode_state &state, rune_t &codepoint, uint8_t byte)
{
    auto type = uint32_t{ utf8d[byte] };

    codepoint = (state != utf8::decode_state::accept) ?
        (byte & 0x3fu) | (codepoint << 6) :
        (0xff >> type) & (byte);

    state = static_cast<utf8::decode_state>(utf8d[256 + static_cast<uint32_t>(state) + type]);
    return state;
}

// Adopted from 'countCodePoints(uint8_t* s, size_t* count)'
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.
std::size_t disvm::runtime::utf8::count_codepoints(const uint8_t *str, const std::size_t max_len)
{
    assert(str != nullptr);
    auto single_character = rune_t{};
    auto points = std::size_t{ 0 };
    auto state = utf8::decode_state::accept;

    for (auto i = std::size_t{ 0 }; i < max_len; ++i)
    {
        if (utf8::decode_state::accept == utf8::decode_step(state, single_character, str[i]))
            points++;
    }

    if (state != utf8::decode_state::accept)
        throw invalid_utf8{};

    return points;
}

std::size_t disvm::runtime::utf8::decode(const uint8_t *str, rune_t &codepoint)
{
    assert(str != nullptr);
    auto state = utf8::decode_state::accept;

    codepoint = 0;
    auto begin = str;
    auto curr = str;
    auto c = uint8_t{};
    while (0 != (c = *curr))
    {
        curr++;
        state = utf8::decode_step(state, codepoint, c);
        if (state == utf8::decode_state::accept)
            return (curr - begin);
    }

    // Empty string
    if (begin == curr)
        return 0;

    throw invalid_utf8{};
}

uint8_t *disvm::runtime::utf8::encode(const rune_t rune, uint8_t *str)
{
    assert(str != nullptr);
    if (rune <= utf8::constants::max_codepoint_ascii)
    {
        // Layout: 0xxxxxxx
        str[0] = static_cast<uint8_t>(rune);
        return (str + 1);
    }
    else if (rune <= utf8::constants::max_codepoint_2_byte)
    {
        // Layout: 110xxxxx 10xxxxxx
        str[0] = static_cast<uint8_t>(0xc0 + (rune >> 6));
        str[1] = static_cast<uint8_t>(0x80 + (rune & utf8::constants::multi_byte_value_mask));
        return (str + 2);
    }
    else if (rune <= utf8::constants::max_codepoint_3_byte)
    {
        // Layout: 1110xxxx 10xxxxxx 10xxxxxx
        str[0] = static_cast<uint8_t>(0xe0 + (rune >> 12));
        str[1] = static_cast<uint8_t>(0x80 + ((rune >> 6) & utf8::constants::multi_byte_value_mask));
        str[2] = static_cast<uint8_t>(0x80 + (rune & utf8::constants::multi_byte_value_mask));
        return (str + 3);
    }
    else if (rune <= utf8::constants::max_codepoint_4_byte)
    {
        // Layout: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        str[0] = static_cast<uint8_t>(0xf0 + (rune >> 18));
        str[1] = static_cast<uint8_t>(0x80 + ((rune >> 12) & utf8::constants::multi_byte_value_mask));
        str[2] = static_cast<uint8_t>(0x80 + ((rune >> 6) & utf8::constants::multi_byte_value_mask));
        str[3] = static_cast<uint8_t>(0x80 + (rune & utf8::constants::multi_byte_value_mask));
        return (str + 4);
    }

    assert(false && "Unsupported UTF-8 character");
    return nullptr;
}