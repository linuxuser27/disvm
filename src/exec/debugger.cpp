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
#include <map>
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

    // Debug command context
    struct dbg_cmd_cxt_t
    {
        dbg_cmd_cxt_t(debugger &d, const vm_registers_t &r)
            : dbg{ d }
            , exit_break{ false }
            , initial_register{ r }
            , vm{ d.controller->get_vm_instance() }
        {
            current_register = &initial_register;
        }

        bool exit_break;

        debugger &dbg;
        const vm_t &vm;
        const vm_registers_t *current_register;
        const vm_registers_t &initial_register;
    };

    // Debug command exec operation
    using dbg_cmd_exec_t = void(*)(const std::vector<std::string> &, dbg_cmd_cxt_t &cxt);

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
            << "  Thread ID:  " << r.thread.get_thread_id() << "\n"
            << "     Module:  " << r.module_ref->module->module_name->str() << "  PC: " << r.pc << "\n"
            << "     Opcode:  " << r.module_ref->code_section[r.pc].op << "\n";

        return register_string.str();
    }

    std::ostream& operator<<(std::ostream &ss, const vm_string_t *s)
    {
        auto escaped_string = std::stringstream{};
        auto str = s->str();
        if (str != nullptr)
        {
            while (*str != '\0')
            {
                switch (*str)
                {
                case '\\':
                    escaped_string << "\\\\";
                    break;
                case '\a':
                    escaped_string << "\\a";
                    break;
                case '\b':
                    escaped_string << "\\b";
                    break;
                case '\f':
                    escaped_string << "\\f";
                    break;
                case '\n':
                    escaped_string << "\\n";
                    break;
                case '\r':
                    escaped_string << "\\r";
                    break;
                case '\t':
                    escaped_string << "\\t";
                    break;
                case '\v':
                    escaped_string << "\\v";
                    break;
                default:
                    escaped_string << *str;
                }

                str++;
            }
        }

        ss << "string\n"
           << "\tlength:  " << s->get_length() << "\n"
           << "\tvalue:   \"" << escaped_string.str() << "\"\n";

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

    std::ostream& operator<<(std::ostream &ss, const vm_alloc_t *alloc)
    {
        if (alloc == nullptr)
        {
            ss << "<nil>";
        }
        else if (alloc->alloc_type == intrinsic_type_desc::type<vm_string_t>())
        {
            ss << vm_alloc_t::from_allocation<vm_string_t>(alloc->get_allocation());
        }
        else if (alloc->alloc_type == intrinsic_type_desc::type<vm_array_t>())
        {
            ss << vm_alloc_t::from_allocation<vm_array_t>(alloc->get_allocation());
        }
        else if (alloc->alloc_type == intrinsic_type_desc::type<vm_list_t>())
        {
            ss << vm_alloc_t::from_allocation<vm_list_t>(alloc->get_allocation());
        }
        else if (alloc->alloc_type == intrinsic_type_desc::type<vm_channel_t>())
        {
            ss << vm_alloc_t::from_allocation<vm_channel_t>(alloc->get_allocation());
        }
        else
        {
            ss << "alloc - size:  " << alloc->alloc_type->size_in_bytes << "\n";

            enum_pointer_fields(*alloc->alloc_type, alloc->get_allocation(), [&ss](pointer_t p, std::size_t o)
            {
                ss << "  [" << o << "]  " << vm_alloc_t::from_allocation(p);
            });
        }

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

    void cmd_continue(const std::vector<std::string> &, dbg_cmd_cxt_t &cxt)
    {
        cxt.exit_break = true;
    }

    void cmd_print_registers(const std::vector<std::string> &, dbg_cmd_cxt_t &cxt)
    {
        auto s = registers_to_string(*cxt.current_register);
        debug_print_info(s);
    }

    void cmd_print_threads(const std::vector<std::string> &, dbg_cmd_cxt_t &cxt)
    {
        auto msg = std::stringstream{};
        for (auto t : cxt.vm.get_scheduler_control().get_all_threads())
            msg << "  " << std::setw(5) << t->get_thread_id() << "  -  " << t->get_state() << "\n";

        debug_print_info(msg.str());
    }

    void cmd_print_modules(const std::vector<std::string> &, dbg_cmd_cxt_t &cxt)
    {
        auto msg = std::stringstream{};
        cxt.vm.enum_loaded_modules([&msg](const loaded_vm_module_t &m)
        {
            auto module_instance = m.module.lock();
            if (module_instance != nullptr)
            {
                msg << std::setw(3) << m.load_id << ": "
                    << std::setw(16) << module_instance->module_name->str()
                    << std::setw(0) << "  -  " << m.origin->str() << "\n";
            }

            return true;
        });

        debug_print_info(msg.str());
    }

    void cmd_breakpoint_set(const std::vector<std::string> &a, dbg_cmd_cxt_t &cxt)
    {
        if (a.size() < 3)
        {
            debug_print_error("Must supply an IP and module ID");
            return;
        }

        vm_pc_t pc;
        try
        {
            pc = std::stoul(a[1]);
        }
        catch (...)
        {
            debug_print_error("Invalid IP format");
            return;
        }

        uint32_t module_id;
        try
        {
            module_id = std::stoul(a[2]);
        }
        catch (...)
        {
            debug_print_error("Invalid module ID format");
            return;
        }

        auto module = std::shared_ptr<vm_module_t>{};
        cxt.vm.enum_loaded_modules([module_id, &module](const loaded_vm_module_t &m)
        {
            if (module_id != m.load_id)
                return true;

            // Found the matching ID so stop
            module = m.module.lock();
            return false;
        });

        if (module == nullptr)
        {
            debug_print_error("Invalid module ID");
            return;
        }
        else if (util::has_flag(module->header.runtime_flag, runtime_flags_t::builtin))
        {
            debug_print_error("Unable to set breakpoint in built-in module");
            return;
        }

        if (pc < 0 || module->code_section.size() <= static_cast<std::size_t>(pc))
        {
            debug_print_error("Invalid IP for module");
            return;
        }

        cxt.dbg.breakpoint_cookies.push_back(cxt.dbg.controller->set_breakpoint(module, pc));
    }

    void cmd_breakpoint_clear(const std::vector<std::string> &a, dbg_cmd_cxt_t &cxt)
    {
        if (a.size() == 1)
        {
            debug_print_error("Must supply a breakpoint ID or wildcard");
            return;
        }

        auto controller = cxt.dbg.controller;
        if (a[1].compare("*") == 0)
        {
            // Clear all breakpoints
            for (auto cookie : cxt.dbg.breakpoint_cookies)
                controller->clear_breakpoint(cookie);

            cxt.dbg.breakpoint_cookies.clear();
            return;
        }

        // Clear specific breakpoint
        cookie_t breakpoint_cookie;
        try
        {
            breakpoint_cookie = static_cast<cookie_t>(std::stoul(a[1]));
        }
        catch (...)
        {
            debug_print_error("Invalid breakpoint ID format");
            return;
        }

        auto cookie_iter = std::find_if(
            std::begin(cxt.dbg.breakpoint_cookies),
            std::end(cxt.dbg.breakpoint_cookies),
            [breakpoint_cookie] (const cookie_t c) { return c == breakpoint_cookie; });
        if (cookie_iter == cxt.dbg.breakpoint_cookies.end())
        {
            debug_print_error("Unknown breakpoint ID");
            return;
        }

        // Clear the breakpoint
        controller->clear_breakpoint(breakpoint_cookie);

        // Remove the cookie from the list
        cxt.dbg.breakpoint_cookies.erase(cookie_iter);
    }

    void cmd_breakpoint_list(const std::vector<std::string> &, dbg_cmd_cxt_t &cxt)
    {
        auto msg = std::stringstream{};
        for (auto cookie : cxt.dbg.breakpoint_cookies)
        {
            const auto details = cxt.dbg.controller->get_breakpoint_details(cookie);
            msg << std::setw(3) << cookie << ": "
                << std::setw(16) << details.module->module_name->str()
                << std::setw(0) << " @ " << details.pc << "\n";
        }

        debug_print_info(msg.str());
    }

    void cmd_stackwalk(const std::vector<std::string> &a, dbg_cmd_cxt_t &cxt)
    {
        auto thread_registers = std::map<uint32_t, const vm_registers_t*>{};
        if (a.size() == 1)
        {
            thread_registers[cxt.current_register->thread.get_thread_id()] = cxt.current_register;
        }
        else
        {
            uint32_t thread_id;
            if (a[1].compare("*") == 0)
            {
                thread_id = 0;
            }
            else
            {
                try
                {
                    thread_id = std::stoul(a[1]);
                }
                catch (...)
                {
                    debug_print_error("Invalid thread ID format");
                    return;
                }
            }

            // Find the thread(s)
            auto all_threads = cxt.vm.get_scheduler_control().get_all_threads();
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
            return;
        }

        auto msg = std::stringstream{};
        for (auto t : thread_registers)
        {
            msg << "Thread " << t.first << ":\n";
            msg << stack_to_string(*t.second);
            msg << "\n";
        }

        debug_print_info(msg.str());
    }

    void cmd_switchthread(const std::vector<std::string> &a, dbg_cmd_cxt_t &cxt)
    {
        if (a.size() == 1)
        {
            debug_print_error("Must supply a target thread ID");
            return;
        }

        uint32_t thread_id;
        try
        {
            thread_id = std::stoul(a[1]);
        }
        catch (...)
        {
            debug_print_error("Invalid thread ID format");
            return;
        }

        auto all_threads = cxt.vm.get_scheduler_control().get_all_threads();

        // Find the thread
        for (auto t : all_threads)
        {
            const auto tid = t->get_thread_id();
            if (tid == thread_id)
            {
                cxt.current_register = &t->get_registers();
                return;
            }
        }

        debug_print_error("Unknown thread ID");
    }

    void cmd_disassemble(const std::vector<std::string> &a, dbg_cmd_cxt_t &cxt)
    {
        auto disassemble_length = int{ 1 };
        if (a.size() > 1)
        {
            try
            {
                disassemble_length = std::stoi(a[1]);
            }
            catch (...)
            {
                debug_print_error("Invalid disassemble count format");
                return;
            }
        }

        const auto &r = *cxt.current_register;
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
    }

    void cmd_examine(const std::vector<std::string> &a, dbg_cmd_cxt_t &cxt)
    {
        if (a.size() < 3)
        {
            debug_print_error("Invalid number of arguments");
            return;
        }

        std::shared_ptr<const type_descriptor_t> base_type;
        word_t *base_pointer;
        auto &base_ptr_id = a[1];
        if (base_ptr_id.compare("mp") == 0)
        {
            base_type = cxt.current_register->mp_base->alloc_type;
            base_pointer = reinterpret_cast<word_t *>(cxt.current_register->mp_base->get_allocation());
        }
        else if (base_ptr_id.compare("fp") == 0)
        {
            auto frame = cxt.current_register->stack.peek_frame();
            if (frame == nullptr)
            {
                debug_print_error("Invalid frame pointer ");
                return;
            }

            base_type = frame->frame_type;
            base_pointer = reinterpret_cast<word_t *>(frame->base());
        }
        else
        {
            debug_print_error("Invalid memory base pointer");
            return;
        }

        assert(base_type != nullptr && base_pointer != nullptr);

        const auto pointer_size = static_cast<int>(sizeof(pointer_t));
        auto byte_offset1 = int{ 0 };
        auto byte_offset2 = int{ -1 };
        try
        {
            byte_offset1 = std::stoi(a[2]);

            // The 4th argument is optional
            if (a.size() >= 4)
                byte_offset2 = std::stoi(a[3]);
        }
        catch (...)
        {
            debug_print_error("Invalid memory offset format");
            return;
        }

        if (0 > byte_offset1 || byte_offset1 >= base_type->size_in_bytes)
        {
            debug_print_error("First offset out of range");
            return;
        }

        auto msg = std::stringstream{};
        if (byte_offset2 != -1)
            msg << "  " << byte_offset2 << "(" << byte_offset1 << "(" << a[1] << ")):\n";
        else
            msg << "  " << byte_offset1 << "(" << a[1] << "):\n";

        auto value_is_pointer = bool{ false };
        const auto pointer_offset1 = byte_offset1 / pointer_size;
        auto value_to_examine = reinterpret_cast<pointer_t>(base_pointer[pointer_offset1]);
        if (runtime::is_offset_pointer(*base_type, pointer_offset1))
        {
            value_is_pointer = true;
            auto alloc_to_examine = vm_alloc_t::from_allocation(value_to_examine);
            if (byte_offset2 != -1)
            {
                if (alloc_to_examine == nullptr)
                {
                    debug_print_error("First offset is nil");
                    return;
                }

                if (0 > byte_offset2 || byte_offset2 >= alloc_to_examine->alloc_type->size_in_bytes)
                {
                    debug_print_error("Second offset out of range");
                    return;
                }

                // Update data pointer for 2nd indirection
                base_pointer = reinterpret_cast<word_t *>(alloc_to_examine->get_allocation());
                base_type = alloc_to_examine->alloc_type;

                const auto pointer_offset2 = byte_offset2 / pointer_size;
                value_to_examine = reinterpret_cast<pointer_t>(base_pointer[pointer_offset2]);
                value_is_pointer = runtime::is_offset_pointer(*base_type, pointer_offset2);
            }
        }

        if (value_is_pointer)
            msg << "  " << vm_alloc_t::from_allocation(value_to_examine);
        else
            msg << "  " << std::setw(8) << std::showbase << std::hex << value_to_examine << std::dec;

        debug_print_info(msg.str());
    }

    // Forward declaration
    void print_help();

    void cmd_help(const std::vector<std::string> &, dbg_cmd_cxt_t &)
    {
        print_help();
    }

    // ee
    void cmd_tlk_tilde_star(const std::vector<std::string> &, dbg_cmd_cxt_t &)
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
    }

    const dbg_cmd_t cmds[] =
    {
        { "c", "Continue", nullptr, nullptr, cmd_continue },

        { "r", "Print registers", nullptr, nullptr, cmd_print_registers },
        { "t", "Print all threads", nullptr, nullptr, cmd_print_threads },
        { "m", "Print all loaded modules", nullptr, nullptr, cmd_print_modules },

        { "bs", "Set breakpoint at the specified IP in a loaded module", "bs [0-9]+ [0-9]+", "bs 3 5 (Breakpoint at IP 3 in module 5)", cmd_breakpoint_set },
        { "bc", "Clear breakpoint(s)", "bc ([0-9]+|\\*)", "bc 3, bc *", cmd_breakpoint_clear },
        { "bl", "List breakpoints", nullptr, nullptr, cmd_breakpoint_list },

        { "sw", "Stack walk for the current or supplied thread", "sw ([0-9]+|\\*)?", "sw, sw 34, sw *", cmd_stackwalk },
        { "~", "Switch to thread", "~ [0-9]+", "~ 42", cmd_switchthread },
        { "d", "Disassemble the next/previous N instructions", "d (-?[0-9]+)?", "d, d -4, d 5", cmd_disassemble },
        { "x", "Examine memory", "x [mp|fp] [0-9]+ ([0-9]+)?", "x mp 3, x fp 20 6", cmd_examine },
        { "?", "Print help", nullptr, nullptr, cmd_help },

        // Undocumented commands
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

    void prompt(debugger &debugger, const vm_registers_t &r)
    {
        std::cout
            << console_modifiers::green_font
            << console_modifiers::bold
            << "'?' for help\n"
            << console_modifiers::reset_all;

        std::string cmd;

        dbg_cmd_cxt_t cxt{ debugger, r };
        for (;;)
        {
            std::cout
                << console_modifiers::green_font
                << console_modifiers::bold
                << cxt.current_register->thread.get_thread_id()
                << " >>> "
                << console_modifiers::reset_all;

            std::getline(std::cin, cmd);
            if (cmd.empty())
                continue;

            const auto cmd_tokens = split(cmd);
            if (cmd_tokens.empty())
                continue;

            auto cmd_exec = find_cmd(cmd_tokens[0]);
            if (cmd_exec == nullptr)
            {
                debug_print_error("Unknown command. '?' for help.");
                continue;
            }

            cmd_exec(cmd_tokens, cxt);
            if (cxt.exit_break)
                break;
        }
    }
}

debugger::debugger()
    : _tool_id{ 0 }
    , controller{ nullptr }
{ }

void debugger::on_load(vm_tool_controller_t &controller_, std::size_t tool_id)
{
    _tool_id = tool_id;
    controller = &controller_;

    _event_cookies.push_back(controller->subscribe_event(vm_event_t::thread_begin, [](vm_event_t, vm_event_context_t &cxt)
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

    _event_cookies.push_back(controller->subscribe_event(vm_event_t::thread_end, [](vm_event_t, vm_event_context_t &cxt)
    {
        auto ss = std::stringstream{};
        ss << "Thread "
            << cxt.value1.thread->get_thread_id()
            << " ended.";

        debug_print_info(ss.str());
    }));

    _event_cookies.push_back(controller->subscribe_event(vm_event_t::exception_raised, [](vm_event_t, vm_event_context_t &cxt)
    {
        auto ss = std::stringstream{};
        ss << "Exception '"
            << cxt.value2.str->str()
            << "' thrown.";

        debug_print_info(ss.str());
    }));

    _event_cookies.push_back(controller->subscribe_event(vm_event_t::exception_unhandled, [&](vm_event_t, vm_event_context_t &cxt)
    {
        auto ss = std::stringstream{};
        ss << "Unhandled exception '"
            << cxt.value2.str->str()
            << "'";

        debug_print_error(ss.str());
        prompt(*this, *cxt.value1.registers);
    }));

    _event_cookies.push_back(controller->subscribe_event(vm_event_t::module_thread_load, [&](vm_event_t, vm_event_context_t &cxt)
    {
        auto m = (*cxt.value2.module);
        auto ss = std::stringstream{};
        ss << "Module "
            << m->module_name->str()
            << " loaded";

        if (util::has_flag(m->header.runtime_flag, runtime_flags_t::builtin))
            ss << " (built-in)";


        debug_print_info(ss.str());
    }));

    _event_cookies.push_back(controller->subscribe_event(vm_event_t::breakpoint, [&](vm_event_t, vm_event_context_t &cxt)
    {
        auto ss = std::stringstream{};
        ss << "Breakpoint hit";
        if (cxt.value2.cookie != 0)
            ss << " [" << cxt.value2.cookie << "]";

        debug_print_info(ss.str());

        auto reg_str = registers_to_string(*cxt.value1.registers);
        debug_print_info(reg_str);

        prompt(*this, *cxt.value1.registers);
    }));
}

void debugger::on_unload()
{
    assert(controller != nullptr);

    for (auto ec : _event_cookies)
        controller->unsubscribe_event(ec);

    controller = nullptr;
}