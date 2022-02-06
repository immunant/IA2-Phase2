#pragma once

// stack order is rtl
enum class CAbiArgKind {
  Integral,
  Float,
};

struct CAbiSignature {
  std::vector<CAbiArgKind> args;
  CAbiArgKind ret;
  bool variadic;
};
