//
// Dis VM
// File: file_system.cpp
// Author: arr
//

#include <iostream>
#include <fstream>
#include <mutex>
#include <utils.h>
#include "sys_utils.h"

namespace
{
    std::mutex _create_file_stream_mutex;
}

std::unique_ptr<std::fstream> disvm::runtime::sys::create_file_stream(const char *path, std::ios::openmode mode, cfs_flags_t flags)
{
    auto file = std::unique_ptr<std::fstream>{};
    if (flags == cfs_flags_t::none)
    {
        file = std::make_unique<std::fstream>(path, mode);
    }
    else
    {
        std::unique_lock<std::mutex> lock{ _create_file_stream_mutex, std::defer_lock };
        if (util::has_flag(flags, cfs_flags_t::atomic))
            lock.lock();

        if (util::has_flag(flags, cfs_flags_t::ensure_create))
        {
            std::fstream file_exists{ path };
            if (file_exists)
                return{};
        }

        file = std::make_unique<std::fstream>(path, mode);
    }

    // Return null if file stream is not open.
    if (!file->is_open())
        return{};

    return file;
}
