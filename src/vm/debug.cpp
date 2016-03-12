//
// Dis VM
// File: debug.cpp
// Author: arr
//

#include <mutex>
#include <array>
#include <debug.h>

namespace
{
    std::mutex log_mutex;
    disvm::debug::log_callback_t log_callback = nullptr;
}

std::array<bool, static_cast<std::size_t>(disvm::debug::component_trace_t::last) + 1> disvm::debug::trace_enabled = {};

disvm::debug::log_callback_t disvm::debug::set_logging_callback(log_callback_t cb)
{
    std::lock_guard<std::mutex> lock{ log_mutex };
    auto tmp = log_callback;
    log_callback = cb;
    return tmp;
}

void disvm::debug::log_msg(const component_trace_t origin, const log_level_t level, const char *msg_fmt, ...)
{
    if (log_callback == nullptr)
        return;

    if (!is_component_tracing_enabled(origin))
        return;

    std::va_list args;
    va_start(args, msg_fmt);

    {
        std::lock_guard<std::mutex> lock{ log_mutex };
        if (log_callback != nullptr)
            log_callback(origin, level, msg_fmt, args);
    }

    va_end(args);
}