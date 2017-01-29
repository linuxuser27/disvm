//
// Dis VM
// File: module_resolver.h
// Author: arr
//

#ifndef _DISVM_SRC_VM_MODULE_RESOLVER_H_
#define _DISVM_SRC_VM_MODULE_RESOLVER_H_

#include <cstdint>
#include <string>
#include <vector>
#include <runtime.h>

namespace disvm
{
    namespace runtime
    {
        // Default module resolver
        class default_resolver_t final : public vm_module_resolver_t
        {
        public:
            default_resolver_t(disvm::vm_t &vm);
            default_resolver_t(disvm::vm_t &vm, std::vector<std::string> probing_paths);
            ~default_resolver_t();

        public: // vm_module_resolver_t
            bool try_resolve_module(const char *path, std::unique_ptr<vm_module_t> &new_module);

        private:
            const std::vector<std::string> _probing_paths;
            disvm::vm_t &_vm;
        };
    }
}

#endif // _DISVM_SRC_VM_MODULE_RESOLVER_H_
