//
// Dis VM
// File: debug.h
// Author: arr
//

#ifndef _DISVM_SRC_INCLUDE_DEBUG_H_
#define _DISVM_SRC_INCLUDE_DEBUG_H_

#include <array>
#include <cstdarg>
#include <cassert>

namespace disvm
{
    namespace debug
    {
        // Components that have tracing
        enum class component_trace_t
        {
            builtin,
            module,
            thread,
            stack,
            channel,
            exception,
            memory, // Noisy component
            addressing, // Noisy component [DEBUG build only]

            // Potentially logged from a VM extension.
            scheduler,
            garbage_collector,

            // Tracking purposes
            last = garbage_collector
        };

        // Trace component enable state.
        extern std::array<bool, static_cast<std::size_t>(disvm::debug::component_trace_t::last) + 1> trace_enabled;

        // Set tracing for a component
        inline void set_component_tracing(const component_trace_t c, bool is_enabled)
        {
            trace_enabled[static_cast<const std::size_t>(c)] = is_enabled;
        }

        // Check if tracing is enabled on a component (compile time optimization)
        template<component_trace_t C>
        bool is_component_tracing_enabled()
        {
            return trace_enabled[static_cast<const std::size_t>(C)];
        }

        // Check if tracing is enabled on a component
        inline bool is_component_tracing_enabled(const component_trace_t c)
        {
            return trace_enabled[static_cast<const std::size_t>(c)];
        }

        // Message log level
        enum class log_level_t
        {
            warning,
            debug,
        };

        using log_callback_t = void(*)(const component_trace_t origin, const log_level_t level, const char *msg_fmt, std::va_list args);

        // Set the logging callback
        // Returns the previous callback.
        log_callback_t set_logging_callback(log_callback_t cb);

        // Log a message
        void log_msg(const component_trace_t origin, const log_level_t level, const char *msg_fmt, ...);

        //
        // Memory debugging
        //

        inline void assign_debug_memory(void *dest, std::size_t size_in_bytes)
        {
#ifndef NDEBUG
            const auto debug_memory = int{ 0xda }; // Dante Alighieri's initials
            std::memset(dest, debug_memory, size_in_bytes);
#endif
        }

        template<typename T>
        void assign_debug_pointer(T **t)
        {
#ifndef NDEBUG
            assert(t != nullptr);
            *t = reinterpret_cast<T *>(0xbada110c);
#endif
        }
    }
}

#endif // _DISVM_SRC_INCLUDE_DEBUG_H_