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

#define COMPARTMENT_IDENT(name) IA2_CONCAT(name##_, IA2_COMPARTMENT)

#if !IA2_ENABLE

/*
 * The rewriter needs to generate call gate wrappers for these functions so make
 * them no-ops with the same function signatures as the real versions when
 * compartments are disabled.
 */
__attribute__((used)) void COMPARTMENT_IDENT(ia2_compartment_destructor)(void) { }

#else

__attribute__((visibility("default"))) __thread void *COMPARTMENT_IDENT(ia2_stackptr) __attribute__((used));

/*
** Initializes a compartment with protection key `n`. Called automatically as a
** constructor when the ELF defining this symbol is loaded. This must only be
** called once for each key. The compartment includes all segments in the ELF
** except the `ia2_shared_data` section, if one exists.
**/


__attribute__((visibility("default"))) void COMPARTMENT_IDENT(init_tls)(void) {
  struct PhdrSearchArgs args = {
      .pkey = IA2_COMPARTMENT,
      .address = (const void *)&COMPARTMENT_IDENT(init_tls),
  };
  dl_iterate_phdr(protect_tls_pages, &args);
}

/* Magic symbol from the linker that marks the start of the ELF headers */
extern Elf64_Ehdr __ehdr_start __attribute__((weak));

/* Filled in by runtime initialization with contents of  .fini_array and
 * .fini */
volatile struct FinalizerInfo COMPARTMENT_IDENT(finalizers)
    __attribute__((__section__(".data.rel.ro.1"))) = {0};

extern void COMPARTMENT_IDENT(__wrap_ia2_compartment_destructor)(void);

__attribute__((visibility("default"))) __attribute__((no_sanitize("undefined"))) __attribute__((used)) void
COMPARTMENT_IDENT(ia2_compartment_destructor)(void) {
  static bool ran_destructors = false;
  if (ran_destructors)
    return;
  assert(&__ehdr_start);

  void (**fini_array)(void) = (void (**)(void))(((uint8_t *)&__ehdr_start) +
                                                COMPARTMENT_IDENT(finalizers).fini_array_offset);
  for (size_t i = 0; i < (COMPARTMENT_IDENT(finalizers).fini_array_size / sizeof(void *)); i++) {
    fini_array[i]();
  }
  if (COMPARTMENT_IDENT(finalizers).fini_offset) {
    void (*fini)(void) =
        (void (*)(void))(((uint8_t *)&__ehdr_start) + COMPARTMENT_IDENT(finalizers).fini_offset);
    fini();
  }

  ran_destructors = true;
}

/* This pointer MUST be in a read-only (.data.rel.ro) section. */
/* TODO: Check this in the resulting binary */
void (*const COMPARTMENT_IDENT(compartment_destructor_ptr))(void) =
    COMPARTMENT_IDENT(__wrap_ia2_compartment_destructor);

#endif // !IA2_ENABLE
