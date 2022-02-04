#pragma once
#include <vector>

// stack order is rtl
enum class CAbiArgKind {
  Integral,
  Float,
};

struct CAbiSignature {
  std::vector<CAbiArgKind> args;
  std::vector<CAbiArgKind> ret;
  bool variadic;
};
