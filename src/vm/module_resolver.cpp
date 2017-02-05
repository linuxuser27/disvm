//
// Dis VM
// File: module_resolver.cpp
// Author: arr
//

#include <cassert>
#include <fstream>
#include <sstream>
#include <disvm.h>
#include <debug.h>
#include <exceptions.h>
#include "module_resolver.h"

using disvm::debug::component_trace_t;
using disvm::debug::log_level_t;

using disvm::runtime::default_resolver_t;
using disvm::runtime::vm_module_t;
using disvm::runtime::vm_module_resolver_t;

// Empty destructor for vm module resolver 'interface'
vm_module_resolver_t::~vm_module_resolver_t()
{ }

default_resolver_t::default_resolver_t(disvm::vm_t &vm)
    : _vm{ vm }
{ }

default_resolver_t::default_resolver_t(disvm::vm_t &vm, std::vector<std::string> probing_paths)
    : _probing_paths{ std::move(probing_paths) }
    , _vm{ vm }
{ }

default_resolver_t::~default_resolver_t()
{ }

bool default_resolver_t::try_resolve_module(const char *path, std::unique_ptr<vm_module_t> &new_module)
{
    assert(path != nullptr);

    auto log = disvm::debug::is_component_tracing_enabled<component_trace_t::module>();
    if (log)
        disvm::debug::log_msg(component_trace_t::module, log_level_t::debug, "resolve: try module path: >>%s<<", path);

    auto module_file = std::ifstream{ path, std::ifstream::binary };
    if (!module_file.is_open())
    {
        // The raw path isn't valid, try using probing paths
        for (auto p : _probing_paths)
        {
            p.append(path);
            if (log)
                disvm::debug::log_msg(component_trace_t::module, log_level_t::debug, "resolve: try module path: >>%s<<", p.c_str());

            module_file = std::ifstream{ p, std::ifstream::binary };
            if (module_file.is_open())
            {
                if (log)
                    disvm::debug::log_msg(component_trace_t::module, log_level_t::debug, "resolve: successful modified module path: >>%s<< >>%s<<", path, p.c_str());

                break;
            }
        }

        if (!module_file.is_open())
        {
            push_syscall_error_message(_vm, "Unable to resolve path");
            return false;
        }
    }

    new_module = disvm::read_module(module_file);
    assert(new_module != nullptr);

    return true;
}
