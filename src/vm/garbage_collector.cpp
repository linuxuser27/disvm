//
// Dis VM
// File: garbage_collector.cpp
// Author: arr
//

#include <cinttypes>
#include <array>
#include <queue>
#include <debug.h>
#include <vm_memory.h>
#include <exceptions.h>
#include "garbage_collector.h"

using namespace disvm;
using namespace disvm::runtime;

// Empty destructors for vm garbage collector 'interfaces'
vm_garbage_collector_t::~vm_garbage_collector_t()
{
}

std::unique_ptr<vm_garbage_collector_t> disvm::runtime::create_no_op_gc(vm_t &)
{
    class no_op_gc : public vm_garbage_collector_t
    {
    public:
        void track_allocation(vm_alloc_t *) override
        {
        }

        void enum_tracked_allocations(vm_alloc_callback_t) const override
        {
        }

        bool collect(std::vector<std::shared_ptr<const vm_thread_t>> &) override
        {
            return true;
        }
    };

    return std::make_unique<no_op_gc>();
}

default_garbage_collector_t::default_garbage_collector_t(vm_t &vm)
    : _collection_epoch{ 0 }
    , _vm{ vm }
{
}

default_garbage_collector_t::~default_garbage_collector_t()
{
    std::lock_guard<std::mutex> lock{ _tracking_allocs_lock };
    for (auto a : _tracking_allocs)
        dec_ref_count_and_free(a);
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

    gc_colour_t get_current_colour(const std::size_t epoch)
    {
        return current_colours[epoch % current_colours.size()];
    }

    gc_colour_t get_sweeper_colour(const std::size_t epoch)
    {
        return sweeper_colours[epoch % sweeper_colours.size()];
    }

    gc_colour_t get_gc_colour(const vm_alloc_t *a)
    {
        return *reinterpret_cast<const gc_colour_t *>(&a->gc_reserved);
    }

    void set_gc_colour(vm_alloc_t *a, const gc_colour_t c)
    {
        a->gc_reserved = reinterpret_cast<pointer_t>(c);
    }
}

void default_garbage_collector_t::track_allocation(vm_alloc_t *alloc)
{
    assert(alloc != nullptr);

    if (debug::is_component_tracing_enabled<debug::component_trace_t::garbage_collector>())
        debug::log_msg(debug::component_trace_t::garbage_collector, debug::log_level_t::debug, "gc: track: %#" PRIxPTR "\n", alloc);

    alloc->add_ref();
    set_gc_colour(alloc, get_current_colour(_collection_epoch));

    std::lock_guard<std::mutex> lock{ _tracking_allocs_lock };
    _tracking_allocs.push_back(alloc);
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
    void mark(const std::vector<std::shared_ptr<const vm_thread_t>> &threads, const gc_colour_t curr_colour)
    {
        std::queue<vm_alloc_t *> allocs;

        auto add_pointer_to_queue = std::function<void(pointer_t p)>{ [&allocs](pointer_t p) -> void
        {
            auto a = vm_alloc_t::from_allocation(p);
            allocs.push(a);
        } };

        // Get roots from threads
        for (const auto &t : threads)
        {
            const auto &r = t->get_registers();

            // Current MP
            allocs.push(r.mp_base);

            // Traverse the stack for roots
            for (auto frame = r.stack.peek_frame(); frame != nullptr; frame = frame->prev_frame())
            {
                // Previous MP
                if (frame->prev_module_ref() != nullptr)
                {
                    auto prev_mp = frame->prev_module_ref()->mp_base;
                    allocs.push(prev_mp);
                }

                if (frame->frame_type->map_in_bytes == 0)
                    continue;

                enum_pointer_fields(*frame->frame_type, frame->base(), add_pointer_to_queue);
            }
        }

        const auto log_enabled = debug::is_component_tracing_enabled<debug::component_trace_t::garbage_collector>();
        if (log_enabled)
        {
            debug::log_msg(debug::component_trace_t::garbage_collector, debug::log_level_t::debug, "gc: roots found: %" PRIuPTR "\n", allocs.size());

            // If logging is enabled update the callback with logging
            add_pointer_to_queue = std::function<void(pointer_t p)>{ [&allocs](pointer_t p) -> void
            {
                auto a = vm_alloc_t::from_allocation(p);
                debug::log_msg(debug::component_trace_t::garbage_collector, debug::log_level_t::debug, "\tref: %#" PRIxPTR "\n", a);
                allocs.push(a);
            } };
        }

        // Traverse the graph
        while (!allocs.empty())
        {
            auto curr = allocs.front();
            allocs.pop();

            if (log_enabled)
                debug::log_msg(debug::component_trace_t::garbage_collector, debug::log_level_t::debug, "gc: mark: alloc: %#" PRIxPTR "\n", curr);

            if (curr->alloc_type->map_in_bytes > 0)
                enum_pointer_fields(*curr->alloc_type, curr->get_allocation(), add_pointer_to_queue);

            set_gc_colour(curr, curr_colour);
        }
    }

    void sweep(std::deque<vm_alloc_t *> &allocs, const gc_colour_t sweeper_colour)
    {
        auto allocs_local = std::move(allocs);

        auto freed = uint32_t{ 0 };
        for (auto a : allocs_local)
        {
            const auto colour = get_gc_colour(a);
            if (colour == sweeper_colour)
            {
                dec_ref_count_and_free(a);
                freed++;
                continue;
            }

            allocs.push_back(a);
        }

        if (debug::is_component_tracing_enabled<debug::component_trace_t::garbage_collector>())
            debug::log_msg(debug::component_trace_t::garbage_collector, debug::log_level_t::debug, "gc: sweep: %u\n", freed);
    }
}

bool default_garbage_collector_t::collect(std::vector<std::shared_ptr<const vm_thread_t>> &threads)
{
    // No need to lock the actual memory allocator since the contract
    // for this function call indicates all VM threads should be blocked.
    std::lock_guard<std::mutex> lock{ _tracking_allocs_lock };

    if (threads.empty() || _tracking_allocs.empty())
        return false;

    const auto log_enabled = debug::is_component_tracing_enabled<debug::component_trace_t::garbage_collector>();
    if (log_enabled)
        debug::log_msg(debug::component_trace_t::garbage_collector, debug::log_level_t::debug, "gc: begin: collect\n");

    const auto curr_colour = get_current_colour(_collection_epoch);
    mark(threads, curr_colour);

    const auto sweeper_colour = get_sweeper_colour(_collection_epoch);
    sweep(_tracking_allocs, sweeper_colour);

    if (log_enabled)
        debug::log_msg(debug::component_trace_t::garbage_collector, debug::log_level_t::debug, "gc: end: collect\n");

    ++_collection_epoch;
    return true;
}