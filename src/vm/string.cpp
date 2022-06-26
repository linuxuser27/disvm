//
// Dis VM
// File: string.cpp
// Author: arr
//

#include <cinttypes>
#include <runtime.hpp>
#include <utf8.hpp>
#include <vm_memory.hpp>
#include <debug.hpp>
#include <exceptions.hpp>

using disvm::debug::component_trace_t;
using disvm::debug::log_level_t;

using disvm::runtime::managed_ptr_t;
using disvm::runtime::intrinsic_type_desc;
using disvm::runtime::type_descriptor_t;
using disvm::runtime::vm_string_t;
using disvm::runtime::word_t;

managed_ptr_t<const type_descriptor_t> vm_string_t::type_desc()
{
    return intrinsic_type_desc::type<vm_string_t>();
}

namespace
{
    // Enumeration for character type compare cases.
    enum class compare_case_t
    {
        ascii_ascii,
        ascii_rune,
        rune_ascii,
        rune_rune
    };

    // Scenario matrix for compare cases
    const compare_case_t scenario_matrix[][2] =
    {
        { compare_case_t::ascii_ascii, compare_case_t::ascii_rune },
        { compare_case_t::rune_ascii, compare_case_t::rune_rune },
    };

    // Case indices
    const auto ascii_idx = std::size_t{ 0 };
    const auto rune_idx = std::size_t{ 1 };

    // Compare function for character types.
    template <typename CharacterType1, typename CharacterType2>
    int _compare(word_t len, const CharacterType1 *s1, const CharacterType2 *s2)
    {
        for (auto i = word_t{ 0 }; i < len; ++i)
        {
            const auto sc1 = static_cast<disvm::runtime::rune_t>(s1[i]);
            const auto sc2 = static_cast<disvm::runtime::rune_t>(s2[i]);

            if (sc1 != sc2)
                return (sc1 < sc2) ? (-1) : 1;
        }

        return 0;
    }
}

int vm_string_t::compare(const vm_string_t *s1, const vm_string_t *s2)
{
    auto len_1 = word_t{ 0 };
    auto scen_1 = ascii_idx;
    const void *str_mem_1 = "";
    if (s1 != nullptr)
    {
        len_1 = s1->_length;
        scen_1 = s1->_character_size == sizeof(char) ? ascii_idx : rune_idx;
        str_mem_1 = (s1->_is_alloc) ? s1->_mem.alloc : s1->_mem.local;
    }

    auto len_2 = word_t{ 0 };
    auto scen_2 = ascii_idx;
    const void *str_mem_2 = "";
    if (s2 != nullptr)
    {
        len_2 = s2->_length;
        scen_2 = s2->_character_size == sizeof(char) ? ascii_idx : rune_idx;
        str_mem_2 = (s2->_is_alloc) ? s2->_mem.alloc : s2->_mem.local;
    }

    // Check the simple compare case between the same string.
    if (str_mem_1 == str_mem_2)
        return 0;

    const auto scenario = scenario_matrix[scen_1][scen_2];

    auto result = int{ 0 };
    const auto min_length = std::min(len_1, len_2);
    switch (scenario)
    {
    default:
        assert(false && "Unknown compare scenario");
    case compare_case_t::ascii_ascii:
    case compare_case_t::rune_rune:
        result = std::memcmp(str_mem_1, str_mem_2, min_length);
        break;
    case compare_case_t::ascii_rune:
        result = _compare(min_length, reinterpret_cast<const char *>(str_mem_1), reinterpret_cast<const runtime::rune_t *>(str_mem_2));
        break;
    case compare_case_t::rune_ascii:
        result = _compare(min_length, reinterpret_cast<const runtime::rune_t *>(str_mem_1), reinterpret_cast<const char *>(str_mem_2));
        break;
    }

    if (result != 0)
        return result;

    // Comparing lengths if one string is a proper sub-string of the other.
    return (len_1 - len_2);
}

namespace
{
    template<typename DestCharacterType, typename SourceCharacterType>
    void copy_characters_to(DestCharacterType *dest, word_t dest_start, const SourceCharacterType *source, word_t source_length)
    {
        for (auto i = word_t{ 0 }; i < source_length; ++i)
            dest[dest_start + i] = static_cast<DestCharacterType>(source[i]);
    }

    template<>
    void copy_characters_to(char *dest, word_t dest_start, const char *source, word_t source_length)
    {
        assert(dest != nullptr && source != nullptr && dest != source);
        std::memcpy((dest + dest_start), source, source_length);
    }

    template<>
    void copy_characters_to(disvm::runtime::rune_t *dest, word_t dest_start, const disvm::runtime::rune_t *source, word_t source_length)
    {
        assert(dest != nullptr && source != nullptr && dest != source);
        std::memcpy((dest + dest_start), source, (source_length * sizeof(disvm::runtime::rune_t)));
    }

    void combine(
        bool dest_is_rune,
        uint8_t *dest,
        bool source1_is_rune,
        word_t source1_length,
        const uint8_t *source1,
        bool source2_is_rune,
        word_t source2_length,
        const uint8_t *source2)
    {
        if (dest_is_rune)
        {
            auto dest_rune = reinterpret_cast<disvm::runtime::rune_t *>(dest);
            if (source1_is_rune)
                copy_characters_to(dest_rune, 0, reinterpret_cast<const disvm::runtime::rune_t *>(source1), source1_length);
            else
                copy_characters_to(dest_rune, 0, source1, source1_length);

            if (source2_is_rune)
                copy_characters_to(dest_rune, source1_length, reinterpret_cast<const disvm::runtime::rune_t *>(source2), source2_length);
            else
                copy_characters_to(dest_rune, source1_length, source2, source2_length);
        }
        else
        {
            copy_characters_to(dest, 0, source1, source1_length);
            copy_characters_to(dest, source1_length, source2, source2_length);
        }
    }

    word_t compute_max_length(word_t current_length)
    {
        // Length + 1 (null) + space for string expansion
        return (current_length + 1 + (current_length / 4));
    }

    template<typename T, word_t S>
    constexpr word_t compute_max_length(const T(&)[S])
    {
        return S - 1;
    }
}

vm_string_t::vm_string_t()
    : vm_alloc_t(vm_string_t::type_desc())
    , _character_size{ sizeof(char) }
    , _encoded_str{ nullptr }
    , _is_alloc{ false }
    , _length{ 0 }
    , _length_max{ compute_max_length(_mem.local) }
    , _mem{}
{ }

vm_string_t::vm_string_t(std::size_t encoded_str_len, const uint8_t *encoded_str)
    : vm_alloc_t(vm_string_t::type_desc())
    , _character_size{ sizeof(char) }
    , _encoded_str{ nullptr }
    , _is_alloc{ false }
    , _length{ 0 }
    , _length_max{ compute_max_length(_mem.local) }
    , _mem{}
{
    assert((encoded_str_len > 0 && encoded_str != nullptr) || encoded_str_len == 0);
    if (encoded_str_len == 0)
        encoded_str = nullptr;

    const auto utf8_length = utf8::count_codepoints(encoded_str, encoded_str_len);

    // Check if the supplied string is completely valid UTF-8 - only valid UTF-8 strings can be DisVM strings.
    if (encoded_str_len != utf8_length.byte_count)
        throw invalid_utf8{};

    _character_size = (encoded_str_len != utf8_length.codepoint_count) ? sizeof(rune_t) : sizeof(char);

    _length = static_cast<word_t>(utf8_length.codepoint_count);
    _length_max = compute_max_length(_mem.local) / _character_size;

    // Allocate memory based on character count and size
    auto dest = _mem.local;
    if (encoded_str_len > static_cast<std::size_t>(compute_max_length(_mem.local)))
    {
        // If we are going to allocate, make sure it is larger than needed.
        _length_max = compute_max_length(_length);
        _mem.alloc = alloc_unmanaged_memory<uint8_t>(_length_max * _character_size);
        _is_alloc = true;

        dest = _mem.alloc;
    }

    // Copy over the entire buffer if ASCII or convert and copy if multi-byte
    if (_character_size == sizeof(char))
    {
        std::memcpy(dest, encoded_str, encoded_str_len);
    }
    else
    {
        auto rune_dest = reinterpret_cast<rune_t *>(dest);
        for (auto i = word_t{ 0 }; i < _length; ++i)
            encoded_str += utf8::decode(encoded_str, rune_dest[i]);
    }

    if (disvm::debug::is_component_tracing_enabled<component_trace_t::memory>())
        disvm::debug::log_msg(component_trace_t::memory, log_level_t::debug, "init: vm string: %d %d >>%s<<", _length, (_length_max * _character_size), dest);
}

vm_string_t::vm_string_t(const vm_string_t &s1, const vm_string_t &s2)
    : vm_alloc_t(vm_string_t::type_desc())
    , _character_size{ std::max(s1._character_size, s2._character_size) }
    , _encoded_str{ nullptr }
    , _is_alloc{ true }
    , _length{ s1._length + s2._length }
    , _length_max{ compute_max_length(s1._length + s2._length) }
    , _mem{}
{
    _mem.alloc = alloc_unmanaged_memory<uint8_t>(_length_max * _character_size);

    const auto s1_data = (s1._is_alloc) ? s1._mem.alloc : s1._mem.local;
    const auto s2_data = (s2._is_alloc) ? s2._mem.alloc : s2._mem.local;

    // Combine the two strings
    combine(
        _character_size == sizeof(rune_t),
        _mem.alloc,
        s1._character_size == sizeof(rune_t),
        s1._length,
        s1_data,
        s2._character_size == sizeof(rune_t),
        s2._length,
        s2_data);
}

vm_string_t::vm_string_t(const vm_string_t &other, word_t begin_index, word_t end_index)
    : vm_alloc_t(other.alloc_type)
    , _character_size{ other._character_size }
    , _encoded_str{ nullptr }
    , _is_alloc{ other._is_alloc }
    , _length{ end_index - begin_index }
    , _length_max{ compute_max_length(end_index - begin_index) }
    , _mem{}
{
    if (end_index < begin_index)
        throw out_of_range_memory{};

    if (begin_index < 0)
        throw index_out_of_range_memory{ 0, other._length, begin_index };

    if (other._length < end_index)
        throw index_out_of_range_memory{ 0, other._length, end_index };

    // Allocate memory based on character count and size
    auto src = other._mem.local;
    auto dest = _mem.local;
    if (_is_alloc)
    {
        src = other._mem.alloc;
        _mem.alloc = alloc_unmanaged_memory<uint8_t>(_length_max * _character_size);
        dest = _mem.alloc;
    }

    // Copy over string content starting at the correct index (char vs. rune_t)
    const auto length_in_bytes = _length * _character_size;
    std::memcpy(dest, (src + (begin_index * _character_size)), length_in_bytes);

    if (disvm::debug::is_component_tracing_enabled<component_trace_t::memory>())
        disvm::debug::log_msg(component_trace_t::memory, log_level_t::debug, "copy: vm string: %#" PRIxPTR " %d %d %#" PRIxPTR, &other, begin_index, end_index, this);
}

vm_string_t::~vm_string_t()
{
    // Determine if local or allocated memory
    auto source = (_is_alloc) ? _mem.alloc : _mem.local;
    if (static_cast<void *>(source) != static_cast<void *>(_encoded_str))
        free_unmanaged_memory(_encoded_str);

    if (_is_alloc)
        free_unmanaged_memory(source);

    debug::assign_debug_pointer(&source);
    debug::assign_debug_pointer(&_encoded_str);

    if (disvm::debug::is_component_tracing_enabled<component_trace_t::memory>())
        disvm::debug::log_msg(component_trace_t::memory, log_level_t::debug, "destroy: vm string");
}

vm_string_t &vm_string_t::append(const vm_string_t &other)
{
    // This function does not adhere to the immutable string contract.
    // Therefore, it is unsafe to call this function if there is more
    // than 1 reference to this string instance.
    assert(get_ref_count() <= 1);

    // Check if the new string has any length at all.
    const auto other_length = other._length;
    assert(other_length >= 0);
    if (other_length == 0)
        return *this;

    const auto other_is_rune = other._character_size == sizeof(rune_t);
    const auto other_source = (other._is_alloc) ? other._mem.alloc : other._mem.local;

    const auto local_is_rune = _character_size == sizeof(rune_t);
    const auto local_length = _length;
    const auto local_source = (_is_alloc) ? _mem.alloc : _mem.local;

    // Release the encoded string for this instance.
    if (_encoded_str != reinterpret_cast<char*>(local_source))
        free_unmanaged_memory(_encoded_str);

    _encoded_str = nullptr;

    // Define the values for the appended string
    const auto new_length = local_length + other_length;
    const auto new_length_max = compute_max_length(new_length);
    const auto new_is_rune = local_is_rune || other_is_rune;
    uint8_t *new_source = nullptr;

    // Check if this string and the supplied string have the same character size.
    if (new_is_rune)
        new_source = alloc_unmanaged_memory<uint8_t>(new_length_max * sizeof(rune_t));
    else
        new_source = alloc_unmanaged_memory<uint8_t>(new_length_max);

    // Combine the two string sources
    combine(
        new_is_rune,
        new_source,
        local_is_rune,
        local_length,
        local_source,
        other_is_rune,
        other_length,
        other_source);

    _length = new_length;
    _length_max = new_length_max;

    // Free the local allocation
    if (_is_alloc)
    {
        free_unmanaged_memory(local_source);
        debug::assign_debug_pointer(&_mem.alloc);
    }

    _mem.alloc = new_source;
    _is_alloc = true;
    _character_size = new_is_rune ? sizeof(rune_t) : sizeof(char);

    return *this;
}

word_t vm_string_t::get_length() const
{
    return _length;
}

disvm::runtime::rune_t vm_string_t::get_rune(word_t index) const
{
    if (index < 0 || _length <= index)
        throw index_out_of_range_memory{ 0, _length - 1, index };

    // Determine if local or allocated memory
    auto source = reinterpret_cast<const char *>(_mem.local);
    if (_is_alloc)
        source = reinterpret_cast<const char *>(_mem.alloc);

    // Check if multi-byte or ASCII
    if (_character_size != sizeof(char))
        return reinterpret_cast<const disvm::runtime::rune_t *>(source)[index];
    else
        return static_cast<const disvm::runtime::rune_t>(source[index]);
}

void vm_string_t::set_rune(word_t index, disvm::runtime::rune_t value)
{
    // This function does not adhere to the immutable string contract.
    // Therefore, it is unsafe to call this function if there is more
    // than 1 reference to this string instance.
    assert(get_ref_count() <= 1);

    if (value > utf8::constants::max_supported_codepoint)
        throw invalid_utf8{};

    // Index equal to length is allowed to grow the string
    if (index < 0 || _length < index)
        throw index_out_of_range_memory{ 0, _length, index };

    // Determine if local or allocated memory
    auto source = (_is_alloc) ? _mem.alloc : _mem.local;

    // Release the encoded string for this instance.
    if (_encoded_str != reinterpret_cast<char*>(source))
        free_unmanaged_memory(_encoded_str);

    _encoded_str = nullptr;

    const auto new_character_size = (value > utf8::constants::max_codepoint_ascii) ? sizeof(rune_t) : sizeof(char);
    assert(new_character_size < std::numeric_limits<uint8_t>::max());

    if (new_character_size > _character_size) // Expand the string if the new character is non-ASCII.
    {
        _character_size = static_cast<uint8_t>(new_character_size);
        assert(_character_size == sizeof(rune_t));

        _length_max = (_length == 0) ? static_cast<word_t>(compute_max_length(_mem.local)) : compute_max_length(_length);
        auto new_alloc = alloc_unmanaged_memory<rune_t>(_length_max * _character_size);

        // Copy source into the new alloc.
        copy_characters_to(new_alloc, 0, source, _length);

        // Free memory, if allocated
        if (_is_alloc)
            free_unmanaged_memory(source);

        debug::assign_debug_memory(&_mem, sizeof(_mem));

        _is_alloc = true;
        _mem.alloc = reinterpret_cast<uint8_t *>(new_alloc);
        source = _mem.alloc;
    }
    else if (static_cast<word_t>(_length_max) <= index) // Check if string can accomodate the new character.
    {
        _length_max = (_length == 0) ? static_cast<word_t>(compute_max_length(_mem.local)) : compute_max_length(_length);
        auto new_alloc = alloc_unmanaged_memory<uint8_t>(_length_max * _character_size);
        std::memcpy(new_alloc, source, _length * _character_size);

        // Free memory, if allocated
        if (_is_alloc)
            free_unmanaged_memory(source);

        debug::assign_debug_memory(&_mem, sizeof(_mem));

        _is_alloc = true;
        _mem.alloc = new_alloc;
        source = _mem.alloc;
    }

    // Check if multi-byte or ASCII.
    if (_character_size != sizeof(char))
        reinterpret_cast<rune_t *>(source)[index] = value;
    else
        source[index] = static_cast<uint8_t>(value & utf8::constants::max_codepoint_ascii);

    // Increment the length, if the index matches the length.
    if (_length == index)
        _length++;
}

int vm_string_t::compare_to(const vm_string_t *s) const
{
    return vm_string_t::compare(this, s);
}

const char *vm_string_t::str() const
{
    if (_encoded_str != nullptr)
        return _encoded_str;

    {
        // [PERF] Use a spin-lock instead of a mutex here.
        std::lock_guard<std::mutex> lock{ _encoded_str_lock };

        if (_encoded_str != nullptr)
            return _encoded_str;

        // If the string is ASCII then return the memory
        if (_character_size == sizeof(char))
        {
            auto str = (_is_alloc) ? _mem.alloc : _mem.local;
            if (_length_max <= _length)
            {
                auto tmp = alloc_unmanaged_memory<char>(_length + 1); // +1 for null
                str = static_cast<uint8_t *>(std::memcpy(tmp, str, _length));
            }

            _encoded_str = reinterpret_cast<char *>(str);
            assert(_encoded_str[_length] == '\0');
            return _encoded_str;
        }

        const auto rune_str = (_is_alloc) ? reinterpret_cast<rune_t *>(_mem.alloc) : reinterpret_cast<rune_t *>(_mem.local);
        const auto str_len = (_length * _character_size) + 1; // Plus 1 for a null terminator
        auto encoded_str_local = alloc_unmanaged_memory<char>(str_len);

        auto s = reinterpret_cast<uint8_t *>(encoded_str_local);
        for (auto i = word_t{ 0 }; i < _length; ++i)
        {
            auto curr_rune = rune_str[i];
            s = utf8::encode(curr_rune, s);
        }

        // Ensure the final value is null.
        *s = 0;

        _encoded_str = encoded_str_local;
        return _encoded_str;
    }
}