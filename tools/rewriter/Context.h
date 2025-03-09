#pragma once

#include "TypeOps.h"

#include <string>
#include <unordered_map>

typedef std::string Function;

class Context {
public:
  /// Key is target function name.
  /// Value is pre/post condition function name.
  std::unordered_multimap<Function, Function> pre_condition_funcs;
  std::unordered_multimap<Function, Function> post_condition_funcs;

  TypeInfoInterner types;

  /// Key is function name.
  std::unordered_map<std::string, TypeId> constructors;
  std::unordered_map<std::string, TypeId> destructors;
};
