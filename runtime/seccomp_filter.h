#pragma once
struct sock_fprog;

extern struct sock_fprog ia2_filter_prog;
extern struct sock_fprog example_filter_prog;

int configure_seccomp(const struct sock_fprog *prog);

#define prog_for_filter(filter_name)                 \
  {                                                  \
    .len = (unsigned short)(sizeof(filter_name) /    \
                            sizeof(filter_name[0])), \
    .filter = filter_name,                           \
  }

// shorthand for an equality comparison jump of 0 (eq) or 1 (neq) followed by
// a return of the given policy (used in the equal case)
#define BPF_SYSCALL_POLICY(name, policy)                  \
  BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_##name, 0, 1), \
      BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_##policy)
