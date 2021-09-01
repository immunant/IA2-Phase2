#pragma once

#define IA2_WRAP_FUNCTION(name)                 \
    __asm__(".symver " #name ",__ia2_" #name "@IA2")
