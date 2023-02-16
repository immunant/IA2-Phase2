/* pad-tls: tool to pad TLS segment of ELF files so that a given compartment's
TLS variables will only possibly share a page with another compartment's
padding, not its TLS variables */

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <elf.h>

enum patch_result {
  PATCH_SUCCESS,
  BAD_MAGIC,
  WRONG_ARCH,
  NO_TLS,
};

static const char *patch_errors[] = {
    "invalid ELF magic number",
    "ELF architecture is not x86-64",
    "no TLS segment in ELF program headers",
};

enum patch_result patch_tls(void *elf, int64_t delta, uint64_t *size_out) {
  Elf64_Ehdr *ehdr = elf;

  if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG)) {
    return BAD_MAGIC;
  }

  if (ehdr->e_machine != EM_X86_64) {
    return WRONG_ARCH;
  }

  char *ph_addr = ((char *)elf) + ehdr->e_phoff;
  for (int i = 0; i < ehdr->e_phnum; i++) {
    Elf64_Phdr *phdr = (Elf64_Phdr *)ph_addr;

    if (phdr->p_type == PT_TLS) {
      phdr->p_memsz += delta;
      if (size_out) {
        *size_out = phdr->p_memsz;
      }
      return PATCH_SUCCESS;
    }

    ph_addr += ehdr->e_phentsize;
  }

  return NO_TLS;
}

const int64_t delta = 4096;

void usage(char **argv) {
  fprintf(
      stderr,
      "usage: %s [--allow-no-tls] <ELF>: adds %ld bytes to TLS segment size of ELF binary\n",
      basename(argv[0]), delta);
}

int main(int argc, char **argv) {
  char *filename;
  bool allow_no_tls = false;

  if (argc > 2 && !strcmp(argv[1], "--allow-no-tls")) {
    allow_no_tls = true;
    filename = argv[2];
  } else if (argc == 2) {
    filename = argv[1];
  } else {
    usage(argv);
    return 1;
  }

  int fd = open(filename, O_RDWR);
  if (fd < 0) {
    fprintf(stderr, "%s: could not be opened: %s\n", filename, strerror(errno));
    return 1;
  }

  struct stat stat;
  fstat(fd, &stat);
  size_t size = (size_t)stat.st_size;

  void *elf = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (elf == MAP_FAILED) {
    fprintf(stderr, "%s: could not mmap: %s\n", filename, strerror(errno));
    return 1;
  }

  uint64_t new_size = 0;
  enum patch_result result = patch_tls(elf, delta, &new_size);
  if (result != PATCH_SUCCESS) {
    if (allow_no_tls && result == NO_TLS) {
      return 0;
    }
    fprintf(stderr, "%s: %s\n", filename, patch_errors[result - 1]);
    return (int)result;
  }

  printf("%s: TLS size now 0x%lx\n", filename, new_size);
  return 0;
}
