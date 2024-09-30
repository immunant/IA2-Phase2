#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "ia2_internal.h"
#include "ia2.h"

#if __x86_64__

__attribute__((__used__)) uint32_t ia2_get_pkru() {
  uint32_t pkru = 0;
  __asm__ volatile("rdpkru" : "=a"(pkru) : "a"(0), "d"(0), "c"(0));
  return pkru;
}

size_t ia2_get_pkey() {
  uint32_t pkru;
  __asm__("rdpkru" : "=a"(pkru) : "a"(0), "d"(0), "c"(0));
  switch (pkru) {
  case 0xFFFFFFFC: {
    return 0;
  }
  case 0xFFFFFFF0: {
    return 1;
  }
  case 0xFFFFFFCC: {
    return 2;
  }
  case 0xFFFFFF3C: {
    return 3;
  }
  case 0xFFFFFCFC: {
    return 4;
  }
  case 0xFFFFF3FC: {
    return 5;
  }
  case 0xFFFFCFFC: {
    return 6;
  }
  case 0xFFFF3FFC: {
    return 7;
  }
  case 0xFFFCFFFC: {
    return 8;
  }
  case 0xFFF3FFFC: {
    return 9;
  }
  case 0xFFCFFFFC: {
    return 10;
  }
  case 0xFF3FFFFC: {
    return 11;
  }
  case 0xFCFFFFFC: {
    return 12;
  }
  case 0xF3FFFFFC: {
    return 13;
  }
  case 0xCFFFFFFC: {
    return 14;
  }
  case 0x3FFFFFFC: {
    return 15;
  }
  // TODO: We currently treat any unexpected PKRU value as pkey 0 (the shared
  // heap) for simplicity since glibc(?) initializes the PKRU to 0x55555554
  // (usually). We don't set the PKRU until the first compartment transition, so
  // let's default to using the shared heap before our first wrpkru. When we
  // initialize the PKRU properly (see issue #95) we should probably abort when
  // we see unexpected PKRU values.
  default: {
    return 0;
  }
  }
}
size_t ia2_get_tag(void) __attribute__((alias("ia2_get_pkey")));

#elif __aarch64__

size_t ia2_get_x18(void) {
    size_t x18;
    asm("mov %0, x18" : "=r"(x18));
    return x18 >> 56;
}
size_t ia2_get_tag(void) __attribute__((alias("ia2_get_x18")));

// TODO: insert_tag could probably be cleaned up a bit, but I'm not sure if the
// generated code could be simplified since addg encodes the tag as an imm field
#define _addg(out_ptr, in_ptr, tag) \
    asm("addg %0, %1, #0, %2" : "=r"(out_ptr) : "r"(in_ptr), "i"(tag)); \

#define insert_tag(ptr, tag) \
    ({ \
        uint64_t _res; \
        switch (tag) { \
            case 0: { _addg(_res, ptr, 0); break; } \
            case 1: { _addg(_res, ptr, 1); break; } \
            case 2: { _addg(_res, ptr, 2); break; } \
            case 3: { _addg(_res, ptr, 3); break; } \
            case 4: { _addg(_res, ptr, 4); break; } \
            case 5: { _addg(_res, ptr, 5); break; } \
            case 6: { _addg(_res, ptr, 6); break; } \
            case 7: { _addg(_res, ptr, 7); break; } \
            case 8: { _addg(_res, ptr, 8); break; } \
            case 9: { _addg(_res, ptr, 9); break; } \
            case 10: { _addg(_res, ptr, 10); break; } \
            case 11: { _addg(_res, ptr, 11); break; } \
            case 12: { _addg(_res, ptr, 12); break; } \
            case 13: { _addg(_res, ptr, 13); break; } \
            case 14: { _addg(_res, ptr, 14); break; } \
            case 15: { _addg(_res, ptr, 15); break; } \
        } \
        _res; \
    })


#define set_tag(tagged_ptr) \
    asm volatile("st2g %0, [%0]" :: "r"(tagged_ptr) : "memory");

int ia2_mprotect_with_tag(void *addr, size_t len, int prot, int tag) {
    int res = mprotect(addr, len, prot | PROT_MTE);
    if (res != 0) {
        /* Skip memory tagging if mprotect returned an error */
        return res;
    }
    assert((len % PAGE_SIZE) == 0);
    /* Assuming we're using st2g. stgm is undefined at EL0 so it's not an option */
    const int granule_sz = 32;
    const int granules_per_page = PAGE_SIZE / 32;
    for (int i = 0; i < granules_per_page; i++) {
        // TODO: It may be possible to simplify this to be more efficient using the addg imm offset
        uint64_t tagged_ptr = insert_tag((uint64_t)addr + (i * granule_sz), tag);
        set_tag(tagged_ptr);
    }
    return 0;
}
#endif

// Reserve one extra shared range for the RELRO segment that we are
// also sharing, in addition to the special shared sections in PhdrSearchArgs.
#define NUM_SHARED_RANGES IA2_MAX_NUM_SHARED_SECTION_COUNT + 1

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

static bool in_extra_libraries(struct dl_phdr_info *info, const char *extra_libraries) {
  if (!extra_libraries) {
    return false;
  }
  char *library_name = basename(info->dlpi_name);
  size_t library_name_len = strlen(library_name);
  if (library_name_len == 0) {
    return false;
  }
  size_t extra_libraries_len = strlen(extra_libraries);
  size_t extra_libraries_pos = 0;
  size_t library_name_pos = 0;
  // Iterate over extra_libraries, which is a semi-colon seperated list of
  // library names. At the top of this loop we always start at the beginning of
  // a library name in extra_libraries. We don't require exact matches, but only
  // that the extra library is a prefix of the current library name, because
  // system libraries often have a verion number suffix after the `.so`.
  while (extra_libraries_pos < extra_libraries_len) {
    // Compare this extra library with the current library character by
    // character
    for (library_name_pos = 0; extra_libraries_pos < extra_libraries_len &&
                               library_name_pos < library_name_len;
         extra_libraries_pos++, library_name_pos++) {
      if (extra_libraries[extra_libraries_pos] !=
          library_name[library_name_pos])
        break;
    }
    if (library_name_pos == library_name_len) {
      // Exact match
      return true;
    }
    if (extra_libraries_pos == extra_libraries_len ||
        extra_libraries[extra_libraries_pos] == ';') {
      // Extra library is a prefix of the given library. Many system libraries
      // have version numbers appended after the .so, which we want to ignore.
      return true;
    }

    // Move to next extra library
    while (extra_libraries_pos < extra_libraries_len &&
           extra_libraries[extra_libraries_pos] != ';') {
      extra_libraries_pos++;
    }
    // Skip the semicolon, if present
    extra_libraries_pos++;
  }
  return false;
}

/// Map ELF segment flags to mprotect access flags
static int segment_flags_to_access_flags(Elf64_Word flags) {
  return ((flags & PF_X) != 0 ? PROT_EXEC : 0) |
         ((flags & PF_W) != 0 ? PROT_WRITE : 0) |
         ((flags & PF_R) != 0 ? PROT_READ : 0);
}

int protect_tls_pages(struct dl_phdr_info *info, size_t size, void *data) {
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

  // Protect TLS segment.
  for (size_t i = 0; i < info->dlpi_phnum; i++) {
    Elf64_Phdr phdr = info->dlpi_phdr[i];
    if (phdr.p_type != PT_TLS) {
      continue;
    }

    uint64_t start = (uint64_t)info->dlpi_tls_data;
    size_t len = phdr.p_memsz;

    uint64_t start_round_down = start & ~0xFFFUL;
    uint64_t start_moved_by = start & 0xFFFUL;
    // p_memsz is 0x1000 more than the size of what we actually need to protect.
    // Thus the "rounding up" here is done via p_memsz being larger than it
    // needs to be, followed by the truncation below.
    size_t len_round_up = (phdr.p_memsz + start_moved_by) & ~0xFFFUL;
    uint64_t end = start_round_down + len_round_up;

    if (len_round_up == 0) {
      const char *libname = basename(info->dlpi_name);
      // dlpi_name is "" for the main executable.
      if (libname && libname[0] == '\0') {
        libname = "main";
      }
      printf("TLS segment of %s is not padded\n", libname);
      exit(-1);
    }

    // Look for the untrusted stack pointer, in case this lib defines it.
    extern __thread void *ia2_stackptr_0;
    uint64_t untrusted_stackptr_addr = (uint64_t)&ia2_stackptr_0;
    if ((untrusted_stackptr_addr & 0xFFF) != 0) {
      printf("address of ia2_stackptr_0 (%p) is not page-aligned\n",
             (void *)untrusted_stackptr_addr);
      exit(-1);
    }

    // Protect the TLS region except the page of the untrusted stack pointer.
    // The untrusted stack pointer is page-aligned, so it starts its page, and
    // it is followed by padding that ensures nothing else occupies the rest of
    // its page.
    if (untrusted_stackptr_addr >= start && untrusted_stackptr_addr < end) {
      // Protect TLS region start to the beginning of the untrusted region.
      if (untrusted_stackptr_addr > start_round_down) {
        int mprotect_err = ia2_mprotect_with_tag(
            (void *)start_round_down, untrusted_stackptr_addr - start_round_down,
            PROT_READ | PROT_WRITE, search_args->pkey);
        if (mprotect_err != 0) {
          printf("ia2_mprotect_with_tag failed: %s\n", strerror(errno));
          exit(-1);
        }
      }
      uint64_t after_untrusted_region_start = untrusted_stackptr_addr + 0x1000;
      uint64_t after_untrusted_region_len = end - after_untrusted_region_start;
      if (after_untrusted_region_len > 0) {
        int mprotect_err = ia2_mprotect_with_tag((void *)after_untrusted_region_start,
                                         after_untrusted_region_len,
                                         PROT_READ | PROT_WRITE, search_args->pkey);
        if (mprotect_err != 0) {
          printf("ia2_mprotect_with_tag failed: %s\n", strerror(errno));
          exit(-1);
        }
      }
    } else {
      int mprotect_err =
          ia2_mprotect_with_tag((void *)start_round_down, len_round_up,
                        PROT_READ | PROT_WRITE, search_args->pkey);
      if (mprotect_err != 0) {
        printf("ia2_mprotect_with_tag failed: %s\n", strerror(errno));
        exit(-1);
      }
    }
  }

  return 0;
}

int protect_pages(struct dl_phdr_info *info, size_t size, void *data) {
  if (!data || !info) {
    printf("Passed invalid args to dl_iterate_phdr callback\n");
    exit(-1);
  }

  struct PhdrSearchArgs *search_args = (struct PhdrSearchArgs *)data;

  size_t cur_pkey = ia2_get_tag();
  if (cur_pkey != search_args->pkey) {
    fprintf(stderr, "Invalid pkey, expected %" PRId32 ", found %zu\n",
            search_args->pkey, cur_pkey);
    abort();
  }
  Elf64_Addr address = (Elf64_Addr)search_args->address;
  bool extra = in_extra_libraries(info, search_args->extra_libraries);
  if (!in_loaded_segment(info, address) && !extra) {
    // Continue iterating to check the next object
    return 0;
  }

  if (extra) {
    search_args->found_library_count++;
  }

  // printf("protecting library: %s\n", basename(info->dlpi_name));

  struct AddressRange shared_ranges[NUM_SHARED_RANGES] = {0};
  size_t shared_range_count = 0;
  for (size_t i = 0; i < IA2_MAX_NUM_SHARED_SECTION_COUNT; i++) {
    if (!search_args->shared_sections ||
        !search_args->shared_sections[i].start ||
        !search_args->shared_sections[i].end) {
      break;
    }

    struct AddressRange *cur_range = &shared_ranges[shared_range_count];

    cur_range->start = (uint64_t)search_args->shared_sections[i].start;
    uint64_t aligned_start = cur_range->start & ~0xFFFUL;
    if (aligned_start != cur_range->start) {
      printf("Start of section %p is not page-aligned\n",
             search_args->shared_sections[i].start);
      exit(-1);
    }

    cur_range->end = (uint64_t)search_args->shared_sections[i].end;
    uint64_t aligned_end = (cur_range->end + 0xFFFUL) & ~0xFFFUL;
    if (aligned_end != cur_range->end) {
      printf("End of section %p is not page-aligned\n",
             search_args->shared_sections[i].end);
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
        // Probe each page to ensure that we can read from it. This ensures that
        // we at least have read permission to the page before we ia2_mprotect_with_tag
        // it to exclude all other compartments from accessing the page. We only
        // use pkeys to grant both read and write permission together and rely
        // on normal page permissions to create read-only regions, so reading is
        // sufficient to prove we have access in the current compartment. After
        // we finish protecting these pages, no other compartment may re-protect
        // them because it cannot read from the pages at this probe.
        for (size_t i = 0; i < cur_end - start; i += PAGE_SIZE) {
          volatile char *cur = (volatile char *)start + i;
          (void)*cur;
        }
        // TODO: Inline ia2_mprotect_with_tag call and make sure the pkey is in a
        // register here so we can disallow calls to the libc function
        int mprotect_err = ia2_mprotect_with_tag((void *)start, cur_end - start,
                                         access_flags, (int)cur_pkey);
        if (mprotect_err != 0) {
          printf("ia2_mprotect_with_tag failed: %s\n", strerror(errno));
          exit(-1);
        }

        // Look for the next non-overlapping region
        start = cur_end;
      }
    }
  }

  // We need to continue, as we might be protecting both dependencies and a main
  // library
  return 0;
}
