#pragma once

#include <set>
#include <string>

// Registry of ld.so / loader-facing functions that should be forced into
// compartment 1 call gates, even when they are only used implicitly by the
// dynamic linker. The list mirrors glibc/ld-linux; slimmer libcs (e.g., musl
// lacks dladdr1/dlvsym) may omit some entries, so autowrap code must tolerate
// missing signatures.
class LdsoFunctionRegistry {
public:
    static const std::set<std::string>& get_ldso_functions() {
        static const std::set<std::string> ldso_functions = {
            // ld.so internal probe
            "_dl_debug_state",
            // Public loader APIs that may be invoked implicitly or via libc
            "dlopen",
            "dlmopen",
            "dlclose",
            "dlsym",
            "dlvsym",
            "dladdr",
            "dladdr1",
            "dlinfo",
            "dlerror",
            "dl_iterate_phdr"
        };
        return ldso_functions;
    }
    
    static bool is_ldso_function(const std::string& fn_name) {
        return get_ldso_functions().contains(fn_name);
    }
};
