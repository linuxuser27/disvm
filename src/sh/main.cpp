//
// Dis VM - Shell
// File: main.cpp
// Author: arr
//

#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <disvm.h>
#include <debug.h>

int main(int argc, char* argv[])
{
    auto module = "basic_limbo_tests.dis";
    //auto module = "C:\\inferno\\dis\\limbo.dis";
    //auto module = "empty.dis";

    if (argc >= 2)
        module = argv[1];

    clock_t t = std::clock();

    //disvm::debug::set_component_tracing(disvm::debug::component_trace_t::builtin, true);
    //disvm::debug::set_component_tracing(disvm::debug::component_trace_t::memory, true);
    //disvm::debug::set_component_tracing(disvm::debug::component_trace_t::module, true);
    //disvm::debug::set_component_tracing(disvm::debug::component_trace_t::exception, true);
    //disvm::debug::set_component_tracing(disvm::debug::component_trace_t::scheduler, true);
    //disvm::debug::set_component_tracing(disvm::debug::component_trace_t::channel, true);
    //disvm::debug::set_component_tracing(disvm::debug::component_trace_t::addressing, true);
    //disvm::debug::set_component_tracing(disvm::debug::component_trace_t::stack, true);
    //disvm::debug::set_component_tracing(disvm::debug::component_trace_t::thread, true);
    //disvm::debug::set_component_tracing(disvm::debug::component_trace_t::garbage_collector, true);
    disvm::debug::set_logging_callback([](const disvm::debug::component_trace_t, const disvm::debug::log_level_t l, const char *msg, std::va_list ls) { std::vprintf(msg, ls); });

    int c = 1;
    for (;;)
    {
        std::printf("####### VM test loop (%d) #######\n", c++);
        disvm::vm_t v{ 4, 2048 };

        for (auto i = 0; i < 10; ++i)
            v.exec(module);

        v.spin_sleep_till_idle(std::chrono::milliseconds(10));
    }

    t = std::clock() - t;
    std::printf("\n\n%d msec.\n", t);

    return 0;
}

