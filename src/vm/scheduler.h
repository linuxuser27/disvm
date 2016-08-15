//
// Dis VM
// File: scheduler.h
// Author: arr
//

#ifndef _DISVM_SRC_VM_SCHEDULER_H_
#define _DISVM_SRC_VM_SCHEDULER_H_

#include <cstdint>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <forward_list>
#include <unordered_set>
#include <unordered_map>
#include <atomic>
#include <disvm.h>

namespace disvm
{
    namespace runtime
    {
        // Default multi-threaded scheduler
        class default_scheduler_t final : public vm_scheduler_t, public vm_scheduler_control_t
        {
        private: // static
            static void worker_main(default_scheduler_t &instance);

        public:
            default_scheduler_t(vm_t &vm, uint32_t system_thread_count, uint32_t thread_quanta);

            ~default_scheduler_t();

        public: // vm_scheduler_t
            bool is_idle() const override;

            vm_scheduler_control_t &get_controller() const override;

            const vm_thread_t& schedule_thread(std::unique_ptr<vm_thread_t> thread) override;

            void set_tool_dispatch_on_all_threads(vm_tool_dispatch_t *dispatch) override;

        public: // vm_scheduler_control_t
            void enqueue_blocked_thread(uint32_t thread_id) override;

            std::vector<std::shared_ptr<const vm_thread_t>> get_all_threads() const override;

            std::vector<std::shared_ptr<const vm_thread_t>> get_runnable_threads() const override;

        private:
            struct thread_instance_t final
            {
                thread_instance_t(std::unique_ptr<vm_thread_t> t)
                    : vm_thread{ std::move(t) }
                {
                }

                ~thread_instance_t()
                {
                    if (vm_thread != nullptr)
                        vm_thread->release();
                }

                std::shared_ptr<vm_thread_t> vm_thread;
                std::mutex system_thread_ownership;
            };

            std::shared_ptr<thread_instance_t> next_thread(std::shared_ptr<thread_instance_t> prev_thread);

            void perform_gc(bool is_gc_thread, std::unique_lock<std::mutex> &all_vm_threads_lock);

            // Add the thread to the queue in a non-thread safe manner.
            // Returns 'true' if the runnable thread queue has been updated, otherwise 'false'.
            bool enqueue_thread_unsafe(std::shared_ptr<thread_instance_t> thread, runtime::vm_thread_state_t current_state);

        private:
            vm_t &_vm;

            const uint32_t _worker_thread_count;
            std::vector<std::thread> _worker_pool;

            std::condition_variable _worker_event;
            std::atomic_bool _terminating;

            const uint32_t _vm_thread_quanta;
            mutable std::mutex _vm_threads_lock;
            std::deque<std::shared_ptr<thread_instance_t>> _runnable_vm_threads;
            std::size_t _running_vm_thread_count;

            std::mutex _gc_wait;
            std::condition_variable _gc_done_event;
            std::atomic_bool _gc_complete; // Used to avoid spurious wakeups
            std::size_t _gc_counter;

            std::unordered_set<uint32_t> _blocked_vm_thread_ids;
            std::forward_list<uint32_t> _non_runnable_vm_thread_ids;

            using all_thread_map_t = std::unordered_map<uint32_t, std::shared_ptr<thread_instance_t>>;
            all_thread_map_t _all_vm_threads;
        };
    }
}

#endif // _DISVM_SRC_VM_SCHEDULER_H_