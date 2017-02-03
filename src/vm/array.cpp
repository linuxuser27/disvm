//
// Dis VM
// File: array.cpp
// Author: arr
//

#include <runtime.h>
#include <vm_memory.h>
#include <debug.h>
#include <exceptions.h>

using disvm::debug::component_trace_t;
using disvm::debug::log_level_t;

using disvm::runtime::intrinsic_type_desc;
using disvm::runtime::pointer_t;
using disvm::runtime::type_descriptor_t;
using disvm::runtime::vm_alloc_t;
using disvm::runtime::vm_array_t;
using disvm::runtime::vm_string_t;
using disvm::runtime::word_t;

std::shared_ptr<const type_descriptor_t> vm_array_t::type_desc()
{
    return intrinsic_type_desc::type<vm_array_t>();
}

vm_array_t::vm_array_t(std::shared_ptr<const type_descriptor_t> td, word_t length)
    : vm_alloc_t(vm_array_t::type_desc())
    , _arr{ nullptr }
    , _element_type{ td }
    , _length{ length }
    , _original{ nullptr }
{
    assert(_element_type != nullptr);
    assert(_length >= 0);
    _arr = alloc_memory<byte_t>(_length * _element_type->size_in_bytes);
    disvm::debug::log_msg(component_trace_t::memory, log_level_t::debug, "init: vm array: %d", _length);

    // [SPEC] Element memory is already initialized to zero based on the alloc_memory contract
}

vm_array_t::vm_array_t(vm_array_t &original, word_t begin_index, word_t length)
    : vm_alloc_t(vm_array_t::type_desc())
    , _arr{ nullptr }
    , _element_type{ original.get_element_type() }
    , _length{ length }
    , _original{ &original }
{
    assert(0 <= begin_index && (begin_index < _original->get_length() || (begin_index == _original->get_length() && length == 0)));

    if (disvm::debug::is_component_tracing_enabled<component_trace_t::memory>())
        disvm::debug::log_msg(component_trace_t::memory, log_level_t::debug, "init: vm array: slice: %d %d", begin_index, _length);

    // Set the data to reference the original array
    _arr = _original->_arr + (begin_index * _element_type->size_in_bytes);

    // Check if this is a slice of a slice.
    if (_original->_original != nullptr)
    {
        auto actual_original = _original->_original;
        dec_ref_count_and_free(_original);

        _original = actual_original;
        _original->add_ref();
    }
}

vm_array_t::vm_array_t(const vm_string_t *s)
    : vm_alloc_t(vm_array_t::type_desc())
    , _arr{ nullptr }
    , _element_type{ intrinsic_type_desc::type<byte_t>() }
    , _length{ 0 }
    , _original{ nullptr }
{
    if (s != nullptr && s->get_length() > 0)
    {
        auto str = s->str();

        // Ignoring the null terminator.
        _length = std::strlen(str);
        _arr = alloc_memory<byte_t>(_length);
        static_assert(sizeof(byte_t) == sizeof(char), "String characters should be byte size");

        std::memcpy(_arr, str, _length);
    }

    if (disvm::debug::is_component_tracing_enabled<component_trace_t::memory>())
        disvm::debug::log_msg(component_trace_t::memory, log_level_t::debug, "init: vm array: vm string: %d", _length);
}

vm_array_t::~vm_array_t()
{
    if (_original != nullptr)
    {
        // If the array is a sliced (based off of another array) then the
        // inner pointer simply points into the original array.
        debug::assign_debug_pointer(&_arr);

        dec_ref_count_and_free(_original);
        debug::assign_debug_pointer(&_original);

        disvm::debug::log_msg(component_trace_t::memory, log_level_t::debug, "destroy: vm array: slice");
    }
    else
    {
        // Free the array
        auto arr_local = _arr;
        const auto array_len = _length;
        const auto &element_type = *_element_type;
        disvm::debug::log_msg(component_trace_t::memory, log_level_t::debug, "begin: destroy: elements: %d", array_len);
        for (word_t i = 0; i < array_len; ++i)
        {
            destroy_memory(element_type, arr_local);
            arr_local += element_type.size_in_bytes;
        }
        disvm::debug::log_msg(component_trace_t::memory, log_level_t::debug, "end: destroy: elements: %d", array_len);

        // Free array memory
        free_memory(_arr);

        debug::assign_debug_pointer(&_arr);
        disvm::debug::log_msg(component_trace_t::memory, log_level_t::debug, "destroy: vm array");
    }

}

std::shared_ptr<const type_descriptor_t> vm_array_t::get_element_type() const
{
    return _element_type;
}

word_t vm_array_t::get_length() const
{
    return _length;
}

pointer_t vm_array_t::at(word_t index) const
{
    if (index < 0 || _length <= index)
        throw index_out_of_range_memory{ 0, _length - 1, index };

    return reinterpret_cast<pointer_t>(_arr + (index * _element_type->size_in_bytes));
}

void vm_array_t::copy_from(const vm_array_t &source, word_t this_begin_index)
{
    if (this_begin_index < 0 || _length < (this_begin_index + source._length))
        throw out_of_range_memory{};

    if (!source._element_type->is_equal(_element_type.get()))
        throw type_violation{};

    auto dest_arr = _arr + (this_begin_index * _element_type->size_in_bytes);

    // If we have reference types, increment references in the
    // source array and decrement them in the destination array.
    if (_element_type->map_in_bytes != 0)
    {
        // [PERF] There is a chance this copy is within the same array, therefore
        // a check could be made to not ref count memory that is going to be moved.
        const auto &type_desc = *_element_type;
        auto source_array_element = source._arr;
        auto dest_array_element = dest_arr;
        for (auto i = word_t{ 0 }; i < source._length; ++i)
        {
            inc_ref_count_in_memory(type_desc, source_array_element);
            source_array_element += type_desc.size_in_bytes;

            dec_ref_count_in_memory(type_desc, dest_array_element);
            dest_array_element += type_desc.size_in_bytes;
        }
    }

    // Copy data from within the same array is permitted.
    std::memmove(dest_arr, source._arr, (source._length * _element_type->size_in_bytes));
}