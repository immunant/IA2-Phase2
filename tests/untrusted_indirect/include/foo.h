#ifndef FOO_H
#define FOO_H
#include <stdbool.h>
#include <stdint.h>

// CHECK: typedef struct IA2_fnptr__ZTSPFmmmE callback_t;
typedef uint64_t (*callback_t)(uint64_t, uint64_t);

bool register_callback(callback_t cb);
uint64_t apply_callback(uint64_t x, uint64_t y);
void unregister_callback();

#endif
