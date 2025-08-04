#include <ia2.h>
#include <ia2_test_runner.h>

#define IA2_COMPARTMENT 3
#include <ia2_compartment_init.inc>

#include "lib.h"
#include "lib_2/lib_2.h"
#include "main/main.h"
#include <ia2_allocator.h>
#include <threads.h>

DEFINE_LIB(1, 2);
