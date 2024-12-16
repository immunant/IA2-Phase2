#pragma once
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include "clang/AST/AST.h"

class ArgLocation {
public:
  enum class Kind {
    Integral,
    Float,
    Memory,
  };

private:
  ArgLocation(Kind kind, unsigned size, unsigned align) : _kind(kind), _size(size), _align(align) {}

  // Is this argument on the stack? False for pointers to indirect memory on the
  // stack (unless spilled to the stack as well).
  bool _onStack = false;
  bool _indirectOnStack = false;
  bool _allocated = false;
  Kind _kind;
  unsigned _size;
  unsigned _align;
  const char *_reg = nullptr;
  size_t _stack_offset = 0;

public:
  static ArgLocation Register(Kind kind, unsigned size) {
    return ArgLocation(kind, size, size);
  }
  static ArgLocation Stack(int64_t size, int64_t align) {
    auto loc = ArgLocation(Kind::Memory, static_cast<unsigned>(size), static_cast<unsigned>(align));
    loc._onStack = true;
    return loc;
  }
  static ArgLocation Indirect(unsigned size, unsigned align) {
    auto loc = ArgLocation(Kind::Integral, static_cast<unsigned>(size), static_cast<unsigned>(align));
    loc._indirectOnStack = true;
    return loc;
  }

  void allocate_reg(const char *r) {
    assert(!_allocated && _reg == nullptr);
    _allocated = true;
    _reg = r;
  }
  void allocate_stack(size_t offset) {
    assert(!_allocated);
    _allocated = true;
    _onStack = true;
    _stack_offset = offset;
  }
  void set_indirect_on_stack() {
    assert(!_indirectOnStack);
    _indirectOnStack = true;
  }

  bool is_allocated() const { return _allocated; }
  bool is_stack() const { return _onStack; }
  bool is_indirect() const { return _indirectOnStack; }
  bool is_128bit_float() const {
    return _reg != nullptr && ((_reg[0] == 'x' && _reg[1] == 'm' && _reg[2] == 'm') || _reg[0] == 'q');
  }
  const char *as_str() const {
    if (_onStack) {
      return "<stack>";
    } else {
      return _reg;
    }
  }
  operator const char *() const { return as_str(); }
  Kind kind() const { return _kind; }
  size_t size() const { return is_128bit_float() ? 16 : _size; }
  size_t align() const { return _align; }
  size_t stack_offset() const { return _stack_offset; }
};

struct AbiSignature {
  std::vector<ArgLocation> args;
  std::vector<ArgLocation> ret;
  bool variadic;
};

struct ApiSignature {
  const clang::FunctionProtoType *prototype;
};

struct FnSignature {
  ApiSignature api;
  AbiSignature abi;
};
