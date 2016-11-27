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

using namespace disvm;
using namespace disvm::runtime;

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

    class vm_fd_t final : public Sys_FD
    {
    public: // static
        static void finalizer(vm_alloc_t *fileHandle)
        {
            auto fd = fileHandle->get_allocation<vm_fd_t>();

            if (debug::is_component_tracing_enabled<debug::component_trace_t::builtin>())
            {
                debug::log_msg(
                    debug::component_trace_t::builtin,
                    debug::log_level_t::debug,
                    "sys: fd: finalize: %d\n",
                    fd->fd);
            }

            sys::drop_fd_record(fd->fd);
            fd->~vm_fd_t();
        }

        static std::tuple<word_t, vm_alloc_t *> create(std::unique_ptr<std::iostream> s, word_t fd_mode, vm_string_t *fd_path)
        {
            assert(s != nullptr);
            auto alloc = vm_alloc_t::allocate(T_FD);
            const auto fd = sys::create_fd_record(alloc);
            auto vm_fd = ::new(alloc->get_allocation())vm_fd_t{ fd, std::move(s), fd_mode, fd_path };

            return std::make_tuple(vm_fd->fd, alloc);
        }

    private:
        vm_fd_t(const word_t fd, std::unique_ptr<std::iostream> s, const word_t fd_mode, vm_string_t *fd_path)
            : Sys_FD{ fd }
            , _fd_mode{ fd_mode }
            , _fd_path{ fd_path }
            , _istream{ s.get() }
            , _ostream{ s.get() }
        {
            _stream = std::move(s);

            if (_fd_path != nullptr)
                _fd_path->add_ref();
        }

    public:
        vm_fd_t(const word_t fd, std::istream &is)
            : Sys_FD{ fd }
            , _fd_mode{}
            , _fd_path{ nullptr }
            , _istream{ &is }
            , _ostream{ nullptr }
        {
        }

        vm_fd_t(const word_t fd, std::ostream &os)
            : Sys_FD{ fd }
            , _fd_mode{}
            , _fd_path{ nullptr }
            , _istream{ nullptr }
            , _ostream{ &os }
        {
        }

        ~vm_fd_t()
        {
            debug::assign_debug_pointer(&_istream);
            debug::assign_debug_pointer(&_ostream);
            if (_stream != nullptr)
                _stream.reset();

            // Honor the close flag
            if ((_fd_mode & Sys_ORCLOSE) && _fd_path != nullptr)
                std::remove(_fd_path->str());

            dec_ref_count_and_free(_fd_path);
            debug::assign_debug_pointer(&_fd_path);
        }

        word_t get_original_mode() const
        {
            return _fd_mode;
        }

        vm_string_t *get_original_path() const
        {
            return _fd_path;
        }

        word_t read(const word_t buffer_size_in_bytes, void *buffer)
        {
            if (_istream == nullptr)
                throw vm_system_exception{ "File descriptor not open for read operation" };

            _istream->read(static_cast<char *>(buffer), static_cast<const std::streamsize>(buffer_size_in_bytes));
            if (_istream->bad())
                throw vm_user_exception{ "Read operation error" };

            return static_cast<word_t>(_istream->gcount());
        }

        void write(const word_t buffer_size_in_bytes, void *buffer)
        {
            if (_ostream == nullptr)
                throw vm_system_exception{ "File descriptor not open for write operation" };

            _ostream->write(static_cast<char *>(buffer), static_cast<const std::streamsize>(buffer_size_in_bytes));
            if (_ostream->bad())
                throw vm_user_exception{ "Write operation error" };
        }

    private:
        const word_t _fd_mode;
        vm_string_t *_fd_path;
        std::istream *_istream;
        std::ostream *_ostream;
        std::unique_ptr<std::ios> _stream;
    };

    vm_fd_t *fd_stdin = nullptr;
    vm_fd_t *fd_stdout = nullptr;
    vm_fd_t *fd_stderr = nullptr;

    std::ios::openmode convert_to_openmode(const word_t mode)
    {
        auto om = std::ios::openmode{};

        // Lowest bit is for read/write.
        if (mode & Sys_OWRITE)
            om |= std::ios::out;
        else
            om |= std::ios::in; // Sys_OREAD

        if (mode & Sys_ORDWR)
            om |= (std::ios::in | std::ios::out);

        if (mode & Sys_OTRUNC)
            om |= std::ios::trunc;

        return om;
    }
}

void
Sysmodinit(void)
{
    builtin::register_module_exports(Sys_PATH, Sysmodlen, Sysmodtab);
    T_Qid = type_descriptor_t::create(sizeof(Sys_Qid), sizeof(Sys_Qid_map), Sys_Qid_map);
    T_Dir = type_descriptor_t::create(sizeof(Sys_Dir), sizeof(Sys_Dir_map), Sys_Dir_map);
    T_FD = type_descriptor_t::create(sizeof(vm_fd_t), sizeof(Sys_FD_map), Sys_FD_map, vm_fd_t::finalizer);
    T_Connection = type_descriptor_t::create(sizeof(Sys_Connection), sizeof(Sys_Connection_map), Sys_Connection_map);
    T_FileIO = type_descriptor_t::create(sizeof(Sys_FileIO), sizeof(Sys_FileIO_map), Sys_FileIO_map);

    // Initialize default file descriptors

    {
        auto vm_stdin = vm_alloc_t::allocate(T_FD);
        const auto fd = sys::create_fd_record(vm_stdin);
        fd_stdin = ::new(vm_stdin->get_allocation())vm_fd_t{ fd, std::cin };
        debug::log_msg(debug::component_trace_t::builtin, debug::log_level_t::debug, "sys: stdin: %d\n", fd);
    }

    {
        auto vm_stdout = vm_alloc_t::allocate(T_FD);
        const auto fd = sys::create_fd_record(vm_stdout);
        fd_stdout = ::new(vm_stdout->get_allocation())vm_fd_t{ fd, std::cout };
        debug::log_msg(debug::component_trace_t::builtin, debug::log_level_t::debug, "sys: stdout: %d\n", fd);
    }

    {
        auto vm_stderr = vm_alloc_t::allocate(T_FD);
        const auto fd = sys::create_fd_record(vm_stderr);
        fd_stderr = ::new(vm_stderr->get_allocation())vm_fd_t{ fd, std::cerr };
        debug::log_msg(debug::component_trace_t::builtin, debug::log_level_t::debug, "sys: stderr: %d\n", fd);
    }
}

void
Sys_announce(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_announce>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Sys_aprint(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_aprint>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Sys_bind(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_bind>();
    throw vm_system_exception{ "Instruction not implemented" };
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

    auto state = utf8::decode_state_t::accept;
    auto result = runtime::rune_t{};
    const auto buffer_len = buffer->get_length();
    for (auto i = n; i < buffer_len; ++i)
    {
        auto b = buffer->at<byte_t>(i);
        state = utf8::decode_step(state, result, b);
        if (state == utf8::decode_state_t::accept)
        {
            fp.ret->t0 = result;
            fp.ret->t1 = (i - n) + 1; // Number of bytes consumed
            fp.ret->t2 = 1;
            return;
        }
        else if (state == utf8::decode_state_t::reject)
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

    const auto rune = static_cast<runtime::rune_t>(fp.c);

    static_assert(Sys_UTFmax == sizeof(rune), "UTF max should be same size as rune");
    uint8_t buffer_local[Sys_UTFmax];
    auto begin = buffer_local;

    auto end = utf8::encode(rune, begin);
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
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Sys_create(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_create>();

    auto path = vm_alloc_t::from_allocation<vm_string_t>(fp.s);
    const auto open_mode = convert_to_openmode(fp.mode);

    auto cfs_flag = sys::cfs_flags_t::none;
    if (fp.mode & Sys_OEXCL)
        cfs_flag = (sys::cfs_flags_t::atomic | sys::cfs_flags_t::ensure_create);

    // [TODO] Apply the requested permissions to the file.
    auto file_stream = sys::create_file_stream(path->str(), (open_mode | std::ios::binary), cfs_flag);

    // Return null if file stream is not open.
    if (file_stream == nullptr)
    {
        *fp.ret = nullptr;
        return;
    }

    vm_alloc_t *fd_alloc;
    auto fd = word_t{};
    std::tie(fd, fd_alloc) = vm_fd_t::create(std::move(file_stream), fp.mode, path);

    if (debug::is_component_tracing_enabled<debug::component_trace_t::builtin>())
    {
        debug::log_msg(
            debug::component_trace_t::builtin,
            debug::log_level_t::debug,
            "sys: create: %d >>%s<< %#x %#x\n",
            fd,
            path->str(),
            fp.mode,
            fp.perm);
    }

    *fp.ret = fd_alloc->get_allocation();
}

void
Sys_dial(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_dial>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Sys_dirread(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_dirread>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Sys_dup(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_dup>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Sys_export(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_export>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Sys_fauth(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_fauth>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Sys_fd2path(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_fd2path>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Sys_fildes(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_fildes>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Sys_file2chan(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_file2chan>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Sys_fprint(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_fprint>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Sys_fstat(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_fstat>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Sys_fversion(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_fversion>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Sys_fwstat(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_fwstat>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Sys_iounit(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_iounit>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Sys_listen(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_listen>();
    throw vm_system_exception{ "Instruction not implemented" };
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
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Sys_open(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_open>();

    auto path = vm_alloc_t::from_allocation<vm_string_t>(fp.s);
    const auto open_mode = convert_to_openmode(fp.mode);

    // [TODO] Handle opening devices - not just files (i.e. /dev/random, /dev/source, /proc/1234)
    auto file_stream = sys::create_file_stream(path->str(), (open_mode | std::ios::binary));

    // Return null if file stream is not open.
    if (file_stream == nullptr)
    {
        *fp.ret = nullptr;
        return;
    }

    vm_alloc_t *fd_alloc;
    auto fd = word_t{};
    std::tie(fd, fd_alloc) = vm_fd_t::create(std::move(file_stream), fp.mode, path);

    if (debug::is_component_tracing_enabled<debug::component_trace_t::builtin>())
    {
        debug::log_msg(
            debug::component_trace_t::builtin,
            debug::log_level_t::debug,
            "sys: open: %d >>%s<< %#x\n",
            fd,
            path->str(),
            fp.mode);
    }

    *fp.ret = fd_alloc->get_allocation();
}

void
Sys_pctl(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_pctl>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Sys_pipe(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_pipe>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Sys_pread(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_pread>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Sys_print(vm_registers_t &r, vm_t &vm)
{
    auto fp_base = r.stack.peek_frame()->base();
    auto &fp = r.stack.peek_frame()->base<F_Sys_print>();
    auto str = vm_alloc_t::from_allocation<vm_string_t>(fp.s);

    if (str == nullptr)
        throw dereference_nil{ "Print string" };

    auto static_buffer = std::array<char, 1024>{};
    auto msg_args = &fp.vargs;
    auto written = sys::printf_to_buffer(*str, msg_args, fp_base, static_buffer.size(), static_buffer.data());
    if (written >= 0)
    {
        fd_stdout->write(written, static_buffer.data());
    }
    else
    {
        auto dynamic_buffer = std::vector<char>(static_buffer.size() * 2);
        written = sys::printf_to_dynamic_buffer(*str, msg_args, fp_base, dynamic_buffer);
        fd_stdout->write(written, dynamic_buffer.data());
    }

    *fp.ret = written;
}

void
Sys_pwrite(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_pwrite>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Sys_read(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_read>();

    const auto buffer = vm_alloc_t::from_allocation<vm_array_t>(fp.buf);
    assert(buffer->get_element_type()->size_in_bytes == intrinsic_type_desc::type<byte_t>()->size_in_bytes);

    auto n = fp.n;
    if (buffer == nullptr || n <= 0)
    {
        *fp.ret = 0;
        return;
    }

    // [SPEC] Supplying a size greater than the buffer length is
    // equivalent to indicating the entire buffer should be filled.
    n = std::min(n, buffer->get_length());

    auto fd_alloc = vm_alloc_t::from_allocation(fp.fd);
    assert(fd_alloc->alloc_type == T_FD);

    auto fd = fd_alloc->get_allocation<vm_fd_t>();
    *fp.ret = fd->read(n, buffer->at(0));
}

void
Sys_readn(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_readn>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Sys_remove(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_remove>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Sys_seek(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_seek>();
    throw vm_system_exception{ "Instruction not implemented" };
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
    auto &fp = r.stack.peek_frame()->base<F_Sys_sprint>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Sys_stat(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_stat>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Sys_stream(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_stream>();
    throw vm_system_exception{ "Instruction not implemented" };
}

namespace
{
    bool is_delim(const runtime::rune_t c, const vm_string_t &delim)
    {
        const auto max = delim.get_length();
        assert(max > 0);

        // Optimization for 3 or less delimiters
        switch (max)
        {
        default:
            break;
        case 1:
            return (delim.get_rune(0) == c);
        case 2:
            return (delim.get_rune(0) == c) || (delim.get_rune(1) == c);
        case 3:
            return (delim.get_rune(0) == c) || (delim.get_rune(1) == c) || (delim.get_rune(2) == c);
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
        pt_ref(curr_node->value()) = s->get_allocation();
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
        pt_ref(curr_node->value()) = token->get_allocation();

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
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Sys_utfbytes(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_utfbytes>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Sys_werrstr(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_werrstr>();
    throw vm_system_exception{ "Instruction not implemented" };
}

void
Sys_write(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_write>();

    const auto buffer = vm_alloc_t::from_allocation<vm_array_t>(fp.buf);
    assert(buffer->get_element_type()->size_in_bytes == intrinsic_type_desc::type<byte_t>()->size_in_bytes);

    auto n = fp.n;
    if (buffer == nullptr || n <= 0)
    {
        *fp.ret = 0;
        return;
    }

    // [SPEC] Supplying a size greater than the buffer length is
    // equivalent to indicating the entire buffer should be written.
    n = std::min(n, buffer->get_length());

    auto fd_alloc = vm_alloc_t::from_allocation(fp.fd);
    assert(fd_alloc->alloc_type == T_FD);

    auto fd = fd_alloc->get_allocation<vm_fd_t>();
    fd->write(n, buffer->at(0));

    *fp.ret = n;
}

void
Sys_wstat(vm_registers_t &r, vm_t &vm)
{
    auto &fp = r.stack.peek_frame()->base<F_Sys_wstat>();
    throw vm_system_exception{ "Instruction not implemented" };
}