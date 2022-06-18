//
// Dis VM
// File: fd_manager.cpp
// Author: arr
//

#include <array>
#include <mutex>
#include <vector>
#include <queue>
#include <exceptions.hpp>
#include <vm_memory.hpp>
#include "sys_utils.hpp"

using disvm::runtime::vm_alloc_t;
using disvm::runtime::word_t;

using disvm::runtime::sys::vm_fd_t;

namespace
{
    std::mutex fd_map_lock;
    auto fd_map = std::vector<vm_fd_t *>{};
    auto fd_available = std::priority_queue<word_t, std::vector<word_t>, std::greater<word_t>>{};
}

word_t disvm::runtime::sys::create_fd_record(vm_fd_t * vm_fd)
{
    assert(vm_fd != nullptr);

    std::lock_guard<std::mutex> lock{ fd_map_lock };

    word_t new_fd;
    if (fd_available.empty())
    {
        new_fd = fd_map.size();
        fd_map.push_back(vm_fd);
    }
    else
    {
        new_fd = fd_available.top();
        fd_available.pop();

        assert(fd_map[new_fd] == nullptr && "Reused FD should be null");
        fd_map[new_fd] = vm_fd;
    }

    return new_fd;
}

bool disvm::runtime::sys::try_update_fd_record(word_t fd, vm_fd_t *vm_fd)
{
    assert(fd != sys::vm_invalid_fd);
    assert(vm_fd != nullptr);

    std::lock_guard<std::mutex> lock{ fd_map_lock };
    if (fd < 0 || static_cast<word_t>(fd_map.size()) <= fd)
        return false;

    auto vm_fd_curr = fd_map[fd];
    auto current_ref = vm_fd_curr->release();
    if (current_ref == 0)
        delete vm_fd_curr;

    fd_map[fd] = vm_fd;
    return true;
}

vm_fd_t * disvm::runtime::sys::fetch_fd_record(word_t fd)
{
    assert(fd != sys::vm_invalid_fd);

    std::lock_guard<std::mutex> lock{ fd_map_lock };
    if (fd < 0 || static_cast<word_t>(fd_map.size()) <= fd)
        return nullptr;

    auto vm_fd = fd_map[fd];
    if (vm_fd != nullptr)
        vm_fd->add_ref();

    return vm_fd;
}

void disvm::runtime::sys::drop_fd_record(word_t fd)
{
    assert(fd != sys::vm_invalid_fd);

    std::lock_guard<std::mutex> lock{ fd_map_lock };
    if (fd < 0 || static_cast<word_t>(fd_map.size()) <= fd)
        return;

    assert(fd_map[fd] != nullptr);

    // Manual management of file descriptors, since there is an inherent life time
    // race amoung the enclosing ADT instances that can have a reference.
    auto vm_fd = fd_map[fd];
    auto current_ref = vm_fd->release();
    if (current_ref == 0)
    {
        delete vm_fd;
        fd_map[fd] = nullptr;
        fd_available.push(fd);
    }
}
