#include <ia2_loader.h>
#include <ia2.h>
#include <stdatomic.h>
#include <stdint.h>

_Thread_local bool ia2_in_loader_gate = false;
IA2_SHARED_DATA _Atomic bool ia2_pkru_gates_active = false;
IA2_SHARED_DATA _Atomic unsigned long ia2_loader_alloc_count = 0;
IA2_SHARED_DATA _Atomic unsigned long ia2_loader_mmap_count = 0;
IA2_SHARED_DATA _Atomic unsigned long ia2_pkru_gate_switch_count = 0;

void ia2_loader_gate_enter(void) {}
void ia2_loader_gate_exit(void) {}

unsigned long ia2_get_loader_alloc_count(void) {
  return atomic_load(&ia2_loader_alloc_count);
}

unsigned long ia2_get_loader_mmap_count(void) {
  return atomic_load(&ia2_loader_mmap_count);
}

#ifdef IA2_DEBUG
IA2_SHARED_DATA _Atomic unsigned long ia2_dlopen_count = 0;
IA2_SHARED_DATA _Atomic unsigned long ia2_dlmopen_count = 0;
IA2_SHARED_DATA _Atomic unsigned long ia2_dlclose_count = 0;
IA2_SHARED_DATA _Atomic unsigned long ia2_dlsym_count = 0;
IA2_SHARED_DATA _Atomic unsigned long ia2_dlvsym_count = 0;
IA2_SHARED_DATA _Atomic unsigned long ia2_dladdr_count = 0;
IA2_SHARED_DATA _Atomic unsigned long ia2_dladdr1_count = 0;
IA2_SHARED_DATA _Atomic unsigned long ia2_dlinfo_count = 0;
IA2_SHARED_DATA _Atomic unsigned long ia2_dlerror_count = 0;
IA2_SHARED_DATA _Atomic unsigned long ia2_dl_iterate_phdr_count = 0;

unsigned int ia2_get_loader_gate_depth(void) {
  return 0;
}

unsigned int ia2_get_pkru_gate_depth(void) {
  return 0;
}

unsigned long ia2_get_pkru_gate_switch_count(void) {
  return atomic_load(&ia2_pkru_gate_switch_count);
}

unsigned long ia2_get_dlopen_count(void) {
  return 0;
}

unsigned long ia2_get_dlmopen_count(void) {
  return 0;
}

unsigned long ia2_get_dlclose_count(void) {
  return 0;
}

unsigned long ia2_get_dlsym_count(void) {
  return 0;
}

unsigned long ia2_get_dlvsym_count(void) {
  return 0;
}

unsigned long ia2_get_dladdr_count(void) {
  return 0;
}

unsigned long ia2_get_dladdr1_count(void) {
  return 0;
}

unsigned long ia2_get_dlinfo_count(void) {
  return 0;
}

unsigned long ia2_get_dlerror_count(void) {
  return 0;
}

unsigned long ia2_get_dl_iterate_phdr_count(void) {
  return 0;
}

uint32_t ia2_get_current_pkru(void) {
  return 0;
}
#endif
