#include <criterion/criterion.h>
#include <ia2.h>

#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

#include "lib.h"
#include "main/main.h"
#include "lib_2/lib_2.h"
#include <threads.h>
#include <ia2_allocator.h>

DEFINE_LIB(1, 2);
