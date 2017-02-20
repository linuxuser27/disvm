//
// Dis VM
// File: Sysmod.cpp
// Author: arr
//

#include <array>
#include <algorithm>
#include <chrono>
#include <thread>
#include <iostream>
#include <fstream>
#include <streambuf>
#include <exceptions.h>
#include <debug.h>
#include <utf8.h>
#include <vm_memory.h>
#include "Sysmod.h"
#include "sys_utils.h"
#include "fd_types.h"

using disvm::vm_t;

using disvm::debug::component_trace_t;
using disvm::debug::log_level_t;

using disvm::runtime::dereference_nil;
using disvm::runtime::intrinsic_type_desc;
using disvm::runtime::out_of_range_memory;
using disvm::runtime::type_descriptor_t;
using disvm::runtime::vm_alloc_t;
using disvm::runtime::vm_array_t;
using disvm::runtime::vm_list_t;
using disvm::runtime::vm_string_t;
using disvm::runtime::vm_syscall_exception;
using disvm::runtime::vm_system_exception;
using disvm::runtime::vm_user_exception;
using disvm::runtime::marshallable_user_exception;
using disvm::runtime::word_t;

using disvm::runtime::sys::open_mode_t;
using disvm::runtime::sys::seek_start_t;
using disvm::runtime::sys::vm_fd_t;

namespace
{
    disvm::runtime::builtin::vm_runtab_t Sysmodtab[] = {
        "announce",0xb7c4ac0,Sys_announce,40,2,{ 0x0,0x80, },
        "aprint",0x77442d46,Sys_aprint,0,0,{ 0 },
        "bind",0x66326d91,Sys_bind,48,2,{ 0x0,0xc0, },
        "byte2char",0x3d6094f9,Sys_byte2char,40,2,{ 0x0,0x80, },
        "char2byte",0x2ba5ab41,Sys_char2byte,48,2,{ 0x0,0x40, },
        "chdir",0xc6935858,Sys_chdir,40,2,{ 0x0,0x80, },
        "create",0x54db77d9,Sys_create,48,2,{ 0x0,0x80, },
        "dial",0x29e90174,Sys_dial,40,2,{ 0x0,0xc0, },
        "dirread",0x72210d71,Sys_dirread,40,2,{ 0x0,0x80, },
        "dup",0x6584767b,Sys_dup,40,0,{ 0 },
        "export",0x6fc6dc03,Sys_export,48,2,{ 0x0,0xc0, },
        "fauth",0x20ccc34b,Sys_fauth,40,2,{ 0x0,0xc0, },
        "fd2path",0x749c6042,Sys_fd2path,40,2,{ 0x0,0x80, },
        "fildes",0x1478f993,Sys_fildes,40,0,{ 0 },
        "file2chan",0x9f34d686,Sys_file2chan,40,2,{ 0x0,0xc0, },
        "fprint",0xf46486c8,Sys_fprint,0,0,{ 0 },
        "fstat",0xda4499c2,Sys_fstat,40,2,{ 0x0,0x80, },
        "fversion",0xfe9c0a06,Sys_fversion,48,2,{ 0x0,0xa0, },
        "fwstat",0x50a6c7e0,Sys_fwstat,104,2,{ 0x0,0xbc, },
        "iounit",0x5583b730,Sys_iounit,40,2,{ 0x0,0x80, },
        "listen",0xb97416e0,Sys_listen,48,2,{ 0x0,0xe0, },
        "millisec",0x616977e8,Sys_millisec,32,0,{ 0 },
        "mount",0x74c17b3a,Sys_mount,56,2,{ 0x0,0xe8, },
        "open",0x8f477f99,Sys_open,40,2,{ 0x0,0x80, },
        "pctl",0x5df27fb,Sys_pctl,40,2,{ 0x0,0x40, },
        "pipe",0x1f2c52ea,Sys_pipe,40,2,{ 0x0,0x80, },
        "pread",0x9d8aac6,Sys_pread,56,2,{ 0x0,0xc0, },
        "print",0xac849033,Sys_print,0,0,{ 0 },
        "pwrite",0x9d8aac6,Sys_pwrite,56,2,{ 0x0,0xc0, },
        "read",0x7cfef557,Sys_read,48,2,{ 0x0,0xc0, },
        "readn",0x7cfef557,Sys_readn,48,2,{ 0x0,0xc0, },
        "remove",0xc6935858,Sys_remove,40,2,{ 0x0,0x80, },
        "seek",0xaeccaddb,Sys_seek,56,2,{ 0x0,0x80, },
        "sleep",0xe67bf126,Sys_sleep,40,0,{ 0 },
        "sprint",0x4c0624b6,Sys_sprint,0,0,{ 0 },
        "stat",0x319328dd,Sys_stat,40,2,{ 0x0,0x80, },
        "stream",0xb9e8f9ea,Sys_stream,48,2,{ 0x0,0xc0, },
        "tokenize",0x57338f20,Sys_tokenize,40,2,{ 0x0,0xc0, },
        "unmount",0x21e337e3,Sys_unmount,40,2,{ 0x0,0xc0, },
        "utfbytes",0x1d4a1f4,Sys_utfbytes,40,2,{ 0x0,0x80, },
        "werrstr",0xc6935858,Sys_werrstr,40,2,{ 0x0,0x80, },
        "write",0x7cfef557,Sys_write,48,2,{ 0x0,0xc0, },
        "wstat",0x56b02096,Sys_wstat,104,2,{ 0x0,0xbc, },
        0
    };

    const word_t Sysmodlen = 43;

    std::shared_ptr<const type_descriptor_t> T_Qid;
    std::shared_ptr<const type_descriptor_t> T_Dir;
    std::shared_ptr<const type_descriptor_t> T_FD;
    std::shared_ptr<const type_descriptor_t> T_Connection;
    std::shared_ptr<const type_descriptor_t> T_FileIO;

    class Sys_FD_Impl final : public Sys_FD
    {
    public: // static
        static void finalizer(vm_alloc_t *fileHandle)
        {
            auto fd = fileHandle->get_allocation<Sys_FD_Impl>();

            fd->~Sys_FD_Impl();

            if (disvm::debug::is_component_tracing_enabled<component_trace_t::builtin>())
            {
                disvm::debug::log_msg(
                    component_trace_t::builtin,
                    log_level_t::debug,
                    "sys: fd: finalize: %d",
                    fd->fd);
            }
        }

        // The ref count of the supplied vm_fd_t instance is updated.
        static vm_alloc_t * create_new(vm_fd_t *vm_fd)
        {
            assert(vm_fd != nullptr);
            assert(vm_fd->get_ref_count() == 1 && "Should be a new FD");

            auto alloc = vm_alloc_t::allocate(T_FD);
            auto fd_impl = ::new(alloc->get_allocation())Sys_FD_Impl{};

            fd_impl->fd = disvm::runtime::sys::create_fd_record(vm_fd);
            fd_impl->impl = vm_fd;
            vm_fd->add_ref();

            return alloc;
        }

        // The ref count of the supplied vm_fd_t instance is _not_ altered.
        static vm_alloc_t * create_existing(word_t fd, vm_fd_t *vm_fd)
        {
            assert(vm_fd != nullptr);
            assert(fd >= 0);

            auto alloc = vm_alloc_t::allocate(T_FD);
            auto fd_impl = ::new(alloc->get_allocation())Sys_FD_Impl{};

            fd_impl->fd = fd;
            fd_impl->impl = vm_fd;

            return alloc;
        }

    public:
        ~Sys_FD_Impl()
        {
            disvm::runtime::sys::drop_fd_record(fd);
            disvm::runtime::dec_ref_count_and_free(impl);
            disvm::debug::assign_debug_pointer(&impl);
        }

        vm_fd_t *impl;

    private:
        Sys_FD_Impl() = default;
    };

    // File descriptors for std streams.
    word_t fd_stdin = disvm::runtime::sys::vm_invalid_fd;
    word_t fd_stdout = disvm::runtime::sys::vm_invalid_fd;
    word_t fd_stderr = disvm::runtime::sys::vm_invalid_fd;

    word_t printf_to_fd(
        disvm::vm_t &vm,
        vm_fd_t &fd,
        const disvm::runtime::vm_string_t &msg_fmt,
        disvm::runtime::byte_t *msg_args,
        disvm::runtime::pointer_t base)
    {
        auto static_buffer = std::array<char, 1024>{};
        auto written = disvm::runtime::sys::printf_to_buffer(vm, msg_fmt, msg_args, base, static_buffer.size(), static_buffer.data());
        if (written >= 0)
        {
            fd.write(vm, written, static_buffer.data());
        }
        else
        {
            auto dynamic_buffer = std::vector<char>(static_buffer.size() * 2);
            written = disvm::runtime::sys::printf_to_dynamic_buffer(vm, msg_fmt, msg_args, base, dynamic_buffer);
            fd.write(vm, written, dynamic_buffer.data());
        }

        return written;
    }

    open_mode_t convert_to_openmode(const word_t mode)
    {
        auto om = open_mode_t{};

        // Lowest bit is for read/write.
        if (mode & Sys_OWRITE)
            om |= open_mode_t::write;
        else
            om |= open_mode_t::read; // Sys_OREAD

        if (mode & Sys_ORDWR)
            om |= (open_mode_t::write | open_mode_t::read);

        if (mode & Sys_OTRUNC)
            om |= open_mode_t::truncate;

        if (mode & Sys_OEXCL)
            om |= open_mode_t::atomic;

        if (mode & Sys_ORCLOSE)
            om |= open_mode_t::delete_on_close;

        return om;
    }

    seek_start_t convert_to_seekdir(const word_t start)
    {
        switch (start)
        {
        case Sys_SEEKSTART:
            return seek_start_t::start;
        case Sys_SEEKRELA:
            return seek_start_t::relative;
        case Sys_SEEKEND:
            return seek_start_t::end;
        default:
            throw vm_user_exception{ "Invalid seek mode" };
        }
    }
}

void
Sysmodinit(void)
{
    disvm::runtime::builtin::register_module_exports(Sys_PATH, Sysmodlen, Sysmodtab);
    T_Qid = type_descriptor_t::create(sizeof(Sys_Qid), sizeof(Sys_Qid_map), Sys_Qid_map);
    T_Dir = type_descriptor_t::create(sizeof(Sys_Dir), sizeof(Sys_Dir_map), Sys_Dir_map);
    T_FD = type_descriptor_t::create(sizeof(Sys_FD_Impl), sizeof(Sys_FD_map), Sys_FD_map, Sys_FD_Impl::finalizer);
    T_Connection = type_descriptor_t::create(sizeof(Sys_Connection), sizeof(Sys_Connection_map), Sys_Connection_map);
    T_FileIO = type_descriptor_t::create(sizeof(Sys_FileIO), sizeof(Sys_FileIO_map), Sys_FileIO_map);

    // Initialize default file descriptors
    auto std_streams = disvm::runtime::sys::get_std_streams();

    // Initialize in this order to assign POSIX defined values for the 'standard' streams.
    // http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/unistd.h.html

    fd_stdin = disvm::runtime::sys::create_fd_record(std_streams.input);
    assert(fd_stdin == 0);
    disvm::debug::log_msg(component_trace_t::builtin, log_level_t::debug, "sys: stdin: %d", fd_stdin);

    fd_stdout = disvm::runtime::sys::create_fd_record(std_streams.output);
    assert(fd_stdout == 1);
    disvm::debug::log_msg(component_trace_t::builtin, log_level_t::debug, "sys: stdout: %d", fd_stdout);

    fd_stderr = disvm::runtime::sys::create_fd_record(std_streams.error);
    assert(fd_stderr == 2);
    disvm::debug::log_msg(component_trace_t::builtin, log_level_t::debug, "sys: stderr: %d", fd_stderr);
}

void
Sys_announce(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_announce>();
    throw vm_system_exception{ "Function not implemented" };
}

void
Sys_aprint(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_aprint>();

    //disvm::runtime::dec_ref_count_and_free(vm_alloc_t::from_allocation(*fp.ret));
    //*fp.ret = nullptr;
    throw vm_system_exception{ "Function not implemented" };
}

void
Sys_bind(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_bind>();
    throw vm_system_exception{ "Function not implemented" };
}

void
Sys_byte2char(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_byte2char>();

    auto buffer = vm_alloc_t::from_allocation<vm_array_t>(fp.buf);
    assert(buffer->get_element_type()->size_in_bytes == intrinsic_type_desc::type<byte_t>()->size_in_bytes);

    const auto n = fp.n;
    if (buffer == nullptr || n >= buffer->get_length())
        throw out_of_range_memory{};

    // Initialize for failure.
    fp.ret->t0 = Sys_UTFerror;
    fp.ret->t1 = 0;
    fp.ret->t2 = 0;

    auto state = disvm::runtime::utf8::decode_state_t::accept;
    auto result = disvm::runtime::rune_t{};
    const auto buffer_len = buffer->get_length();
    for (auto i = n; i < buffer_len; ++i)
    {
        auto b = buffer->at<byte_t>(i);
        state = disvm::runtime::utf8::decode_step(state, result, b);
        if (state == disvm::runtime::utf8::decode_state_t::accept)
        {
            fp.ret->t0 = result;
            fp.ret->t1 = (i - n) + 1; // Number of bytes consumed
            fp.ret->t2 = 1;
            return;
        }
        else if (state == disvm::runtime::utf8::decode_state_t::reject)
        {
            fp.ret->t1 = 1;
            return;
        }
    }
}

void
Sys_char2byte(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_char2byte>();

    auto buffer = vm_alloc_t::from_allocation<vm_array_t>(fp.buf);
    assert(buffer->get_element_type()->size_in_bytes == intrinsic_type_desc::type<byte_t>()->size_in_bytes);

    const auto n = fp.n;
    if (buffer == nullptr || n >= buffer->get_length())
        throw out_of_range_memory{};

    const auto rune = static_cast<disvm::runtime::rune_t>(fp.c);

    static_assert(Sys_UTFmax == sizeof(rune), "UTF max should be same size as rune");
    uint8_t buffer_local[Sys_UTFmax];
    auto begin = buffer_local;

    auto end = disvm::runtime::utf8::encode(rune, begin);
    const auto count = static_cast<word_t>(end - begin);

    const auto final_index = n + count;
    if (final_index > buffer->get_length())
    {
        *fp.ret = 0;
        return;
    }

    // Insert the bytes into the buffer
    for (auto i = word_t{ n }; i < final_index; ++i)
    {
        buffer->at<uint8_t>(i) = *begin;
        begin++;
    }

    *fp.ret = count;
}

void
Sys_chdir(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_chdir>();
    throw vm_system_exception{ "Function not implemented" };
}

void
Sys_create(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_create>();

    dec_ref_count_and_free(vm_alloc_t::from_allocation(*fp.ret));
    *fp.ret = nullptr;

    auto path = vm_alloc_t::from_allocation<vm_string_t>(fp.s);
    const auto open_mode = convert_to_openmode(fp.mode);

    // [TODO] Apply the requested permissions to the file - fp.perm
    auto vm_fd = disvm::runtime::sys::create_from_file_path(path, open_mode);

    // Return null if file is not open.
    if (vm_fd == nullptr)
        return;

    auto fd_alloc = Sys_FD_Impl::create_new(vm_fd);
    *fp.ret = fd_alloc->get_allocation();

    if (disvm::debug::is_component_tracing_enabled<component_trace_t::builtin>())
    {
        auto fd_impl = fd_alloc->get_allocation<Sys_FD_Impl>();
        disvm::debug::log_msg(
            component_trace_t::builtin,
            log_level_t::debug,
            "sys: create: %d >>%s<< %#x %#x",
            fd_impl->fd,
            path->str(),
            fp.mode,
            fp.perm);
    }
}

void
Sys_dial(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_dial>();
    throw vm_system_exception{ "Function not implemented" };
}

void
Sys_dirread(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_dirread>();
    throw vm_system_exception{ "Function not implemented" };
}

namespace
{
    // [SPEC] In the Inferno OS, this error is represented by
    // Ebadfd - 'fd out of range or not open' (emu/port/error.c).
    auto invalid_fd_error = "Invalid FD value";
}

void
Sys_dup(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_dup>();

    const auto old_fd = fp.old;
    auto fd_maybe = disvm::runtime::sys::fetch_fd_record(old_fd);
    if (fd_maybe == nullptr)
        throw marshallable_user_exception{ invalid_fd_error };

    auto new_fd_maybe = fp.new_;
    if (new_fd_maybe == disvm::runtime::sys::vm_invalid_fd)
    {
        new_fd_maybe = disvm::runtime::sys::create_fd_record(fd_maybe);
    }
    else if (!disvm::runtime::sys::try_update_fd_record(new_fd_maybe, fd_maybe))
    {
        throw marshallable_user_exception{ invalid_fd_error };
    }

    *fp.ret = new_fd_maybe;

    if (disvm::debug::is_component_tracing_enabled<component_trace_t::builtin>())
        disvm::debug::log_msg(component_trace_t::builtin, log_level_t::debug, "sys: fd: dup %d %d", old_fd, new_fd_maybe);
}

void
Sys_export(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_export>();
    throw vm_system_exception{ "Function not implemented" };
}

void
Sys_fauth(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_fauth>();

    //disvm::runtime::dec_ref_count_and_free(vm_alloc_t::from_allocation(*fp.ret));
    //*fp.ret = nullptr;
    throw vm_system_exception{ "Function not implemented" };
}

void
Sys_fd2path(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_fd2path>();

    //disvm::runtime::dec_ref_count_and_free(vm_alloc_t::from_allocation(*fp.ret));
    //*fp.ret = nullptr;
    throw vm_system_exception{ "Function not implemented" };
}

void
Sys_fildes(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_fildes>();

    dec_ref_count_and_free(vm_alloc_t::from_allocation(*fp.ret));
    *fp.ret = nullptr;

    auto vm_fd = disvm::runtime::sys::fetch_fd_record(fp.fd);
    if (vm_fd != nullptr)
    {
        auto fd_alloc = Sys_FD_Impl::create_existing(fp.fd, vm_fd);
        *fp.ret = fd_alloc->get_allocation();
    }
}

void
Sys_file2chan(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_file2chan>();

    //disvm::runtime::dec_ref_count_and_free(vm_alloc_t::from_allocation(*fp.ret));
    //*fp.ret = nullptr;
    throw vm_system_exception{ "Function not implemented" };
}

void
Sys_fprint(vm_registers_t &r, vm_t &vm)
{
    auto fp_base = r.stack.peek_frame()->base();
    auto &fp = r.stack.peek_frame()->base<F_Sys_fprint>();
    auto fd_alloc = vm_alloc_t::from_allocation(fp.fd);
    assert(fd_alloc->alloc_type == T_FD);
    auto fd = fd_alloc->get_allocation<Sys_FD_Impl>();
    auto str = vm_alloc_t::from_allocation<vm_string_t>(fp.s);
    if (str == nullptr)
        throw dereference_nil{ "Print string to FD" };

    *fp.ret = printf_to_fd(vm, *fd->impl, *str, &fp.vargs, fp_base);
}

void
Sys_fstat(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_fstat>();
    throw vm_system_exception{ "Function not implemented" };
}

void
Sys_fversion(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_fversion>();
    throw vm_system_exception{ "Function not implemented" };
}

void
Sys_fwstat(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_fwstat>();
    throw vm_system_exception{ "Function not implemented" };
}

void
Sys_iounit(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_iounit>();
    throw vm_system_exception{ "Function not implemented" };
}

void
Sys_listen(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_listen>();
    throw vm_system_exception{ "Function not implemented" };
}

void
Sys_millisec(vm_registers_t &r, vm_t &vm)
{
    const auto current_time = std::chrono::steady_clock::now();
    const auto current_time_in_ms = std::chrono::duration_cast<std::chrono::milliseconds>(current_time.time_since_epoch());

    auto &fp = r.stack.peek_frame()->base<F_Sys_millisec>();
    *fp.ret = static_cast<word_t>(current_time_in_ms.count());
}

void
Sys_mount(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_mount>();
    throw vm_system_exception{ "Function not implemented" };
}

void
Sys_open(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_open>();

    dec_ref_count_and_free(vm_alloc_t::from_allocation(*fp.ret));
    *fp.ret = nullptr;

    auto path = vm_alloc_t::from_allocation<vm_string_t>(fp.s);
    const auto open_mode = convert_to_openmode(fp.mode);

    // [TODO] Handle opening devices - not just files (i.e. /dev/random, /dev/source, /proc/1234)
    auto vm_fd = disvm::runtime::sys::create_from_file_path(path, open_mode | open_mode_t::ensure_exists);

    // Return null if file is not open.
    if (vm_fd == nullptr)
        return;

    auto fd_alloc = Sys_FD_Impl::create_new(vm_fd);
    *fp.ret = fd_alloc->get_allocation();

    if (disvm::debug::is_component_tracing_enabled<component_trace_t::builtin>())
    {
        auto fd_impl = fd_alloc->get_allocation<Sys_FD_Impl>();
        disvm::debug::log_msg(
            component_trace_t::builtin,
            log_level_t::debug,
            "sys: open: %d >>%s<< %#x",
            fd_impl->fd,
            path->str(),
            fp.mode);
    }
}

void
Sys_pctl(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_pctl>();
    throw vm_system_exception{ "Function not implemented" };
}

void
Sys_pipe(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_pipe>();
    throw vm_system_exception{ "Function not implemented" };
}

void
Sys_pread(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_pread>();
    throw vm_system_exception{ "Function not implemented" };
}

void
Sys_print(vm_registers_t &r, vm_t &vm)
{
    auto fp_base = r.stack.peek_frame()->base();
    auto &fp = r.stack.peek_frame()->base<F_Sys_print>();
    auto str = vm_alloc_t::from_allocation<vm_string_t>(fp.s);
    if (str == nullptr)
        throw dereference_nil{ "Print string" };

    auto std_output = disvm::runtime::sys::fetch_fd_record(fd_stdout);
    if (std_output == nullptr)
        throw vm_system_exception{ "stdout not available" };

    *fp.ret = printf_to_fd(vm, *std_output, *str, &fp.vargs, fp_base);
    disvm::runtime::dec_ref_count_and_free(std_output);
}

void
Sys_pwrite(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_pwrite>();
    throw vm_system_exception{ "Function not implemented" };
}

void
Sys_read(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_read>();

    const auto buffer = vm_alloc_t::from_allocation<vm_array_t>(fp.buf);

    auto n = fp.n;
    if (buffer == nullptr || n <= 0)
    {
        *fp.ret = 0;
        return;
    }

    assert(buffer->get_element_type()->size_in_bytes == intrinsic_type_desc::type<byte_t>()->size_in_bytes);

    // [SPEC] Supplying a size greater than the buffer length is
    // equivalent to indicating the entire buffer should be filled.
    n = std::min(n, buffer->get_length());

    auto fd_alloc = vm_alloc_t::from_allocation(fp.fd);
    if (fd_alloc == nullptr)
        throw dereference_nil{ "Read from file descriptor" };

    assert(fd_alloc->alloc_type == T_FD);

    auto fd = fd_alloc->get_allocation<Sys_FD_Impl>();
    *fp.ret = fd->impl->read(vm, n, buffer->at(0));
}

void
Sys_readn(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_readn>();
    throw vm_system_exception{ "Function not implemented" };
}

void
Sys_remove(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_remove>();
    auto str = vm_alloc_t::from_allocation<vm_string_t>(fp.s);
    if (str == nullptr)
        throw dereference_nil{ "Remove path" };

    // [PAL] There are a lot of implementation details with this function.
    // Consider adding a PAL function to optimize per platform.
    *fp.ret = std::remove(str->str());
}

void
Sys_seek(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_seek>();

    auto fd_alloc = vm_alloc_t::from_allocation(fp.fd);
    if (fd_alloc == nullptr)
        throw dereference_nil{ "Seek in file descriptor" };

    assert(fd_alloc->alloc_type == T_FD);
    auto fd = fd_alloc->get_allocation<Sys_FD_Impl>();

    const auto start = convert_to_seekdir(fp.start);
    *fp.ret = fd->impl->seek(vm, start, fp.off);
}

void
Sys_sleep(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_sleep>();

    const auto period_ms = fp.period;

    // [TODO] Thread should yield to another according to the sleep() specification.
    std::this_thread::sleep_for(std::chrono::milliseconds(period_ms));

    *fp.ret = 0;
}

void
Sys_sprint(vm_registers_t &r, vm_t &vm)
{
    auto fp_base = r.stack.peek_frame()->base();
    auto &fp = r.stack.peek_frame()->base<F_Sys_sprint>();
    auto str = vm_alloc_t::from_allocation<vm_string_t>(fp.s);
    if (str == nullptr)
        throw dereference_nil{ "Print string to string" };

    vm_string_t *new_string;
    auto static_buffer = std::array<char, 128>{};
    auto msg_args = &fp.vargs;
    auto written = disvm::runtime::sys::printf_to_buffer(vm, *str, msg_args, fp_base, static_buffer.size(), static_buffer.data());
    if (written >= 0)
    {
        new_string = new vm_string_t{ static_cast<std::size_t>(written), reinterpret_cast<uint8_t *>(static_buffer.data()) };
    }
    else
    {
        auto dynamic_buffer = std::vector<char>(static_buffer.size() * 2);
        written = disvm::runtime::sys::printf_to_dynamic_buffer(vm, *str, msg_args, fp_base, dynamic_buffer);
        new_string = new vm_string_t{ static_cast<std::size_t>(written), reinterpret_cast<uint8_t *>(dynamic_buffer.data()) };
    }

    *fp.ret = new_string->get_allocation();
}

void
Sys_stat(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_stat>();
    throw vm_system_exception{ "Function not implemented" };
}

void
Sys_stream(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_stream>();
    throw vm_system_exception{ "Function not implemented" };
}

namespace
{
    bool is_delim(const disvm::runtime::rune_t c, const vm_string_t &delim)
    {
        const auto max = delim.get_length();

        // Optimization for 3 or less delimiters
        switch (max)
        {
        default:
            break;
        case 3:
            if (delim.get_rune(2) != c)
            {
        case 2:
                if (delim.get_rune(1) != c)
                {
        case 1:
                    if (delim.get_rune(0) != c)
                    {
        case 0:
                        return false;
                    }
                }
            }

            return true;
        }

        for (auto i = word_t{ 0 }; i < max; ++i)
        {
            if (delim.get_rune(i) == c)
                return true;
        }

        return false;
    }
}

void
Sys_tokenize(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_tokenize>();

    // Initialize for failure.
    fp.ret->t0 = 0;
    dec_ref_count_and_free(vm_alloc_t::from_allocation(fp.ret->t1));
    fp.ret->t1 = nullptr;

    auto s = vm_alloc_t::from_allocation<vm_string_t>(fp.s);
    if (s == nullptr || s->get_length() == 0)
        return;

    auto curr_node = new vm_list_t{ intrinsic_type_desc::type<pointer_t>() };

    // Set the head of the list
    fp.ret->t1 = curr_node->get_allocation();

    auto delim = vm_alloc_t::from_allocation<vm_string_t>(fp.delim);
    if (delim == nullptr || delim->get_length() == 0)
    {
        // Add supplied string to list
        disvm::runtime::pt_ref(curr_node->value()) = s->get_allocation();
        s->add_ref();

        fp.ret->t0 = 1;
        return;
    }

    // Tokenize the string
    vm_list_t *prev_node = nullptr;
    auto n = word_t{ 0 };
    auto begin = word_t{ 0 };
    auto end = word_t{ 0 };
    const auto len = s->get_length();
    while (begin < len)
    {
        // Consume delimiters
        while (begin < len)
        {
            const auto rune = s->get_rune(begin);
            if (!is_delim(rune, *delim))
                break;
            ++begin;
        }

        end = begin;

        // Consume non-delimiters
        while (end < len)
        {
            const auto rune = s->get_rune(end);
            if (is_delim(rune, *delim))
                break;
            ++end;
        }

        // No progress - It is and isn't a delimiter (i.e. no more characters).
        if (begin == end)
            break;

        // Current node is initially non-null
        if (curr_node == nullptr)
            curr_node = new vm_list_t{ intrinsic_type_desc::type<pointer_t>() };

        const auto token = new vm_string_t{ *s, begin, end };
        disvm::runtime::pt_ref(curr_node->value()) = token->get_allocation();

        // Previous node is initially null
        if (prev_node != nullptr)
        {
            prev_node->set_tail(curr_node);
            curr_node->release();
        }

        prev_node = curr_node;
        curr_node = nullptr;

        begin = end;
        ++n;
    }

    fp.ret->t0 = n;
}

void
Sys_unmount(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_unmount>();
    throw vm_system_exception{ "Function not implemented" };
}

void
Sys_utfbytes(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_utfbytes>();
    const auto buffer = vm_alloc_t::from_allocation<vm_array_t>(fp.buf);

    const auto n = fp.n;
    if (buffer == nullptr || n > buffer->get_length())
        throw out_of_range_memory{};

    assert(buffer->get_element_type()->size_in_bytes == intrinsic_type_desc::type<byte_t>()->size_in_bytes);

    auto raw_buffer = reinterpret_cast<uint8_t *>(buffer->at(0));

    const auto len = disvm::runtime::utf8::count_codepoints(raw_buffer, n);
    *fp.ret = static_cast<word_t>(len.byte_count);
    assert(*fp.ret <= n && "Code points should not exceed number of bytes processed");
}

void
Sys_werrstr(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_werrstr>();

    auto new_err = "";
    auto s = vm_alloc_t::from_allocation<vm_string_t>(fp.s);
    if (s != nullptr)
        new_err = s->str();

    disvm::runtime::push_syscall_error_message(vm, new_err);
    *fp.ret = 0;
}

void
Sys_write(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_write>();

    const auto buffer = vm_alloc_t::from_allocation<vm_array_t>(fp.buf);

    auto n = fp.n;
    if (buffer == nullptr || n <= 0)
    {
        *fp.ret = 0;
        return;
    }

    assert(buffer->get_element_type()->size_in_bytes == intrinsic_type_desc::type<byte_t>()->size_in_bytes);

    // [SPEC] Supplying a size greater than the buffer length is
    // equivalent to indicating the entire buffer should be written.
    n = std::min(n, buffer->get_length());

    auto fd_alloc = vm_alloc_t::from_allocation(fp.fd);
    if (fd_alloc == nullptr)
        throw dereference_nil{ "Write to file descriptor" };

    assert(fd_alloc->alloc_type == T_FD);

    auto fd = fd_alloc->get_allocation<Sys_FD_Impl>();
    fd->impl->write(vm, n, buffer->at(0));

    *fp.ret = n;
}

void
Sys_wstat(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_wstat>();
    throw vm_system_exception{ "Function not implemented" };
}