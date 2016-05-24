//
// Dis VM
// File: debugger.cpp
// Author: arr
//

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <algorithm>
#include <locale>
#include <map>
#include <vm_tools.h>
#include <utils.h>
#include <vm_memory.h>
#include <vm_asm.h>
#include "exec.h"

using namespace disvm;
using namespace disvm::runtime;

namespace
{
    // https://en.wikipedia.org/wiki/ANSI_escape_code
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

    // Debug command exec operation
    using dbg_cmd_exec_t = bool(*)(const std::vector<std::string> &, const vm_t &, const vm_registers_t &);

    // Debug command
    struct dbg_cmd_t
    {
        const char *cmd;
        const char *description;
        const char *syntax;
        const char *example;
        dbg_cmd_exec_t exec;
    };

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
            << "  Thread ID:  " << r.thread.get_thread_id() << "\n"
            << "     Module:  " << r.module_ref->module->module_name->str() << "\n"
            << "         PC:  " << r.pc << "\n"
            << "     Opcode:  " << r.module_ref->code_section[r.pc].op << "\n";

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

    std::ostream& operator<<(std::ostream &ss, const vm_thread_state_t s)
    {
        switch (s)
        {
        case vm_thread_state_t::unknown:
            ss << "unknown";
            break;
        case vm_thread_state_t::blocked_in_alt:
            ss << "blocked-in-alt";
            break;
        case vm_thread_state_t::blocked_sending:
            ss << "blocked-sending";
            break;
        case vm_thread_state_t::blocked_receiving:
            ss << "blocked-receiving";
            break;
        case vm_thread_state_t::debug:
            ss << "debug";
            break;
        case vm_thread_state_t::ready:
            ss << "ready";
            break;
        case vm_thread_state_t::running:
            ss << "running";
            break;
        case vm_thread_state_t::release:
            ss << "release";
            break;
        case vm_thread_state_t::exiting:
            ss << "exiting";
            break;
        case vm_thread_state_t::empty_stack:
            ss << "empty-stack";
            break;
        case vm_thread_state_t::broken:
            ss << "broken";
            break;
        default:
            ss << "??";
        }

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

    bool cmd_continue(const std::vector<std::string> &, const vm_t &, const vm_registers_t &)
    {
        return false;
    }

    bool cmd_print_registers(const std::vector<std::string> &, const vm_t &, const vm_registers_t &r)
    {
        auto s = registers_to_string(r);
        debug_print_info(s);

        return true;
    }

    bool cmd_print_threads(const std::vector<std::string> &, const vm_t &vm, const vm_registers_t &)
    {
        auto msg = std::stringstream{};
        msg << "Threads:\n";
        for (auto t : vm.get_scheduler_control().get_all_threads())
            msg << "  " << std::setw(5) << t->get_thread_id() << "  -  " << t->get_state() << "\n";

        debug_print_info(msg.str());

        return true;
    }

    bool cmd_print_modules(const std::vector<std::string> &, const vm_t &vm, const vm_registers_t &)
    {
        auto msg = std::stringstream{};
        msg << "Modules:\n";
        for (auto m : vm.get_loaded_modules())
            msg << "  " << m->module_name->str() << "\n";

        debug_print_info(msg.str());

        return true;
    }

    bool cmd_stacktrace(const std::vector<std::string> &a, const vm_t &vm, const vm_registers_t &r)
    {
        auto thread_registers = std::map<uint32_t, const vm_registers_t*>{};
        if (a.size() == 1)
        {
            thread_registers[r.thread.get_thread_id()] = &r;
        }
        else
        {
            uint32_t thread_id;
            auto all_threads = vm.get_scheduler_control().get_all_threads();
            if (a[1].compare("*") == 0)
            {
                thread_id = 0;
            }
            else
            {
                try
                {
                    thread_id = std::stoi(a[1]);
                }
                catch (...)
                {
                    debug_print_error("Invalid thread ID format");
                    return true;
                }
            }

            // Find the thread(s)
            for (auto t : all_threads)
            {
                const auto tid = t->get_thread_id();
                if (thread_id == 0 || tid == thread_id)
                {
                    thread_registers[tid] = &t->get_registers();

                    // If we are looking for a single thread, we found it.
                    if (thread_id != 0)
                        break;
                }
            }
        }

        if (thread_registers.empty())
        {
            debug_print_error("Unknown thread ID");
            return true;
        }

        auto msg = std::stringstream{};
        for (auto t : thread_registers)
        {
            msg << "Thread " << t.first << ":\n";
            msg << stack_to_string(*t.second);
            msg << "\n";
        }

        debug_print_info(msg.str());

        return true;
    }

    // Forward declaration
    void prompt(const vm_t &, const vm_registers_t &);

    bool cmd_switchthread(const std::vector<std::string> &a, const vm_t &vm, const vm_registers_t &r)
    {
        if (a.size() == 1)
        {
            debug_print_error("Must supply a target thread ID");
            return true;
        }

        uint32_t thread_id;
        try
        {
            thread_id = std::stoi(a[1]);
        }
        catch (...)
        {
            debug_print_error("Invalid thread ID format");
            return true;
        }

        auto all_threads = vm.get_scheduler_control().get_all_threads();

        // Find the thread
        for (auto t : all_threads)
        {
            const auto tid = t->get_thread_id();
            if (tid == thread_id)
            {
                prompt(vm, t->get_registers());

                // If we are returning from a thread switch we are not continuing, so return false to indicate as such.
                return false;
            }
        }

        debug_print_error("Unknown thread ID");
        return true;
    }

    bool cmd_disassemble(const std::vector<std::string> &a, const vm_t &, const vm_registers_t &r)
    {
        auto disassemble_length = 1;
        if (a.size() > 1)
        {
            try
            {
                disassemble_length = std::stoi(a[1]);
            }
            catch (...)
            {
                debug_print_error("Invalid disassemble count format");
                return true;
            }
        }

        const auto &code_section = r.module_ref->code_section;

        auto begin_index = static_cast<std::size_t>(r.pc);
        auto end_index = static_cast<std::size_t>(r.pc + 1);
        if (disassemble_length < 0)
        {
            const auto begin_maybe = r.pc - (-disassemble_length) + 1;
            begin_index = static_cast<std::size_t>(std::max(begin_maybe, 0));
        }
        else
        {
            const auto end_maybe = static_cast<std::size_t>(r.pc + disassemble_length);
            end_index = std::min(end_maybe, code_section.size());
        }

        auto msg = std::stringstream{};
        for (auto i = begin_index; i < end_index; ++i)
            msg << "  " << std::setw(5) << i << ": " << code_section[i].op << "\n";

        debug_print_info(msg.str());

        return true;
    }

    // Forward declaration
    void print_help();

    bool cmd_help(const std::vector<std::string> &, const vm_t &, const vm_registers_t &)
    {
        print_help();
        return true;
    }

    // Easter egg
    bool cmd_tlk_tilde_star(const std::vector<std::string> &, const vm_t &, const vm_registers_t &)
    {
        std::cout
            << console_modifiers::black_font
            << console_modifiers::white_bkg
            << "H"
            << console_modifiers::reset_all
            << console_modifiers::red_font
            << console_modifiers::bold
            << "e"
            << console_modifiers::green_font
            << console_modifiers::bold
            << "l"
            << console_modifiers::yellow_font
            << console_modifiers::bold
            << "l"
            << console_modifiers::blue_font
            << console_modifiers::bold
            << "o"
            << console_modifiers::magenta_font
            << console_modifiers::bold
            << ", "
            << console_modifiers::black_font
            << console_modifiers::white_bkg
            << "W"
            << console_modifiers::reset_all
            << console_modifiers::red_font
            << console_modifiers::bold
            << "o"
            << console_modifiers::green_font
            << console_modifiers::bold
            << "r"
            << console_modifiers::yellow_font
            << console_modifiers::bold
            << "l"
            << console_modifiers::blue_font
            << console_modifiers::bold
            << "d"
            << console_modifiers::magenta_font
            << console_modifiers::bold
            << "!\n"
            << console_modifiers::reset_all;

        return true;
    }

    const dbg_cmd_t cmds[] =
    {
        { "c", "Continue", nullptr, nullptr, cmd_continue },
        { "r", "Print registers", nullptr, nullptr, cmd_print_registers },
        { "t", "Print all threads", nullptr, nullptr, cmd_print_threads },
        { "m", "Print all loaded modules", nullptr, nullptr, cmd_print_modules },
        { "sw", "Stack walk/back trace", "bt ([0-9]+|\\*)?", "bt, bt 34, bt *", cmd_stacktrace },
        { "st", "Switch to thread", "st [0-9]+", "st 42", cmd_switchthread },
        { "d", "Disassemble the next/previous N instructions", "d (-?[0-9]+)?", "d, d -4, d 5", cmd_disassemble },
        { "?", "Print help", nullptr, nullptr, cmd_help },
        { "tlk~*", nullptr, nullptr, nullptr, cmd_tlk_tilde_star }
    };

    dbg_cmd_exec_t find_cmd(const std::string &cmd_maybe)
    {
        for (auto i = std::size_t{}; i < sizeof(cmds) / sizeof(cmds[0]); ++i)
        {
            if (cmd_maybe.compare(cmds[i].cmd) == 0)
                return cmds[i].exec;
        }

        // [TODO] Didn't find the cmd, try ignoring case?
        return nullptr;
    }

    void print_help()
    {
        auto ss = std::stringstream{};
        ss << "Commands:\n";
        for (auto i = std::size_t{}; i < sizeof(cmds) / sizeof(cmds[0]); ++i)
        {
            auto &c = cmds[i];

            // Ignore if undocumented
            if (c.description == nullptr)
                continue;

            ss << "  " << c.cmd << "\t" << c.description << "\n";

            if (c.syntax != nullptr)
                ss << "\t  Syntax: " << c.syntax << "\n";

            if (c.example != nullptr)
                ss << "\t  Example(s): " << c.example << "\n";
        }

        debug_print_info(ss.str());
    }

    // split the string using the supplied delimiter
    std::vector<std::string> split(const std::string &str, const char delim = ' ')
    {
        auto split_string = std::vector<std::string>{};

        auto curr = str.begin();
        auto end = str.cend();
        while (curr != end)
        {
            auto curr_begin = curr;
            while (curr != end && *curr != delim)
                ++curr;

            if (curr_begin != curr)
                split_string.push_back(std::move(std::string{ curr_begin, curr }));

            if (curr == end)
                break;

            ++curr;
        }

        return split_string;
    }

    void prompt(const vm_t &vm, const vm_registers_t &r)
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

            const auto cmd_tokens = split(cmd);
            if (cmd_tokens.empty())
                continue;

            auto cmd_exec = find_cmd(cmd_tokens[0]);
            if (cmd_exec != nullptr)
            {
                if (!cmd_exec(cmd_tokens, vm, r))
                    break;
            }
            else
            {
                debug_print_error("Unknown command");
                print_help();
            }
        }
    }
}

void debugger::on_load(vm_t &vm, vm_tool_controller_t &controller, std::size_t tool_id)
{
    _tool_id = tool_id;
    _vm = &vm;

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

    _event_cookies.push_back(controller.subscribe_event(vm_event_t::exception_unhandled, [&](vm_event_t, vm_event_context_t &cxt)
    {
        auto ss = std::stringstream{};
        ss << "Unhandled exception '"
            << cxt.value2.str->str()
            << "'";

        debug_print_error(ss.str());
        prompt(*_vm, *cxt.value1.registers);
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
    _vm = nullptr;
}