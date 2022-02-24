#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <sstream>
#include <vector>

#include "CAbi.h"

struct ParamLocation {
private:
  ParamLocation(const char *reg) : reg(reg) {}

public:
  const char *reg;

  static ParamLocation Register(const char *regname) {
    return ParamLocation(regname);
  }
  static ParamLocation Stack() { return ParamLocation(nullptr); }

  bool is_stack() const { return reg == nullptr; }
  bool is_xmm() const {
    return reg != nullptr && reg[0] == 'x' && reg[1] == 'm' && reg[2] == 'm';
  }
  const char *as_str() const {
    if (reg) {
      return reg;
    } else {
      return "<stack>";
    }
  }
  operator const char *() const { return as_str(); }
  size_t size() const { return is_xmm() ? 16 : 8; }
};

const std::array<const char *, 6> int_param_reg_order = {"rdi", "rsi", "rdx",
                                                         "rcx", "r8",  "r9"};
const std::array<const char *, 8> xmms = {"xmm0", "xmm1", "xmm2", "xmm3",
                                          "xmm4", "xmm5", "xmm6", "xmm7"};

const std::array<const char *, 6> int_ret_reg_order = {"rax", "rdx"};

const std::array<const char *, 3> cabi_arg_kind_names = {"int", "float", "mem"};

/// Compute abi locations for parameters of a C-abi function, given its sequence
/// of argument kinds.
std::vector<ParamLocation> param_locations(const CAbiSignature &func) {
  std::vector<ParamLocation> locs = {};
  size_t ints_used = 0;
  // if the return is in memory, the first integer argument is the location to
  // write the return value
  size_t memory_return_slots =
      std::count_if(func.ret.begin(), func.ret.end(),
                    [](auto &x) { return x == CAbiArgKind::Memory; });
  if (memory_return_slots > 0) {
    locs.push_back(ParamLocation::Register(int_param_reg_order[0]));
    ints_used++;
  }
  size_t floats_used = 0;
  for (const auto &arg : func.args) {
    switch (arg) {
    case CAbiArgKind::Integral:
      if (ints_used < int_param_reg_order.size()) {
        locs.push_back(ParamLocation::Register(int_param_reg_order[ints_used]));
        ints_used += 1;
      } else {
        locs.push_back(ParamLocation::Stack());
      }
      break;
    case CAbiArgKind::Float:
      if (floats_used < xmms.size()) {
        locs.push_back(ParamLocation::Register(xmms[floats_used]));
        floats_used += 1;
      } else {
        locs.push_back(ParamLocation::Stack());
      }
      break;
    case CAbiArgKind::Memory:
      locs.push_back(ParamLocation::Stack());
      break;
    }
  }
  return locs;
}

std::vector<ParamLocation> return_locations(const CAbiSignature &func) {
  std::vector<ParamLocation> locs = {};
  size_t size_in_qwords = func.ret.size();

  if (func.ret.empty()) {
    return locs;
  }

  for (auto &kind : func.ret) {
    switch (kind) {
    case CAbiArgKind::Integral:
      locs.push_back(ParamLocation::Register(int_ret_reg_order[0]));
      if (size_in_qwords == 2) {
        locs.push_back(ParamLocation::Register(int_ret_reg_order[1]));
      }
      break;
    case CAbiArgKind::Float:
      // TODO: handle x87 in st0 and complex x87 in st0+st1
      locs.push_back(ParamLocation::Register(xmms[0]));
      if (size_in_qwords == 2) {
        locs.push_back(ParamLocation::Register(xmms[1]));
      }
      break;
    case CAbiArgKind::Memory:
      // memory return also returns address in first return register
      if (locs.empty()) {
        locs.push_back(ParamLocation::Register(int_ret_reg_order[0]));
      }
      locs.push_back(ParamLocation::Stack());
      break;
    }
  }

  return locs;
}

#define INDENT "    "
#define COMMENT_PREFIX "// "

static void add_asm_line(std::stringstream &ss, const std::string &s) {
  ss << INDENT << "\"" << s << "\\n\"" << std::endl;
}

static void add_raw_line(std::stringstream &ss, const std::string &s) {
  ss << INDENT << s << std::endl;
}

static void add_comment_line(std::stringstream &ss, const std::string &s) {
  ss << INDENT << COMMENT_PREFIX << s << std::endl;
}

static void emit_reg_push(std::stringstream &ss, const ParamLocation &loc) {
  using namespace std::string_literals;

  assert(!loc.is_stack());
  if (loc.is_xmm()) {
    add_asm_line(ss, "subq $16, %rsp");
    add_asm_line(ss, "movdqu %"s + loc.as_str() + ", (%rsp)");
  } else {
    add_asm_line(ss, "pushq %"s + loc.as_str());
  }
}

static void emit_reg_pop(std::stringstream &ss, const ParamLocation &loc) {
  using namespace std::string_literals;

  assert(!loc.is_stack());
  if (loc.is_xmm()) {
    add_asm_line(ss, "movdqu (%rsp), %"s + loc.as_str());
    add_asm_line(ss, "addq $16, %rsp");
  } else {
    add_asm_line(ss, "popq %"s + loc.as_str());
  }
}

static void append_arg_kinds(std::stringstream &ss,
                             std::vector<CAbiArgKind> args) {
  bool first = true;
  for (auto arg : args) {
    if (!first) {
      ss << ", ";
    }
    ss << cabi_arg_kind_names[(int)arg];
    first = false;
  }
}

static std::string sig_string(const CAbiSignature &sig, const std::string &name) {
  std::stringstream ss = {};
  ss << name << "(";
  append_arg_kinds(ss, sig.args);
  ss << ")";
  if (!sig.ret.empty()) {
    ss << " -> ";
    append_arg_kinds(ss, sig.ret);
  }
  return ss.str();
}

std::string emit_asm_wrapper(const CAbiSignature &sig, const std::string &name) {
  using namespace std::string_literals;

  std::stringstream ss = {};

  auto param_locs = param_locations(sig);
  size_t stack_arg_count = std::count_if(param_locs.begin(),
    param_locs.end(), [](auto &x) { return x.is_stack(); });
  size_t stack_arg_size = stack_arg_count * 8;
  size_t reg_arg_count = param_locs.size() - stack_arg_count;

  auto return_locs = return_locations(sig);
  size_t stack_return_count = std::count_if(return_locs.begin(),
    return_locs.end(), [](auto &x) { return x.is_stack(); });
  size_t stack_return_size = stack_return_count * 8;
  size_t reg_return_size = 0;
  for (auto loc : return_locs) {
      if (!loc.is_stack()) {
          reg_return_size += loc.size();
      }
  }

  // Before the call the compartment's stack consists of the stack arguments and
  // return value, if any, plus the return address.
  // The return value takes up `stack_return_size + 1` qwords since we also need
  // to store a pointer to the previous return value memory which was passed in
  // via rdi. If the return value does not use memory, this entire subexpression
  // is zero. We also unconditionally add one qword for the return address which
  // is implicitly pushed onto the stack by the call.
  size_t compartment_stack_space = stack_arg_size + (stack_return_size > 0 ? 8 + stack_return_size : 0) + 8;
  // Before each call the value %rsp + 8 should be a multiple of 16. In other words, `stack_alignment` should equal 8.
  size_t stack_alignment = compartment_stack_space % 16;


  add_comment_line(ss, "Wrapper for "s + sig_string(sig, name) + ":");
  // Declare symbol
  ss << INDENT << "\".global __ia2_" << name << "\\n\"" << std::endl;
  ss << INDENT << "\"__ia2_" << name << ":\\n\"" << std::endl;

  // Save trusted stack pointer
  add_comment_line(ss, "Save trusted stack pointer");
  add_asm_line(ss, "movq ia2_trusted_stackptr@GOTPCREL(%rip), %rax");
  add_asm_line(ss, "movq %rsp, (%rax)");

  // Switch to untrusted stack
  add_comment_line(ss, "Switch to untrusted stack");
  add_asm_line(ss, "movq ia2_untrusted_stackptr@GOTPCREL(%rip), %rsp");
  add_asm_line(ss, "movq (%rsp), %rsp");

  // When returning via memory, the address of the return value is passed in
  // rdi. Since this is not a shared buffer, we can't reuse the caller's return
  // memory. Instead we must save the initial rdi then allocate space on the
  // compartment's stack and set rdi to that address.
  if (stack_return_size > 0) {
    add_comment_line(ss, "Allocate space on the compartment's stack for the return value");
    add_asm_line(ss, "subq $"s + std::to_string(stack_return_size) + ", %rsp");
    add_comment_line(ss, "Save address of the caller's return value");
    add_asm_line(ss, "pushq %rdi");
    add_comment_line(ss, "Set rdi to the compartment's return memory");
    add_asm_line(ss, "movq %rsp, %rdi");
    add_asm_line(ss, "addq $8, %rdi");
  }

  if (stack_alignment != 8) {
      assert(stack_alignment == 0);
      add_asm_line(ss, "subq $8, %rsp");
  }

  // Copy stack args to untrusted stack
  if (stack_arg_count > 0) {
    // Use rax to point at the trusted stack which we are copying from
    add_comment_line(ss, "Copy stack arguments from the caller's stack to the compartment's");
    add_asm_line(ss, "movq ia2_trusted_stackptr@GOTPCREL(%rip), %rax");
    add_asm_line(ss, "movq (%rax), %rax");
    // This is effectively a memcpy of size `stack_arg_size` from the caller's
    // stack to the compartment's
    size_t arg_stack_offset = 0;
    for (int i = 0; i < stack_arg_size; i += 8) {
      add_asm_line(ss, "pushq " + std::to_string(stack_arg_size - i) + "(%rax)");
    }
  }


  // Zero out all unused registers. First we save all registers containing args
  if (reg_arg_count > 0) {
    add_comment_line(ss, "Save used arg regs as they are needed post-scrubbing");
    for (const auto &loc : param_locs) {
      if (!loc.is_stack()) {
        emit_reg_push(ss, loc);
      }
    }
  }

  // Zero all registers except rsp
  //add_comment_line(ss, "Scrub registers before call");
  //add_asm_line(ss, "call __libia2_scrub_registers");

  // Restore used arg regs after zeroing registers
  if (reg_arg_count > 0) {
    add_comment_line(ss, "Restore arg regs for call");
    for (auto loc = param_locs.rbegin(); loc != param_locs.rend(); loc++) {
      if (!loc->is_stack()) {
        emit_reg_pop(ss, *loc);
      }
    }
  }

  // Change pkru to untrusted using rax, r10 and r11 as scratch registers
  add_comment_line(ss, "Change pkru to untrusted");
  add_raw_line(ss, "GATE_PUSH");

  // Call wrapped function
  add_comment_line(ss, "call wrapped function");
  add_asm_line(ss, "call "s + name);

  add_comment_line(ss, "Change pkru to trusted");
  add_asm_line(ss, "movq %rax, %r9");
  // Change pkru to trusted using rax, r10 and r11 as scratch registers
  add_raw_line(ss, "GATE_POP");
  add_asm_line(ss, "movq %r9, %rax");

  // Free stack space used for stack args on the untrusted stack
  if (stack_arg_size > 0) {
    add_comment_line(ss, "Free stack space used for stack args");
    add_asm_line(ss, "addq $"s + std::to_string(stack_arg_size) + ", %rsp");
  }

  if (stack_alignment != 8) {
    assert(stack_alignment == 0);
    add_asm_line(ss, "addq $8, %rsp");
  }

  // Copy any stack returns to caller's stack
  if (stack_return_size > 0) {
    // If the return value is in memory, we must've pushed the initial rdi
    // (which pointed to the caller's return memory) onto the compartment's stack

    add_asm_line(ss, "popq %rax");

    add_comment_line(ss, "Copy stack return values to caller's stack");
    add_comment_line(ss, "Copy "s + std::to_string(stack_return_size) + " bytes from *rax to *rdi");
    for (int i = 0; i < stack_return_size; i += 8) {
      add_asm_line(ss, "popq "s + std::to_string(i) + "(%rax)");
      //add_asm_line(ss, "movq "s + std::to_string(i) + "(%rax), %rsi");
      //add_asm_line(ss, "movq %rsi, "s + std::to_string(i) + "(%rdi)");
    }
    // Free space for return value on the compartment's stack
    //add_asm_line(ss, "addq $"s + std::to_string(stack_return_size) + ", %rsp");
  }

  add_comment_line(ss, "Push return regs to caller's stack before scrubbing registers");
  add_asm_line(ss, "movq ia2_trusted_stackptr@GOTPCREL(%rip), %rsp");
  add_asm_line(ss, "movq (%rsp), %rsp");
  // Push return regs to the caller's stack
  for (auto loc : return_locs) {
    if (!loc.is_stack()) {
        emit_reg_push(ss, loc);
    }
  }

  // Zero all registers except rsp
  //add_comment_line(ss, "Scrub registers after call");
  //add_asm_line(ss, "call __libia2_scrub_registers");

  // pop return regs
  add_comment_line(ss, "Pop return regs");
  for (auto loc = return_locs.rbegin(); loc != return_locs.rend(); loc++) {
    if (!loc->is_stack()) {
      emit_reg_pop(ss, *loc);
    }
  }

  // Return
  add_comment_line(ss, "Return");
  add_asm_line(ss, "ret");

  return ss.str();
}
