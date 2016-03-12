//
// Dis VM
// File: list.cpp
// Author: arr
//

#include <runtime.h>
#include <vm_memory.h>
#include <debug.h>

using namespace disvm;
using namespace disvm::runtime;

std::shared_ptr<const type_descriptor_t> vm_list_t::type_desc()
{
    return intrinsic_type_desc::type<vm_list_t>();
}

vm_list_t::vm_list_t(std::shared_ptr<const type_descriptor_t> td, vm_list_t *tail)
    : vm_alloc_t(vm_list_t::type_desc())
    , _element_type{ td }
    , _is_alloc{ sizeof(_mem) < td->size_in_bytes }
    , _mem{}
    , _tail{ tail }
{
    assert(_element_type != nullptr);
    if (_tail != nullptr)
    {
        assert(tail->get_element_type() == _element_type);
        _tail->add_ref();
    }

    // If the default list element storage is not enough, allocate more memory.
    if (_is_alloc)
    {
        _mem.alloc = static_cast<pointer_t>(alloc_memory(_element_type->size_in_bytes));
        init_memory(*_element_type, _mem.alloc);
    }
    else
    {
        init_memory(*_element_type, &_mem.local);
    }

    debug::log_msg(debug::component_trace_t::memory, debug::log_level_t::debug, "init: vm list: new\n");
}

vm_list_t::~vm_list_t()
{
    // Remove tail
    dec_ref_count_and_free(_tail);
    _tail = nullptr;

    // Destroy and free the list element, if default element size was surpassed.
    if (_is_alloc)
    {
        destroy_memory(*_element_type, _mem.alloc);
        free_memory(_mem.alloc);
    }
    else
    {
        destroy_memory(*_element_type, &_mem.local);
    }

    debug::assign_debug_pointer(&_mem.alloc);
    debug::log_msg(debug::component_trace_t::memory, debug::log_level_t::debug, "destroy: vm list\n");
}

std::shared_ptr<const type_descriptor_t> vm_list_t::get_element_type() const
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

vm_list_t *vm_list_t::drop_tail()
{
    auto tail = _tail;
    _tail = nullptr;
    tail->release();
    return tail;
}

pointer_t vm_list_t::value() const
{
    if (_is_alloc)
        return _mem.alloc;
    else
        return reinterpret_cast<pointer_t>(&_mem.local);
}