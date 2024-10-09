#pragma once
#include <vector>

/* the SysV ABI parameter classification of an eightbyte */
enum class CAbiArgKind {
  Integral,
  Float,
  Memory,
};

struct CAbiArgLocation {
  CAbiArgKind kind;

  // For Memory kind
  unsigned size = 8;
  unsigned align = 8;
};

struct CAbiSignature {
  std::vector<CAbiArgLocation> args;
  std::vector<CAbiArgLocation> ret;
  bool variadic;
};
