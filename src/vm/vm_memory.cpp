//
// Dis VM
// File: vm_memory.cpp
// Author: arr
//

#include <memory>
#include <atomic>
#include <bitset>
#include <mutex>
#include <limits>
#include <vector>
#include <condition_variable>
#include <cinttypes>
#include <vm_memory.h>
#include <debug.h>
#include <exceptions.h>

using namespace disvm;
using namespace disvm::runtime;

namespace
{
    std::mutex global_lock;
    std::condition_variable release_lock;

    enum class lock_state_t
    {
        none,
        pending,
        active
    };

    std::atomic<uint32_t> allocating_thread_count;
    std::atomic<lock_state_t> lock_state;

    struct safe_alloc final
    {
        safe_alloc()
        {
            allocating_thread_count++;
            if (lock_state != lock_state_t::none)
            {
                allocating_thread_count--;

                std::unique_lock<std::mutex> lock{ global_lock };
                release_lock.wait(lock, [&]{ return lock_state == lock_state_t::none; });

                allocating_thread_count++;
            }
        }

        ~safe_alloc()
        {
            assert(allocating_thread_count > 0);
            allocating_thread_count--;
        }
    };
}

global_alloc_lock_t::global_alloc_lock_t(bool owner)
    : _owner{ owner }
{ }

global_alloc_lock_t::global_alloc_lock_t(global_alloc_lock_t &&other)
    : _owner{ other._owner }
{
    other._owner = false;
}

global_alloc_lock_t::~global_alloc_lock_t()
{
    if (_owner)
    {
        lock_state = lock_state_t::none;
        release_lock.notify_all();
    }
}

// Lock all allocations on the VM heap.
global_alloc_lock_t disvm::runtime::get_global_alloc_lock()
{
    auto expected = lock_state_t::none;

    // Block while trying to get lock
    while (!lock_state.compare_exchange_strong(expected, lock_state_t::pending))
    {
        std::unique_lock<std::mutex> lock{ global_lock };
        release_lock.wait(lock);
    }

    while (allocating_thread_count != 0)
    {
        // Spin-wait for all allocating threads to stop
    }

    // Transition to active lock state
    lock_state = lock_state_t::active;

    return{ true };
}

void *disvm::runtime::alloc_memory(std::size_t amount_in_bytes)
{
    safe_alloc s;

    auto memory = std::malloc(amount_in_bytes);
    if (memory == nullptr)
        throw vm_system_exception{ "Out of memory" };

    if (debug::is_component_tracing_enabled<debug::component_trace_t::memory>())
        debug::log_msg(debug::component_trace_t::memory, debug::log_level_t::debug, "alloc: %#" PRIxPTR " %d\n", memory, amount_in_bytes);

    return memory;
}

void *disvm::runtime::calloc_memory(std::size_t amount_in_bytes)
{
    safe_alloc s;

    auto memory = std::calloc(amount_in_bytes, sizeof(uint8_t));
    if (memory == nullptr)
        throw vm_system_exception{ "Out of memory" };

    if (debug::is_component_tracing_enabled<debug::component_trace_t::memory>())
        debug::log_msg(debug::component_trace_t::memory, debug::log_level_t::debug, "alloc: zeroed: %#" PRIxPTR " %d\n", memory, amount_in_bytes);

    return memory;
}

void disvm::runtime::free_memory(void *memory)
{
    if (memory == nullptr)
        return;

    if (debug::is_component_tracing_enabled<debug::component_trace_t::memory>())
        debug::log_msg(debug::component_trace_t::memory, debug::log_level_t::debug, "free: %#" PRIxPTR "\n", memory);

    std::free(memory);
}

void disvm::runtime::init_memory(const type_descriptor_t &type_desc, void *data)
{
    assert(data != nullptr);
    if (type_desc.size_in_bytes == 0)
        return;

    debug::assign_debug_memory(data, type_desc.size_in_bytes);

    auto memory = reinterpret_cast<word_t *>(data);
    for (auto i = word_t{ 0 }; i < type_desc.map_in_bytes; ++i, memory += 8)
    {
        const auto words8 = type_desc.pointer_map[i];
        if (words8 != 0)
        {
            const auto flags = std::bitset<sizeof(words8) * 8>{ words8 };

            // Enumerating the flags in reverse order so memory access is linear.
            if (flags[7]) memory[0] = runtime_constants::nil;
            if (flags[6]) memory[1] = runtime_constants::nil;
            if (flags[5]) memory[2] = runtime_constants::nil;
            if (flags[4]) memory[3] = runtime_constants::nil;
            if (flags[3]) memory[4] = runtime_constants::nil;
            if (flags[2]) memory[5] = runtime_constants::nil;
            if (flags[1]) memory[6] = runtime_constants::nil;
            if (flags[0]) memory[7] = runtime_constants::nil;
        }
    }
}

namespace
{
    void dec_ref_and_free_pointer_field(pointer_t pointer_field)
    {
        assert(pointer_field != nullptr);
        dec_ref_count_and_free(vm_alloc_t::from_allocation(pointer_field));
    }

    void add_ref_pointer_field(pointer_t pointer_field)
    {
        assert(pointer_field != nullptr);
        vm_alloc_t::from_allocation(pointer_field)->add_ref();
    }
}

void disvm::runtime::destroy_memory(const type_descriptor_t &type_desc, void *data)
{
    enum_pointer_fields(type_desc, data, dec_ref_and_free_pointer_field);

    debug::assign_debug_memory(data, type_desc.size_in_bytes);
}

void disvm::runtime::inc_ref_count_in_memory(const type_descriptor_t &type_desc, void *data)
{
    if (data == nullptr)
        return;

    enum_pointer_fields(type_desc, data, add_ref_pointer_field);
}

void disvm::runtime::dec_ref_count_and_free(vm_alloc_t *alloc)
{
    if (alloc == nullptr)
        return;

    auto current_ref = alloc->release();
    if (current_ref == 0)
        delete alloc;
}

const zero_memory_t vm_alloc_t::zero_memory = {};

void *vm_alloc_t::operator new(std::size_t sz)
{
    return alloc_memory(sz);
}

void vm_alloc_t::operator delete(void *ptr)
{
    free_memory(ptr);
}

vm_alloc_t *vm_alloc_t::allocate(std::shared_ptr<const type_descriptor_t> td)
{
    assert(td != nullptr);
    if (td->size_in_bytes <= 0)
        throw vm_system_exception{ "Invalid dynamic memory allocation size" };

    auto mem = alloc_memory(sizeof(vm_alloc_t) + td->size_in_bytes);
    auto alloc = ::new(mem)vm_alloc_t{ td };

    // Initialize pointer types
    init_memory(*td, alloc->get_allocation());

    debug::log_msg(debug::component_trace_t::memory, debug::log_level_t::debug, "init: vm alloc: %d\n", td->size_in_bytes);
    return alloc;
}

vm_alloc_t *vm_alloc_t::allocate(std::shared_ptr<const type_descriptor_t> td, const zero_memory_t &)
{
    assert(td != nullptr);
    if (td->size_in_bytes <= 0)
        throw vm_system_exception{ "Invalid dynamic memory allocation size" };

    // No need to initialize memory since it is already zeroed out.
    auto mem = calloc_memory(sizeof(vm_alloc_t) + td->size_in_bytes);
    auto alloc = ::new(mem)vm_alloc_t{ td };

    debug::log_msg(debug::component_trace_t::memory, debug::log_level_t::debug, "initz: vm alloc: %d\n", td->size_in_bytes);
    return alloc;
}

vm_alloc_t *vm_alloc_t::copy(const vm_alloc_t &other)
{
    auto alloc_copy = vm_alloc_t::allocate(other.alloc_type);
    
    // Copy the current memory contents of the allocation
    std::memcpy(alloc_copy->get_allocation(), other.get_allocation(), alloc_copy->alloc_type->size_in_bytes);

    // Ref count all dynamic allocations
    inc_ref_count_in_memory(*alloc_copy->alloc_type, alloc_copy->get_allocation());

    debug::log_msg(debug::component_trace_t::memory, debug::log_level_t::debug, "copy: vm alloc: %#" PRIxPTR " %#"  PRIxPTR "\n", &other, alloc_copy);
    return alloc_copy;
}

vm_alloc_t *vm_alloc_t::from_allocation(pointer_t allocation)
{
    if (allocation == nullptr)
        return nullptr;

    return reinterpret_cast<vm_alloc_t *>(reinterpret_cast<uint8_t *>(allocation) - sizeof(vm_alloc_t));
}

vm_alloc_t::vm_alloc_t(std::shared_ptr<const type_descriptor_t> td)
    : alloc_type{ td }
    , gc_reserved{ nullptr }
    , _ref_count{ 1 }
{
    assert(alloc_type != nullptr);
}

vm_alloc_t::~vm_alloc_t()
{
    assert(_ref_count == 0);

    auto finalizer = alloc_type->finalizer;
    if (finalizer != type_descriptor_t::no_finalizer)
        finalizer(this);

    // Free pointer types
    destroy_memory(*alloc_type, get_allocation());
    debug::log_msg(debug::component_trace_t::memory, debug::log_level_t::debug, "destroy: vm alloc\n");
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

pointer_t vm_alloc_t::get_allocation() const
{
    // Remove const on the this pointer so the offset can be computed.
    auto non_const_this = const_cast<vm_alloc_t *>(this);
    return reinterpret_cast<pointer_t>(reinterpret_cast<uint8_t *>(non_const_this)+sizeof(vm_alloc_t));
}

namespace
{
    namespace hidden_type_desc
    {
        const type_descriptor_t byte{ sizeof(byte_t), 0, nullptr, type_descriptor_t::no_finalizer };
        const type_descriptor_t short_word{ sizeof(short_word_t), 0, nullptr, type_descriptor_t::no_finalizer };
        const type_descriptor_t word{ sizeof(word_t), 0, nullptr, type_descriptor_t::no_finalizer };
        const type_descriptor_t short_real{ sizeof(short_real_t), 0, nullptr, type_descriptor_t::no_finalizer };
        const type_descriptor_t real{ sizeof(real_t), 0, nullptr, type_descriptor_t::no_finalizer };
        const type_descriptor_t big{ sizeof(big_t), 0, nullptr, type_descriptor_t::no_finalizer };

        const byte_t pointer_map[] = { 0x80 };
        const type_descriptor_t pointer{ sizeof(pointer_t), (sizeof(pointer_map) / sizeof(pointer_map[0])), pointer_map, type_descriptor_t::no_finalizer };

        const type_descriptor_t vm_array{ 0, 0, nullptr, type_descriptor_t::no_finalizer };
        const type_descriptor_t vm_list{ 0, 0, nullptr, type_descriptor_t::no_finalizer };
        const type_descriptor_t vm_channel{ 0, 0, nullptr, type_descriptor_t::no_finalizer };
        const type_descriptor_t vm_string{ 0, 0, nullptr, type_descriptor_t::no_finalizer };

        const type_descriptor_t vm_module_ref{ 0, 0, nullptr, type_descriptor_t::no_finalizer };
        const type_descriptor_t vm_stack{ 0, 0, nullptr, type_descriptor_t::no_finalizer };
        const type_descriptor_t vm_thread{ 0, 0, nullptr, type_descriptor_t::no_finalizer };

        struct
        {
            void operator()(const type_descriptor_t *)
            {
                // Not allocated on heap
            }
        } deleter;
    }
}

template<>
static std::shared_ptr<const type_descriptor_t> intrinsic_type_desc::type<byte_t>() { return{ &hidden_type_desc::byte, hidden_type_desc::deleter }; }

template<>
static std::shared_ptr<const type_descriptor_t> intrinsic_type_desc::type<short_word_t>() { return{ &hidden_type_desc::short_word, hidden_type_desc::deleter }; }

template<>
static std::shared_ptr<const type_descriptor_t> intrinsic_type_desc::type<word_t>() { return{ &hidden_type_desc::word, hidden_type_desc::deleter }; }

template<>
static std::shared_ptr<const type_descriptor_t> intrinsic_type_desc::type<short_real_t>() { return{ &hidden_type_desc::short_real, hidden_type_desc::deleter }; }

template<>
static std::shared_ptr<const type_descriptor_t> intrinsic_type_desc::type<real_t>() { return{ &hidden_type_desc::real, hidden_type_desc::deleter }; }

template<>
static std::shared_ptr<const type_descriptor_t> intrinsic_type_desc::type<big_t>() { return{ &hidden_type_desc::big, hidden_type_desc::deleter }; }

template<>
static std::shared_ptr<const type_descriptor_t> intrinsic_type_desc::type<pointer_t>() { return{ &hidden_type_desc::pointer, hidden_type_desc::deleter }; }

template<>
static std::shared_ptr<const type_descriptor_t> intrinsic_type_desc::type<vm_array_t>() { return{ &hidden_type_desc::vm_array, hidden_type_desc::deleter }; }

template<>
static std::shared_ptr<const type_descriptor_t> intrinsic_type_desc::type<vm_list_t>() { return{ &hidden_type_desc::vm_list, hidden_type_desc::deleter }; }

template<>
static std::shared_ptr<const type_descriptor_t> intrinsic_type_desc::type<vm_channel_t>() { return{ &hidden_type_desc::vm_channel, hidden_type_desc::deleter }; }

template<>
static std::shared_ptr<const type_descriptor_t> intrinsic_type_desc::type<vm_string_t>() { return{ &hidden_type_desc::vm_string, hidden_type_desc::deleter }; }

template<>
static std::shared_ptr<const type_descriptor_t> intrinsic_type_desc::type<vm_module_ref_t>() { return{ &hidden_type_desc::vm_module_ref, hidden_type_desc::deleter }; }

template<>
static std::shared_ptr<const type_descriptor_t> intrinsic_type_desc::type<vm_stack_t>() { return{ &hidden_type_desc::vm_stack, hidden_type_desc::deleter }; }

template<>
static std::shared_ptr<const type_descriptor_t> intrinsic_type_desc::type<vm_thread_t>() { return{ &hidden_type_desc::vm_thread, hidden_type_desc::deleter }; }

vm_alloc_instance_finalizer_t type_descriptor_t::no_finalizer = nullptr;

std::shared_ptr<const type_descriptor_t> type_descriptor_t::create(const word_t size_in_bytes)
{
    return type_descriptor_t::create(size_in_bytes, 0, nullptr);
}

std::shared_ptr<const type_descriptor_t> type_descriptor_t::create(const word_t size_in_bytes, const std::vector<byte_t> &pointer_map)
{
    assert(pointer_map.size() < static_cast<std::size_t>(std::numeric_limits<word_t>::max()));
    return type_descriptor_t::create(size_in_bytes, static_cast<word_t>(pointer_map.size()), pointer_map.data());
}

std::shared_ptr<const type_descriptor_t> type_descriptor_t::create(
    const word_t size_in_bytes,
    const word_t pointer_map_length,
    const byte_t *pointer_map,
    const vm_alloc_instance_finalizer_t finalizer)
{
    struct
    {
        void operator()(type_descriptor_t *td)
        {
            free_memory(const_cast<byte_t *>(td->pointer_map));
            debug::assign_debug_pointer(const_cast<byte_t **>(&td->pointer_map));

            if (debug::is_component_tracing_enabled<debug::component_trace_t::memory>())
                debug::log_msg(debug::component_trace_t::memory, debug::log_level_t::debug, "destroy: type descriptor\n");

            free_memory(td);
        }
    } deleter;

    auto new_type_memory = alloc_memory(sizeof(type_descriptor_t));

    byte_t *pointer_map_local = nullptr;
    if (pointer_map_length > 0)
    {
        pointer_map_local = alloc_memory<byte_t>(pointer_map_length);
        for (auto i = word_t{ 0 }; i < pointer_map_length; ++i)
            pointer_map_local[i] = pointer_map[i];
    }

    auto new_type = ::new(new_type_memory) type_descriptor_t{ size_in_bytes, pointer_map_length, pointer_map_local, finalizer };
    return std::shared_ptr<type_descriptor_t>{ new_type, deleter };
}

type_descriptor_t::type_descriptor_t(
    word_t size_in_bytes,
    word_t map_in_bytes,
    const byte_t * pointer_map,
    vm_alloc_instance_finalizer_t finalizer)
    : size_in_bytes{ size_in_bytes }
    , map_in_bytes{ map_in_bytes }
    , pointer_map{ pointer_map }
    , finalizer{ finalizer }
{
    if (debug::is_component_tracing_enabled<debug::component_trace_t::memory>())
        debug::log_msg(debug::component_trace_t::memory, debug::log_level_t::debug, "init: type descriptor\n");
}
