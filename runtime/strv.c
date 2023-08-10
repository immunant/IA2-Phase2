#include "strv.h"
#include <string.h>

/* compute length of a NULL-terminated string vector */
size_t strvlen(const char **strv) {
  size_t len = 0;
  while (strv[len] != NULL) {
    len++;
  }
  return len;
}

/* free a NULL-terminated string vector */
void strvfree(char **strv) {
  for (size_t i = 0; strv[i] != NULL; i++) {
    free(strv[i]);
  }
  free(strv);
}

/* map a function over each item of a NULL-terminated string vector, returning a
 * new string vector */
char **strvmap(const char **strv, char *(*fn)(const char *)) {
  size_t len = strvlen(strv);
  char **out = malloc(len * sizeof(char *));
  for (int i = 0; i < len; i++) {
    out[i] = fn(strv[i]);
  }
  return out;
}
