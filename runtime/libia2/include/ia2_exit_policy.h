#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  IA2_EXIT_POLICY_UNION = 0,
  IA2_EXIT_POLICY_CALLGATE,
  IA2_EXIT_POLICY_AUTO,
} ia2_exit_policy_t;

ia2_exit_policy_t ia2_exit_policy_get(void);
const char *ia2_exit_policy_name(ia2_exit_policy_t policy);

#ifdef __cplusplus
}
#endif

