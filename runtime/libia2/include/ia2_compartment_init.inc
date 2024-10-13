/* -*-c-*- vi:filetype=c:
 */
#ifndef IA2_COMPARTMENT
#error Missing IA2_COMPARTMENT
#endif

#include <elf.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include <ia2_internal.h>
#include <ia2.h>

#ifndef IA2_COMPARTMENT_LIBRARIES
#define IA2_COMPARTMENT_LIBRARIES NULL
#endif

#define CONCAT_(x, y) x##y
#define CONCAT(x, y) CONCAT_(x, y)
#define COMPARTMENT_IDENT(name) CONCAT(name##_, IA2_COMPARTMENT)

/* Fully expand x to a string if x is a macro (e.g. IA2_COMPARTMENT) */
#define XSTR(x) STR(x)
#define STR(x) #x

#if !IA2_ENABLE

/*
 * The rewriter needs to generate call gate wrappers for these functions so make
 * them no-ops with the same function signatures as the real versions when
 * compartments are disabled.
 */
__attribute__((used)) void COMPARTMENT_IDENT(ia2_setup_destructors)(void) { }
__attribute__((used)) void COMPARTMENT_IDENT(ia2_compartment_destructor)(void) { }

#else

__attribute__((visibility("default"))) __thread void *COMPARTMENT_IDENT(ia2_stackptr) __attribute__((used));

/*
** Initializes a compartment with protection key `n`. Called automatically as a
** constructor when the ELF defining this symbol is loaded. This must only be
** called once for each key. The compartment includes all segments in the ELF
** except the `ia2_shared_data` section, if one exists.
**/

extern int ia2_n_pkeys_to_alloc;
extern char __start_ia2_shared_data __attribute__((visibility("hidden"))),
    __stop_ia2_shared_data __attribute__((visibility("hidden")));

void ensure_pkeys_allocated(int *n_to_alloc);
__attribute__((constructor)) static void COMPARTMENT_IDENT(init_pkey)() {
  ensure_pkeys_allocated(&ia2_n_pkeys_to_alloc);
  struct IA2SharedSection shared_sections[2] = {{
                                                    &__start_ia2_shared_data,
                                                    &__stop_ia2_shared_data,
                                                },
                                                {NULL, NULL}};
  struct PhdrSearchArgs args = {
      .pkey = IA2_COMPARTMENT,
      .address = &COMPARTMENT_IDENT(init_pkey),
      .extra_libraries = IA2_COMPARTMENT_LIBRARIES,
      .found_library_count = 0,
      .shared_sections = shared_sections,
  };

#if defined(__x86_64__)
  __asm__ volatile(
      /* Set PKRU to the compartment's value */
      "xorl %%ecx, %%ecx\n"
      "xorl %%edx, %%edx\n"
      "mov_pkru_eax " XSTR(IA2_COMPARTMENT) "\n"
      "wrpkru\n"
      :
      :
      : "rax", "rcx", "rdx");
#elif defined(__aarch64__)
  __asm__ volatile("movz_shifted_tag_x18 " XSTR(IA2_COMPARTMENT) "\n");
#endif

  dl_iterate_phdr(protect_pages, &args);

#if defined(__x86_64__)
  __asm__ volatile(
      /* Set PKRU to fully untrusted (no access) */
      "xorl %%ecx, %%ecx\n"
      "xorl %%edx, %%edx\n"
      "xorl %%eax, %%eax\n"
      "wrpkru\n"
      :
      :
      : "rax", "rcx", "rdx");
#elif defined(__aarch64__)
  __asm__ volatile("movz_shifted_tag_x18 0\n");
#endif

  /* Check that we found all extra libraries */
  const char *cur_pos = args.extra_libraries;
  int extra_library_count = 0;
  while (cur_pos) {
    extra_library_count++;
    cur_pos = strchr(cur_pos, ';');
    if (cur_pos) {
      cur_pos++;
    }
  }
  if (extra_library_count != args.found_library_count) {
    fprintf(
        stderr,
        "WARNING: Not all libraries in IA2_COMPARTMENT_LIBRARIES were found.\n");
  }
}

__attribute__((visibility("default"))) void COMPARTMENT_IDENT(init_tls)(void) {
  struct PhdrSearchArgs args = {
      .pkey = IA2_COMPARTMENT,
      .address = &COMPARTMENT_IDENT(init_tls),
  };
  dl_iterate_phdr(protect_tls_pages, &args);
}

/* Magic symbol from the linker that marks the start of the ELF headers */
extern Elf64_Ehdr __ehdr_start __attribute__((weak));

struct FinalizerInfo {
  uint64_t fini_offset;
  uint64_t fini_array_offset;
  uint64_t fini_array_size;
};

/* Filled in by runtime initialization with contents of  .fini_array and
 * .fini */
static volatile struct FinalizerInfo finalizers
    __attribute__((__section__(".data.rel.ro.1"))) = {0};

extern void COMPARTMENT_IDENT(__wrap_ia2_compartment_destructor)(void);

__attribute__((visibility("default"))) __attribute__((no_sanitize("undefined"))) __attribute__((used)) void
COMPARTMENT_IDENT(ia2_compartment_destructor)(void) {
  static bool ran_destructors = false;
  if (ran_destructors)
    return;
  assert(&__ehdr_start);

  void (**fini_array)(void) = (void (**)(void))(((uint8_t *)&__ehdr_start) +
                                                finalizers.fini_array_offset);
  for (size_t i = 0; i < (finalizers.fini_array_size / sizeof(void *)); i++) {
    fini_array[i]();
  }
  if (finalizers.fini_offset) {
    void (*fini)(void) =
        (void (*)(void))(((uint8_t *)&__ehdr_start) + finalizers.fini_offset);
    fini();
  }

  ran_destructors = true;
}

/* This pointer MUST be in a read-only (.data.rel.ro) section. */
/* TODO: Check this in the resulting binary */
static void (*const compartment_destructor_ptr)(void) =
    COMPARTMENT_IDENT(__wrap_ia2_compartment_destructor);

/* UBSan doesn't like this access, so opt it out of sanitization. */
__attribute__((no_sanitize("undefined")))
static inline uint32_t ubsan_access_phdr_type(Elf64_Phdr *phdr, int i) {
    return phdr[i].p_type;
}

__attribute__((visibility("default"))) void COMPARTMENT_IDENT(ia2_setup_destructors)(void) {
  int res = 0;
  Elf64_Ehdr *ehdr = &__ehdr_start;
  Elf64_Phdr *phdr = (Elf64_Phdr *)((uint8_t *)ehdr + __ehdr_start.e_phoff);
  assert(sizeof(Elf64_Phdr) == ehdr->e_phentsize);
  Elf64_Phdr *dynamic_phdr = NULL;
  for (int i = 0; i < ehdr->e_phnum; i++) {
    if (ubsan_access_phdr_type(phdr, i) == PT_DYNAMIC) {
      dynamic_phdr = &phdr[i];
      break;
    }
  }
  if (!dynamic_phdr) {
    return;
  }
  Elf64_Dyn *dynamic =
      (Elf64_Dyn *)((uintptr_t)&__ehdr_start + dynamic_phdr->p_vaddr);
  void *rewrite_base = NULL;
  void *finalizers_region =
      (void *)IA2_ROUND_DOWN((uintptr_t)&finalizers, PAGE_SIZE);
  size_t finalizers_region_size =
      IA2_ROUND_DOWN(((uintptr_t)&finalizers) + sizeof(finalizers) + PAGE_SIZE,
                     PAGE_SIZE) -
      (uintptr_t)finalizers_region;
  res = mprotect(finalizers_region, finalizers_region_size,
                 PROT_READ | PROT_WRITE);
  if (res != 0) {
    fprintf(stderr, "Failed to set relro section writable: %d\n", res);
    exit(1);
  }
  for (size_t i = 0; i < dynamic_phdr->p_memsz / sizeof(Elf64_Dyn); i++) {
    if (dynamic[i].d_tag == DT_FINI || dynamic[i].d_tag == DT_FINI_ARRAY ||
        dynamic[i].d_tag == DT_FINI_ARRAYSZ) {
      uintptr_t dynamic_entry_addr = (uintptr_t)&dynamic[i].d_un.d_val;
      bool in_finalizers_mapping =
          dynamic_entry_addr >= (uintptr_t)finalizers_region &&
          (uintptr_t)&dynamic[i + 1] <=
              (uintptr_t)finalizers_region + finalizers_region_size;
      bool in_dynamic_mapping =
          rewrite_base &&
          (uintptr_t)rewrite_base + PAGE_SIZE <= dynamic_entry_addr;
      if (!in_finalizers_mapping && !in_dynamic_mapping) {
        if (rewrite_base) {
          res = mprotect(rewrite_base, PAGE_SIZE, PROT_READ);
          if (res != 0) {
            fprintf(stderr,
                    "Failed to reset dynamic section to read-only: %d\n", res);
            exit(1);
          }
        }
        rewrite_base = (void *)IA2_ROUND_DOWN((uintptr_t)&dynamic[i].d_un.d_val,
                                              PAGE_SIZE);
        /* ensure that we don't cross a page boundary. */
        assert(((uintptr_t)&dynamic[i + 1] - (uintptr_t)rewrite_base) <=
               PAGE_SIZE);
        res = mprotect(rewrite_base, PAGE_SIZE, PROT_READ | PROT_WRITE);
        if (res != 0) {
          fprintf(stderr, "Failed to set dynamic section writable: %d\n", res);
          exit(1);
        }
      }
      if (dynamic[i].d_tag == DT_FINI) {
        finalizers.fini_offset = dynamic[i].d_un.d_val;
        dynamic[i].d_un.d_val =
            (Elf64_Xword)&COMPARTMENT_IDENT(__wrap_ia2_compartment_destructor) -
            (Elf64_Xword)&__ehdr_start;
      } else if (dynamic[i].d_tag == DT_FINI_ARRAY) {
        finalizers.fini_array_offset = dynamic[i].d_un.d_val;
        dynamic[i].d_un.d_val = (Elf64_Xword)&compartment_destructor_ptr -
                                (Elf64_Xword)&__ehdr_start;
      } else if (dynamic[i].d_tag == DT_FINI_ARRAYSZ) {
        finalizers.fini_array_size = dynamic[i].d_un.d_val;
        dynamic[i].d_un.d_val = 1;
      }
    }
  }
  if (rewrite_base) {
    res = mprotect(rewrite_base, PAGE_SIZE, PROT_READ);
    if (res != 0) {
      fprintf(stderr, "Failed to reset dynamic section to read-only: %d\n",
              res);
      exit(1);
    }
  }
  res = mprotect(finalizers_region, finalizers_region_size, PROT_READ);
  if (res != 0) {
    fprintf(stderr, "Failed to reset relro section to read-only: %d\n", res);
    exit(1);
  }
}
#endif // !IA2_ENABLE
