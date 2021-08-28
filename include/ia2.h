#pragma once

#define IA2_WRAP_FUNCTION(name)                 \
    __asm__(".symver " #name ",__libia2_" #name "@IA2")
