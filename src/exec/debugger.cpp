//
// Dis VM
// File: debugger.cpp
// Author: arr
//

#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <algorithm>
#include <map>
#include <set>
#include <utils.hpp>
#include <vm_memory.hpp>
#include <vm_asm.hpp>
#include <exceptions.hpp>
#include "exec.hpp"

using disvm::vm_t;
using disvm::loaded_vm_module_t;
using disvm::opcode_t;

using disvm::runtime::type_descriptor_t;
using disvm::runtime::intrinsic_type_desc;
using disvm::runtime::word_t;
using disvm::runtime::pointer_t;
using disvm::runtime::module_id_t;
using disvm::runtime::runtime_flags_t;
using disvm::runtime::cookie_t;
using disvm::runtime::vm_pc_t;
using disvm::runtime::vm_registers_t;
using disvm::runtime::vm_module_t;
using disvm::runtime::vm_module_ref_t;
using disvm::runtime::vm_alloc_t;
using disvm::runtime::vm_string_t;
using disvm::runtime::vm_array_t;
using disvm::runtime::vm_list_t;
using disvm::runtime::vm_channel_t;
using disvm::runtime::vm_thread_state_t;
using disvm::runtime::vm_trap_flags_t;
using disvm::runtime::vm_term_request;
using disvm::runtime::vm_tool_controller_t;
using disvm::runtime::vm_event_t;
using disvm::runtime::vm_event_context_t;

using disvm::symbol::function_name_format_t;
using disvm::symbol::advance_pc_t;
using disvm::symbol::symbol_data_t;

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

    bool vm_debug_enabled = false;

    template<typename T>
    struct vm_dbg_type final
    {
        T t;
    };

    struct debug_cmd_error_t : std::runtime_error
    {
        debug_cmd_error_t(const char *message)
            : std::runtime_error{ message }
        { }
    };

    void debug_print_error(const char *str)
    {
        std::cout
            << console_modifiers::red_font
            << console_modifiers::bold
            << "Error: "
            << str
            << console_modifiers::reset_all
            << "\n";
    }

    void debug_print_error(const std::string &str)
    {
        debug_print_error(str.c_str());
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
        static std::string last_successful_cmd_option;

        dbg_cmd_cxt_t(debugger &d, vm_registers_t &r, bool transient_context = false)
            : dbg{ d }
            , exit_break{ false }
            , current_register{ &r }
            , initial_register{ r }
            , transient_context{ transient_context }
            , vm{ d.controller->get_vm_instance() }
        {
            last_successful_cmd = dbg.get_option(last_successful_cmd_option);
        }

        ~dbg_cmd_cxt_t()
        {
            if (!transient_context)
                dbg.set_option(last_successful_cmd_option, std::move(last_successful_cmd));
        }

        const bool transient_context;
        bool exit_break;
        std::string last_successful_cmd;

        debugger &dbg;
        const vm_t &vm;
        const vm_registers_t *current_register;
        vm_registers_t &initial_register;
    };

    std::string dbg_cmd_cxt_t::last_successful_cmd_option = "last_successful_cmd";

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

    std::string stack_to_string(const vm_registers_t &r, debugger &dbg)
    {
        auto msg = std::stringstream{};
        walk_stack(r, [&msg, &dbg](const pointer_t, const vm_pc_t pc, const vm_module_ref_t &module_ref)
        {
            auto func_maybe = dbg.resolve_to_function_source_line(module_ref.module->vm_id, pc, pc, function_name_format_t::name);
            if (!func_maybe.empty())
            {
                assert(func_maybe.size() == 1);
                msg << module_ref.module->module_name->str() << "!" << func_maybe[0] << "  ";
            }
            else
            {
                auto module_name = "<No Name>";
                if (module_ref.module->module_name != nullptr)
                    module_name = module_ref.module->module_name->str();

                msg << module_name;
            }

            msg << "@" << pc << "\n";
            return true;
        });

        return msg.str();
    }

    std::string registers_to_string(const vm_registers_t &r)
    {
        auto register_string = std::stringstream{};

        register_string
            << "  Thread ID:  " << r.thread.get_thread_id()
            << "\n     Module:  " << r.module_ref->module->module_name->str()
            << "\n         PC:  " << r.pc;

        if (!r.module_ref->is_builtin_module())
        {
            assert(static_cast<std::size_t>(r.pc) < r.module_ref->code_section.size());
            const auto &op = r.module_ref->code_section[r.pc].op;
            register_string
                << "\n        Src:  " << op.source
                << "\n        Mid:  " << op.middle
                << "\n        Dst:  " << op.destination;
        }

        register_string << "\n";
        return register_string.str();
    }

    std::ostream& operator<<(std::ostream &ss, const std::shared_ptr<const type_descriptor_t> &t)
    {
        ss << "size: " << t->size_in_bytes;

#ifndef NDEBUG
        ss << " n: " << t->debug_type_name;
#endif

        return ss;
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

        ss << escaped_string.str() << "    length: " << s->get_length() << " - string";

        return ss;
    }

    std::ostream& operator<<(std::ostream &ss, const vm_array_t *a)
    {
        ss << "length: " << a->get_length()
            << " - array of " << a->get_element_type();

        return ss;
    }

    std::ostream& operator<<(std::ostream &ss, const vm_list_t *l)
    {
        ss << "length: " << l->get_length()
            << " - list of " << l->get_element_type();

        return ss;
    }

    std::ostream& operator<<(std::ostream &ss, const vm_channel_t *c)
    {
        ss << "buffer size: " << c->get_buffer_size()
            << " - channel of" << c->get_data_type();

        return ss;
    }

    std::ostream& operator<<(std::ostream &ss, const vm_dbg_type<const vm_alloc_t *> &dbg_alloc)
    {
        if (!vm_debug_enabled)
            return ss;

        return ss
            << " [[ref: " << dbg_alloc.t->get_ref_count()
            << " addr: " << reinterpret_cast<const void *>(dbg_alloc.t)
            << " gc_res: " << reinterpret_cast<const void *>(dbg_alloc.t->gc_reserved)
            << "]]";
    }

    std::ostream& safe_alloc_print(std::ostream &ss, const vm_alloc_t *alloc, int recurse_depth)
    {
        if (alloc == nullptr)
            return ss << "<nil>";

        vm_dbg_type<const vm_alloc_t *> dbg_alloc{ alloc };
        if (alloc->alloc_type == intrinsic_type_desc::type<vm_string_t>())
        {
            ss << vm_alloc_t::from_allocation<vm_string_t>(alloc->get_allocation()) << dbg_alloc;
        }
        else if (alloc->alloc_type == intrinsic_type_desc::type<vm_array_t>())
        {
            ss << vm_alloc_t::from_allocation<vm_array_t>(alloc->get_allocation()) << dbg_alloc;
        }
        else if (alloc->alloc_type == intrinsic_type_desc::type<vm_list_t>())
        {
            ss << vm_alloc_t::from_allocation<vm_list_t>(alloc->get_allocation()) << dbg_alloc;
        }
        else if (alloc->alloc_type == intrinsic_type_desc::type<vm_channel_t>())
        {
            ss << vm_alloc_t::from_allocation<vm_channel_t>(alloc->get_allocation()) << dbg_alloc;
        }
        else
        {
            ss << alloc->alloc_type << " - alloc" << dbg_alloc;

            const auto base_address = reinterpret_cast<std::uintptr_t>(alloc->get_allocation());
            enum_pointer_fields(*alloc->alloc_type, alloc->get_allocation(), [&ss, recurse_depth, base_address](pointer_t *p)
            {
                ss << "\n";
                std::fill_n(std::ostream_iterator<std::ostream::char_type>{ ss }, 4 * recurse_depth, ' ');
                const auto p_address = reinterpret_cast<std::uintptr_t>(p);
                ss << "[" << std::size_t{ p_address - base_address } << "]  ";

                if (recurse_depth < 2)
                    safe_alloc_print(ss, vm_alloc_t::from_allocation(*p), recurse_depth + 1);
                else
                    ss << "...";
            });
        }

        return ss;
    }

    std::ostream& operator<<(std::ostream &ss, const vm_alloc_t *alloc)
    {
        return safe_alloc_print(ss, alloc, 0);
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

    void cmd_step(const std::vector<std::string> &, dbg_cmd_cxt_t &cxt)
    {
        cxt.dbg.controller->set_thread_trap_flag(*cxt.current_register, vm_trap_flags_t::instruction);
        cxt.exit_break = true;
    }

    void cmd_term(const std::vector<std::string> &, dbg_cmd_cxt_t &)
    {
        throw vm_term_request{};
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
            msg << "  " << std::setw(5) << t->get_thread_id() << "  -  " << t->get_registers().current_thread_state << "\n";

        debug_print_info(msg.str());
    }

    void cmd_print_modules(const std::vector<std::string> &, dbg_cmd_cxt_t &cxt)
    {
        auto msg = std::stringstream{};
        cxt.vm.enum_loaded_modules([&msg, &cxt](const loaded_vm_module_t &m)
        {
            auto module_instance = m.module.lock();
            if (module_instance != nullptr)
            {
                msg << std::setw(3) << m.vm_id << ": "
                    << std::setw(16) << module_instance->module_name->str()
                    << std::setw(0) << "  -  " << m.origin->str();

                if (cxt.dbg.symbols_exist(m.vm_id))
                    msg << " [symbols loaded]";

                msg << "\n";
            }

            return true;
        });

        debug_print_info(msg.str());
    }

    void cmd_print_symbols(const std::vector<std::string> &a, dbg_cmd_cxt_t &cxt)
    {
        if (a.size() < 2)
            throw debug_cmd_error_t{ "Must supply a module ID" };

        module_id_t vm_id;
        try
        {
            vm_id = std::stoul(a[1]);
        }
        catch (...)
        {
            throw debug_cmd_error_t{ "Invalid module ID format" };
        }

        if (!cxt.dbg.symbols_exist(vm_id))
            throw debug_cmd_error_t{ "No symbols loaded for module" };

        // Interpret the last non-module ID value as the partial name to match.
        const char *name_to_match = nullptr;
        if (a.size() > 2)
            name_to_match = a.back().c_str();

        auto msg = std::stringstream{};
        auto module_funcs = cxt.dbg.get_module_functions(vm_id, name_to_match);
        msg << " [PC range]      Function";
        for (auto &f : module_funcs)
            msg << "\n  " << f;

        debug_print_info(msg.str());
    }

    void cmd_breakpoint_set(const std::vector<std::string> &a, dbg_cmd_cxt_t &cxt)
    {
        if (a.size() < 2)
            throw debug_cmd_error_t{ "Must supply an IP" };

        vm_pc_t pc;
        try
        {
            pc = std::stoul(a[1]);
        }
        catch (...)
        {
            throw debug_cmd_error_t{ "Invalid IP format" };
        }

        auto module = std::const_pointer_cast<vm_module_t>(cxt.current_register->module_ref->module);
        if (a.size() > 2)
        {
            module.reset();
            uint32_t module_id;
            try
            {
                module_id = std::stoul(a[2]);
            }
            catch (...)
            {
                throw debug_cmd_error_t{ "Invalid module ID format" };
            }

            cxt.vm.enum_loaded_modules([module_id, &module](const loaded_vm_module_t &m)
            {
                if (module_id != m.vm_id)
                    return true;

                // Found the matching ID so stop
                module = m.module.lock();
                return false;
            });
        }

        if (module == nullptr)
        {
            throw debug_cmd_error_t{ "Invalid module ID" };
        }
        else if (disvm::util::has_flag(module->header.runtime_flag, runtime_flags_t::builtin))
        {
            throw debug_cmd_error_t{ "Unable to set breakpoint in built-in module" };
        }

        if (pc < 0 || module->code_section.size() <= static_cast<std::size_t>(pc))
            throw debug_cmd_error_t{ "Invalid IP for module" };

        cxt.dbg.breakpoint_cookies.emplace(cxt.dbg.controller->set_breakpoint(module, pc));
    }

    void cmd_breakpoint_clear(const std::vector<std::string> &a, dbg_cmd_cxt_t &cxt)
    {
        if (a.size() == 1)
            throw debug_cmd_error_t{ "Must supply a breakpoint ID or wildcard" };

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
            throw debug_cmd_error_t{ "Invalid breakpoint ID format" };
        }

        auto cookie_iter = std::find_if(
            std::begin(cxt.dbg.breakpoint_cookies),
            std::end(cxt.dbg.breakpoint_cookies),
            [breakpoint_cookie] (const cookie_t c) { return c == breakpoint_cookie; });
        if (cookie_iter == cxt.dbg.breakpoint_cookies.end())
            throw debug_cmd_error_t{ "Unknown breakpoint ID" };

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
                << std::setw(0);

            auto func_maybe = cxt.dbg.resolve_to_function_source_line(details.module->vm_id, details.pc, details.pc, function_name_format_t::name);
            if (!func_maybe.empty())
            {
                assert(func_maybe.size() == 1);
                msg << "!" << func_maybe[0] << "  ";
            }

            msg << "@" << details.pc << "\n";
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
                    throw debug_cmd_error_t{ "Invalid thread ID format" };
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
            throw debug_cmd_error_t{ "Unknown thread ID" };

        auto msg = std::stringstream{};
        for (auto t : thread_registers)
        {
            msg << "Thread " << t.first << ":\n";
            msg << stack_to_string(*t.second, cxt.dbg);
            msg << "\n";
        }

        debug_print_info(msg.str());
    }

    void cmd_switchthread(const std::vector<std::string> &a, dbg_cmd_cxt_t &cxt)
    {
        if (a.size() == 1)
            throw debug_cmd_error_t{ "Must supply a target thread ID" };

        uint32_t thread_id;
        try
        {
            thread_id = std::stoul(a[1]);
        }
        catch (...)
        {
            throw debug_cmd_error_t{ "Invalid thread ID format" };
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

        throw debug_cmd_error_t{ "Unknown thread ID" };
    }

    void cmd_disassemble(const std::vector<std::string> &a, dbg_cmd_cxt_t &cxt)
    {
        const auto &r = *cxt.current_register;
        auto begin_pc = r.pc;

        const auto default_disassemble_length = int{ 4 };
        auto disassemble_length = default_disassemble_length;
        for (auto i = std::size_t{ 1 }; i < a.size(); ++i)
        {
            auto absolute_pc = false;
            auto disassemble_length_maybe = a[i];
            if (disassemble_length_maybe[0] == '@')
            {
                absolute_pc = true;
                disassemble_length_maybe.erase(0, 1);
            }

            try
            {
                disassemble_length = std::stoi(disassemble_length_maybe);
            }
            catch (...)
            {
                throw debug_cmd_error_t{ "Invalid disassemble count format" };
            }

            if (absolute_pc)
            {
                begin_pc = disassemble_length;
                disassemble_length = default_disassemble_length;
            }
        }

        auto end_pc = begin_pc;
        auto &dbg = cxt.dbg;
        const auto &code_section = r.module_ref->code_section;

        if (begin_pc < 0 || static_cast<int32_t>(code_section.size()) <= end_pc)
            throw debug_cmd_error_t{ "Invalid absolute pc" };

        if (disassemble_length < 0)
        {
            const auto begin_maybe = begin_pc - (-disassemble_length);
            begin_pc = std::max(begin_maybe, 0);
        }
        else
        {
            const auto end_maybe = begin_pc + disassemble_length;
            end_pc = std::min(end_maybe, static_cast<int32_t>(code_section.size() - 1));
        }

        auto resolved_pc = dbg.resolve_to_function_source_line(r.module_ref->module->vm_id, begin_pc, end_pc, function_name_format_t::name);
        assert(resolved_pc.empty() || resolved_pc.size() == static_cast<size_t>(end_pc - begin_pc + 1) && "Resolved PCs should have failed or match the PC count");

        auto msg = std::stringstream{};
        for (auto curr_pc = begin_pc; curr_pc <= end_pc; ++curr_pc)
        {
            auto op = code_section[curr_pc].op;

            auto op_prefix = "   ";

            // If the opcode is a breakpoint then try and replace it with the real opcode
            if (op.opcode == opcode_t::brkpt)
            {
                // Look up each of the known cookies to try and find the matching PC and module
                for (auto bp_cookie : dbg.breakpoint_cookies)
                {
                    const auto details = dbg.controller->get_breakpoint_details(bp_cookie);
                    if (details.pc == curr_pc && details.module == r.module_ref->module)
                    {
                        op.opcode = details.original_opcode;
                        op_prefix = " b ";
                        break;
                    }
                }
            }

            // Print source location if resolved
            if (!resolved_pc.empty())
            {
                const auto &rpc = resolved_pc[curr_pc - begin_pc];
                if (!rpc.empty())
                    msg << rpc << "\n";
            }

            msg << op_prefix << std::setw(5) << curr_pc << ": " << op << "\n";
        }

        debug_print_info(msg.str());
    }

    void cmd_examine(const std::vector<std::string> &a, dbg_cmd_cxt_t &cxt)
    {
        if (a.size() < 3)
            throw debug_cmd_error_t{ "Invalid number of arguments" };

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
                throw debug_cmd_error_t{ "Invalid frame pointer " };

            base_type = frame->frame_type;
            base_pointer = reinterpret_cast<word_t *>(frame->base());
        }
        else
        {
            throw debug_cmd_error_t{ "Invalid memory base pointer" };
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
            throw debug_cmd_error_t{ "Invalid memory offset format" };
        }

        if (0 > byte_offset1 || byte_offset1 >= base_type->size_in_bytes)
            throw debug_cmd_error_t{ "First offset out of range" };

        auto msg = std::stringstream{};
        if (byte_offset2 != -1)
            msg << byte_offset2 << "(" << byte_offset1 << "(" << a[1] << ")):\n";
        else
            msg << byte_offset1 << "(" << a[1] << "):\n";

        auto value_is_pointer = bool{ false };
        const auto pointer_offset1 = byte_offset1 / pointer_size;
        auto value_to_examine = reinterpret_cast<pointer_t>(base_pointer[pointer_offset1]);
        if (disvm::runtime::is_offset_pointer(*base_type, pointer_offset1))
        {
            value_is_pointer = true;
            auto alloc_to_examine = vm_alloc_t::from_allocation(value_to_examine);
            if (byte_offset2 != -1)
            {
                if (alloc_to_examine == nullptr)
                    throw debug_cmd_error_t{ "First offset is nil" };

                if (0 > byte_offset2 || byte_offset2 >= alloc_to_examine->alloc_type->size_in_bytes)
                    throw debug_cmd_error_t{ "Second offset out of range" };

                // Update data pointer for 2nd indirection
                base_pointer = reinterpret_cast<word_t *>(alloc_to_examine->get_allocation());
                base_type = alloc_to_examine->alloc_type;

                const auto pointer_offset2 = byte_offset2 / pointer_size;
                value_to_examine = reinterpret_cast<pointer_t>(base_pointer[pointer_offset2]);
                value_is_pointer = disvm::runtime::is_offset_pointer(*base_type, pointer_offset2);
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

    std::set<std::string> dbg_options =
    {
        { "bp-cmd" },
        { "step-cmd" },
        { "vm-dbg" }
    };

    void set_value_cmd(const std::vector<std::string> &args, dbg_cmd_cxt_t &cxt)
    {
        auto msg = std::stringstream{};
        if (args.size() == 1)
        {
            msg << "Debug options\n";
            for (auto &option : dbg_options)
            {
                auto v = cxt.dbg.get_option(option);
                msg << std::setw(16) << option
                    << ": "
                    << (v.empty() ? "<none>" : v)
                    << "\n";
            }

            debug_print_info(msg.str());
            return;
        }

        auto &curr_option = args[1];
        if (std::cend(dbg_options) == dbg_options.find(curr_option))
            throw debug_cmd_error_t{ "Invalid debug option" };

        auto ss_value = std::stringstream{};

        // Reconstitute the parsed debug arguments - in-case of space breaks.
        for (auto i = std::size_t{ 2 }; i < args.size(); ++i)
        {
            ss_value << args[i];

            // For the last parsed arg
            if (i + 1 < args.size())
                ss_value << " ";
        }

        auto new_value = ss_value.str();
        if (std::strcmp(curr_option.c_str(), "vm-dbg") == 0)
        {
            vm_debug_enabled = !new_value.empty();
            if (vm_debug_enabled)
                new_value = "true";
            else
                new_value = "";
        }

        cxt.dbg.set_option(curr_option, new_value);
    }

    const dbg_cmd_t cmds[] =
    {
        { "c", "Continue", nullptr, nullptr, cmd_continue },
        { "s", "Step over", nullptr, nullptr, cmd_step },
        { "term", "Terminate the VM instance", nullptr, nullptr, cmd_term },

        { "r", "Print registers", nullptr, nullptr, cmd_print_registers },
        { "t", "Print all threads", nullptr, nullptr, cmd_print_threads },
        { "m", "Print all loaded modules", nullptr, nullptr, cmd_print_modules },
        { "sym", "Print symbols for the supplied module", "sym [0-9]+ <partial function name>?", "sym 2 foo (Print symbols in module 2 containing 'foo')", cmd_print_symbols },

        { "bs", "Set breakpoint at the specified IP in a loaded module", "bs [0-9]+ [0-9]*", "bs 3 5 (Breakpoint at IP 3 in module 5), bs 4 (Breakpoint at IP 4 in current module)", cmd_breakpoint_set },
        { "bc", "Clear breakpoint(s)", "bc ([0-9]+|\\*)", "bc 3, bc *", cmd_breakpoint_clear },
        { "bl", "List breakpoints", nullptr, nullptr, cmd_breakpoint_list },

        { "sw", "Stack walk for the current or supplied thread", "sw ([0-9]+|\\*)?", "sw, sw 34, sw *", cmd_stackwalk },
        { "~", "Switch to thread", "~ [0-9]+", "~ 42", cmd_switchthread },
        { "d", "Disassemble the next/previous N instructions (default N = 4)", "d (@?-?[0-9]+)? (-?[0-9]+)?", "d, d -4, d 5, d @36 3", cmd_disassemble },
        { "x", "Examine memory", "x [mp|fp] [0-9]+ ([0-9]+)?", "x mp 3, x fp 20 6", cmd_examine },

        { "set" , "Set a debug option", "set [ _a-zA-Z0-9]*", "'set' for available options", set_value_cmd },
        { "?", "Print help", nullptr, nullptr, cmd_help },

        // Undocumented commands
        { "tlk~*", nullptr, nullptr, nullptr, cmd_tlk_tilde_star }
    };

    const auto cmds_length = sizeof(cmds) / sizeof(cmds[0]);

    dbg_cmd_exec_t find_cmd(const std::string &cmd_maybe)
    {
        for (auto i = std::size_t{}; i < cmds_length; ++i)
        {
            if (cmd_maybe.compare(cmds[i].cmd) == 0)
                return cmds[i].exec;
        }

        // [TODO] Didn't find the cmd, try ignoring case?
        throw debug_cmd_error_t{ "Unknown command. '?' for help." };
    }

    void print_help()
    {
        auto msg = std::stringstream{};
        msg << "Commands:\n";
        for (auto i = std::size_t{}; i < cmds_length; ++i)
        {
            auto &c = cmds[i];

            // Ignore if undocumented
            if (c.description == nullptr)
                continue;

            msg << "  " << c.cmd << "    " << c.description << "\n";

            if (c.syntax != nullptr)
                msg << "      Syntax: " << c.syntax << "\n";

            if (c.example != nullptr)
                msg << "      Example(s): " << c.example << "\n";
        }

        debug_print_info(msg.str());
    }

    void execute_single_command(dbg_cmd_cxt_t &cxt, std::string cmd)
    {
        const auto cmd_tokens = disvm::util::split(cmd);
        if (cmd_tokens.empty())
            return;

        try
        {
            auto cmd_exec = find_cmd(cmd_tokens[0]);
            cmd_exec(cmd_tokens, cxt);

            cxt.last_successful_cmd = std::move(cmd);
        }
        catch (const debug_cmd_error_t &e)
        {
            debug_print_error(e.what());
        }
    }

    void prompt(debugger &debugger, vm_registers_t &r)
    {
        debugger.controller->suspend_all_threads();

        if (debugger.first_break)
        {
            debugger.first_break = false;
            std::cout
                << console_modifiers::green_font
                << console_modifiers::bold
                << "'?' for help\n"
                << console_modifiers::reset_all;
        }

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
                cmd = cxt.last_successful_cmd;

            assert(!cxt.exit_break && "Precondition on exit_break state before execute failed");
            execute_single_command(cxt, std::move(cmd));
            if (cxt.exit_break)
                break;
        }

        debugger.controller->resume_all_threads();
    }
}

std::ostream& operator<<(std::ostream &ss, const debugger_options o)
{
    ss << "Break on enter: " << std::boolalpha << disvm::util::has_flag(o, debugger_options::break_on_enter) << "\n"
       << "Break on module load: " << std::boolalpha << disvm::util::has_flag(o, debugger_options::break_on_module_load) << "\n"
       << "Break on exception: " << std::boolalpha << disvm::util::has_flag(o, debugger_options::break_on_exception) << "\n";

    return ss;
}

debugger::debugger(const debugger_options opt)
    : _options{ opt }
    , _tool_id{ 0 }
    , controller{ nullptr }
    , first_break{ true }
{ }

std::vector<std::string> debugger::resolve_to_function_source_line(
    module_id_t vm_id,
    vm_pc_t begin_pc,
    vm_pc_t end_pc,
    function_name_format_t fmt) const
{
    auto iter = _vm_id_to_symbol_info.find(vm_id);
    if (iter == std::cend(_vm_id_to_symbol_info))
        return{};

    auto &d = *(iter->second.iter);

    // Position the debug symbol iterator
    d.set_current_pc(begin_pc);

    std::vector<std::string> resolved_functions_by_pc(end_pc - begin_pc + 1);
    vm_pc_t next_pc = begin_pc;
    do
    {
        if (next_pc > end_pc)
            break;

        std::stringstream ss;
        const auto function_name = d.current_function_name(fmt);
        if (!function_name.empty())
            ss << function_name << " at ";

        const auto source_loc = d.current_source_location();
        ss << d.get_source_by_id(source_loc.begin_source_id)
            << ':'
            << source_loc.begin_line;

        resolved_functions_by_pc[next_pc - begin_pc] = std::move(ss.str());

    } while (d.try_advance_pc(advance_pc_t::next_debug_statement, &next_pc));

    return resolved_functions_by_pc;
}

std::vector<std::string> debugger::get_module_functions(module_id_t vm_id, const char *match) const
{
    std::vector<std::string> funcs;
    auto iter = _vm_id_to_symbol_info.find(vm_id);
    if (iter != std::cend(_vm_id_to_symbol_info))
    {
        auto f_infos = iter->second.data->get_functions(function_name_format_t::declaration);
        for (auto &fi : f_infos)
        {
            // Only return functions that match the supplied string
            if (match != nullptr
                && fi.name.find(match) == std::string::npos)
                continue;

            std::stringstream ss;
            ss << "["
                << std::setw(5) << fi.entry_pc
                << ","
                << std::setw(5) << fi.limit_pc
                << "]  "
                << fi.name;

            funcs.push_back(ss.str());
        }
    }

    return funcs;
}

bool debugger::symbols_exist(module_id_t vm_id) const
{
    return _vm_id_to_symbol_info.find(vm_id) != std::cend(_vm_id_to_symbol_info);
}

std::string debugger::get_option(const std::string &option) const
{
    auto iter = _debugger_options.find(option);
    if (iter == std::cend(_debugger_options))
        return{};

    return iter->second;
}

void debugger::set_option(const std::string &option, std::string value)
{
    _debugger_options[option] = std::move(value);
}

void debugger::on_load(vm_tool_controller_t &controller_, std::size_t tool_id)
{
    _tool_id = tool_id;
    controller = &controller_;

    _event_cookies.emplace(controller->subscribe_event(vm_event_t::thread_begin, [](vm_event_t, vm_event_context_t &cxt)
    {
        auto t = cxt.value1.thread;
        auto ss = std::stringstream{};
        ss << "Thread "
            << t->get_thread_id()
            << " started in "
            << t->get_registers().module_ref->module->module_name->str()
            << " @"
            << t->get_registers().pc;

        debug_print_info(ss.str());
    }));

    _event_cookies.emplace(controller->subscribe_event(vm_event_t::thread_end, [](vm_event_t, vm_event_context_t &cxt)
    {
        auto ss = std::stringstream{};
        ss << "Thread "
            << cxt.value1.thread->get_thread_id()
            << " ended.";

        debug_print_info(ss.str());
    }));

    _event_cookies.emplace(controller->subscribe_event(vm_event_t::exception_raised, [&](vm_event_t, vm_event_context_t &cxt)
    {
        auto ss = std::stringstream{};
        ss << "Exception '"
            << cxt.value2.str->str()
            << "' thrown.";

        debug_print_info(ss.str());

        if (disvm::util::has_flag(_options, debugger_options::break_on_exception))
            prompt(*this, *cxt.value1.registers);
    }));

    _event_cookies.emplace(controller->subscribe_event(vm_event_t::exception_unhandled, [&](vm_event_t, vm_event_context_t &cxt)
    {
        auto ss = std::stringstream{};
        ss << "Unhandled exception '"
            << cxt.value2.str->str()
            << "'";

        debug_print_error(ss.str());
        prompt(*this, *cxt.value1.registers);
    }));

    _event_cookies.emplace(controller->subscribe_event(vm_event_t::module_vm_load, [&](vm_event_t, vm_event_context_t &cxt)
    {
        auto loaded_mod = cxt.value1.loaded_mod;
        assert(loaded_mod != nullptr);
        load_symbols(*loaded_mod);
    }));

    _event_cookies.emplace(controller->subscribe_event(vm_event_t::module_thread_load, [&](vm_event_t, vm_event_context_t &cxt)
    {
        auto m = (*cxt.value2.module);
        auto ss = std::stringstream{};
        ss << "Module "
            << m->module_name->str()
            << " loaded";

        if (disvm::util::has_flag(m->header.runtime_flag, runtime_flags_t::builtin))
            ss << " (built-in)";

        debug_print_info(ss.str());

        // If the break on enter flag was set, it will be true
        // and the first module load will be the user entry module.
        if (disvm::util::has_flag(_options, debugger_options::break_on_enter))
        {
            // Reset the break on enter flag and set a breakpoint
            // on the entry PC for the module - 'break on enter'
            _options ^= debugger_options::break_on_enter;
            breakpoint_cookies.emplace(controller->set_breakpoint(m, m->header.entry_pc));
        }

        if (disvm::util::has_flag(_options, debugger_options::break_on_module_load))
            prompt(*this, *cxt.value1.registers);
    }));

    _event_cookies.emplace(controller->subscribe_event(vm_event_t::breakpoint, [&](vm_event_t, vm_event_context_t &cxt)
    {
        auto ss = std::stringstream{};
        ss << "Breakpoint hit";
        if (cxt.value2.cookie != 0)
            ss << " [" << cxt.value2.cookie << "]";

        debug_print_info(ss.str());

        auto cmd = get_option({ "bp-cmd" });
        if (!cmd.empty())
        {
            dbg_cmd_cxt_t dbg_cxt{ *this, *cxt.value1.registers, true };
            execute_single_command(dbg_cxt, std::move(cmd));
        }

        prompt(*this, *cxt.value1.registers);
    }));

    _event_cookies.emplace(controller->subscribe_event(vm_event_t::trap, [&](vm_event_t, vm_event_context_t &cxt)
    {
        auto cmd = get_option({ "step-cmd" });
        if (!cmd.empty())
        {
            dbg_cmd_cxt_t dbg_cxt{ *this, *cxt.value1.registers, true };
            execute_single_command(dbg_cxt, std::move(cmd));
        }

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

void debugger::load_symbols(const disvm::loaded_vm_module_t &loaded_mod)
{
    auto m = loaded_mod.module.lock();
    assert(m != nullptr && "Loaded module event should have valid module reference");

    auto symbol_extension = ".sbl";
    std::string symbol_path_maybe{ loaded_mod.origin->str() };
    const auto ext_index = symbol_path_maybe.find_last_of('.');
    if (ext_index == std::string::npos)
    {
        // Module doesn't have an extension so just append the symbol extension.
        symbol_path_maybe.append(symbol_extension);
    }
    else
    {
        // Replace the module extension with the symbol extension.
        symbol_path_maybe.replace(ext_index, std::string::npos, symbol_extension);
    }

    std::ifstream sbl_file{ symbol_path_maybe, std::ifstream::in };
    if (!sbl_file.is_open())
        return;

    std::unique_ptr<symbol_data_t> symbol_data;
    try
    {
        symbol_data = disvm::symbol::read(sbl_file);
    }
    catch (const std::runtime_error &e)
    {
        // File with assumed symbol name/extension is not the correct format
        auto msg = std::stringstream{};
        msg << "Assumed symbol file '"
            << symbol_path_maybe
            << "' has invalid symbol format. ("
            << e.what()
            << ")";
        debug_print_error(msg.str());
        return;
    }

    // Check if the symbol data matches the loaded module
    if (0 != symbol_data->get_module_name().compare(m->module_name->str())
        || symbol_data->get_instruction_count() != m->code_section.size())
    {
        // Symbol file does not match the loaded modules - stale file?
        auto msg = std::stringstream{};
        msg << "Symbol file '"
            << symbol_path_maybe
            << "' does not match loaded module '"
            << loaded_mod.origin->str()
            << "'.";
        debug_print_error(msg.str());
        return;
    }

    const auto vm_id = loaded_mod.vm_id;
    assert(_vm_id_to_symbol_info.find(vm_id) == std::end(_vm_id_to_symbol_info) && "Module VM ID should be unique");
    auto symbol_iter = symbol_data->get_pc_iter();
    _vm_id_to_symbol_info[vm_id] = { std::move(symbol_data), std::move(symbol_iter) };

    auto msg = std::stringstream{};
    msg << "Symbol file '"
        << symbol_path_maybe
        << "' loaded for '"
        << loaded_mod.origin->str()
        << "'.";
    debug_print_info(msg.str());
}