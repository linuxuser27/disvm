//
// Dis VM
// File: garbage_collector.cpp
// Author: arr
//

#include <cinttypes>
#include <array>
#include <stack>
#include <debug.h>
#include <vm_memory.h>
#include <exceptions.h>
#include "garbage_collector.h"

using disvm::vm_t;

using disvm::debug::component_trace_t;
using disvm::debug::log_level_t;

using disvm::runtime::default_garbage_collector_t;
using disvm::runtime::intrinsic_type_desc;
using disvm::runtime::pointer_t;
using disvm::runtime::type_descriptor_t;
using disvm::runtime::vm_alloc_t;
using disvm::runtime::vm_alloc_callback_t;
using disvm::runtime::vm_garbage_collector_t;
using disvm::runtime::vm_memory_allocator_t;
using disvm::runtime::vm_string_t;
using disvm::runtime::vm_thread_t;
using disvm::runtime::vm_tool_controller_t;

// Empty destructors for vm garbage collector 'interfaces'
vm_garbage_collector_t::~vm_garbage_collector_t()
{
}

std::unique_ptr<vm_garbage_collector_t> disvm::runtime::create_no_op_gc(vm_t &)
{
    class no_op_gc final : public vm_garbage_collector_t
    {
    public:
        vm_memory_allocator_t get_allocator() const
        {
            return{ std::calloc, std::free };
        }

        void track_allocation(vm_alloc_t *) override
        {
        }

        void enum_tracked_allocations(vm_alloc_callback_t) const override
        {
        }

        bool collect(std::vector<std::shared_ptr<const vm_thread_t>>) override
        {
            return true;
        }
    };

    return std::make_unique<no_op_gc>();
}

namespace
{
    enum class gc_colour_t
    {
        white,
        grey,
        black,
    };

    const auto current_colours = std::array<gc_colour_t, 3>{ gc_colour_t::white, gc_colour_t::grey, gc_colour_t::black };
    const auto sweeper_colours = std::array<gc_colour_t, 3>{ gc_colour_t::grey, gc_colour_t::black, gc_colour_t::white };

    constexpr gc_colour_t get_current_colour(const std::size_t epoch)
    {
        return current_colours[epoch % current_colours.size()];
    }

    constexpr gc_colour_t get_sweeper_colour(const std::size_t epoch)
    {
        return sweeper_colours[epoch % sweeper_colours.size()];
    }

    constexpr gc_colour_t get_gc_colour(const vm_alloc_t *a)
    {
        return *reinterpret_cast<const gc_colour_t *>(&a->gc_reserved);
    }

    void set_gc_colour(vm_alloc_t *a, const gc_colour_t c)
    {
        a->gc_reserved = reinterpret_cast<pointer_t>(c);
    }

    class mark_cxt_t final : public std::stack<vm_alloc_t *, std::vector<vm_alloc_t *>>
    {
    public:
        mark_cxt_t()
            : string_type{ intrinsic_type_desc::type<vm_string_t>().get() } // Using raw pointer since it is a built-in type and has unlimited lifetime
            , curr_colour{ gc_colour_t::white }
        { }

        const type_descriptor_t *string_type;
        gc_colour_t curr_colour;

        vm_alloc_t *pop()
        {
            auto a = top();
            std::stack<vm_alloc_t *, std::vector<vm_alloc_t *>>::pop();
            return a;
        }
    };
}

default_garbage_collector_t::default_garbage_collector_t(vm_t &vm)
    : _collection_epoch{ 0 }
    , _mark_cxt{ new mark_cxt_t{} }
    , _vm{ vm }
{
}

default_garbage_collector_t::~default_garbage_collector_t()
{
    std::lock_guard<std::mutex> lock{ _tracking_allocs_lock };
    for (auto a : _tracking_allocs)
        dec_ref_count_and_free(a);

    auto cxt = static_cast<mark_cxt_t *>(_mark_cxt);
    delete cxt;
}

vm_memory_allocator_t default_garbage_collector_t::get_allocator() const
{
    return{ std::calloc, std::free };
}

void default_garbage_collector_t::track_allocation(vm_alloc_t *alloc)
{
    assert(alloc != nullptr);

    if (disvm::debug::is_component_tracing_enabled<component_trace_t::garbage_collector>())
        disvm::debug::log_msg(component_trace_t::garbage_collector, log_level_t::debug, "gc: track: %#" PRIxPTR, alloc);

    alloc->add_ref();
    set_gc_colour(alloc, get_current_colour(_collection_epoch));

    std::lock_guard<std::mutex> lock{ _tracking_allocs_lock };
    _tracking_allocs.emplace_front(alloc);
}

void default_garbage_collector_t::enum_tracked_allocations(vm_alloc_callback_t callback) const
{
    if (callback == nullptr)
        throw vm_system_exception{ "Callback should not be null" };

    std::lock_guard<std::mutex> lock{ _tracking_allocs_lock };
    for (auto a : _tracking_allocs)
        callback(a);
}

namespace
{
    void mark_pointers(pointer_t p, void *cxt)
    {
        auto a = vm_alloc_t::from_allocation(p);

        assert(cxt != nullptr);
        auto c = static_cast<mark_cxt_t *>(cxt);

        // Don't examine vm strings or allocations with the current colour
        if (c->curr_colour != get_gc_colour(a) && a->alloc_type.get() != c->string_type)
            c->push(a);
    }

    void mark(std::vector<std::shared_ptr<const vm_thread_t>> threads, mark_cxt_t &mark_cxt)
    {
        // Get roots from threads
        for (auto &t : threads)
        {
            auto &r = t->get_registers();

            // Current MP
            mark_cxt.push(r.mp_base);

            // Traverse the stack for roots
            for (auto frame = r.stack.peek_frame(); frame != nullptr; frame = frame->prev_frame())
            {
                // Previous MP
                if (frame->prev_module_ref() != nullptr)
                {
                    auto prev_mp = frame->prev_module_ref()->mp_base;
                    mark_cxt.push(prev_mp);
                }

                if (frame->frame_type->map_in_bytes > 0)
                    enum_pointer_fields(*frame->frame_type, frame->base(), mark_pointers, &mark_cxt);
            }
        }

        if (disvm::debug::is_component_tracing_enabled<component_trace_t::garbage_collector>())
            disvm::debug::log_msg(component_trace_t::garbage_collector, log_level_t::debug, "gc: roots found: %" PRIuPTR, mark_cxt.size());

        // Traverse the graph
        while (!mark_cxt.empty())
        {
            auto curr = mark_cxt.pop();
            set_gc_colour(curr, mark_cxt.curr_colour);

            if (curr->alloc_type->map_in_bytes > 0)
                enum_pointer_fields(*curr->alloc_type, curr->get_allocation(), mark_pointers, &mark_cxt);
        }
    }

    void sweep(std::forward_list<vm_alloc_t *> &tracking_allocs, const gc_colour_t sweeper_colour)
    {
        // Remove sweeper colour allocations.
        tracking_allocs.remove_if([sweeper_colour](vm_alloc_t *a)
        {
            const auto colour = get_gc_colour(a);
            const auto remove = (colour == sweeper_colour);
            if (remove)
                dec_ref_count_and_free(a);

            return remove;
        });
    }
}

bool default_garbage_collector_t::collect(std::vector<std::shared_ptr<const vm_thread_t>> threads)
{
    // No need to lock the actual memory allocator since the contract
    // for this function call indicates all VM threads should be blocked.
    std::lock_guard<std::mutex> lock{ _tracking_allocs_lock };

    if (_tracking_allocs.empty())
        return false;

    std::chrono::high_resolution_clock::time_point start;
    const auto log_enabled = debug::is_component_tracing_enabled<component_trace_t::duration>();
    if (log_enabled)
    {
        start = std::chrono::high_resolution_clock::now();
        disvm::debug::log_msg(component_trace_t::duration, log_level_t::debug, "gc: begin: collect");
    }

    auto mark_cxt = static_cast<mark_cxt_t *>(_mark_cxt);
    assert(mark_cxt != nullptr);
    mark_cxt->curr_colour = get_current_colour(_collection_epoch);

    assert(mark_cxt->empty() && "Marking context should be empty before mark phase");
    mark(std::move(threads), *mark_cxt);
    assert(mark_cxt->empty() && "Marking context should be empty after mark phase");

    const auto sweeper_colour = get_sweeper_colour(_collection_epoch);
    sweep(_tracking_allocs, sweeper_colour);

    if (log_enabled)
    {
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start);
        disvm::debug::log_msg(component_trace_t::duration, log_level_t::debug, "gc: end: collect: %lld us", elapsed.count());
    }

    ++_collection_epoch;
    return true;
}