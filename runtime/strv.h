#pragma once

#include <stdlib.h>

size_t strvlen(const char **strv);
void strvfree(char **strv);
char **strvmap(const char **strv, char *(*fn)(const char *));
