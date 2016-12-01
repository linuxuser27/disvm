//
// Dis VM
// File: stack.cpp
// Author: arr
//

#include <cinttypes>
#include <runtime.h>
#include <builtin_module.h>
#include <vm_memory.h>
#include <debug.h>
#include <exceptions.h>

using namespace disvm;
using namespace disvm::runtime;

namespace
{
    struct vm_stack_page
    {
        vm_stack_page *prev_page;
        std::uintptr_t page_limit_addr;
        vm_frame_t *page_top_frame; // Do not free. This is used as a tracking pointer.

        void *stack_data() const
        {
            // The beginning of the stack data is at the end of the struct.
            return reinterpret_cast<uint8_t *>(const_cast<vm_stack_page *>(this)) + sizeof(vm_stack_page);
        }

        bool contains_frame(const vm_frame_t *frame) const
        {
            return reinterpret_cast<std::uintptr_t>(stack_data()) <= reinterpret_cast<std::uintptr_t>(frame) && reinterpret_cast<std::uintptr_t>(frame) < page_limit_addr;
        }
    };

    struct vm_stack_layout final : public vm_alloc_t
    {
        static std::shared_ptr<const type_descriptor_t> type_desc()
        {
            static auto td = type_descriptor_t::create(0);
            return td;
        }

        vm_stack_layout(std::size_t stack_page_size)
            : vm_alloc_t(type_desc())
            , stack_page_size{ stack_page_size }
            , top_frame{ nullptr }
            , top_page{ nullptr }
        {
            new_page();
        }

        ~vm_stack_layout()
        {
            assert(top_page != nullptr);
            free_memory(top_page);
        }

        const std::size_t stack_page_size;

        vm_stack_page *top_page;
        vm_frame_t *top_frame; // Do not free. This is used as a tracking pointer.

        // Create a new 'top page' and return stack data address
        void *new_page()
        {
            auto current_stack_page = top_page;
            top_page = calloc_memory<vm_stack_page>(sizeof(vm_stack_page) + stack_page_size);

            // Initialize the stack allocation.
            top_page->prev_page = current_stack_page;

            // The top frame member is only for tracking and so is initialized to null.
            top_page->page_top_frame = nullptr;
            top_page->page_limit_addr = reinterpret_cast<std::uintptr_t>(top_page)+stack_page_size;

            if (debug::is_component_tracing_enabled<debug::component_trace_t::memory>())
                debug::log_msg(debug::component_trace_t::memory, debug::log_level_t::debug, "alloc: vm stack alloc: %#" PRIxPTR " %#"  PRIxPTR "\n", top_page, top_page->page_limit_addr);

            return top_page->stack_data();
        }

        // Drop the current page and move to the previous one
        void drop_page()
        {
            auto current_stack_page = top_page->prev_page;
            assert(current_stack_page != nullptr);

            free_memory(top_page);

            if (debug::is_component_tracing_enabled<debug::component_trace_t::memory>())
                debug::log_msg(debug::component_trace_t::memory, debug::log_level_t::debug, "free: vm stack alloc\n");

            // Set the top page
            top_page = current_stack_page;
        }
    };
}

vm_frame_t::vm_frame_t(std::shared_ptr<const type_descriptor_t> &td)
    : frame_type{ std::move(td) }
{
    assert(frame_type != nullptr);

    // Initialize the new frame
    init_memory(*frame_type, base());

    prev_pc() = runtime_constants::invalid_program_counter;
    prev_frame() = nullptr;
    prev_module_ref() = nullptr;

    if (debug::is_component_tracing_enabled<debug::component_trace_t::memory>())
        debug::log_msg(debug::component_trace_t::memory, debug::log_level_t::debug, "init: vm frame: %#" PRIxPTR " %d\n", this, frame_type->size_in_bytes);
}

vm_frame_t::~vm_frame_t()
{
    dec_ref_count_and_free(prev_module_ref());

    destroy_memory(*frame_type, base());

    if (debug::is_component_tracing_enabled<debug::component_trace_t::memory>())
        debug::log_msg(debug::component_trace_t::memory, debug::log_level_t::debug, "destroy: vm frame: %#" PRIxPTR "\n", this);
}

void vm_frame_t::copy_frame_contents(const vm_frame_t &other)
{
    auto this_frame_base = reinterpret_cast<uint8_t *>(base());
    auto this_frame_contents = this_frame_base + sizeof(vm_frame_base_alloc_t);

    auto other_frame_base = reinterpret_cast<uint8_t *>(other.base());
    auto other_frame_contents = other_frame_base + sizeof(vm_frame_base_alloc_t);

    assert(frame_type->size_in_bytes > sizeof(vm_frame_base_alloc_t));
    std::memcpy(this_frame_contents, other_frame_contents, frame_type->size_in_bytes - sizeof(vm_frame_base_alloc_t));

    inc_ref_count_in_memory(*frame_type, this_frame_base);
}

pointer_t vm_frame_t::base() const
{
    // The beginning of the frame data is at the end of the frame struct.
    return reinterpret_cast<pointer_t>(reinterpret_cast<uint8_t *>(const_cast<vm_frame_t *>(this)) + sizeof(vm_frame_t));
}

vm_pc_t &vm_frame_t::prev_pc() const
{
    return base<vm_frame_base_alloc_t>().p_pc;
}

vm_frame_t *&vm_frame_t::prev_frame() const
{
    return base<vm_frame_base_alloc_t>().p_fp;
}

vm_module_ref_t *&vm_frame_t::prev_module_ref() const
{
    return base<vm_frame_base_alloc_t>().p_mr;
}

pointer_t vm_frame_t::fixed_point_register_1() const
{
    const auto offset = vm_frame_constants::fixed_point_register_1_offset;
    assert(offset < frame_type->size_in_bytes);

    auto p = reinterpret_cast<uint8_t *>(base()) + offset;
    return reinterpret_cast<pointer_t>(p);
}

pointer_t vm_frame_t::fixed_point_register_2() const
{
    const auto offset = vm_frame_constants::fixed_point_register_2_offset;
    assert(offset < frame_type->size_in_bytes);

    auto p = reinterpret_cast<uint8_t *>(base()) + offset;
    return reinterpret_cast<pointer_t>(p);
}

vm_stack_t::vm_stack_t(std::size_t stack_extent)
    : _mem{ std::make_unique<vm_stack_layout>(stack_extent) }
{
    assert(0 < stack_extent && stack_extent < static_cast<std::size_t>(std::numeric_limits<word_t>::max()));
    debug::log_msg(debug::component_trace_t::memory, debug::log_level_t::debug, "init: vm stack\n");
}

vm_stack_t::~vm_stack_t()
{
    while (nullptr != pop_frame())
    {
        // Pop all frames off the stack
    }

    // Free stack memory
    _mem->release();
    _mem.reset();

    debug::log_msg(debug::component_trace_t::memory, debug::log_level_t::debug, "destroy: vm stack\n");
}

vm_frame_t *vm_stack_t::alloc_frame(std::shared_ptr<const type_descriptor_t> frame_type)
{
    assert(frame_type != nullptr);
    assert(sizeof(vm_frame_base_alloc_t) < frame_type->size_in_bytes && "Requested frame size less than VM frame base size");

    auto layout = static_cast<vm_stack_layout *>(_mem.get());

    auto page_top_frame = layout->top_page->page_top_frame;
    const auto new_frame_size = sizeof(vm_frame_t) + frame_type->size_in_bytes;
    if (layout->stack_page_size < new_frame_size)
        throw vm_system_exception{ "Requested stack frame larger than stack page" };

    vm_frame_t *new_frame;
    if (page_top_frame == nullptr)
    {
        auto new_frame_addr = layout->top_page->stack_data();
        new_frame = ::new(new_frame_addr)vm_frame_t{ frame_type };
    }
    else
    {
        auto current_page_top_frame_addr = reinterpret_cast<std::uintptr_t>(page_top_frame);
        auto new_frame_addr_maybe = current_page_top_frame_addr + (sizeof(vm_frame_t) + page_top_frame->frame_type->size_in_bytes);

        // If the new frame is not going to fit on the current page, create a new page.
        if ((new_frame_addr_maybe + new_frame_size) >= layout->top_page->page_limit_addr)
            new_frame_addr_maybe = reinterpret_cast<std::uintptr_t>(layout->new_page());

        new_frame = ::new(reinterpret_cast<void *>(new_frame_addr_maybe))vm_frame_t{ frame_type };
    }

    // Record the top frame on the page
    layout->top_page->page_top_frame = new_frame;

    return new_frame;
}

vm_frame_t *vm_stack_t::push_frame()
{
    auto layout = static_cast<vm_stack_layout *>(_mem.get());

    auto current_top_frame = layout->top_frame;
    auto new_top_frame = layout->top_page->page_top_frame;
    new_top_frame->prev_frame() = current_top_frame;

    layout->top_frame = new_top_frame;

    if (debug::is_component_tracing_enabled<debug::component_trace_t::stack>())
        debug::log_msg(debug::component_trace_t::stack, debug::log_level_t::debug, "update: push vm frame: %#" PRIxPTR "\n", new_top_frame);

    return new_top_frame;
}

vm_frame_t *vm_stack_t::pop_frame()
{
    auto layout = static_cast<vm_stack_layout *>(_mem.get());

    auto current_frame = layout->top_frame;
    if (current_frame == nullptr)
        return nullptr;

    {
        // Dispose of the current top frame and update the current frame.
        auto previous_frame = current_frame;
        current_frame = previous_frame->prev_frame();

        // Call destructor on previous frame since we allocated
        // the frame on the stack page.
        previous_frame->~vm_frame_t();
    }

    layout->top_frame = current_frame;
    layout->top_page->page_top_frame = current_frame;

    // Check if the stack page is no longer needed.
    if (current_frame != nullptr)
    {
        // Clean up the stack page allocation if the current frame is on a previous page
        if (!layout->top_page->contains_frame(current_frame))
        {
            layout->drop_page();

            // Ensure the current frame is in the new page.
            assert(layout->top_page->contains_frame(current_frame));
        }
    }

    if (debug::is_component_tracing_enabled<debug::component_trace_t::stack>())
        debug::log_msg(debug::component_trace_t::stack, debug::log_level_t::debug, "update: pop vm frame: %#" PRIxPTR "\n", current_frame);

    return current_frame;
}

vm_frame_t *vm_stack_t::peek_frame() const
{
    auto layout = static_cast<vm_stack_layout *>(_mem.get());
    return layout->top_frame;
}
