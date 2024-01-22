#define _GNU_SOURCE
#include <fcntl.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "seccomp_filter.h"

/* bpf filter to forbid the seccomp syscall */
struct sock_filter forbid_seccomp_filter[] = {
    /* load the syscall number */
    BPF_STMT(BPF_LD | BPF_W | BPF_ABS, (offsetof(struct seccomp_data, nr))),
    /* compare syscall number to seccomp() and jump 0 ahead if equal, 1 ahead if
    not */
    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_seccomp, 0, 1),
    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),
    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
};
struct sock_fprog forbid_seccomp_prog = prog_for_filter(forbid_seccomp_filter);

/* a seccomp filter implementing the IA2 coarse-grained syscall policy */
struct sock_filter ia2_filter[] = {
    /* load the syscall number */
    BPF_STMT(BPF_LD | BPF_W | BPF_ABS, (offsetof(struct seccomp_data, nr))),
    /* allow seccomp(). we will forbid it later with a second seccomp filter
    (above). */
    BPF_SYSCALL_POLICY(seccomp, ALLOW),
    /* memory-management syscalls */
    BPF_SYSCALL_POLICY(mmap, TRACE),
    BPF_SYSCALL_POLICY(mprotect, TRACE),
    BPF_SYSCALL_POLICY(mremap, TRACE),
    BPF_SYSCALL_POLICY(munmap, TRACE),
    BPF_SYSCALL_POLICY(madvise, TRACE),
    /* pkey syscalls */
    BPF_SYSCALL_POLICY(pkey_alloc, ALLOW),
    BPF_SYSCALL_POLICY(pkey_mprotect, TRACE),
    /* basic process syscalls */
    BPF_SYSCALL_POLICY(access, ALLOW),
    BPF_SYSCALL_POLICY(arch_prctl, ALLOW),
    BPF_SYSCALL_POLICY(brk, ALLOW),
    BPF_SYSCALL_POLICY(clone3, ALLOW),
    BPF_SYSCALL_POLICY(close, ALLOW),
    BPF_SYSCALL_POLICY(dup, ALLOW),
    BPF_SYSCALL_POLICY(dup2, ALLOW),
    BPF_SYSCALL_POLICY(execve, ALLOW),
    BPF_SYSCALL_POLICY(exit_group, ALLOW),
    BPF_SYSCALL_POLICY(fcntl, ALLOW),
    BPF_SYSCALL_POLICY(futex, ALLOW),
    BPF_SYSCALL_POLICY(sysinfo, ALLOW), // simple1 uses sysinfo inside qsort inside CRYPTO_THREAD_run_once
    BPF_SYSCALL_POLICY(getcwd, ALLOW),
    BPF_SYSCALL_POLICY(lseek, ALLOW),
    BPF_SYSCALL_POLICY(syslog, ALLOW),
    BPF_SYSCALL_POLICY(getdents, ALLOW),
    BPF_SYSCALL_POLICY(getdents64, ALLOW),
    BPF_SYSCALL_POLICY(getegid, ALLOW),
    BPF_SYSCALL_POLICY(geteuid, ALLOW),
    BPF_SYSCALL_POLICY(getgid, ALLOW),
    BPF_SYSCALL_POLICY(getuid, ALLOW),
    BPF_SYSCALL_POLICY(getppid, ALLOW),
    BPF_SYSCALL_POLICY(setpgid, ALLOW), // used by criterion
    BPF_SYSCALL_POLICY(gettid, ALLOW),
    BPF_SYSCALL_POLICY(getpid, ALLOW),
    BPF_SYSCALL_POLICY(getrandom, ALLOW),
    BPF_SYSCALL_POLICY(prctl, ALLOW),
    BPF_SYSCALL_POLICY(faccessat2, ALLOW),
    BPF_SYSCALL_POLICY(sched_getaffinity, ALLOW),
    BPF_SYSCALL_POLICY(sched_setaffinity, ALLOW),
    BPF_SYSCALL_POLICY(alarm, ALLOW),
    BPF_SYSCALL_POLICY(statx, ALLOW),
    BPF_SYSCALL_POLICY(newfstatat, ALLOW),
    BPF_SYSCALL_POLICY(openat, ALLOW),
    BPF_SYSCALL_POLICY(pread64, ALLOW),
    BPF_SYSCALL_POLICY(prlimit64, ALLOW),
    BPF_SYSCALL_POLICY(pwrite64, ALLOW),
    BPF_SYSCALL_POLICY(read, ALLOW),
    BPF_SYSCALL_POLICY(readv, ALLOW),
    BPF_SYSCALL_POLICY(readlink, ALLOW),
    BPF_SYSCALL_POLICY(readlinkat, ALLOW),
    BPF_SYSCALL_POLICY(eventfd2, ALLOW),
    BPF_SYSCALL_POLICY(epoll_create1, ALLOW),
    BPF_SYSCALL_POLICY(epoll_ctl, ALLOW),
    BPF_SYSCALL_POLICY(socket, ALLOW),
    BPF_SYSCALL_POLICY(connect, ALLOW),
    BPF_SYSCALL_POLICY(setsockopt, ALLOW),
    BPF_SYSCALL_POLICY(bind, ALLOW),
    BPF_SYSCALL_POLICY(listen, ALLOW),
    BPF_SYSCALL_POLICY(accept4, ALLOW),
    BPF_SYSCALL_POLICY(sendmsg, ALLOW),
    BPF_SYSCALL_POLICY(recvmsg, ALLOW),
    BPF_SYSCALL_POLICY(unlink, ALLOW),
    BPF_SYSCALL_POLICY(ftruncate, ALLOW),
    BPF_SYSCALL_POLICY(mincore, ALLOW),
    BPF_SYSCALL_POLICY(clone, ALLOW),
    BPF_SYSCALL_POLICY(wait4, ALLOW),
    BPF_SYSCALL_POLICY(tgkill, ALLOW),
    BPF_SYSCALL_POLICY(rseq, ALLOW),
    BPF_SYSCALL_POLICY(rt_sigaction, ALLOW),
    BPF_SYSCALL_POLICY(rt_sigprocmask, ALLOW),
    BPF_SYSCALL_POLICY(set_robust_list, ALLOW),
    BPF_SYSCALL_POLICY(set_tid_address, ALLOW),
    BPF_SYSCALL_POLICY(write, ALLOW),
    BPF_SYSCALL_POLICY(writev, ALLOW),
    BPF_SYSCALL_POLICY(exit, ALLOW),
    BPF_SYSCALL_POLICY(clock_nanosleep, ALLOW),
    BPF_SYSCALL_POLICY(sigaltstack, ALLOW),
    BPF_SYSCALL_POLICY(epoll_wait, ALLOW),
    BPF_SYSCALL_POLICY(setsid, ALLOW),
    BPF_SYSCALL_POLICY(pipe2, ALLOW),
    BPF_SYSCALL_POLICY(poll, ALLOW),
    BPF_SYSCALL_POLICY(waitid, ALLOW),
    BPF_SYSCALL_POLICY(restart_syscall, ALLOW),
    /* tracee syscalls */
    /* ptrace(PTRACE_TRACEME) dance requires raising SIGSTOP */
    BPF_SYSCALL_POLICY(kill, ALLOW),
    /* allow ioctl(TCGETS) */
    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_ioctl, 0, 3),
    BPF_STMT(BPF_LD | BPF_W | BPF_ABS, (offsetof(struct seccomp_data, args[1]))),
    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, TCGETS, 0, 1),
    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
    /* any other syscall => kill process */
    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),
};
struct sock_fprog ia2_filter_prog = prog_for_filter(ia2_filter);

struct sock_filter example_filter[] = {
    BPF_STMT(BPF_LD | BPF_W | BPF_ABS, (offsetof(struct seccomp_data, nr))),
    BPF_SYSCALL_POLICY(write, ALLOW),
    /* this would compare syscall number to write() and allow if it matches */
    /*BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_write, 0, 1),
    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),*/
    /* allow seccomp() */
    BPF_SYSCALL_POLICY(seccomp, ALLOW),
    /* equivalent: */
    /*BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_seccomp, 0, 1),
    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),*/
    /* compare syscall number to open() and jump to kill insn if different */
    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_open, 0, 3),
    /* load argument 1 */
    BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
             (offsetof(struct seccomp_data, args[1]))),
    /* if argument 1 is equal to O_RDONLY, allow */
    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, O_RDONLY, 0, 1),
    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),
};
struct sock_fprog example_filter_prog = prog_for_filter(example_filter);

/* wrap syscall(SYS_seccomp) */
long seccomp(unsigned int mode, unsigned int flags, void *data) {
  return syscall(SYS_seccomp, mode, flags, data);
}

/* apply the given seccomp filter program, then forbid further seccomp() calls.

this function will fail if PR_SET_NO_NEW_PRIVS is not set, unless running
with CAP_SYS_SECCOMP.

returns less than zero on failure, and the user-notify fd on success. */
int configure_seccomp(const struct sock_fprog *prog) {
  /* we must make two separate calls to seccomp() here because we want to create
  a user notification fd to pass to the supervisor, but we also want to pass
  FLAG_TSYNC, and these two cannot be combined in one call because they
  impose conflicting interpretations on the syscall return value. */
  int sc_unotify_fd = seccomp(SECCOMP_SET_MODE_FILTER,
                              SECCOMP_FILTER_FLAG_NEW_LISTENER, (void *)prog);

  if (sc_unotify_fd < 0) {
    perror("seccomp(SECCOMP_FILTER_FLAG_NEW_LISTENER)");
    return -1;
  }

  /* in our second seccomp() call, forbid further calls to seccomp(), and also
  pass the TSYNC flag we wanted to pass in the first place. */
  int sync_ret = seccomp(SECCOMP_SET_MODE_FILTER,
                         SECCOMP_FILTER_FLAG_TSYNC, (void *)&forbid_seccomp_prog);
  if (sync_ret < 0) {
    perror("seccomp(SECCOMP_FILTER_FLAG_TSYNC)");
    return -1;
  }

  return sc_unotify_fd;
}