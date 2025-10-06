// -----------------------------------------------------------------------------
// Exit call gates
// -----------------------------------------------------------------------------
//
// The x86_64 implementation of `ia2_callgate_enter` and `ia2_callgate_exit`
// is provided in exit_callgates_x86_64.S to avoid stack corruption bugs that
// occur when modifying %rsp in C code with inline assembly.
//
// These functions are used by `__wrap___cxa_finalize` (in
// exit_finalize_callgate.c) to perform PKRU/stack transitions when entering
// the exit compartment (pkey 1, where libc/ld.so live).
//
// For other architectures, an assembly implementation must be provided.
