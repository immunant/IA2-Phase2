# Define QEMU_COMMAND
#
if(NOT LIBIA2_AARCH64)
  #message(FATAL_ERROR "LIBIA2_AARCH64 must be defined to use run_in_qemu.")
  set(QEMU_COMMAND )
else()
  # unless natively AArch64, default to running tests with qemu-aarch64 and a custom LD_LIBRARY_PATH
  if (NOT ${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL aarch64)
    if (NOT DEFINED CMAKE_CROSSCOMPILING_EMULATOR)
      set(CMAKE_CROSSCOMPILING_EMULATOR qemu-aarch64 -E LD_LIBRARY_PATH=/usr/aarch64-linux-gnu/lib:/usr/aarch64-linux-gnu/lib64)
    endif()
  endif()
  set(QEMU_COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR}
    "-one-insn-per-tb"
    "-L" "${CMAKE_BINARY_DIR}/external/glibc/sysroot/usr/"
    "-E" "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/external/glibc/sysroot/usr/lib:/usr/aarch64-linux-gnu/lib:/usr/aarch64-linux-gnu/lib64"
  )
endif()
