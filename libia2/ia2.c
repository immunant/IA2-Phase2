#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <link.h>
#include <stdint.h>

#include "ia2.h"

#ifndef IA2_INIT_COMPARTMENT
#error IA2_INIT_COMPARTMENT must be defined before including ia2.c
#endif

extern char __start_ia2_shared_data __attribute__((weak));
extern char __stop_ia2_shared_data __attribute__((weak));

// This array MUST be in a read-only (.data.rel.ro) section.
// TODO: Check this in the resulting binary
static const char * const shared_sections[][3] = {
    {&__start_ia2_shared_data, &__stop_ia2_shared_data, "ia2_shared_data"},
};

// The number of special ELF sections that may be shared by protect_pages
#define NUM_SHARED_SECTIONS                                                    \
  (sizeof(shared_sections) / sizeof(shared_sections[0]))

// Reserve one extra shared range for the entire read-only segment that we are
// also sharing, in addition to the special-cased sections above.
#define NUM_SHARED_RANGES (NUM_SHARED_SECTIONS + 1)

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

/// Protect pages in the given shared object
///
/// \param info dynamic linker information for the current object
/// \param size size of \p info in bytes
/// \param data pointer to a PhdrSearchArgs structure
///
/// The callback passed to dl_iterate_phdr in the constructor inserted by
/// INIT_COMPARTMENT to pkey_mprotect the pages corresponding to the
/// compartment's loaded segments.
///
/// Iterates over shared objects until an object containing the address \p
/// data->address is found. Protect the pages in that object according to the
/// information in the search arguments.
static int protect_pages(struct dl_phdr_info *info, size_t size, void *data) {
  if (!info) {
    printf("Passed invalid args to dl_iterate_phdr callback\n");
    exit(-1);
  }

  if (!in_loaded_segment(info, (Elf64_Addr)&protect_pages)) {
    // Continue iterating to check the next object
    return 0;
  }

  struct AddressRange shared_ranges[NUM_SHARED_RANGES] = {0};
  size_t shared_range_count = 0;
  for (size_t i = 0; i < NUM_SHARED_SECTIONS; i++) {
    struct AddressRange *cur_range = &shared_ranges[shared_range_count];

    cur_range->start = (uintptr_t)shared_sections[i][0];
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
             shared_sections[i][2]);
      exit(-1);
    }

    cur_range->end = (uintptr_t)shared_sections[i][1];
    if (!cur_range->end) {
      printf("End symbol of shared section %s was unexpectedly NULL\n",
             shared_sections[i][2]);
      exit(-1);
    }
    uint64_t aligned_end = (cur_range->end + 0xFFFUL) & ~0xFFFUL;
    if (aligned_end != cur_range->end) {
      printf("End of section %s is not page-aligned\n", shared_sections[i][2]);
      exit(-1);
    }

    shared_range_count++;
  }

  // Find the RELRO section, if any
  for (size_t i = 0; i < info->dlpi_phnum; i++) {
    Elf64_Phdr phdr = info->dlpi_phdr[i];
    if (phdr.p_type != PT_GNU_RELRO)
      continue;

    struct AddressRange *relro_range = &shared_ranges[shared_range_count++];
    relro_range->start = (info->dlpi_addr + phdr.p_vaddr) & ~0xFFFUL;
    relro_range->end = (relro_range->start + phdr.p_memsz + 0xFFFUL) & ~0xFFFUL;

    break;
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
    if ((phdr.p_type != PT_LOAD)) {
      continue;
    }

    if ((phdr.p_flags & PF_W) == 0) {
      // We don't need to protect any read-only memory that was mapped in from
      // the object file. This data is not assumed to be secret and can be
      // shared by default.
      continue;
    }

    int access_flags = segment_flags_to_access_flags(phdr.p_flags);

    Elf64_Addr start = (info->dlpi_addr + phdr.p_vaddr) & ~0xFFFUL;
    Elf64_Addr seg_end = (start + phdr.p_memsz + 0xFFFUL) & ~0xFFFUL;
    while (start < seg_end) {
      Elf64_Addr cur_end = seg_end;

      // Adjust start and cur_end to bound the first region between start and
      // seg_end that does not overlap with a shared range
      for (size_t j = 0; j < shared_range_count; j++) {
        // Look for a shared range overlapping the start of the current range
        // and remove the overlapping portion.
        if (shared_ranges[j].start <= start && shared_ranges[j].end > start) {
          start = shared_ranges[j].end;
        }

        // Look for a shared range overlapping any of the rest of the current
        // range, and trim the end of the current range to the start of that
        // shared range.
        if (shared_ranges[j].start > start &&
            shared_ranges[j].start < cur_end) {
          cur_end = shared_ranges[j].start;
        }
      }

      if (cur_end > start) {
        int mprotect_err = pkey_mprotect((void *)start, cur_end - start,
                                         access_flags, IA2_INIT_COMPARTMENT);
        if (mprotect_err != 0) {
          printf("pkey_mprotect failed: %s\n", strerror(errno));
          exit(-1);
        }

        // Look for the next non-overlapping region
        start = cur_end;
      }
    }
  }

  // Do not continue, we found the right object
  return 1;
}

extern int ia2_n_pkeys_to_alloc;
void ensure_pkeys_allocated(int *n_to_alloc);

// Initializes a compartment with protection key `IA2_INIT_COMPARTMENT` when the
// ELF that included this file is loaded. The compartment includes all segments
// in the current ELF except the `ia2_shared_data` section, if one exists.
__attribute__((constructor)) static void init_pkey_ctor() {
  ensure_pkeys_allocated(&ia2_n_pkeys_to_alloc);
  dl_iterate_phdr(protect_pages, NULL);
}
