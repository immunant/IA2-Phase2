#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <unistd.h>

#include "seccomp_filter.h"

int main(int argc, char *argv[]) {
  int infd, outfd;
  ssize_t read_bytes;
  char buffer[1024];

  if (argc < 3) {
    printf("Usage:\n\tseccomp-demo <file_to_read> <file_to_create>\n");
    return -1;
  }

  printf("forbidding new privs\n");
  // in order to use seccomp() without CAP_SYS_SECCOMP, we must opt out of being
  // able to gain privs via exec() of setuid binaries as they would inherit our
  // seccomp filters.
  prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);

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
