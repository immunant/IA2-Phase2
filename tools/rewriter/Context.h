#pragma once

#include "TypeOps.h"

class Context {
public:
  /// Key is target function.
  /// Value is pre/post condition function name.
  std::unordered_multimap<std::string, std::string> pre_condition_funcs;
  std::unordered_multimap<std::string, std::string> post_condition_funcs;

  TypeInfoInterner types;

  std::unordered_map<std::string, TypeId> constructors;
  std::unordered_map<std::string, TypeId> destructors;
};
