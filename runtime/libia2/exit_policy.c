#include "ia2_exit_policy.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ia2_exit_policy_t current_policy = IA2_EXIT_POLICY_CALLGATE;
static pthread_once_t policy_init_once = PTHREAD_ONCE_INIT;

static ia2_exit_policy_t parse_policy(const char *value) {
  if (value == NULL) {
    return IA2_EXIT_POLICY_CALLGATE;
  }

  if (strcmp(value, "union") == 0) {
    return IA2_EXIT_POLICY_UNION;
  }
  if (strcmp(value, "callgate") == 0) {
    return IA2_EXIT_POLICY_CALLGATE;
  }
  if (strcmp(value, "auto") == 0) {
    return IA2_EXIT_POLICY_AUTO;
  }

  fprintf(stderr,
          "[IA2_EXIT] WARNING: Unknown IA2_EXIT_POLICY value '%s'; defaulting to 'callgate'\n",
          value);
  return IA2_EXIT_POLICY_CALLGATE;
}

static void init_policy_once(void) {
  const char *env = getenv("IA2_EXIT_POLICY");
  current_policy = parse_policy(env);
}

ia2_exit_policy_t ia2_exit_policy_get(void) {
  pthread_once(&policy_init_once, init_policy_once);
  return current_policy;
}

const char *ia2_exit_policy_name(ia2_exit_policy_t policy) {
  switch (policy) {
  case IA2_EXIT_POLICY_UNION:
    return "union";
  case IA2_EXIT_POLICY_CALLGATE:
    return "callgate";
  case IA2_EXIT_POLICY_AUTO:
    return "auto";
  }
  return "unknown";
}

