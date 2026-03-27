#include <ia2.h>
#include <ia2_test_runner.h>
#include <library.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <link.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

void ia2_main(void) {
  ia2_register_compartment("main", 1, NULL);
  ia2_register_compartment("libtls_one_page_repro_lib.so", 2, NULL);
}

#ifndef TLS_REPRO_MAIN_TLS_BYTES
#define TLS_REPRO_MAIN_TLS_BYTES (16 * 1024)
#endif
#if TLS_REPRO_MAIN_TLS_BYTES < 1
#error "TLS_REPRO_MAIN_TLS_BYTES must be >= 1"
#endif

__thread uint8_t main_large_tls[TLS_REPRO_MAIN_TLS_BYTES];

// Runtime helper from libia2, returns address of ia2_stackptr_<compartment> TLS slot.
void **ia2_stackptr_for_compartment(int compartment);

static long abs_tp_distance(uintptr_t addr) {
  const uintptr_t tp = (uintptr_t)__builtin_thread_pointer();
  return llabs((long long)addr - (long long)tp);
}

static int pkey_for_address(uintptr_t addr) {
  FILE *f = fopen("/proc/self/smaps", "r");
  if (!f) {
    return -1;
  }

  int pkey = -1;
  bool in_region = false;
  char line[512];
  while (fgets(line, sizeof(line), f)) {
    unsigned long start = 0;
    unsigned long end = 0;
    if (sscanf(line, "%lx-%lx", &start, &end) == 2) {
      in_region = (addr >= start) && (addr < end);
      continue;
    }
    if (!in_region) {
      continue;
    }

    unsigned int value = 0;
    if (sscanf(line, "ProtectionKey:%u", &value) == 1) {
      pkey = (int)value;
      break;
    }
  }

  fclose(f);
  return pkey;
}

struct MappingInfo {
  uintptr_t start;
  uintptr_t end;
  char perms[5];
  char path[256];
  bool found;
};

static struct MappingInfo mapping_for_address(uintptr_t addr) {
  struct MappingInfo out = {0};
  FILE *f = fopen("/proc/self/maps", "r");
  if (!f) {
    return out;
  }

  char line[1024];
  while (fgets(line, sizeof(line), f)) {
    unsigned long start = 0;
    unsigned long end = 0;
    char perms[5] = {0};
    unsigned long offset = 0;
    char dev[12] = {0};
    unsigned long inode = 0;
    char path[256] = {0};

    int matched = sscanf(
        line, "%lx-%lx %4s %lx %11s %lu %255[^\n]",
        &start, &end, perms, &offset, dev, &inode, path);
    if (matched < 6) {
      continue;
    }
    if (addr < start || addr >= end) {
      continue;
    }

    out.start = (uintptr_t)start;
    out.end = (uintptr_t)end;
    strncpy(out.perms, perms, sizeof(out.perms) - 1);
    if (matched >= 7) {
      strncpy(out.path, path, sizeof(out.path) - 1);
    } else {
      strncpy(out.path, "[anonymous]", sizeof(out.path) - 1);
    }
    out.found = true;
    break;
  }

  fclose(f);
  return out;
}

struct TlsRegion {
  uintptr_t start_rounded;
  uintptr_t end_rounded;
  uintptr_t start_exact;
  size_t memsz;
  char owner[128];
};

static struct TlsRegion g_tls_regions[128];
static size_t g_tls_region_count = 0;

static const char *short_object_name(const char *name) {
  if (!name || name[0] == '\0') {
    return "main";
  }
  const char *slash = strrchr(name, '/');
  return slash ? slash + 1 : name;
}

static int collect_tls_regions(struct dl_phdr_info *info, size_t _size, void *_data) {
  (void)_size;
  (void)_data;
  if (!info || !info->dlpi_phdr) {
    return 0;
  }

  const long page_size = sysconf(_SC_PAGESIZE);
  if (page_size <= 0) {
    return 0;
  }
  const uintptr_t page_mask = (uintptr_t)page_size - 1;

  for (size_t i = 0; i < info->dlpi_phnum; i++) {
    const Elf64_Phdr phdr = info->dlpi_phdr[i];
    if (phdr.p_type != PT_TLS) {
      continue;
    }
    if (!info->dlpi_tls_data) {
      continue;
    }
    if (g_tls_region_count >= (sizeof(g_tls_regions) / sizeof(g_tls_regions[0]))) {
      return 1;
    }

    const uintptr_t start_exact = (uintptr_t)info->dlpi_tls_data;
    const uintptr_t end_exact = start_exact + phdr.p_memsz;
    const uintptr_t start_rounded = start_exact & ~page_mask;
    uintptr_t end_rounded = (end_exact + page_mask) & ~page_mask;
    if (phdr.p_memsz > 0 && end_rounded <= start_rounded) {
      end_rounded = start_rounded + (uintptr_t)page_size;
    }

    struct TlsRegion *region = &g_tls_regions[g_tls_region_count++];
    region->start_rounded = start_rounded;
    region->end_rounded = end_rounded;
    region->start_exact = start_exact;
    region->memsz = phdr.p_memsz;
    strncpy(region->owner, short_object_name(info->dlpi_name), sizeof(region->owner) - 1);
  }

  return 0;
}

static bool page_overlaps_region(uintptr_t page_start, uintptr_t page_end,
                                 uintptr_t region_start, uintptr_t region_end) {
  return region_end > page_start && region_start < page_end;
}

static void append_tag(char *dst, size_t dst_sz, const char *tag) {
  if (!dst || !tag || dst_sz == 0) {
    return;
  }
  size_t used = strlen(dst);
  if (used >= dst_sz - 1) {
    return;
  }
  if (used != 0 && used < dst_sz - 2) {
    dst[used++] = ' ';
    dst[used] = '\0';
  }
  strncat(dst, tag, dst_sz - strlen(dst) - 1);
}

Test(tls_one_page_repro, thread_pointer_window_inventory) {
  const long page_size = sysconf(_SC_PAGESIZE);
  cr_assert(page_size > 0);
  const uintptr_t page_mask = (uintptr_t)page_size - 1;
  const uintptr_t tp = (uintptr_t)__builtin_thread_pointer();
  const uintptr_t tp_page = tp & ~page_mask;
  const size_t below_pages = 8;
  const uintptr_t window_start =
      tp_page >= below_pages * (uintptr_t)page_size
          ? tp_page - below_pages * (uintptr_t)page_size
          : 0;
  const uintptr_t window_end = tp_page + (uintptr_t)page_size;

  const uintptr_t addr_main_tls = (uintptr_t)&main_large_tls[0];
  const uintptr_t addr_shared_tls = lib2_get_shared_tls_addr();
  const uintptr_t addr_lib2_tls = lib2_get_local_tls_addr();
  const uintptr_t addr_stack0 = (uintptr_t)ia2_stackptr_for_compartment(0);
  const uintptr_t addr_stack1 = (uintptr_t)ia2_stackptr_for_compartment(1);
  const uintptr_t addr_stack2 = (uintptr_t)ia2_stackptr_for_compartment(2);

  g_tls_region_count = 0;
  dl_iterate_phdr(IA2_IGNORE(collect_tls_regions), NULL);

  cr_log_info("tp_window: tp=%#" PRIxPTR " tp_page=%#" PRIxPTR
              " window=[%#" PRIxPTR ", %#" PRIxPTR ") pages=%zu",
              tp, tp_page, window_start, window_end, below_pages + 1);
  cr_log_info("known_addrs: main_tls=%#" PRIxPTR " shared_tls=%#" PRIxPTR
              " lib2_tls=%#" PRIxPTR " stack0=%#" PRIxPTR
              " stack1=%#" PRIxPTR " stack2=%#" PRIxPTR,
              addr_main_tls, addr_shared_tls, addr_lib2_tls,
              addr_stack0, addr_stack1, addr_stack2);
  cr_log_info("known_pkeys: stack0=%d stack1=%d stack2=%d",
              pkey_for_address(addr_stack0),
              pkey_for_address(addr_stack1),
              pkey_for_address(addr_stack2));

  for (uintptr_t page = window_start; page < window_end; page += (uintptr_t)page_size) {
    const uintptr_t page_end = page + (uintptr_t)page_size;
    const int pkey = pkey_for_address(page);
    const struct MappingInfo mapping = mapping_for_address(page);
    char tags[512] = {0};

    if (tp >= page && tp < page_end) {
      append_tag(tags, sizeof(tags), "TP");
    }
    if (addr_main_tls >= page && addr_main_tls < page_end) {
      append_tag(tags, sizeof(tags), "main_tls");
    }
    if (addr_shared_tls >= page && addr_shared_tls < page_end) {
      append_tag(tags, sizeof(tags), "shared_tls");
    }
    if (addr_lib2_tls >= page && addr_lib2_tls < page_end) {
      append_tag(tags, sizeof(tags), "lib2_tls");
    }
    if (addr_stack0 >= page && addr_stack0 < page_end) {
      append_tag(tags, sizeof(tags), "ia2_stackptr_0");
    }
    if (addr_stack1 >= page && addr_stack1 < page_end) {
      append_tag(tags, sizeof(tags), "ia2_stackptr_1");
    }
    if (addr_stack2 >= page && addr_stack2 < page_end) {
      append_tag(tags, sizeof(tags), "ia2_stackptr_2");
    }

    for (size_t i = 0; i < g_tls_region_count; i++) {
      const struct TlsRegion *r = &g_tls_regions[i];
      if (!page_overlaps_region(page, page_end, r->start_rounded, r->end_rounded)) {
        continue;
      }
      char region_tag[192];
      snprintf(region_tag, sizeof(region_tag), "PT_TLS:%s", r->owner);
      append_tag(tags, sizeof(tags), region_tag);
    }

    if (mapping.found) {
      cr_log_info("tp_window_page page=%#" PRIxPTR " pkey=%d map=%#" PRIxPTR "-%#" PRIxPTR
                  " perms=%s path=%s tags=%s",
                  page, pkey, mapping.start, mapping.end,
                  mapping.perms, mapping.path, tags[0] ? tags : "-");
    } else {
      cr_log_info("tp_window_page page=%#" PRIxPTR " pkey=%d map=<unknown> tags=%s",
                  page, pkey, tags[0] ? tags : "-");
    }
  }

  for (size_t i = 0; i < g_tls_region_count; i++) {
    const struct TlsRegion *r = &g_tls_regions[i];
    cr_log_info("pt_tls_region owner=%s start_exact=%#" PRIxPTR " memsz=%zu rounded=[%#" PRIxPTR ", %#" PRIxPTR ")",
                r->owner, r->start_exact, r->memsz, r->start_rounded, r->end_rounded);
  }
}

Test(tls_one_page_repro, tls_more_than_one_page_from_tp) {
  const long page_size = (long)sysconf(_SC_PAGESIZE);
  const uintptr_t tp = (uintptr_t)__builtin_thread_pointer();
  const uintptr_t main_tls_addr = (uintptr_t)&main_large_tls[0];
  const uintptr_t shared_tls_addr = lib2_get_shared_tls_addr();
  const uintptr_t lib2_tls_addr = lib2_get_local_tls_addr();

  const long main_tls_distance = abs_tp_distance((uintptr_t)&main_large_tls[0]);
  const long shared_tls_distance = abs_tp_distance(shared_tls_addr);
  const long lib2_tls_distance = abs_tp_distance(lib2_tls_addr);
  cr_log_info("page_size=%ld", page_size);
  cr_log_info("config_main_tls_bytes=%d", TLS_REPRO_MAIN_TLS_BYTES);
  cr_log_info("config_lib2_tls_bytes=%d", TLS_REPRO_LIB2_TLS_BYTES);
  cr_log_info("config_shared_tls_bytes=%d", TLS_REPRO_SHARED_TLS_BYTES);
  cr_log_info("distance_from_tp(main_tls)=%ld", main_tls_distance);
  cr_log_info("distance_from_tp(shared_tls)=%ld", shared_tls_distance);
  cr_log_info("distance_from_tp(lib2_tls)=%ld", lib2_tls_distance);
  cr_log_info("pkey(tp)=%d", pkey_for_address(tp));
  cr_log_info("pkey(main_tls)=%d", pkey_for_address(main_tls_addr));
  cr_log_info("pkey(shared_tls)=%d", pkey_for_address(shared_tls_addr));
  cr_log_info("pkey(lib2_tls)=%d", pkey_for_address(lib2_tls_addr));
  cr_assert(main_tls_distance > 0);
  cr_assert(shared_tls_distance > 0);
  cr_assert(lib2_tls_distance > 0);
}

Test(tls_one_page_repro, natural_cross_compartment_tls_access) {
  uint32_t prev = lib2_call_shared_tls_bump();
  cr_assert_eq(prev, 1);
  for (uint32_t i = 0; i < 1000; i++) {
    uint32_t cur = lib2_call_shared_tls_bump();
    cr_assert_eq(cur, prev + 1);
    prev = cur;
  }
}
