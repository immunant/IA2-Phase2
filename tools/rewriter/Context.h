#pragma once

#include <string>
#include <unordered_map>

typedef std::string Function;

class Context {
public:
  /// Key is target function.
  /// Value is pre/post condition function name.
  std::unordered_multimap<Function, Function> pre_condition_funcs;
  std::unordered_multimap<Function, Function> post_condition_funcs;

  /// Key is canonical type name.
  /// Value is type ID.
  std::unordered_map<std::string, uint32_t> type_ids;
};
