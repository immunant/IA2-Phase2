#pragma once
#include <vector>

/* the SysV ABI parameter classification of an eightbyte */
enum class CAbiArgKind {
  Integral,
  Float,
  Memory,
};

struct CAbiSignature {
  std::vector<CAbiArgKind> args;
  std::vector<CAbiArgKind> ret;
  bool variadic;
};
