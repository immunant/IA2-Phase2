#include <criterion/criterion.h>
#include <ia2.h>

#define IA2_COMPARTMENT 2
#include <ia2_compartment_init.inc>

#include "lib.h"
#include "main/main.h"
#include "lib_1/lib_1.h"
#include <threads.h>
#include <ia2_allocator.h>

DEFINE_LIB(2, 1);
