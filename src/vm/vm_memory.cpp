//
// Dis VM
// File: vm_memory.cpp
// Author: arr
//

#include <memory>
#include <atomic>
#include <bitset>
#include <cstdlib>
#include <mutex>
#include <limits>
#include <vector>
#include <condition_variable>
#include <cinttypes>
#include <vm_memory.hpp>
#include <debug.hpp>
#include <exceptions.hpp>
#include "utils.hpp"

using disvm::vm_t;

using disvm::debug::component_trace_t;
using disvm::debug::log_level_t;

using disvm::runtime::managed_ptr_t;
using disvm::runtime::type_descriptor_t;
using disvm::runtime::intrinsic_type_desc;
using disvm::runtime::byte_t;
using disvm::runtime::short_word_t;
using disvm::runtime::word_t;
using disvm::runtime::short_real_t;
using disvm::runtime::real_t;
using disvm::runtime::big_t;
using disvm::runtime::pointer_t;
using disvm::runtime::vm_thread_t;
using disvm::runtime::vm_alloc_t;
using disvm::runtime::vm_array_t;
using disvm::runtime::vm_list_t;
using disvm::runtime::vm_string_t;
using disvm::runtime::vm_channel_t;
using disvm::runtime::vm_module_t;
using disvm::runtime::vm_module_ref_t;
using disvm::runtime::vm_stack_t;
using disvm::runtime::vm_system_exception;
using disvm::runtime::vm_alloc_instance_finalizer_t;

// Defined in vm.cpp
extern thread_local disvm::runtime::vm_memory_alloc_t vm_memory_alloc;
extern thread_local disvm::runtime::vm_memory_free_t vm_memory_free_unmanaged;
extern thread_local disvm::runtime::vm_memory_free_t vm_memory_free_managed;

void *disvm::runtime::alloc_memory(std::size_t amount_in_bytes, vm_memory_type_t type)
{
    auto memory = vm_memory_alloc(amount_in_bytes, type);
    if (memory == nullptr)
        throw vm_system_exception{ "Out of memory" };

    if (disvm::debug::is_component_tracing_enabled<component_trace_t::memory>())
        disvm::debug::log_msg(component_trace_t::memory, log_level_t::debug, "alloc: %#" PRIxPTR " %d, type: %d", memory, amount_in_bytes, type);

    return memory;
}

void disvm::runtime::free_unmanaged_memory(void *memory)
{
    vm_memory_free_unmanaged(memory);

    if (memory != nullptr && debug::is_component_tracing_enabled<component_trace_t::memory>())
        disvm::debug::log_msg(component_trace_t::memory, log_level_t::debug, "free: %#" PRIxPTR, memory);
}

void disvm::runtime::free_rooted_memory(void* memory)
{
    vm_memory_free_managed(memory);

    if (memory != nullptr && debug::is_component_tracing_enabled<component_trace_t::memory>())
        disvm::debug::log_msg(component_trace_t::memory, log_level_t::debug, "free rooted: %#" PRIxPTR, memory);
}

void disvm::runtime::init_memory(const type_descriptor_t &type_desc, void *data)
{
    assert(data != nullptr);
    if (type_desc.size_in_bytes == 0)
        return;

    // [SPEC] The Dis VM spec does not guarantee non-pointer offsets
    // to be initialized to zero, but it was found to be an assumption
    // when getting the limbo compiler run on this VM. Therefore this
    // implemetation will make the strong guarantee that memory will
    // always be 0 initialized.
    std::memset(data, 0, type_desc.size_in_bytes);
}

namespace
{
    void dec_ref_and_free_pointer_field(pointer_t *pointer_field)
    {
        assert(pointer_field != nullptr && *pointer_field != nullptr);
        dec_ref_count_and_free(vm_alloc_t::from_allocation(*pointer_field));
    }

    void add_ref_pointer_field(pointer_t *pointer_field)
    {
        assert(pointer_field != nullptr && *pointer_field != nullptr);
        vm_alloc_t::from_allocation(*pointer_field)->add_ref();
    }
}

void disvm::runtime::destroy_memory(const type_descriptor_t &type_desc, void *data)
{
    dec_ref_count_in_memory(type_desc, data);
    debug::assign_debug_memory(data, type_desc.size_in_bytes);
}

void disvm::runtime::inc_ref_count_in_memory(const type_descriptor_t &type_desc, void *data)
{
    if (type_desc.size_in_bytes == 0)
        return;

    enum_pointer_fields(type_desc, data, add_ref_pointer_field);
}

void disvm::runtime::dec_ref_count_in_memory(const type_descriptor_t &type_desc, void *data)
{
    if (type_desc.size_in_bytes == 0)
        return;

    enum_pointer_fields(type_desc, data, dec_ref_and_free_pointer_field);
}

void disvm::runtime::dec_ref_count_and_free(vm_alloc_t *alloc)
{
    if (alloc == nullptr)
        return;

    auto current_ref = alloc->release();
    if (current_ref == 0)
        delete alloc;
}

bool disvm::runtime::is_offset_pointer(const type_descriptor_t &type_desc, std::size_t offset)
{
    if (type_desc.size_in_bytes == 0)
        return false;

    assert(offset < static_cast<std::size_t>(std::numeric_limits<word_t>::max()));
    const auto byte_offset = static_cast<word_t>(offset / 8);
    auto map = type_desc.get_map();
    if (byte_offset < type_desc.map_in_bytes)
    {
        const auto words8 = map[byte_offset];
        assert(sizeof(words8) == 1);
        if (words8 != 0)
        {
            const auto flags = std::bitset<sizeof(words8) * 8>{ words8 };

            // Highest order bit is the first field
            const auto bit_offset = 7 - (offset % 8);
            return flags[bit_offset];
        }
    }

    return false;
}

void *vm_alloc_t::operator new(std::size_t sz)
{
    return alloc_memory(sz, vm_memory_type_t::managed);
}

void vm_alloc_t::operator delete(void *ptr)
{
    vm_memory_free_managed(ptr);
}

vm_alloc_t *vm_alloc_t::allocate(managed_ptr_t<const type_descriptor_t> td)
{
    assert(td.is_valid());
    const auto type_size_in_bytes = td->size_in_bytes;
    if (type_size_in_bytes <= 0)
        throw vm_system_exception{ "Invalid dynamic memory allocation size" };

    auto mem = alloc_memory(sizeof(vm_alloc_t) + type_size_in_bytes, vm_memory_type_t::managed);
    auto alloc = ::new(mem)vm_alloc_t{ std::move(td) };

    if (disvm::debug::is_component_tracing_enabled<component_trace_t::memory>())
        disvm::debug::log_msg(component_trace_t::memory, log_level_t::debug, "init: vm alloc: %d", type_size_in_bytes);

    return alloc;
}

vm_alloc_t *vm_alloc_t::copy(const vm_alloc_t &other)
{
    auto alloc_copy = vm_alloc_t::allocate(other.alloc_type);

    // Copy the current memory contents of the allocation
    std::memcpy(alloc_copy->get_allocation(), other.get_allocation(), alloc_copy->alloc_type->size_in_bytes);

    // Ref count all dynamic allocations
    inc_ref_count_in_memory(*alloc_copy->alloc_type, alloc_copy->get_allocation());

    disvm::debug::log_msg(component_trace_t::memory, log_level_t::debug, "copy: vm alloc: %#" PRIxPTR " %#"  PRIxPTR, &other, alloc_copy);
    return alloc_copy;
}

vm_alloc_t::vm_alloc_t(managed_ptr_t<const type_descriptor_t> td)
    : alloc_type{ std::move(td) }
    , gc_reserved{ nullptr }
    , _ref_count{ 1 }
{
    assert(alloc_type.is_valid());
}

vm_alloc_t::~vm_alloc_t()
{
#ifndef NDEBUG
    if (_ref_count != 0)
        disvm::debug::log_msg(component_trace_t::memory, log_level_t::warning,
            "vm alloc being destroy with non-zero reference count. "
            "This could be okay if this is happening due to frame unwinding from an exception");
#endif

    auto finalizer = alloc_type->finalizer;
    if (finalizer != type_descriptor_t::no_finalizer)
        finalizer(this);

    // Free pointer types
    destroy_memory(*alloc_type, get_allocation());

    if (disvm::debug::is_component_tracing_enabled<component_trace_t::memory>())
        disvm::debug::log_msg(component_trace_t::memory, log_level_t::debug, "destroy: vm alloc");
}

std::size_t vm_alloc_t::add_ref()
{
    assert(_ref_count > 0);

    auto prev_value = _ref_count.fetch_add(1, std::memory_order::memory_order_relaxed);
    return (prev_value + 1);
}

std::size_t vm_alloc_t::release()
{
    assert(_ref_count > 0);

    auto prev_value = _ref_count.fetch_sub(1, std::memory_order::memory_order_relaxed);
    return (prev_value - 1);
}

std::size_t vm_alloc_t::get_ref_count() const
{
    return _ref_count;
}

namespace
{
    namespace hidden_type_desc
    {
#define TYPE_DESC(N,S,MS,M,F) const type_descriptor_t N{ S, MS, M, F, #N }

        TYPE_DESC(byte, sizeof(byte_t), 0, nullptr, type_descriptor_t::no_finalizer);
        TYPE_DESC(short_word, sizeof(short_word_t), 0, nullptr, type_descriptor_t::no_finalizer);
        TYPE_DESC(word, sizeof(word_t), 0, nullptr, type_descriptor_t::no_finalizer);
        TYPE_DESC(short_real, sizeof(short_real_t), 0, nullptr, type_descriptor_t::no_finalizer);
        TYPE_DESC(real, sizeof(real_t), 0, nullptr, type_descriptor_t::no_finalizer);
        TYPE_DESC(big, sizeof(big_t), 0, nullptr, type_descriptor_t::no_finalizer);

        const byte_t pointer_map[] = { 0x80 };
        TYPE_DESC(pointer, sizeof(pointer_t), disvm::util::array_length(pointer_map), pointer_map, type_descriptor_t::no_finalizer);

        TYPE_DESC(type_descriptor, 0, 0, nullptr, type_descriptor_t::no_finalizer);

        TYPE_DESC(vm_array, 0, 0, nullptr, type_descriptor_t::no_finalizer);
        TYPE_DESC(vm_list, 0, 0, nullptr, type_descriptor_t::no_finalizer);
        TYPE_DESC(vm_channel, 0, 0, nullptr, type_descriptor_t::no_finalizer);
        TYPE_DESC(vm_string, 0, 0, nullptr, type_descriptor_t::no_finalizer);

        TYPE_DESC(vm_module_ref, 0, 0, nullptr, type_descriptor_t::no_finalizer);
        TYPE_DESC(vm_stack, 0, 0, nullptr, type_descriptor_t::no_finalizer);
        TYPE_DESC(vm_thread, 0, 0, nullptr, type_descriptor_t::no_finalizer);

        TYPE_DESC(vm_module, 0, 0, nullptr, type_descriptor_t::no_finalizer);

#undef TYPE_DESC
    }
}

template<>
managed_ptr_t<const type_descriptor_t> intrinsic_type_desc::type<byte_t>()
{
    return managed_ptr_t<const type_descriptor_t>{ &hidden_type_desc::byte };
}

template<>
managed_ptr_t<const type_descriptor_t> intrinsic_type_desc::type<short_word_t>()
{
    return managed_ptr_t<const type_descriptor_t>{ &hidden_type_desc::short_word };
}

template<>
managed_ptr_t<const type_descriptor_t> intrinsic_type_desc::type<word_t>()
{
    return managed_ptr_t<const type_descriptor_t>{ &hidden_type_desc::word };
}

template<>
managed_ptr_t<const type_descriptor_t> intrinsic_type_desc::type<short_real_t>()
{
    return managed_ptr_t<const type_descriptor_t>{ &hidden_type_desc::short_real };
}

template<>
managed_ptr_t<const type_descriptor_t> intrinsic_type_desc::type<real_t>()
{
    return managed_ptr_t<const type_descriptor_t>{ &hidden_type_desc::real };
}

template<>
managed_ptr_t<const type_descriptor_t> intrinsic_type_desc::type<big_t>()
{
    return managed_ptr_t<const type_descriptor_t>{ &hidden_type_desc::big };
}

template<>
managed_ptr_t<const type_descriptor_t> intrinsic_type_desc::type<pointer_t>()
{
    return managed_ptr_t<const type_descriptor_t>{ &hidden_type_desc::pointer };
}

template<>
managed_ptr_t<const type_descriptor_t> intrinsic_type_desc::type<type_descriptor_t>()
{
    return managed_ptr_t<const type_descriptor_t>{ &hidden_type_desc::type_descriptor };
}

template<>
managed_ptr_t<const type_descriptor_t> intrinsic_type_desc::type<vm_array_t>()
{
    return managed_ptr_t<const type_descriptor_t>{ &hidden_type_desc::vm_array };
}

template<>
managed_ptr_t<const type_descriptor_t> intrinsic_type_desc::type<vm_list_t>()
{
    return managed_ptr_t<const type_descriptor_t>{ &hidden_type_desc::vm_list };
}

template<>
managed_ptr_t<const type_descriptor_t> intrinsic_type_desc::type<vm_channel_t>()
{
    return managed_ptr_t<const type_descriptor_t>{ &hidden_type_desc::vm_channel };
}

template<>
managed_ptr_t<const type_descriptor_t> intrinsic_type_desc::type<vm_string_t>()
{
    return managed_ptr_t<const type_descriptor_t>{ &hidden_type_desc::vm_string };
}

template<>
managed_ptr_t<const type_descriptor_t> intrinsic_type_desc::type<vm_module_ref_t>()
{
    return managed_ptr_t<const type_descriptor_t>{ &hidden_type_desc::vm_module_ref };
}

template<>
managed_ptr_t<const type_descriptor_t> intrinsic_type_desc::type<vm_stack_t>()
{
    return managed_ptr_t<const type_descriptor_t>{ &hidden_type_desc::vm_stack };
}

template<>
managed_ptr_t<const type_descriptor_t> intrinsic_type_desc::type<vm_thread_t>()
{
    return managed_ptr_t<const type_descriptor_t>{ &hidden_type_desc::vm_thread };
}

template<>
managed_ptr_t<const type_descriptor_t> intrinsic_type_desc::type<vm_module_t>()
{
    return managed_ptr_t<const type_descriptor_t>{ &hidden_type_desc::vm_module };
}

vm_alloc_instance_finalizer_t type_descriptor_t::no_finalizer = nullptr;

managed_ptr_t<const type_descriptor_t> type_descriptor_t::type_desc()
{
    return intrinsic_type_desc::type<type_descriptor_t>();
}

type_descriptor_t::type_descriptor_t(
    word_t size_in_bytes,
    word_t map_in_bytes,
    const byte_t * pointer_map,
    vm_alloc_instance_finalizer_t finalizer,
    const char *debug_name)
    : vm_alloc_t(type_desc())
    , size_in_bytes{ size_in_bytes }
    , map_in_bytes{ map_in_bytes }
    , _pointer_map{}
    , finalizer{ finalizer }
#ifndef NDEBUG
    , debug_type_name{ debug_name }
#endif
{
    if (map_in_bytes == 0)
    {
        // nop - map is zero initialized above
    }
    else if (map_in_bytes <= sizeof(_pointer_map))
    {
        std::memcpy(_pointer_map.local, pointer_map, map_in_bytes);
    }
    else
    {
        _pointer_map.alloc = alloc_unmanaged_memory<byte_t>(map_in_bytes);
        for (auto i = word_t{ 0 }; i < map_in_bytes; ++i)
            _pointer_map.alloc[i] = pointer_map[i];
    }

    (void)debug_name;
    if (disvm::debug::is_component_tracing_enabled<component_trace_t::memory>())
        disvm::debug::log_msg(component_trace_t::memory, log_level_t::debug, "init: type descriptor");
}

type_descriptor_t::~type_descriptor_t()
{
    if (map_in_bytes > sizeof(_pointer_map))
    {
        free_unmanaged_memory(_pointer_map.alloc);
        debug::assign_debug_pointer(reinterpret_cast<byte_t**>(&_pointer_map));
    }

    if (disvm::debug::is_component_tracing_enabled<component_trace_t::memory>())
        disvm::debug::log_msg(component_trace_t::memory, log_level_t::debug, "destroy: type descriptor");
}

bool type_descriptor_t::is_equal(managed_ptr_t<const type_descriptor_t> const &other) const
{
    if (reinterpret_cast<std::intptr_t>(this) == static_cast<std::intptr_t>(other))
        return true;

    bool equal = size_in_bytes == other->size_in_bytes
        && map_in_bytes == other->map_in_bytes
        && 0 == std::memcmp(get_map(), other->get_map(), map_in_bytes)
        && finalizer == other->finalizer;

    return equal;
}

const byte_t* type_descriptor_t::get_map() const
{
    return map_in_bytes <= sizeof(_pointer_map) ? _pointer_map.local : _pointer_map.alloc;
}
