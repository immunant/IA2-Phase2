#define _GNU_SOURCE
#include <fcntl.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <linux/unistd.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

// bpf filter to forbid the seccomp syscall
struct sock_filter forbid_seccomp_filter[] = {
    // load the syscall number
    BPF_STMT(BPF_LD | BPF_W | BPF_ABS, (offsetof(struct seccomp_data, nr))),
    // compare syscall number to seccomp() and jump 0 ahead if equal, 1 ahead if
    // not
    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_seccomp, 0, 1),
    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL),
    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
};

struct sock_fprog forbid_seccomp_prog = {
    .len = (unsigned short)(sizeof(forbid_seccomp_filter) /
                            sizeof(forbid_seccomp_filter[0])),
    .filter = forbid_seccomp_filter,
};

// shorthand for an equality comparison jump of 0 (eq) or 1 (neq) followed by
// a return of the given policy (used in the equal case)
#define BPF_SYSCALL_POLICY(name, policy)                                       \
  BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_##name, 0, 1),                      \
      BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_##policy)

long syscall(long no, ...);

int configure_seccomp(void) {
  struct sock_filter filter[] = {
      BPF_STMT(BPF_LD | BPF_W | BPF_ABS, (offsetof(struct seccomp_data, nr))),
      BPF_SYSCALL_POLICY(write, ALLOW),
      // this would compare syscall number to write() and allow if it matches
      /*BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_write, 0, 1),
      BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),*/
      // allow seccomp()
      BPF_SYSCALL_POLICY(seccomp, ALLOW),
      // equivalent:
      /*BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_seccomp, 0, 1),
      BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),*/
      // compare syscall number to open() and jump to kill insn if different
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_open, 0, 3),
      // load argument 1
      BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
               (offsetof(struct seccomp_data, args[1]))),
      // if argument 1 is equal to O_RDONLY, allow
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, O_RDONLY, 0, 1),
      BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
      BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL)};

  struct sock_fprog prog = {
      .len = (unsigned short)(sizeof(filter) / sizeof(filter[0])),
      .filter = filter,
  };

  printf("forbidding new privs\n");
  // in order to use seccomp() without CAP_SYS_SECCOMP, we must opt out of being
  // able to gain privs via exec() of setuid binaries as they would inherit our
  // seccomp filters.
  prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);

  printf("calling seccomp()\n");
  // we must make two separate calls to seccomp() here because we want to create
  // a user notification fd to pass to the supervisor, but we also want to pass
  // FLAG_TSYNC, and these two cannot be combined in one call because they
  // impose conflicting interpretations on the syscall return value.
  int sc_unotify_fd = syscall(SYS_seccomp, SECCOMP_SET_MODE_FILTER,
                              SECCOMP_FILTER_FLAG_NEW_LISTENER, &prog);
  printf("fd=%d\n", sc_unotify_fd);
  if (sc_unotify_fd < 0)
    return -1;
  int sync_ret = syscall(SYS_seccomp, SECCOMP_SET_MODE_FILTER,
                         SECCOMP_FILTER_FLAG_TSYNC, &forbid_seccomp_prog);
  printf("ret=%d\n", sync_ret);
  if (sync_ret < 0)
    return -1;
  // prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog);
  return 0;
}

int main(int argc, char *argv[]) {
  int infd, outfd;
  ssize_t read_bytes;
  char buffer[1024];

  if (argc < 3) {
    printf("Usage:\n\tdup_file <input path> <output_path>\n");
    return -1;
  }

  if (configure_seccomp() < 0) {
    return -1;
  }

  const char *infile = argv[1];
  const char *outfile = argv[2];

  printf("opening %s O_RDONLY\n", infile);
  if ((infd = open(infile, O_RDONLY)) < 0) {
    perror("open ro");
    return 1;
  }

  printf("opening %s O_WRONLY|O_CREAT\n", outfile);
  if ((outfd = open(outfile, O_WRONLY | O_CREAT, 0644)) < 0) {
    perror("open rw");
    return 1;
  }

  while ((read_bytes = read(infd, &buffer, 1024)) > 0) {
    int ret = write(outfd, &buffer, (ssize_t)read_bytes);
    if (ret < 0) {
      perror("write");
      return 1;
    }
  }

  if (read_bytes < 0) {
    perror("read");
    return 1;
  }

  close(infd);
  close(outfd);
  return 0;
}
