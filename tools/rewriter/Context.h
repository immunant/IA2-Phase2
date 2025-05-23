#pragma once

#include "TypeOps.h"

#include <string>
#include <unordered_map>

typedef std::string Function;

class Context {
public:
  /// Key is target function.
  /// Value is pre/post condition function name.
  std::unordered_multimap<Function, Function> pre_condition_funcs;
  std::unordered_multimap<Function, Function> post_condition_funcs;

  TypeInfoInterner types;
};
