#pragma once

#include "TypeOps.h"

#include <string>
#include <unordered_map>

class Context {
public:
  /// Key is target function.
  /// Value is pre/post condition function name.
  std::unordered_multimap<std::string, std::string> pre_condition_funcs;
  std::unordered_multimap<std::string, std::string> post_condition_funcs;

  /// Key is canonical type name.
  /// Value is type ID.
  std::unordered_map<std::string, TypeId> type_ids;
};
