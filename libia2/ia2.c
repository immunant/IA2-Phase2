#include "ia2.h"

int protect_pages(struct dl_phdr_info *info, size_t size, void *data) {
  // Use the address in `data` (PhdrSearchArgs) to determine which program
  // headers belong to the compartment being initialized.
  if (!data || !info) {
    printf("Passed invalid args to dl_iterate_phdr callback\n");
    exit(-1);
  }
  struct PhdrSearchArgs *search_args = (struct PhdrSearchArgs *)data;
  Elf64_Addr address = (Elf64_Addr)search_args->address;
  bool this_compartment = false;
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
      this_compartment = true;
      break;
    }
  }
  if (!this_compartment) {
    return 0;
  }
  const char *ignored_sections[NUM_IGNORED_SECTIONS][2] = {
      {"__start_ia2_shared_data", "__stop_ia2_shared_data"},
      {"__start_ia2_shared_rodata", "__stop_ia2_shared_rodata"},
  };
  struct AddressRange {
    uint64_t start;
    uint64_t end;
  };
  struct AddressRange ignored_ranges[NUM_IGNORED_SECTIONS];
  void *lib = dlopen(info->dlpi_name, RTLD_NOW);
  if (!lib) {
    exit(-1);
  }
  for (size_t i = 0; i < NUM_IGNORED_SECTIONS; i++) {
    // Clear any potential old error conditions
    dlerror();
    ignored_ranges[i].start = (uint64_t)dlsym(lib, ignored_sections[i][0]);
    uint64_t aligned_start = ignored_ranges[i].start & ~0xFFFUL;
    if (aligned_start != ignored_ranges[i].start) {
      printf("Start of section %s is not page-aligned\n",
             ignored_sections[i][0]);
      exit(-1);
    }
    if (dlerror()) {
      exit(-1);
    }
    ignored_ranges[i].end = (uint64_t)dlsym(lib, ignored_sections[i][1]);
    uint64_t aligned_end = (ignored_ranges[i].end + 0xFFFUL) & ~0xFFFUL;
    if (aligned_end != ignored_ranges[i].end) {
      printf("End of section %s is not page-aligned\n", ignored_sections[i][1]);
      exit(-1);
    }
    if (dlerror()) {
      exit(-1);
    }
  }
  struct SegmentInfo {
    uint64_t addr;
    size_t size;
    int prot;
  };
  // Allocate one SegmentInfo per program header plus 2 extra in case
  // ia2_shared_data or ia2_shared_rodata have non-zero size and split their
  // corresponding segments.
  struct SegmentInfo segment_info[NUM_PHDRS + NUM_IGNORED_SECTIONS] = {0};

  // TODO: We avoid dynamically allocating space for each of the `dlpi_phnum`
  // the SegmentInfo structs for simplicity. NUM_PHDRS is only an estimate, so
  // if this assert fails we should increment it. Once we test partition
  // allocator more thoroughly we should use malloc here and free the structs at
  // the end of this function.
  assert(info->dlpi_phnum <= NUM_PHDRS);
  // The latter halves of split program headers (if any) are stored starting at
  // segment_info[dlpi_phnum]. If a shared section doesn't exist, the unused
  // `SegmentInfo` will have its size field set to zero
  size_t extra_seg_info = info->dlpi_phnum;

  for (size_t i = 0; i < info->dlpi_phnum; i++) {
    Elf64_Phdr phdr = info->dlpi_phdr[i];
    if ((phdr.p_type != PT_LOAD) && (phdr.p_type != PT_GNU_RELRO)) {
      continue;
    }
    int prot = PROT_NONE;
    if ((phdr.p_flags & PF_X) != 0) {
      prot |= PROT_EXEC;
    }
    if ((phdr.p_flags & PF_W) != 0) {
      prot |= PROT_WRITE;
    }
    if ((phdr.p_flags & PF_R) != 0) {
      prot |= PROT_READ;
    }
    Elf64_Addr start = (info->dlpi_addr + phdr.p_vaddr) & ~0xFFFUL;
    Elf64_Addr stop = (start + phdr.p_memsz + 0xFFFUL) & ~0xFFFUL;
    // TODO: This logic assumes that each segment may only be split by one
    // shared section
    for (size_t j = 0; j < NUM_IGNORED_SECTIONS; j++) {
      if ((ignored_ranges[j].start <= start) &&
          (ignored_ranges[j].end >= start)) {
        start = ignored_ranges[j].end;
      } else if ((ignored_ranges[j].end >= stop) &&
                 (ignored_ranges[j].start <= stop)) {
        stop = ignored_ranges[j].start;
      } else if ((ignored_ranges[j].end <= stop) &&
                 (ignored_ranges[j].start >= start)) {
        size_t len = stop - ignored_ranges[j].end;
        segment_info[extra_seg_info].addr = ignored_ranges[j].end;
        segment_info[extra_seg_info].size = len;
        segment_info[extra_seg_info].prot = prot;

        stop = ignored_ranges[j].start;
        extra_seg_info += 1;
      }
    }
    size_t len = stop - start;
    segment_info[i].addr = start;
    segment_info[i].size = len;
    segment_info[i].prot = prot;
  }
  for (size_t i = 0; i < info->dlpi_phnum + NUM_IGNORED_SECTIONS; i++) {
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
  return 1;
}
