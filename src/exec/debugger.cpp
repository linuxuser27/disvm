//
// Dis VM
// File: debugger.cpp
// Author: arr
//

#include <iostream>
#include <sstream>
#include <string>
#include <locale>
#include <vm_tools.h>
#include <utils.h>
#include <vm_memory.h>
#include <vm_asm.h>
#include "exec.h"

using namespace disvm;
using namespace disvm::runtime;

namespace
{
    std::string stack_to_string(const vm_registers_t &r)
    {
        auto msg = std::stringstream{};
        walk_stack(r, [&msg](const pointer_t, const vm_pc_t pc, const vm_module_ref_t &module_ref)
        {
            auto module_name = "<No Name>";
            if (module_ref.module->module_name != nullptr)
                module_name = module_ref.module->module_name->str();

            msg << module_name << "@" << pc << "\n";
            return true;
        });

        return msg.str();
    }

    std::string registers_to_string(const vm_registers_t &r)
    {
        auto register_string = std::stringstream{};

        register_string
            << "Registers:\n"
            << "      Thread ID:  " << r.thread.get_thread_id() << "\n"
            << "         Module:  " << r.module_ref->module->module_name->str() << "\n"
            << "Program counter:  " << r.pc << "\n"
            << "         Opcode:  " << r.module_ref->code_section[r.pc].op << "\n";

        return register_string.str();
    }

    std::ostream& operator<<(std::ostream &ss, const vm_string_t *s)
    {
        ss << "string\n"
            << "\tlength:  " << s->get_length() << "\n"
            << "\tvalue:   " << s->str() << "\n";

        return ss;
    }

    std::ostream& operator<<(std::ostream &ss, const vm_array_t *a)
    {
        ss << "array\n"
            << "\tlength:  " << a->get_length() << "\n";

        return ss;
    }

    std::ostream& operator<<(std::ostream &ss, const vm_list_t *l)
    {
        ss << "list\n"
            << "\tlength:  " << l->get_length() << "\n";

        return ss;
    }

    std::ostream& operator<<(std::ostream &ss, const vm_channel_t *c)
    {
        ss << "channel\n"
            << "\tbuffer size:  " << c->get_buffer_size() << "\n";

        return ss;
    }

    std::ostream& operator<<(std::ostream &ss, const vm_alloc_t *a)
    {
        ss << "alloc\n"
            << "\tsize:  " << a->alloc_type->size_in_bytes << "\n";

        return ss;
    }

    std::string to_string_frame_pointers(vm_registers_t &r)
    {
        auto frame = r.stack.peek_frame();
        if (frame == nullptr)
            return "<empty>";

        auto frame_string = std::stringstream{};
        enum_pointer_fields(*frame->frame_type, frame->base(), [&frame_string](pointer_t p)
        {
            frame_string << std::showbase << std::hex << reinterpret_cast<uintptr_t>(p) << " : " << std::dec;

            auto alloc = vm_alloc_t::from_allocation(p);
            if (alloc->alloc_type == intrinsic_type_desc::type<vm_string_t>())
                frame_string << vm_alloc_t::from_allocation<vm_string_t>(p);
            else if (alloc->alloc_type == intrinsic_type_desc::type<vm_array_t>())
                frame_string << vm_alloc_t::from_allocation<vm_array_t>(p);
            else if (alloc->alloc_type == intrinsic_type_desc::type<vm_list_t>())
                frame_string << vm_alloc_t::from_allocation<vm_list_t>(p);
            else if (alloc->alloc_type == intrinsic_type_desc::type<vm_channel_t>())
                frame_string << vm_alloc_t::from_allocation<vm_channel_t>(p);
            else
                frame_string << alloc;
        });

        return frame_string.str();
    }

    namespace console_modifiers
    {
#define CSI "\33["

        const auto black_font = CSI "30m";
        const auto red_font = CSI "31m";
        const auto green_font = CSI "32m";
        const auto yellow_font = CSI "33m";
        const auto blue_font = CSI "34m";
        const auto magenta_font = CSI "35m";
        const auto cyan_font = CSI "36m";
        const auto white_font = CSI "37m";

        const auto black_bkg = CSI "40m";
        const auto red_bkg = CSI "41m";
        const auto green_bkg = CSI "42m";
        const auto yellow_bkg = CSI "43m";
        const auto blue_bkg = CSI "44m";
        const auto magenta_bkg = CSI "45m";
        const auto cyan_bkg = CSI "46m";
        const auto white_bkg = CSI "47m";

        const auto bold = CSI "1m";

        const auto reset_all = CSI "0m";

#undef CSI
    }

    void debug_print_error(const std::string &str)
    {
        std::cout
            << console_modifiers::red_font
            << console_modifiers::bold
            << "Error: "
            << str
            << console_modifiers::reset_all
            << "\n";
    }

    void debug_print_info(const std::string &str)
    {
        std::cout
            << console_modifiers::yellow_font
            << console_modifiers::bold
            << str
            << console_modifiers::reset_all
            << "\n";
    }

    auto prompt_help =
R"(Help:
    c C - continue
    r R - print registers
    w W - print stack
     ?  - help
)";

    void prompt(const vm_registers_t &r)
    {
        std::cout
            << console_modifiers::green_font
            << console_modifiers::bold
            << "'?' for help\n"
            << console_modifiers::reset_all;

        std::locale loc;
        std::string cmd;
        for (;;)
        {
            std::cout
                << console_modifiers::green_font
                << console_modifiers::bold
                << r.thread.get_thread_id()
                << " >>> "
                << console_modifiers::reset_all;

            std::getline(std::cin, cmd);
            if (cmd.empty())
                continue;

            auto c = std::toupper(cmd[0], loc);
            if (c == 'R')
            {
                auto str = registers_to_string(r);
                debug_print_info(str);
            }
            else if (c == 'W')
            {
                auto str = stack_to_string(r);
                debug_print_info(str);
            }
            else if (c == 'C')
            {
                break;
            }
            else if (c == '?')
            {
                debug_print_info(prompt_help);
            }
            else
            {
                debug_print_error("Unknown command");
                debug_print_info(prompt_help);
            }
        }
    }
}

void debugger::on_load(vm_t &vm, vm_tool_controller_t &controller, std::size_t tool_id)
{
    _tool_id = tool_id;

    _event_cookies.push_back(controller.subscribe_event(vm_event_t::thread_begin, [](vm_event_t, vm_event_context_t &cxt)
    {
        auto t = cxt.value1.thread;
        auto ss = std::stringstream{};
        ss << "Thread "
            << t->get_thread_id()
            << " started in "
            << t->get_registers().module_ref->module->module_name->str()
            << " @ "
            << t->get_registers().pc;

        debug_print_info(ss.str());
    }));

    _event_cookies.push_back(controller.subscribe_event(vm_event_t::thread_end, [](vm_event_t, vm_event_context_t &cxt)
    {
        auto ss = std::stringstream{};
        ss << "Thread "
            << cxt.value1.thread->get_thread_id()
            << " ended.";

        debug_print_info(ss.str());
    }));

    _event_cookies.push_back(controller.subscribe_event(vm_event_t::exception_raised, [](vm_event_t, vm_event_context_t &cxt)
    {
        auto ss = std::stringstream{};
        ss << "Exception '"
            << cxt.value2.str->str()
            << "' thrown.";

        debug_print_info(ss.str());
    }));

    _event_cookies.push_back(controller.subscribe_event(vm_event_t::exception_unhandled, [](vm_event_t, vm_event_context_t &cxt)
    {
        auto ss = std::stringstream{};
        ss << "Unhandled exception '"
            << cxt.value2.str->str()
            << "' thrown.";

        debug_print_error(ss.str());
        prompt(*cxt.value1.registers);
    }));

    _event_cookies.push_back(controller.subscribe_event(vm_event_t::module_load, [](vm_event_t, vm_event_context_t &cxt)
    {
        auto m = (*cxt.value1.module);
        auto ss = std::stringstream{};
        ss << "Module "
            << m->module_name->str()
            << " loaded (";

        if (!util::has_flag(m->header.runtime_flag, runtime_flags_t::builtin))
            ss << cxt.value2.str->str();
        else
            ss << "built-in";

        ss << ")";

        debug_print_info(ss.str());
    }));

    _controller = &controller;
}

void debugger::on_unload()
{
    assert(_controller != nullptr);

    for (auto ec : _event_cookies)
        _controller->unsubscribe_event(ec);

    _controller = nullptr;
}