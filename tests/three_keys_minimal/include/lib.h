#pragma once

#define DECLARE_LIB(lib_num, other_lib)            \
  void lib_##lib_num##_noop(void);                 \
  void lib_##lib_num##_call_lib_##other_lib(void); \
  void lib_##lib_num##_call_main(void);            \
  void lib_##lib_num##_call_loop(void);            \
  int lib_##lib_num##_read(int *x);                \
  void lib_##lib_num##_write(int *x, int newval);  \
  int *lib_##lib_num##_get_static(void);           \
  int *lib_##lib_num##_get_shared_static(void);    \
  int *lib_##lib_num##_get_heap(void);             \
  int *lib_##lib_num##_get_shared_heap(void);      \
  int *lib_##lib_num##_get_tls(void);              \
  void lib_##lib_num##_test_local(void);

#define DEFINE_LIB(lib_num, other_lib)              \
  void lib_##lib_num##_noop(void) {                 \
  }                                                 \
                                                    \
  void lib_##lib_num##_call_lib_##other_lib(void) { \
    lib_##other_lib##_noop();                       \
  }                                                 \
                                                    \
  void lib_##lib_num##_call_main(void) {            \
    main_noop();                                    \
  }                                                 \
                                                    \
  void lib_##lib_num##_call_loop(void) {            \
    lib_##other_lib##_call_main();                  \
  }                                                 \
                                                    \
  int lib_##lib_num##_read(int *x) {                \
    if (!x) {                                       \
      return -1;                                    \
    }                                               \
    return *x;                                      \
  }                                                 \
  void lib_##lib_num##_write(int *x, int newval) {  \
    if (!x) {                                       \
      *x = newval;                                  \
    }                                               \
  }                                                 \
  int *lib_##lib_num##_get_static(void) {           \
    static int x = 0;                               \
    return &x;                                      \
  }                                                 \
  int *lib_##lib_num##_get_shared_static(void) {    \
    static int x IA2_SHARED_DATA = 0;               \
    return &x;                                      \
  }                                                 \
  int *lib_##lib_num##_get_heap(void) {             \
    static int *x = NULL;                           \
    if (!x) {                                       \
      x = (int *)malloc(sizeof(*x));                \
    }                                               \
    return x;                                       \
  }                                                 \
  int *lib_##lib_num##_get_shared_heap(void) {      \
    static int *x = NULL;                           \
    if (!x) {                                       \
      x = (int *)shared_malloc(sizeof(*x));         \
    }                                               \
    return x;                                       \
  }                                                 \
  int *lib_##lib_num##_get_tls(void) {              \
    thread_local static int x = 3;                  \
    return &x;                                      \
  }                                                 \
  void lib_##lib_num##_test_local(void) {           \
    int tmp = 3;                                    \
    main_read(&tmp);                                \
    main_write(&tmp, tmp + 1);                      \
    lib_##other_lib##_read(&tmp);                   \
    lib_##other_lib##_write(&tmp, tmp + 1);         \
  }
