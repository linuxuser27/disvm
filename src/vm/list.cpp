//
// Dis VM
// File: list.cpp
// Author: arr
//

#include <runtime.hpp>
#include <exceptions.hpp>
#include <vm_memory.hpp>
#include <debug.hpp>

using disvm::debug::component_trace_t;
using disvm::debug::log_level_t;

using disvm::runtime::managed_ptr_t;
using disvm::runtime::intrinsic_type_desc;
using disvm::runtime::pointer_t;
using disvm::runtime::type_descriptor_t;
using disvm::runtime::vm_alloc_t;
using disvm::runtime::vm_list_t;
using disvm::runtime::word_t;

managed_ptr_t<const type_descriptor_t> vm_list_t::type_desc()
{
    return intrinsic_type_desc::type<vm_list_t>();
}

vm_list_t::vm_list_t(managed_ptr_t<const type_descriptor_t> td, vm_list_t *tail)
    : vm_alloc_t(vm_list_t::type_desc())
    , _is_alloc{ sizeof(_mem) < td->size_in_bytes }
    , _mem{}
    , _element_type{ std::move(td) }
    , _tail{ nullptr }
{
    assert(_element_type.is_valid());
    set_tail(tail);

    // If the default list element storage is not enough, allocate more memory.
    // [SPEC] Element memory is already initialized to zero based on the alloc_memory contract
    if (_is_alloc)
        _mem.alloc = static_cast<pointer_t>(alloc_memory(_element_type->size_in_bytes, vm_memory_type_t::unmanaged));

    if (disvm::debug::is_component_tracing_enabled<component_trace_t::memory>())
        disvm::debug::log_msg(component_trace_t::memory, log_level_t::debug, "init: vm list");
}

vm_list_t::~vm_list_t()
{
    // Destroy and free the list element, if default element size was surpassed.
    if (_is_alloc)
    {
        destroy_memory(*_element_type, _mem.alloc);
        free_unmanaged_memory(_mem.alloc);
    }
    else
    {
        destroy_memory(*_element_type, &_mem.local);
    }

    debug::assign_debug_pointer(&_mem.alloc);

    // Remove tail
    dec_ref_count_and_free(_tail);
    debug::assign_debug_pointer(&_tail);

    if (disvm::debug::is_component_tracing_enabled<component_trace_t::memory>())
        disvm::debug::log_msg(component_trace_t::memory, log_level_t::debug, "destroy: vm list");
}

managed_ptr_t<const type_descriptor_t> vm_list_t::get_element_type() const
{
    return _element_type;
}

word_t vm_list_t::get_length() const
{
    auto len = word_t{ 1 };
    auto list = _tail;
    while (list != nullptr)
    {
        len++;
        list = list->get_tail();
    }

    return len;
}

vm_list_t *vm_list_t::get_tail() const
{
    return _tail;
}

void vm_list_t::set_tail(vm_list_t *new_tail)
{
    if (new_tail != nullptr)
    {
        if (!new_tail->get_element_type()->is_equal(_element_type))
            throw type_violation{};

        new_tail->add_ref();
    }

    auto prev_tail = _tail;
    _tail = new_tail;

    dec_ref_count_and_free(prev_tail);
}

pointer_t vm_list_t::value() const
{
    if (_is_alloc)
        return _mem.alloc;
    else
        return reinterpret_cast<pointer_t>(&_mem.local);
}