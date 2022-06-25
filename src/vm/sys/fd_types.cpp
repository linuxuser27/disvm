//
// Dis VM
// File: fd_types.cpp
// Author: arr
//

#include <cstdio>
#include <algorithm>
#include <streambuf>

#include <debug.hpp>
#include <exceptions.hpp>
#include <vm_memory.hpp>
#include "fd_types.hpp"

using disvm::vm_t;

using disvm::debug::component_trace_t;
using disvm::debug::log_level_t;

using disvm::runtime::big_t;
using disvm::runtime::byte_t;
using disvm::runtime::managed_ptr_t;
using disvm::runtime::type_descriptor_t;
using disvm::runtime::vm_string_t;
using disvm::runtime::word_t;
using disvm::runtime::vm_system_exception;
using disvm::runtime::vm_syscall_exception;
using disvm::runtime::vm_user_exception;

using disvm::runtime::sys::open_mode_t;
using disvm::runtime::sys::seek_start_t;
using disvm::runtime::sys::std_streams;
using disvm::runtime::sys::vm_fd_t;

vm_fd_t::vm_fd_t(managed_ptr_t<const type_descriptor_t> td)
    : vm_alloc_t(std::move(td))
{ }

namespace
{
    namespace hidden_type_desc
    {
        const type_descriptor_t vm_fd_t{ 0, 0, nullptr, type_descriptor_t::no_finalizer, "vm_fd_t" };
    }
}

managed_ptr_t<const type_descriptor_t> vm_fd_t::type_desc()
{
    return managed_ptr_t<const type_descriptor_t>{ &hidden_type_desc::vm_fd_t };
}

namespace
{
    int seek_to_int(const seek_start_t m)
    {
        switch (m)
        {
        default:
            assert(false && "Unknown seek start type");
        case seek_start_t::start:
            return SEEK_SET;
        case seek_start_t::relative:
            return SEEK_CUR;
        case seek_start_t::end:
            return SEEK_END;
        }
    }

    class sys_fd_t final : public vm_fd_t
    {
    public:
        sys_fd_t(std::FILE *sys_fd, vm_string_t *fd_path, const open_mode_t fd_mode)
            : vm_fd_t(type_desc())
            , _fd_mode{ fd_mode }
            , _fd_path{ fd_path }
            , _sys_fd{ sys_fd }
        {
            assert(sys_fd != nullptr);

            if (_fd_path != nullptr)
                _fd_path->add_ref();
        }

        ~sys_fd_t()
        {
            std::fclose(_sys_fd);
            disvm::debug::assign_debug_pointer(&_sys_fd);

            // Honor the close flag
            if (disvm::util::has_flag(_fd_mode, open_mode_t::delete_on_close) && _fd_path != nullptr)
                std::remove(_fd_path->str());

            disvm::runtime::dec_ref_count_and_free(_fd_path);
            disvm::debug::assign_debug_pointer(&_fd_path);
        }

        word_t read(vm_t &vm, const word_t buffer_size_in_bytes, void *buffer) override
        {
            if (!disvm::util::has_flag(_fd_mode, open_mode_t::read))
                throw vm_system_exception{ "File descriptor not open for read operation" };

            auto read = std::fread(buffer, sizeof(byte_t), buffer_size_in_bytes, _sys_fd);
            if (std::ferror(_sys_fd) != 0)
                throw vm_syscall_exception{ vm, "Read operation error" };

            return static_cast<word_t>(read);
        }

        void write(vm_t &vm, const word_t buffer_size_in_bytes, void *buffer) override
        {
            if (!disvm::util::has_flag(_fd_mode, open_mode_t::write))
                throw vm_system_exception{ "File descriptor not open for write operation" };

            std::fwrite(buffer, sizeof(byte_t), buffer_size_in_bytes, _sys_fd);
            if (std::ferror(_sys_fd) != 0)
                throw vm_syscall_exception{ vm, "Write operation error" };
        }

        big_t seek(vm_t &vm, const seek_start_t seek_start, const big_t offset) override
        {
            if (disvm::util::has_flag(_fd_mode, open_mode_t::disable_seek))
                throw vm_user_exception{ "Unable to seek on a fifo file descriptor (e.g. stdin, stdout, stderr)" };

            auto result = std::fseek(_sys_fd, static_cast<long>(offset), seek_to_int(seek_start));
            if (result != 0)
                throw vm_syscall_exception{ vm, "Seek operation error" };

            return static_cast<big_t>(std::ftell(_sys_fd));
        }

    private:
        const open_mode_t _fd_mode;
        vm_string_t *_fd_path;
        std::FILE *_sys_fd;
    };
}

namespace
{
    std::mutex _create_file_path_mutex;

    const std::array<char *, 8> fopen_mode_strings =
    {
                  // R W T
        nullptr,  // 0 0 0
        "rb",     // 1 0 0
        "wb",     // 0 1 0
        "rwb",    // 1 1 0
        nullptr,  // 0 0 1
        "rb+",    // 1 0 1
        "wb+",    // 0 1 1
        "wb+",    // 1 1 1
    };

    const auto fopen_modes = (open_mode_t::read | open_mode_t::write | open_mode_t::truncate);

    constexpr const char *mode_for_fopen(const open_mode_t o)
    {
        return fopen_mode_strings[static_cast<std::size_t>(fopen_modes & o)];
    }
}

vm_fd_t *disvm::runtime::sys::create_from_file_path(vm_string_t *path, open_mode_t mode)
{
    std::unique_lock<std::mutex> lock{ _create_file_path_mutex, std::defer_lock };

    if ((mode & ~fopen_modes) != open_mode_t::none)
    {
        if (util::has_flag(mode, open_mode_t::atomic))
            lock.lock();

        if (util::has_flag(mode, (open_mode_t::ensure_create | open_mode_t::ensure_exists)))
        {
            assert(util::has_flag(mode, open_mode_t::ensure_create) != util::has_flag(mode, open_mode_t::ensure_exists) && "Create/Exists flags are mutually exclusive");
            // Check if the file already exists/permission to read
            auto tmp = std::fopen(path->str(), mode_for_fopen(open_mode_t::read));
            const auto exists = (tmp != nullptr);

            if (exists)
                std::fclose(tmp);

            if (exists && util::has_flag(mode, open_mode_t::ensure_create)
                || !exists && util::has_flag(mode, open_mode_t::ensure_exists))
            {
                return{};
            }
        }
    }

    auto fopen_mode = mode_for_fopen(mode);
    assert(fopen_mode != nullptr);

    std::FILE *file = std::fopen(path->str(), fopen_mode);
    if (file == nullptr)
        return{}; // [TODO] Report system error?

    return new sys_fd_t{ file, path, mode };
}

std_streams disvm::runtime::sys::get_std_streams()
{
    return
    {
        new sys_fd_t{ stdin, nullptr, (open_mode_t::read | open_mode_t::disable_seek) },
        new sys_fd_t{ stdout, nullptr, (open_mode_t::write | open_mode_t::disable_seek) },
        new sys_fd_t{ stderr, nullptr, (open_mode_t::write | open_mode_t::disable_seek) },
    };
}
