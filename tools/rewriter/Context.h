#pragma once

#include "TypeOps.h"

#include <string>
#include <unordered_map>

class Context {
public:
  /// Key is target function name.
  /// Value is pre/post condition function name.
  std::unordered_multimap<std::string, std::string> pre_condition_funcs;
  std::unordered_multimap<std::string, std::string> post_condition_funcs;

  TypeInfoInterner types;

  /// Key is function name.
  std::unordered_map<std::string, TypeId> constructors;
  std::unordered_map<std::string, TypeId> destructors;
};
