#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <link.h>

#include "ia2.h"

static const char *shared_sections[][2] = {
    {"__start_ia2_shared_data", "__stop_ia2_shared_data"},
    {"__start_ia2_shared_rodata", "__stop_ia2_shared_rodata"},
};

// The number of ELF sections that may be shared by protect_pages
#define NUM_SHARED_RANGES (sizeof(shared_sections) / sizeof(shared_sections[0]))

// The number of program headers to allocate space for in protect_pages. This is
// only an estimate of the maximum value of dlpi_phnum below.
#define MAX_NUM_PHDRS 20

struct AddressRange {
  uint64_t start;
  uint64_t end;
};

struct SegmentInfo {
  uint64_t addr;
  size_t size;
  int prot;
};

/// Check if \p address is in a LOAD segment in \p info
static bool in_loaded_segment(struct dl_phdr_info *info, Elf64_Addr address) {
  for (size_t i = 0; i < info->dlpi_phnum; i++) {
    if (!info->dlpi_phdr) {
      exit(-1);
    }
    Elf64_Phdr phdr = info->dlpi_phdr[i];
    if (phdr.p_type != PT_LOAD) {
      continue;
    }
    Elf64_Addr start = info->dlpi_addr + phdr.p_vaddr;
    Elf64_Addr end = start + phdr.p_memsz;
    if (start <= address && address <= end) {
      return true;
    }
  }
  return false;
}

/// Map ELF segment flags to mprotect access flags
static int segment_flags_to_access_flags(Elf64_Word flags) {
  return ((flags & PF_X) != 0 ? PROT_EXEC : 0) |
         ((flags & PF_W) != 0 ? PROT_WRITE : 0) |
         ((flags & PF_R) != 0 ? PROT_READ : 0);
}

int protect_pages(struct dl_phdr_info *info, size_t size, void *data) {
  if (!data || !info) {
    printf("Passed invalid args to dl_iterate_phdr callback\n");
    exit(-1);
  }

  struct PhdrSearchArgs *search_args = (struct PhdrSearchArgs *)data;
  Elf64_Addr address = (Elf64_Addr)search_args->address;
  if (!in_loaded_segment(info, address)) {
    // Continue iterating to check the next object
    return 0;
  }

  void *lib = dlopen(info->dlpi_name, RTLD_NOW);
  if (!lib)
    exit(-1);

  struct AddressRange shared_ranges[NUM_SHARED_RANGES] = {0};
  struct AddressRange *cur_range = &shared_ranges[0];
  for (size_t i = 0; i < NUM_SHARED_RANGES; i++) {
    // Clear any potential old error conditions
    dlerror();

    cur_range->start = (uint64_t)dlsym(lib, shared_sections[i][0]);
    if (!cur_range->start) {
      // We didn't find the start symbol for this shared section. Either the
      // user didn't mark any shared global data or we didn't link with the
      // correct linker script. We can't distinguish these cases here and the
      // first shouldn't be an error, so let's continue.
      continue;
    }
    uint64_t aligned_start = cur_range->start & ~0xFFFUL;
    if (aligned_start != cur_range->start) {
      printf("Start of section %s is not page-aligned\n",
             shared_sections[i][0]);
      exit(-1);
    }

    cur_range->end = (uint64_t)dlsym(lib, shared_sections[i][1]);
    char *dl_err = dlerror();
    if (dl_err) {
      printf("Could not find end symbol of shared section %s: %s\n",
             shared_sections[i][1], dl_err);
      exit(-1);
    }
    if (!cur_range->end) {
      printf("End symbol of shared section %s was unexpectedly NULL\n",
             shared_sections[i][1]);
      exit(-1);
    }
    uint64_t aligned_end = (cur_range->end + 0xFFFUL) & ~0xFFFUL;
    if (aligned_end != cur_range->end) {
      printf("End of section %s is not page-aligned\n", shared_sections[i][1]);
      exit(-1);
    }

    cur_range++;
  }

  // TODO: We avoid dynamically allocating space for each of the `dlpi_phnum`
  // the SegmentInfo structs for simplicity. MAX_NUM_PHDRS is only an estimate,
  // so if this assert fails we should increment it. Once we test partition
  // allocator more thoroughly we should use malloc here and free the structs at
  // the end of this function.
  assert(info->dlpi_phnum <= MAX_NUM_PHDRS);

  // Reserve one SegmentInfo per program header plus one for each shared range
  // in case any of those ranges have non-zero size in the middle of their
  // segment.
  struct SegmentInfo segment_info[MAX_NUM_PHDRS + NUM_SHARED_RANGES] = {0};

  // The latter halves of split program headers (if any) are stored starting at
  // segment_info[dlpi_phnum]. If a shared section doesn't exist, the unused
  // `SegmentInfo` will have its size field set to zero
  size_t extra_seg_info = info->dlpi_phnum;

  for (size_t i = 0; i < info->dlpi_phnum; i++) {
    Elf64_Phdr phdr = info->dlpi_phdr[i];
    if ((phdr.p_type != PT_LOAD) && (phdr.p_type != PT_GNU_RELRO)) {
      continue;
    }

    int prot = segment_flags_to_access_flags(phdr.p_flags);

    Elf64_Addr start = (info->dlpi_addr + phdr.p_vaddr) & ~0xFFFUL;
    Elf64_Addr stop = (start + phdr.p_memsz + 0xFFFUL) & ~0xFFFUL;
    // TODO: This logic assumes that each segment may only be split by one
    // shared section
    for (size_t j = 0; j < NUM_SHARED_RANGES; j++) {
      if ((shared_ranges[j].start <= start) &&
          (shared_ranges[j].end >= start)) {
        start = shared_ranges[j].end;
      } else if ((shared_ranges[j].end >= stop) &&
                 (shared_ranges[j].start <= stop)) {
        stop = shared_ranges[j].start;
      } else if ((shared_ranges[j].end <= stop) &&
                 (shared_ranges[j].start >= start)) {
        size_t len = stop - shared_ranges[j].end;
        segment_info[extra_seg_info].addr = shared_ranges[j].end;
        segment_info[extra_seg_info].size = len;
        segment_info[extra_seg_info].prot = prot;

        stop = shared_ranges[j].start;
        extra_seg_info += 1;
      }
    }
    size_t len = stop - start;
    segment_info[i].addr = start;
    segment_info[i].size = len;
    segment_info[i].prot = prot;
  }

  for (size_t i = 0; i < info->dlpi_phnum + NUM_SHARED_RANGES; i++) {
    // Check if any of the extra entries weren't used
    if (segment_info[i].size != 0) {
      int mprotect_err =
          pkey_mprotect((void *)segment_info[i].addr, segment_info[i].size,
                        segment_info[i].prot, search_args->pkey);
      if (mprotect_err != 0) {
        printf("pkey_mprotect failed: %s\n", strerror(errno));
        exit(-1);
      }
    }
  }

  // Do not continue, we found the right object
  return 1;
}
