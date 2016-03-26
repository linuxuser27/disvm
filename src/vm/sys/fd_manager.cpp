//
// Dis VM
// File: fd_manager.cpp
// Author: arr
//

#include <array>
#include <mutex>
#include <unordered_map>
#include <exceptions.h>
#include <vm_memory.h>
#include "sys_utils.h"

using namespace disvm::runtime;

namespace
{
    using fd_map_t = std::unordered_map<word_t, vm_alloc_t *>;

    auto std_fd = std::array<vm_alloc_t * , 3>
    {
        nullptr, // stdin
        nullptr, // stdout
        nullptr, // stderr
    };

    auto next_fd = word_t{ 0 };

    std::mutex fd_map_lock;
    auto fd_map = fd_map_t{};
}

word_t disvm::runtime::sys::create_fd_record(vm_alloc_t * fd_alloc)
{
    assert(fd_alloc != nullptr);

    {
        std::lock_guard<std::mutex> lock{ fd_map_lock };

        auto new_fd = next_fd;
        next_fd++;
        if (new_fd < static_cast<word_t>(std_fd.size()))
        {
            std_fd[new_fd] = fd_alloc;
        }
        else
        {
            assert(fd_map.cend() == fd_map.find(new_fd));
            fd_map[new_fd] = fd_alloc;
        }

        return new_fd;
    }
}

vm_alloc_t * disvm::runtime::sys::fetch_fd_record(word_t fd)
{
    assert(fd != sys::vm_invalid_fd && "Invalid FD value");

    if (0 <= fd && fd < static_cast<word_t>(std_fd.size()))
        return std_fd[fd];

    {
        std::lock_guard<std::mutex> lock{ fd_map_lock };
        auto iter = fd_map.find(fd);
        if (iter == fd_map.cend())
            throw vm_user_exception{ "Unknown file descriptor" };

        return iter->second;
    }
}

void disvm::runtime::sys::drop_fd_record(word_t fd)
{
    assert(fd != sys::vm_invalid_fd && "Invalid FD value");
    assert(fd >= static_cast<word_t>(std_fd.size()) && "Request to drop std file?");

    {
        std::lock_guard<std::mutex> lock{ fd_map_lock };
        auto iter = fd_map.find(fd);
        if (iter != fd_map.cend())
            fd_map.erase(iter);
    }
}