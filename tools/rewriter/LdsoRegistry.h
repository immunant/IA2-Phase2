#pragma once

#ifdef IA2_LIBC_COMPARTMENT

#include <set>
#include <string>
#include <iostream>

// Registry of ld.so functions that need callgates to compartment 1
// Starting with just _dl_debug_state for proof of concept
class LdsoFunctionRegistry {
public:
    static const std::set<std::string>& get_ldso_functions() {
        static const std::set<std::string> ldso_functions = {
            "_dl_debug_state"
            // TODO: Add more ld.so functions later:
            // "_dl_open", 
            // "_dl_close",
            // "dlopen",
            // "dlclose"
        };
        return ldso_functions;
    }
    
    static bool is_ldso_function(const std::string& fn_name) {
        bool result = get_ldso_functions().contains(fn_name);
        if (result) {
            std::cout << "DEBUG: LdsoRegistry confirmed '" << fn_name << "' is an ld.so function" << std::endl;
        }
        return result;
    }
};

#endif // IA2_LIBC_COMPARTMENT
