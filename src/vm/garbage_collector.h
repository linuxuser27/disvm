//
// Dis VM
// File: garbage_collector.h
// Author: arr
//

#ifndef _DISVM_SRC_VM_GARBAGE_COLLECTOR_H_
#define _DISVM_SRC_VM_GARBAGE_COLLECTOR_H_

#include <cstdint>
#include <mutex>
#include <deque>
#include <vector>
#include <disvm.h>

namespace disvm
{
    namespace runtime
    {
        // Default garbage collector
        class default_garbage_collector_t final : public vm_garbage_collector_t
        {
        public:
            default_garbage_collector_t(vm_t &vm);

            ~default_garbage_collector_t();

        public: // vm_garbage_collector_t
            void track_allocation(vm_alloc_t *alloc) override;

            void enum_tracked_allocations(vm_alloc_callback_t callback) const override;

            bool collect(std::vector<std::shared_ptr<const vm_thread_t>> &threads) override;

        private:
            vm_t &_vm;

            std::size_t _collection_epoch;
            mutable std::mutex _tracking_allocs_lock;
            std::deque<vm_alloc_t *> _tracking_allocs;
        };
    }
}

#endif // _DISVM_SRC_VM_GARBAGE_COLLECTOR_H_
