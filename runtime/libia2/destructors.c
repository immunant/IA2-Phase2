#include <elf.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <ia2_internal.h>

/* UBSan doesn't like this access, so opt it out of sanitization. */
__attribute__((no_sanitize("undefined")))
static uint32_t ubsan_access_phdr_type(Elf64_Phdr *phdr, int i) {
    return phdr[i].p_type;
}

__attribute__((visibility("default"))) void ia2_setup_destructors(Elf64_Ehdr *ehdr, int pkey, void *wrap_ia2_compartment_destructor_arg, void *compartment_destructor_ptr_arg, struct FinalizerInfo *finalizers) {
  int res = 0;
  Elf64_Phdr *phdr = (Elf64_Phdr *)((uint8_t *)ehdr + ehdr->e_phoff);
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
      (Elf64_Dyn *)((uintptr_t)ehdr + dynamic_phdr->p_vaddr);
  void *rewrite_base = NULL;
  void *finalizers_region =
      (void *)IA2_ROUND_DOWN((uintptr_t)finalizers, PAGE_SIZE);
  size_t finalizers_region_size =
      IA2_ROUND_DOWN(((uintptr_t)finalizers) + sizeof(finalizers) + PAGE_SIZE,
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
        finalizers->fini_offset = dynamic[i].d_un.d_val;
        dynamic[i].d_un.d_val =
            //(Elf64_Xword)&COMPARTMENT_IDENT(__wrap_ia2_compartment_destructor) -
            (Elf64_Xword)wrap_ia2_compartment_destructor_arg -
            (Elf64_Xword)ehdr;
      } else if (dynamic[i].d_tag == DT_FINI_ARRAY) {
        finalizers->fini_array_offset = dynamic[i].d_un.d_val;
        //dynamic[i].d_un.d_val = (Elf64_Xword)&compartment_destructor_ptr -
        dynamic[i].d_un.d_val = (Elf64_Xword)compartment_destructor_ptr_arg -
                                (Elf64_Xword)ehdr;
      } else if (dynamic[i].d_tag == DT_FINI_ARRAYSZ) {
        finalizers->fini_array_size = dynamic[i].d_un.d_val;
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
