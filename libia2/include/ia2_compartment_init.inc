/* -*-c-*- vi:filetype=c:
 */
#ifndef IA2_COMPARTMENT
#error Missing IA2_COMPARTMENT
#endif

#include <ia2.h>

#define CONCAT_(x, y) x##y
#define CONCAT(x, y) CONCAT_(x, y)
#define COMPARTMENT_IDENT(name) CONCAT(name##_, IA2_COMPARTMENT)

/* Fully expand x to a string if x is a macro (e.g. IA2_COMPARTMENT) */
#define XSTR(x) STR(x)
#define STR(x) #x

__thread void *COMPARTMENT_IDENT(ia2_stackptr) __attribute__((used));

/*
** Initializes a compartment with protection key `n`. Called automatically as a
** constructor when the ELF defining this symbol is loaded. This must only be
** called once for each key. The compartment includes all segments in the ELF
** except the `ia2_shared_data` section, if one exists.
**/

extern int ia2_n_pkeys_to_alloc;
void ensure_pkeys_allocated(int *n_to_alloc);
__attribute__((constructor)) static void COMPARTMENT_IDENT(init_pkey)() {
  ensure_pkeys_allocated(&ia2_n_pkeys_to_alloc);
  struct PhdrSearchArgs args = {
      .pkey = IA2_COMPARTMENT,
      .address = &COMPARTMENT_IDENT(init_pkey),
  };

  __asm__ volatile(
      /* Set PKRU to the compartment's value */
      "xorl %%ecx, %%ecx\n"
      "xorl %%edx, %%edx\n"
      "mov_pkru_eax " XSTR(IA2_COMPARTMENT) "\n"
      IA2_WRPKRU "\n"
      :
      :
      : "rax", "rcx", "rdx");
  dl_iterate_phdr(protect_pages, &args);
  __asm__ volatile(
      /* Set PKRU to fully untrusted (no access) */
      "xorl %%ecx, %%ecx\n"
      "xorl %%edx, %%edx\n"
      "xorl %%eax, %%eax\n"
      IA2_WRPKRU "\n"
      :
      :
      : "rax", "rcx", "rdx");
}

void COMPARTMENT_IDENT(init_tls)(void) {
  struct PhdrSearchArgs args = {
      .pkey = IA2_COMPARTMENT,
      .address = &COMPARTMENT_IDENT(init_tls),
  };
  dl_iterate_phdr(protect_tls_pages, &args);
}
