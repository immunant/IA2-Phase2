add_ia2_compartment(microbench LIBRARY
  SOURCES microbench_lib.c
  PKEY 0
)

add_ia2_compartment(microbenchmark EXECUTABLE
  SOURCES microbench_main.c
  LIBRARIES b63 m microbench
  PKEY 1
)

add_ia2_compartment(microbench_indirect LIBRARY
  SOURCES microbench_lib_indirect.c
  PKEY 0
)

add_ia2_compartment(microbench_indirect2 LIBRARY
  SOURCES microbench_lib_indirect_compartment2.c
  PKEY 2
)

add_ia2_compartment(microbenchmark_indirect EXECUTABLE
  SOURCES microbench_indirect.c
  LIBRARIES b63 m microbench_indirect microbench_indirect2
  PKEY 1
)
