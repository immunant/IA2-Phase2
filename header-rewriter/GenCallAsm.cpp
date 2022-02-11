#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <ranges>
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
auto param_locations(const CAbiSignature &func) -> std::vector<ParamLocation> {
  std::vector<ParamLocation> locs = {};
  size_t ints_used = 0;
  // if the return is in memory, the first integer argument is the location to
  // write the return value
  size_t memory_return_slots = std::ranges::count_if(
      func.ret, [](auto &x) { return x == CAbiArgKind::Memory; });
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
      } else {
        locs.push_back(ParamLocation::Stack());
      }
      ints_used += 1;
      break;
    case CAbiArgKind::Float:
      if (floats_used < xmms.size()) {
        locs.push_back(ParamLocation::Register(xmms[floats_used]));
      } else {
        locs.push_back(ParamLocation::Stack());
      }
      floats_used += 1;
      break;
    case CAbiArgKind::Memory:
      locs.push_back(ParamLocation::Stack());
      break;
    }
  }
  return locs;
}

auto return_locations(const CAbiSignature &func) -> std::vector<ParamLocation> {
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

static void print_locs(const std::vector<ParamLocation> &locs) {
  for (const auto &loc : locs) {
    std::cout << loc.as_str() << " ";
  }
  std::cout << std::endl;
}

#define INDENT "    "
#define COMMENT_PREFIX "// "

static void add_asm_line(std::stringstream &ss, const std::string &s) {
  ss << INDENT << "\"" << s << "\\n\"" << std::endl;
}

static void add_unquoted_line(std::stringstream &ss, const std::string &s) {
  ss << INDENT << s << std::endl;
}

static void add_comment_line(std::stringstream &ss, const std::string &s) {
  ss << INDENT << COMMENT_PREFIX << s << std::endl;
}

static void emit_reg_push(std::stringstream &ss, const ParamLocation &loc) {
  using namespace std::string_literals;

  assert(!loc.is_stack());
  if (loc.is_xmm()) {
    add_asm_line(ss, "sub rsp, 16");
    add_asm_line(ss, "movdqu [rsp], "s + loc.as_str());
  } else {
    add_asm_line(ss, "push "s + loc.as_str());
  }
}

static void emit_reg_pop(std::stringstream &ss, const ParamLocation &loc) {
  using namespace std::string_literals;

  assert(!loc.is_stack());
  if (loc.is_xmm()) {
    add_asm_line(ss, "movdqu "s + loc.as_str() + ", [rsp]");
    add_asm_line(ss, "add rsp, 16");
  } else {
    add_asm_line(ss, "pop "s + loc.as_str());
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

static auto sig_string(const CAbiSignature &sig, const std::string &name)
    -> std::string {
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

auto emit_call_asm(const CAbiSignature &sig, const std::string &name, int pkey)
    -> std::string {
  using namespace std::string_literals;

  std::stringstream ss = {};

  // use intel syntax to save endangered percent-signs
  add_asm_line(ss, ".intel_syntax noprefix");
  add_comment_line(ss, "wrapper for "s + sig_string(sig, name) + ":");

  // declare symbol
  ss << INDENT << "\".global __ia2_" << name << "\\n\"" << std::endl;
  ss << INDENT << "\"__ia2_" << name << ":\\n\"" << std::endl;

  // save trusted stack ptr to trusted tls
  add_comment_line(ss, "save trusted stack ptr to trusted tls");
  add_asm_line(ss, "mov rax, QWORD PTR ia2_trusted_stackptr@GOTPCREL[rip]");
  add_asm_line(ss, "mov [rax], rsp");

  // switch to untrusted stack
  add_comment_line(ss, "switch to untrusted stack");
  add_asm_line(ss, "mov rsp, QWORD PTR ia2_untrusted_stackptr@GOTPCREL[rip]");
  add_asm_line(ss, "mov rsp, [rsp]");

  auto param_locs = param_locations(sig);
  size_t stack_arg_count =
      std::ranges::count_if(param_locs, &ParamLocation::is_stack);
  size_t stack_arg_size = stack_arg_count * 8;
  size_t reg_arg_count = param_locs.size() - stack_arg_count;
  auto return_locs = return_locations(sig);
  size_t stack_return_count =
      std::ranges::count_if(return_locs, &ParamLocation::is_stack);
  size_t stack_return_size = stack_return_count * 8;

  // if returning via memory, allocate stack space and pass address in rdi
  if (stack_return_size > 0) {
    add_comment_line(ss, "set rdi to location to return via memory");
    add_asm_line(ss, "push rdi");
    add_asm_line(ss, "sub rsp, "s + std::to_string(stack_return_size));
    add_asm_line(ss, "mov rdi, rsp");
  }

  // copy stack args to untrusted stack
  if (stack_arg_count > 0) {
    // use rax to point at the trusted stack
    add_comment_line(ss, "copy stack arguments to the untrusted stack");
    add_asm_line(ss, "mov rax, QWORD PTR ia2_trusted_stackptr@GOTPCREL[rip]");
    add_asm_line(ss, "mov rax, [rax]");
  }
  // effectively memcpy(untrusted_stack, trusted_stack, stack_size);
  size_t arg_stack_offset = stack_arg_size;
  for (const auto &loc : std::ranges::views::reverse(param_locs)) {
    if (loc.is_stack()) {
      arg_stack_offset -= 8;
      add_asm_line(ss, "push qword [rax+"s + std::to_string(arg_stack_offset) +
                           "]");
    }
  }

  // push any reg args
  if (reg_arg_count > 0) {
    add_comment_line(ss,
                     "save used arg regs as they are needed post-scrubbing");
  }
  for (const auto &loc : param_locs) {
    if (!loc.is_stack()) {
      emit_reg_push(ss, loc);
    }
  }

  // call scrub
  add_comment_line(ss, "scrub registers before call");
  add_asm_line(ss, "call __libia2_scrub_registers");

  // change pkru to untrusted
  add_comment_line(ss, "change pkru to untrusted");
  // TODO: this likely isn't necessary
  add_asm_line(ss, "mov rdi, " + std::to_string(pkey));
  add_unquoted_line(ss, "GATE_PUSH");

  // pop arg regs
  if (reg_arg_count > 0) {
    add_comment_line(ss, "restore arg regs for call");
  }
  for (auto loc : std::ranges::views::reverse(param_locs)) {
    if (!loc.is_stack()) {
      emit_reg_pop(ss, loc);
    }
  }

  // call wrapped
  add_comment_line(ss, "call wrapped function");
  add_asm_line(ss, "call "s + name);

  // save return regs while we change pkru
  if (reg_arg_count > 0) {
    add_comment_line(
        ss,
        "push return regs to untrusted stack to preserve them while changing pkru");
  }
  for (const auto &loc : return_locs) {
    if (!loc.is_stack()) {
      emit_reg_push(ss, loc);
    }
  }

  // any prefix of the following may be skipped by untrusted code, so be
  // careful:
  /////////////////////////////////////////////////////////////////////////
  // change pkru to trusted
  add_comment_line(ss, "change pkru to trusted");
  // add_asm_line(ss, "call __libia2_gate_pop");
  add_unquoted_line(ss, "GATE_POP");

  // restore return regs
  if (reg_arg_count > 0) {
    add_comment_line(ss, "pop return regs");
  }
  for (const auto &loc : return_locs) {
    if (!loc.is_stack()) {
      emit_reg_pop(ss, loc);
    }
  }

  // return stack space used for stack args
  if (stack_arg_count > 0) {
    add_comment_line(ss, "return stack space used for stack args");
    add_asm_line(ss, "add rsp, "s + std::to_string(stack_arg_size));
  }

  // copy any stack returns to trusted stack
  if (stack_return_size > 0) {
    // free stack space
    add_asm_line(ss, "add rsp, "s + std::to_string(stack_return_size));
    // if we pushed rdi for memory return, pop it now
    add_comment_line(ss, "copy stack return values to trusted stack");
    add_asm_line(ss, "pop rdi"); // rdi is the implicit return pointer argument
                                 // for stack memory
    add_comment_line(ss, "copy "s + std::to_string(stack_return_size) +
                             " bytes from *rax to *rdi");
  }
  for (int i = 0; i < stack_return_size; i += 8) {
    add_asm_line(ss, "mov rsi, [rax+"s + std::to_string(i) + "]");
    add_asm_line(ss, "mov [rdi+"s + std::to_string(i) + "], rsi");
  }

  add_comment_line(
      ss, "push return regs to trusted stack before scrubbing registers");
  add_asm_line(ss, "mov rdi, QWORD PTR ia2_trusted_stackptr@GOTPCREL[rip]");
  add_asm_line(ss, "mov rdi, [rdi]"); // read the location of top-of-stack
  // push return regs to trusted stack redzone
  auto pushed_offset = 0;
  for (auto loc : return_locs) {
    if (!loc.is_stack()) {
      pushed_offset += loc.size();
      if (loc.is_xmm()) {
        add_asm_line(ss, "movdqu [rdi-"s + std::to_string(pushed_offset) +
                             "], " + loc.as_str());
      } else {
        add_asm_line(ss, "mov [rdi-"s + std::to_string(pushed_offset) + "], " +
                             loc.as_str());
      }
    }
  }

  // call scrub
  add_comment_line(ss, "scrub registers after call");
  add_asm_line(ss, "call __libia2_scrub_registers");

  // switch to trusted stack, adjusting stack ptr for the regs we saved on it
  add_comment_line(
      ss,
      "switch to trusted stack, adjusting stack ptr for any return regs we saved on it");
  add_asm_line(ss, "mov rsp, QWORD PTR ia2_trusted_stackptr@GOTPCREL[rip]");
  add_asm_line(ss, "mov rsp, [rsp]");

  // if any args were pushed to the stack, make room to pop them
  if (pushed_offset > 0)
    add_asm_line(ss, "sub rsp, "s + std::to_string(pushed_offset));

  // pop return regs
  add_comment_line(ss, "pop return regs");
  for (auto loc : return_locs) {
    if (!loc.is_stack()) {
      emit_reg_pop(ss, loc);
    }
  }

  // return
  add_comment_line(ss, "return");
  add_asm_line(ss, "ret");

  // reset syntax to AT&T
  add_asm_line(ss, ".att_syntax");

  return ss.str();
}

/// Dead for now. XXX: should be factored into a runnable test.
static auto gen_call_asm_test_main() -> int {
  using enum CAbiArgKind;
  auto sig = CAbiSignature{
      .args = {Integral, Integral, Integral},
      .ret = {Integral},
      .variadic = false,
  };
  print_locs(param_locations(sig));

  auto sig2 = CAbiSignature{
      .args =
          {
              Integral, Integral, Integral, Integral, Integral, Integral,
              Integral, Integral, Integral, Integral, Integral, Integral,
              Integral, Integral, Integral, Integral, Integral, Integral,
              Integral, Integral, Integral, Integral, Integral, Integral,
              Integral, Integral, Integral, Integral, Integral, Integral,
          },
      .ret = {Integral},
      .variadic = false,
  };
  print_locs(param_locations(sig2));

  auto sig3 = CAbiSignature{
      .args = {Integral, Integral, Integral, Integral, Integral, Integral,
               Integral, Float, Integral},
      .ret = {Integral},
      .variadic = false,
  };
  print_locs(param_locations(sig3));
  auto asm_str = emit_call_asm(sig3, "myfunc", 0);
  std::cout << "asm:\n" << asm_str;

  return 0;
}
