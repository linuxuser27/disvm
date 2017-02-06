//
// Dis VM
// File: scheduler.cpp
// Author: arr
//

#include <debug.h>
#include <iostream>
#include <sstream>
#include <queue>
#include <exceptions.h>
#include "scheduler.h"

using disvm::vm_t;

using disvm::debug::component_trace_t;
using disvm::debug::log_level_t;

using disvm::runtime::vm_thread_t;
using disvm::runtime::vm_scheduler_t;
using disvm::runtime::vm_scheduler_control_t;
using disvm::runtime::vm_tool_dispatch_t;
using disvm::runtime::vm_thread_state_t;
using disvm::runtime::default_scheduler_t;

// Empty destructors for vm scheduler 'interfaces'
vm_scheduler_t::~vm_scheduler_t()
{
}

vm_scheduler_control_t::~vm_scheduler_control_t()
{
}

void default_scheduler_t::worker_main(default_scheduler_t &instance)
{
    disvm::debug::log_msg(component_trace_t::scheduler, log_level_t::debug, "scheduler: worker: start");

    register_system_thread(instance._vm);
    auto current_thread = std::shared_ptr<thread_instance_t>{};

    try
    {
        for (;;)
        {
            current_thread = instance.next_thread(std::move(current_thread));
            if (current_thread == nullptr)
            {
                disvm::debug::log_msg(component_trace_t::scheduler, log_level_t::debug, "scheduler: worker: stop");
                break;
            }

            if (disvm::debug::is_component_tracing_enabled<component_trace_t::scheduler>())
                disvm::debug::log_msg(component_trace_t::scheduler, log_level_t::debug, "scheduler: worker: execute: %d", current_thread->vm_thread->get_thread_id());

            current_thread->vm_thread->execute(instance._vm, instance._vm_thread_quanta);
        }
    }
    catch (const vm_term_request &te)
    {
        std::cerr << te.what() << std::endl;

        instance._terminating = true;
        instance._worker_event.notify_all();
    }
    catch (const vm_system_exception &se)
    {
        std::stringstream err_msg;
        err_msg << se.what() << "\n";

        if (current_thread != nullptr)
        {
            const auto &r = current_thread->vm_thread->get_registers();
            walk_stack(r, [&err_msg](const pointer_t, const vm_pc_t pc, const vm_module_ref_t &module_ref) {
                auto module_name = "<No Name>";
                if (module_ref.module->module_name != nullptr)
                    module_name = module_ref.module->module_name->str();

                err_msg << "    " << module_name << " @" << pc << "\n";
                return true;
            });
        }

        auto err_str = err_msg.str();
        std::cerr << err_str.c_str() << std::endl;

        instance._terminating = true;
        instance._worker_event.notify_all();
    }

    unregister_system_thread(instance._vm);
}

default_scheduler_t::default_scheduler_t(vm_t &vm, uint32_t system_thread_count, uint32_t thread_quanta)
    : _gc_complete{ true }
    , _gc_counter{ 1 }
    , _running_vm_thread_count{ 0 }
    , _terminating{ false }
    , _worker_thread_count{ system_thread_count }
    , _vm{ vm }
    , _vm_thread_quanta{ thread_quanta }
{
    if (_worker_thread_count == 0)
        throw vm_system_exception{ "Work thread count must be > 0" };

    if (_vm_thread_quanta == 0)
        throw vm_system_exception{ "Work thread quanta must be > 0" };
}

default_scheduler_t::~default_scheduler_t()
{
    // Drain the runnable queue and clear out all the threads
    {
        std::lock_guard<std::mutex> lock{ _vm_threads_lock };
        _terminating = true;

        _runnable_vm_thread_ids.clear();
        _all_vm_threads.clear();
    }

    // Notify all worker threads
    _worker_event.notify_all();

    // Wait for all worker threads to finish
    for (auto &w : _worker_pool)
        w.join();

    disvm::debug::log_msg(component_trace_t::scheduler, log_level_t::debug, "scheduler: shutdown");
}

bool default_scheduler_t::is_idle() const
{
    std::lock_guard<std::mutex> lock{ _vm_threads_lock };
    return (_terminating) || (_running_vm_thread_count == 0 && _runnable_vm_thread_ids.empty() && _blocked_vm_thread_ids.empty());
}

vm_scheduler_control_t &default_scheduler_t::get_controller() const
{
    return const_cast<default_scheduler_t&>(*this);
}

void default_scheduler_t::schedule_thread(std::unique_ptr<vm_thread_t> thread)
{
    assert(thread != nullptr);
    if (thread->get_registers().current_thread_state != vm_thread_state_t::ready)
        throw vm_system_exception{ "Scheduled thread in invalid state" };

    if (disvm::debug::is_component_tracing_enabled<component_trace_t::scheduler>())
        disvm::debug::log_msg(component_trace_t::scheduler, log_level_t::debug, "scheduler: scheduled: %d", thread->get_thread_id());

    std::unique_lock<std::mutex> lock{ _vm_threads_lock };

    // Create workers on first scheduled thread
    if (_worker_pool.empty())
    {
        for (auto i = uint32_t{ 0 }; i < _worker_thread_count; ++i)
            _worker_pool.push_back(std::thread{ default_scheduler_t::worker_main, std::ref(*this) });
    }

    const auto new_thread_id = thread->get_thread_id();
    assert(_all_vm_threads.find(new_thread_id) == _all_vm_threads.cend());

    // Allocate a new container and set the thread entry
    _all_vm_threads[new_thread_id] = std::make_shared<thread_instance_t>(std::move(thread));

    _runnable_vm_thread_ids.push_back(new_thread_id);

    // Notify the worker a thread has been enqueued.
    lock.unlock();
    _worker_event.notify_one();
}

void default_scheduler_t::set_tool_dispatch_on_all_threads(vm_tool_dispatch_t *dispatch)
{
    std::unique_lock<std::mutex> lock{ _vm_threads_lock };

    auto threads_to_set = std::queue<uint32_t>{};

    // Set all threads possible and queue the rest
    for (auto entry : _all_vm_threads)
    {
        auto t = entry.second->vm_thread;
        if (!entry.second->system_thread_ownership.try_lock())
        {
            threads_to_set.push(t->get_thread_id());
            continue;
        }

        std::lock_guard<std::mutex> lock_thread{ entry.second->system_thread_ownership, std::adopt_lock };
        t->set_tool_dispatch(dispatch);
    }

    // Loop until all threads have been set
    auto count = threads_to_set.size();
    while (!threads_to_set.empty())
    {
        if (count == 0)
        {
            // Reset count and sleep since all the current threads have been attempted.
            count = threads_to_set.size();

            // Release the vm threads lock so they can make progress
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds{ 50 });
            lock.lock();
        }

        const auto thread_id = threads_to_set.front();
        threads_to_set.pop();
        count--;

        const auto iter = _all_vm_threads.find(thread_id);

        // The thread to set could have been removed while we were sleeping above
        if (iter == _all_vm_threads.cend())
            continue;

        if (!iter->second->system_thread_ownership.try_lock())
        {
            threads_to_set.push(thread_id);
            continue;
        }

        std::lock_guard<std::mutex> lock_thread{ iter->second->system_thread_ownership, std::adopt_lock };
        iter->second->vm_thread->set_tool_dispatch(dispatch);
    }
}

void default_scheduler_t::enqueue_blocked_thread(uint32_t thread_id)
{
    all_thread_map_t::iterator thread_to_enqueue;
    {
        std::lock_guard<std::mutex> lock{ _vm_threads_lock };
        thread_to_enqueue = _all_vm_threads.find(thread_id);
        if (thread_to_enqueue == _all_vm_threads.cend())
        {
            assert(false && "Unknown thread to enqueue");
            return;
        }
    }

    auto thread_container = thread_to_enqueue->second;

    // Lock the vm thread collections and this vm thread
    std::lock(_vm_threads_lock, thread_container->system_thread_ownership);
    std::unique_lock<std::mutex> lock_all{ _vm_threads_lock, std::adopt_lock };
    std::lock_guard<std::mutex> lock_thread{ thread_container->system_thread_ownership, std::adopt_lock };

    // If the thread is not blocked, just return.
    auto blocked_iter = _blocked_vm_thread_ids.find(thread_id);
    if (blocked_iter == _blocked_vm_thread_ids.cend())
    {
        assert(false && "Thread to enqueue not blocked");
        return;
    }

    _blocked_vm_thread_ids.erase(blocked_iter);

    if (disvm::debug::is_component_tracing_enabled<component_trace_t::scheduler>())
        disvm::debug::log_msg(component_trace_t::scheduler, log_level_t::debug, "scheduler: enqueue blocked: %d", thread_id);

    const bool runnable_thread = enqueue_thread_unsafe(thread_container.get(), vm_thread_state_t::ready);
    if (runnable_thread)
    {
        lock_all.unlock();
        _worker_event.notify_one();
    }
}

std::size_t default_scheduler_t::get_system_thread_count() const
{
    return _worker_thread_count;
}

std::vector<std::shared_ptr<const vm_thread_t>> default_scheduler_t::get_all_threads() const
{
    auto result = std::vector<std::shared_ptr<const vm_thread_t>>{};

    {
        std::lock_guard<std::mutex> lock{ _vm_threads_lock };
        for (auto &p : _all_vm_threads)
            result.push_back(p.second->vm_thread);
    }

    return result;
}

std::shared_ptr<default_scheduler_t::thread_instance_t> default_scheduler_t::next_thread(std::shared_ptr<thread_instance_t> prev_thread)
{
    auto next_thread = std::shared_ptr<thread_instance_t>{};

    // Check if the passed in thread should be enqueued
    if (prev_thread != nullptr)
    {
        const auto current_state = prev_thread->vm_thread->get_registers().current_thread_state;

        // This system thread releases ownership of the vm thread after it has been enqueued
        std::lock_guard<std::mutex> vm_thread_ownership{ prev_thread->system_thread_ownership, std::adopt_lock };

        std::unique_lock<std::mutex> lock_all{ _vm_threads_lock };
        --_running_vm_thread_count;
        const bool runnable_thread = enqueue_thread_unsafe(prev_thread.get(), current_state);
        if (runnable_thread)
        {
            lock_all.unlock();
            _worker_event.notify_one();
        }
    }

    for (;;)
    {
        // Check for a vm thread in the queue
        std::unique_lock<std::mutex> lock{ _vm_threads_lock };

        // Check if a GC should be performed.
        // Matching part of the logic in Inferno (emu/port/dis.c).
        if ((_gc_counter & 0xff) == 0)
        {
            const auto running_thread_count_local = _running_vm_thread_count;
            const auto is_gc_thread = running_thread_count_local == 0;
            perform_gc(is_gc_thread, lock);
        }

        if (!_runnable_vm_thread_ids.empty())
        {
            const auto thread_id = _runnable_vm_thread_ids.front();
            _runnable_vm_thread_ids.pop_front();

            auto iter = _all_vm_threads.find(thread_id);
            assert(iter != _all_vm_threads.cend());
            next_thread = iter->second;

            ++_running_vm_thread_count;
            ++_gc_counter;

            // This system thread now takes ownership of the vm thread
            next_thread->system_thread_ownership.lock();
            return next_thread;
        }

        if (!_all_vm_threads.empty() && _blocked_vm_thread_ids.size() == _all_vm_threads.size())
        {
            disvm::debug::log_msg(component_trace_t::scheduler, log_level_t::warning, "scheduler: deadlock detected");
            assert(false && "VM thread deadlock detected");
        }

        if (_terminating)
            return{};

        if (disvm::debug::is_component_tracing_enabled<component_trace_t::scheduler>())
            disvm::debug::log_msg(component_trace_t::scheduler, log_level_t::debug, "scheduler: worker: waiting");

        _worker_event.wait(lock);

        if (_terminating)
            return{};
    }
}

void default_scheduler_t::perform_gc(bool is_gc_thread, std::unique_lock<std::mutex> &all_vm_threads_lock)
{
    if (!is_gc_thread)
    {
        _gc_complete = false;
        all_vm_threads_lock.unlock();
        std::unique_lock<std::mutex> lock{ _gc_wait };
        _gc_done_event.wait(lock, [&]{ return _gc_complete.load(); });

        // After the gc completes, take back the lock.
        all_vm_threads_lock.lock();
    }
    else
    {
        auto result = std::vector<std::shared_ptr<const vm_thread_t>>{};
        for (auto &p : _all_vm_threads)
        {
            auto &vm_thread = p.second->vm_thread;
            if (vm_thread->get_registers().current_thread_state != vm_thread_state_t::broken)
                result.push_back(vm_thread);
        }

        _vm.get_garbage_collector().collect(std::move(result));
        _gc_complete = true;
        _gc_done_event.notify_all();
    }
}

bool default_scheduler_t::enqueue_thread_unsafe(thread_instance_t *thread_instance, vm_thread_state_t current_state)
{
    assert(thread_instance != nullptr);

    const auto thread_id = thread_instance->vm_thread->get_thread_id();
    switch (current_state)
    {
    case vm_thread_state_t::ready:
    case vm_thread_state_t::debug:
    {
        _runnable_vm_thread_ids.push_back(thread_id);

        if (disvm::debug::is_component_tracing_enabled<component_trace_t::scheduler>())
            disvm::debug::log_msg(component_trace_t::scheduler, log_level_t::debug, "scheduler: enqueue: %d", thread_id);

        return true;
    }

    case vm_thread_state_t::blocked_in_alt:
    case vm_thread_state_t::blocked_sending:
    case vm_thread_state_t::blocked_receiving:
    {
        _blocked_vm_thread_ids.insert(thread_id);

        if (disvm::debug::is_component_tracing_enabled<component_trace_t::scheduler>())
            disvm::debug::log_msg(component_trace_t::scheduler, log_level_t::debug, "scheduler: blocked: %d", thread_id);

        break;
    }

    case vm_thread_state_t::empty_stack:
    case vm_thread_state_t::exiting:
    {
        if (disvm::debug::is_component_tracing_enabled<component_trace_t::scheduler>())
            disvm::debug::log_msg(component_trace_t::scheduler, log_level_t::debug, "scheduler: exiting: %d", thread_id);

        // Remove the thread
        auto thread_to_remove = _all_vm_threads.find(thread_id);
        assert(thread_to_remove != _all_vm_threads.cend());
        _all_vm_threads.erase(thread_to_remove);

        break;
    }

    case vm_thread_state_t::broken:
    {
        if (disvm::debug::is_component_tracing_enabled<component_trace_t::scheduler>())
            disvm::debug::log_msg(component_trace_t::scheduler, log_level_t::debug, "scheduler: broken: %d", thread_id);

        auto err_msg = thread_instance->vm_thread->get_error_message();
        assert(err_msg != nullptr);
        throw vm_term_request{ err_msg };
    }

    default:
        assert(false && "Unexpected thread state");
        throw vm_system_exception{ "Unexpected thread state" };
    }

    return false;
}