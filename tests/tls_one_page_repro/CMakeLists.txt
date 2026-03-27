set(TLS_REPRO_MAIN_TLS_BYTES "16384" CACHE STRING
    "Size in bytes of main executable TLS array for tls_one_page_repro")
set(TLS_REPRO_LIB2_TLS_BYTES "1" CACHE STRING
    "Size in bytes of compartment-2 library TLS array for tls_one_page_repro")
set(TLS_REPRO_SHARED_TLS_BYTES "4128" CACHE STRING
    "Size in bytes of shared library TLS array for tls_one_page_repro")

add_compile_definitions(
    TLS_REPRO_MAIN_TLS_BYTES=${TLS_REPRO_MAIN_TLS_BYTES}
    TLS_REPRO_LIB2_TLS_BYTES=${TLS_REPRO_LIB2_TLS_BYTES}
    TLS_REPRO_SHARED_TLS_BYTES=${TLS_REPRO_SHARED_TLS_BYTES}
)

define_shared_lib(
    SRCS library.c
    PKEY 2
    NEEDS_LD_WRAP
)

define_shared_lib(
    LIBNAME shared_tls0
    SRCS shared_tls.c
    NEEDS_LD_WRAP
)

define_test(
    SRCS main.c
    LIBS tls_one_page_repro_lib shared_tls0
    PKEY 1
    NEEDS_LD_WRAP
    CRITERION_TEST
)
