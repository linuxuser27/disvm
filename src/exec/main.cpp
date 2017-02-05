//
// Dis VM - exec program
// File: main.cpp
// Author: arr
//

#include <iostream>
#include <cassert>
#include <array>
#include <memory>
#include <builtin_module.h>
#include <debug.h>
#include <exceptions.h>
#include <vm_asm_sigkind.h>
#include <vm_version.h>
#include "exec.h"

using disvm::vm_t;
using disvm::vm_config_t;
using disvm::opcode_t;

using disvm::assembly::sigkind::sig_stream_t;
using disvm::assembly::sigkind::Tfunction;
using disvm::assembly::sigkind::Tlist;
using disvm::assembly::sigkind::Tstring;
using disvm::assembly::sigkind::Tnone;

using disvm::debug::component_trace_t;
using disvm::debug::log_level_t;

using disvm::runtime::type_descriptor_t;
using disvm::runtime::intrinsic_type_desc;
using disvm::runtime::byte_t;
using disvm::runtime::word_t;
using disvm::runtime::pointer_t;
using disvm::runtime::runtime_flags_t;
using disvm::runtime::vm_module_t;
using disvm::runtime::vm_alloc_t;
using disvm::runtime::vm_string_t;
using disvm::runtime::vm_list_t;
using disvm::runtime::vm_frame_base_alloc_t;
using disvm::runtime::vm_frame_constants;
using disvm::runtime::vm_exec_op_t;
using disvm::runtime::address_mode_t;
using disvm::runtime::address_mode_middle_t;
using disvm::runtime::import_function_t;
using disvm::runtime::import_vm_module_t;
using disvm::runtime::vm_user_exception;
using disvm::runtime::vm_system_exception;

namespace
{
    template<int L>
    std::unique_ptr<vm_string_t> create_string_from_const(const char(&ca)[L])
    {
        return std::make_unique<vm_string_t>(L - 1, reinterpret_cast<const uint8_t*>(ca));
    }

    // Entry frame layout
    struct vm_entry_frame_t final : public vm_frame_base_alloc_t
    {
        word_t noret;
        pointer_t command_module_ref;
        pointer_t command_stack_frame; // [SPEC] Stack frames are not managed by the GC and as such should not be marked as a pointer field. See pointer map below.
    };

    const auto entry_frame_pointer_map = std::vector<byte_t>{ 0x04 };

    std::shared_ptr<const vm_module_t> command_module;

    std::unique_ptr<vm_module_t> create_entry_module(vm_t &vm, const std::vector<char *> &vm_args)
    {
        // Convert arguments to a list
        vm_string_t *command_module_path = nullptr;
        vm_list_t *mod_args = new vm_list_t{ intrinsic_type_desc::type<vm_string_t>() };
        vm_list_t *last = mod_args;
        for (auto raw_arg : vm_args)
        {
            auto curr_arg = new vm_string_t{ std::strlen(raw_arg), reinterpret_cast<uint8_t *>(raw_arg) };
            if (command_module_path == nullptr)
            {
                // First argument is the module to load
                command_module_path = curr_arg;
            }
            else
            {
                auto new_tail = new vm_list_t{ intrinsic_type_desc::type<vm_string_t>() };
                last->set_tail(new_tail);
                new_tail->release();

                last = new_tail;
            }

            disvm::runtime::pt_ref(last->value()) = curr_arg->get_allocation();
        }

        if (command_module_path == nullptr)
            throw vm_user_exception{ "Entry module not supplied" };

        // Pre-load the command module in the VM.
        command_module = vm.load_module(command_module_path->str());

        // Create the entry module
        auto entry_module = std::make_unique<vm_module_t>();
        auto &e = *entry_module;

        // Header details
        e.header.entry_pc = 0;
        e.header.entry_type = 0;
        e.header.runtime_flag = (runtime_flags_t::has_import);
        e.module_name = create_string_from_const("_entry_");

        // Inherit the stack extent from the command module and add the entry frame size.
        e.header.stack_extent = command_module->header.stack_extent + sizeof(vm_entry_frame_t);

        e.type_section =
        {
            type_descriptor_t::create(sizeof(vm_entry_frame_t), entry_frame_pointer_map)
        };

        // Initialize the module base register
        const auto mp_type = type_descriptor_t::create(8, { 0xc0 });
        e.original_mp.reset(vm_alloc_t::allocate(mp_type));
        auto mp_base = e.original_mp->get_allocation<pointer_t>();

        // Take a reference on the command module path and store it on the entry frame
        command_module_path->add_ref();
        mp_base[0] = command_module_path->get_allocation();
        mp_base[1] = mod_args->get_allocation();

        // Define the frame offset for the argument
        const auto first_arg_offset = vm_frame_constants::limbo_first_arg_register_offset();

        e.code_section =
        {
            { vm_exec_op_t
                {
                    opcode_t::load,
                    disvm::assembly::construct_address_code(address_mode_t::offset_indirect_mp, address_mode_middle_t::small_immediate, address_mode_t::offset_indirect_fp),
                    { address_mode_t::offset_indirect_mp, 0, 0 }, // command module path
                    { address_mode_middle_t::small_immediate, 0 }, // import table index
                    { address_mode_t::offset_indirect_fp, 20, 0 } // module reference
                }
            },
            { vm_exec_op_t
                {
                    opcode_t::mframe,
                    disvm::assembly::construct_address_code(address_mode_t::offset_indirect_fp, address_mode_middle_t::small_immediate, address_mode_t::offset_indirect_fp),
                    { address_mode_t::offset_indirect_fp, 20, 0 }, // module reference
                    { address_mode_middle_t::small_immediate, 0 }, // function index into module
                    { address_mode_t::offset_indirect_fp, 24, 0 } // module call frame
                }
            },
            { vm_exec_op_t
                {
                    opcode_t::movp,
                    disvm::assembly::construct_address_code(address_mode_t::offset_indirect_mp, address_mode_middle_t::none, address_mode_t::offset_double_indirect_fp),
                    { address_mode_t::offset_indirect_mp, 4, 0 }, // Argument list
                    { address_mode_middle_t::none },
                    { address_mode_t::offset_double_indirect_fp, 24, first_arg_offset } // module call frame -> argument list offset
                }
            },
            { vm_exec_op_t
                {
                    opcode_t::mcall,
                    disvm::assembly::construct_address_code(address_mode_t::offset_indirect_fp, address_mode_middle_t::small_immediate, address_mode_t::offset_indirect_fp),
                    { address_mode_t::offset_indirect_fp, 24, 0 }, // module call frame
                    { address_mode_middle_t::small_immediate, 0 }, // function index into module
                    { address_mode_t::offset_indirect_fp, 20, 0 } // module reference
                }
            },
            { vm_exec_op_t
                {
                    opcode_t::ret,
                    disvm::assembly::construct_address_code(address_mode_t::none, address_mode_middle_t::none, address_mode_t::none),
                    { address_mode_t::none },
                    { address_mode_middle_t::none },
                    { address_mode_t::none }
                }
            },
        };

        // Create import section for Exec
        // See limbo/Exec.m for type details
        {
            import_function_t func;

            const auto exec_init_sig = 0x2e01144a;

            // Validate the hard-coded type signature on debug builds.
#if !defined(NDEBUG)
            sig_stream_t m;
            m << Tfunction::create(Tlist::create(Tstring::id)).returns(Tnone::id);
            assert(exec_init_sig == m.get_signature_hash());
            assert(exec_init_sig == sig_stream_t::compute_signature_hash("f(Ls)n"));
#endif
            func.sig = exec_init_sig;
            func.name = create_string_from_const("init");

            import_vm_module_t import{};
            import.functions.push_back(std::move(func));

            e.import_section.push_back(std::move(import));
        }

        return entry_module;
    }
}

struct arg_exception_t : std::runtime_error
{
    arg_exception_t(const char *m, const char *arg = nullptr)
        : std::runtime_error{ m }
        , arg{ arg }
    { }

    const char * const arg;
};

struct exec_options
{
    exec_options()
        : debugger{ debugger_options::none }
        , enabled_debugger{ false }
        , quiet_start{ false }
        , print_help{ false }
        , vm_config{}
    { }

    std::vector<char *> vm_args;

    vm_config_t vm_config;

    bool print_help;
    bool quiet_start;
    bool enabled_debugger;
    debugger_options debugger;
};

void print_banner(const exec_options &options)
{
    std::cout << "DisVM " << DISVM_VERSION_MAJOR << '.' << DISVM_VERSION_MINOR << '.' << DISVM_VERSION_PATCH;

    if (DISVM_VERSION_LABEL != nullptr)
        std::cout << '-' << DISVM_VERSION_LABEL;

    std::cout
        << "\n----------------\n"
        << "Debugger enabled: " << std::boolalpha << options.enabled_debugger << "\n"
        << "System thread usage: " << options.vm_config.sys_thread_pool_size << "\n";

    if (options.enabled_debugger)
        std::cout << "\n" << options.debugger << "\n";

    std::cout << std::endl;
}

void print_help()
{
    std::cout
        << "Usage: disvm-exec [-d[e|m|x]*] [-l[s|S|t|T|e|g|m]*] [-gD] [-t <num>] [-q] [-h] <entry module> <args>*\n"
           "    d - Enable debugger\n"
           "         e - Break on entry\n"
           "         m - Break on module load\n"
           "         x - Break on exception (first chance)\n"
           "    g - Garbage collector options\n"
           "         D - Disable\n"
           "    l - Enable logging in a component\n"
           "         s - Scheduler\n"
           "         S - Stack\n"
           "         t - Threads\n"
           "         T - Tool extensions\n"
           "         e - Exceptions\n"
           "         d - Duration of actions\n"
           "         g - Garbage collector (noisy)\n"
           "         m - Memory allocations (noisy)\n"
           "    q - Suppress banner and configuration\n"
           "    t - Specify the number of system threads to use (0 < x <= 4)\n"
           "    h - Print this help (alternative: '?')\n";
}

void log_callback(const component_trace_t origin, const log_level_t level, const char *msg_fmt, std::va_list args)
{
    std::stringstream ss;

    switch (level)
    {
    case log_level_t::warning:
        ss << "w: ";
        break;
    case log_level_t::debug:
        ss << "d: ";
        break;
    }

    std::array<char, 1024> buffer;
    const auto r = std::vsnprintf(buffer.data(), buffer.size(), msg_fmt, args);
    buffer[r] = '\0';

    ss << buffer.data() << "\n";
    if (r == buffer.size() - 1)
        ss << " <Possible truncation>\n";

    std::cout << ss.str();
}

void process_arg(char* arg, std::function<char *()> next, exec_options &options)
{
    assert(arg != nullptr && next != nullptr);
    if (arg[0] != '-' && arg[0] != '/')
    {
        for (; arg != nullptr; arg = next())
            options.vm_args.push_back(arg);

        return;
    }

    auto arg_len = std::strlen(arg);
    if (arg_len < 2)
        throw arg_exception_t{ "Invalid flag format", arg };

    switch (arg[1])
    {
    case 'd':
        options.enabled_debugger = true;
        for (auto i = std::size_t{ 2 }; i < arg_len; ++i)
        {
            switch (arg[i])
            {
            case 'e': options.debugger |= debugger_options::break_on_enter;
                break;
            case 'm': options.debugger |= debugger_options::break_on_module_load;
                break;
            case 'x': options.debugger |= debugger_options::break_on_exception;
                break;
            }
        }
        break;

    case 'g':
        if (arg_len <= 2 || arg[2] != 'D')
            throw arg_exception_t{ "Invalid garbage collector option" };

        options.vm_config.create_gc = disvm::runtime::create_no_op_gc;
        break;

    case 't':
        {
            auto sys_threads_str = next();
            if (sys_threads_str == nullptr)
                throw arg_exception_t{ "System thread requires count" };

            char *end;
            auto sys_threads = ::strtol(sys_threads_str, &end, 10);
            if (sys_threads <= 0 || 4 < sys_threads)
                throw arg_exception_t{ "Invalid system thread value", sys_threads_str };

            options.vm_config.sys_thread_pool_size = sys_threads;
        }
        break;

    case 'l':
        // Set the callback
        disvm::debug::set_logging_callback(log_callback);
        for (auto i = std::size_t{ 2 }; i < arg_len; ++i)
        {
            switch (arg[i])
            {
            case 's': disvm::debug::set_component_tracing(component_trace_t::scheduler, true);
                break;
            case 'S': disvm::debug::set_component_tracing(component_trace_t::stack, true);
                break;
            case 't': disvm::debug::set_component_tracing(component_trace_t::thread, true);
                break;
            case 'T': disvm::debug::set_component_tracing(component_trace_t::tool, true);
                break;
            case 'e': disvm::debug::set_component_tracing(component_trace_t::exception, true);
                break;
            case 'g': disvm::debug::set_component_tracing(component_trace_t::garbage_collector, true);
                break;
            case 'm': disvm::debug::set_component_tracing(component_trace_t::memory, true);
                break;
            case 'd': disvm::debug::set_component_tracing(component_trace_t::duration, true);
                break;
            }
        }
        break;

    case 'q':
        options.quiet_start = true;
        break;

    case 'h':
    case '?':
        options.print_help = true;
        break;

    default:
        throw arg_exception_t{ "Unknown flag", arg };
    }
}

int main(int argc, char* argv[])
{
    exec_options options{};

    try
    {
        // Ignore the process name
        auto arg_idx = int{ 1 };
        auto next_arg = [&]()
        {
            return arg_idx < argc ? argv[arg_idx++] : nullptr;
        };

        for (auto curr_arg = next_arg(); curr_arg != nullptr; curr_arg = next_arg())
            process_arg(curr_arg, next_arg, options);
    }
    catch (const arg_exception_t &ae)
    {
        std::cerr << ae.what() << "\n";
        if (ae.arg != nullptr)
            std::cerr << ": " << ae.arg << "\n";

        print_help();
        return EXIT_FAILURE;
    }

    if (!options.quiet_start)
        print_banner(options);

    if (options.print_help)
    {
        print_help();
        return EXIT_SUCCESS;
    }

    vm_t vm{ std::move(options.vm_config) };

    if (options.enabled_debugger)
        vm.load_tool(std::make_shared<debugger>(options.debugger));

    try
    {
        auto entry = create_entry_module(vm, options.vm_args);

        vm.exec(std::move(entry));

        vm.spin_sleep_till_idle(std::chrono::milliseconds(100));
    }
    catch (const vm_user_exception &ue)
    {
        std::cerr << ue.what() << std::endl;

        return EXIT_FAILURE;
    }
    catch (const vm_system_exception &se)
    {
        std::cerr
            << "Internal exception:\n"
            << se.what()
            << std::endl;

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
