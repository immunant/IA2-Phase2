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

const std::array<const char *, 2> int_ret_reg_order = {"rax", "rdx"};

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
      // FIXME: This returns [rax, rdx, rax, rdx, ...] when func.ret.size is
      // greater than 1. However, we should fix the classification bug seen in
      // the wrapper generated for struct s7 in tests/structs/ before fixing the
      // double counting issue here
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

struct AsmWriter {
  std::stringstream ss;
  std::string terminator;
};

static void add_asm_line(AsmWriter &aw, const std::string &s) {
  aw.ss << INDENT << "\"" << s << "\\n\"";
  if (!aw.terminator.empty()) {
    aw.ss << " " << aw.terminator;
  }
  aw.ss << std::endl;
}

static void add_raw_line(AsmWriter &aw, const std::string &s) {
  aw.ss << INDENT << s;
  if (!aw.terminator.empty()) {
    aw.ss << " " << aw.terminator;
  }
  aw.ss << std::endl;
}

static void add_comment_line(AsmWriter &aw, const std::string &s) {
  aw.ss << INDENT << "/* " << s << " */";
  if (!aw.terminator.empty()) {
    aw.ss << " " << aw.terminator;
  }
  aw.ss << std::endl;
}

static void emit_reg_push(AsmWriter &aw, const ParamLocation &loc) {
  using namespace std::string_literals;

  assert(!loc.is_stack());
  if (loc.is_xmm()) {
    add_asm_line(aw, "subq $16, %rsp");
    add_asm_line(aw, "movdqu %"s + loc.as_str() + ", (%rsp)");
  } else {
    add_asm_line(aw, "pushq %"s + loc.as_str());
  }
}

static void emit_reg_pop(AsmWriter &aw, const ParamLocation &loc) {
  using namespace std::string_literals;

  assert(!loc.is_stack());
  if (loc.is_xmm()) {
    add_asm_line(aw, "movdqu (%rsp), %"s + loc.as_str());
    add_asm_line(aw, "addq $16, %rsp");
  } else {
    add_asm_line(aw, "popq %"s + loc.as_str());
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

static std::string sig_string(const CAbiSignature &sig,
                              const std::string &name) {
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

std::string emit_asm_wrapper(const CAbiSignature &sig, const std::string &name,
                             bool indirect_wrapper) {
  using namespace std::string_literals;

  std::string terminator = {};
  if (indirect_wrapper) {
    terminator = "\\"s;
  }
  AsmWriter aw = {.ss = {}, .terminator = terminator};

  auto param_locs = param_locations(sig);
  size_t stack_arg_count = std::count_if(param_locs.begin(), param_locs.end(),
                                         [](auto &x) { return x.is_stack(); });
  size_t stack_arg_size = stack_arg_count * 8;
  size_t reg_arg_count = param_locs.size() - stack_arg_count;

  auto return_locs = return_locations(sig);
  size_t stack_return_count =
      std::count_if(return_locs.begin(), return_locs.end(),
                    [](auto &x) { return x.is_stack(); });
  size_t stack_return_size = stack_return_count * 8;
  size_t reg_return_size = 0;
  for (auto loc : return_locs) {
    if (!loc.is_stack()) {
      reg_return_size += loc.size();
    }
  }

  /*
    Just before calling the wrapped function, its compartment's stack may
    contain any of the following.

    +-----+
    | top |Top of the stack (stack grows down on x86-64)
    +-----+
    |fnptr|If the call is indirect, place an 8-byte pointer to the callee here.
    +-----+
    |     |Space for the compartment's return value if it has class MEMORY. This
    |ret  |space is only allocated if the pointer to the caller's return value
    |space|memory is also placed on the compartment's stack.
    +-----+
    |     |An 8-byte pointer to the caller's memory for return value. This is
    |ret  |only placed on the stack if the return type has class MEMORY. See
    |ptr  |System V ABI section 3.2.3, subsection "Returning of Values" for
    |     |details.
    +-----+
    |     |8 bytes for alignment. If the size of the other items on the stack
    |     |(including the return address) aren't a multiple 16, these 8 bytes
    |align|are inserted to ensure the stack is aligned for the call. Otherwise
    |     |this doesn't get placed on the stack. The System V ABI specifies this
    |     |must be placed at the end of the argument area so this cannot be
    |     |placed any lower on the stack.
    +-----+
    |stack|Space required for stack arguments. This is initialized from the
    |args |analogous part of the caller's stack. If all arguments are passed in
    |     |registers this space isn't allocated.
    +-----+
    |ret  |Return address for wrapped function. This is implicitly placed on the
    |addr |stack by the call to the wrapped function.
    +-----+


  */

  // The return value takes up `stack_return_size + 1` eightbytes since we also
  // need to store a pointer to the previous return value memory. If the return
  // value doesn't use memory, this entire subexpression is zero.
  size_t compartment_stack_space =
      stack_arg_size + (stack_return_size > 0 ? 8 + stack_return_size : 0);
  if (indirect_wrapper) {
      // Count the space for the function pointer if the call is indirect
      compartment_stack_space += 8;
  }
  // Compute the stack alignment before calling the wrapped function without the
  // 8 bytes for alignment.
  size_t stack_alignment = compartment_stack_space % 16;

  add_comment_line(aw, "Wrapper for "s + sig_string(sig, name) + ":");
  // Define the wrapper symbol
  if (indirect_wrapper) {
    // Jump to a subsection of .text to avoid inlining this wrapper function in
    // the function that invoked the macro for indirect wrappers
    add_asm_line(aw, ".text 1");
    add_raw_line(aw, "\"__ia2_\" UNIQUE_STR(#target) \"_wrapper:\\n\"");
    add_raw_line(aw, "\".equ __ia2_\" UNIQUE_STR(#target) \", .\\n\"");
  } else {
    add_asm_line(aw, ".global __ia2_"s + name);
    add_asm_line(aw, "__ia2_"s + name + ":");
  }

  // Save trusted stack pointer
  add_comment_line(aw, "Save trusted stack pointer");
  add_asm_line(aw, "movq ia2_trusted_stackptr@GOTPCREL(%rip), %rax");
  add_asm_line(aw, "movq %rsp, (%rax)");

  // Switch to untrusted stack
  add_comment_line(aw, "Switch to untrusted stack");
  add_asm_line(aw, "movq ia2_untrusted_stackptr@GOTPCREL(%rip), %rsp");
  add_asm_line(aw, "movq (%rsp), %rsp");

  if (indirect_wrapper) {
    add_raw_line(aw, "\"movq \" UNIQUE_STR(#target) \"@GOTPCREL(%rip), %r10\\n\"");
    add_asm_line(aw, "movq (%r10), %r10");
    add_asm_line(aw, "pushq %r10");
  }

  // When returning via memory, the address of the return value is passed in
  // rdi. Since this memory belongs to the caller, we first allocate space for
  // the return value, then save a pointer to the caller's return value and set
  // rdi to the newly allocated space. Allocating space before saving the old
  // pointer makes it easier to undo these operations after the call. The System
  // V ABI only specifies that the memory for the return value must not overlap
  // any other memory available to the callee so the order of these two stack
  // items doesn't matter.
  if (stack_return_size > 0) {
    add_comment_line(
        aw, "Allocate space on the compartment's stack for the return value");
    add_asm_line(aw, "subq $"s + std::to_string(stack_return_size) + ", %rsp");
    add_comment_line(aw, "Save address of the caller's return value");
    add_asm_line(aw, "pushq %rdi");
    add_comment_line(aw, "Set rdi to the compartment's return value memory");
    add_asm_line(aw, "movq %rsp, %rdi");
    // The new return value is 8 bytes above the bottom of the stack so we need
    // to add 8 to rdi
    add_asm_line(aw, "addq $8, %rdi");
  }

  // Insert 8 bytes to align the stack to 16 bytes if necessary.
  if (stack_alignment != 0) {
    assert(stack_alignment == 8);
    add_asm_line(aw, "subq $8, %rsp");
  }

  // Copy stack args to untrusted stack
  if (stack_arg_count > 0) {
    // Set rax to the caller's stack so we can copy the stack args to the
    // compartment's stack.
    add_comment_line(
        aw, "Copy stack arguments from the caller's stack to the compartment");
    add_asm_line(aw, "movq ia2_trusted_stackptr@GOTPCREL(%rip), %rax");
    add_asm_line(aw, "movq (%rax), %rax");
    // This is effectively a memcpy of size `stack_arg_size` from the caller's
    // stack to the compartment's
    for (int i = 0; i < stack_arg_size; i += 8) {
      // The index into the caller's stack is backwards since pushq will copy to
      // the compartment's stack from the highest addresses to the lowest.
      add_asm_line(aw,
                   "pushq " + std::to_string(stack_arg_size - i) + "(%rax)");
    }
  }

  // Zero out all unused registers. First we save all registers containing args.
  // These pushes have matching pops before calling the wrapped function so this
  // stack space is not shown in the diagram above.
  if (reg_arg_count > 0) {
    add_comment_line(aw, "Save registers containing arguments");
    for (const auto &loc : param_locs) {
      if (!loc.is_stack()) {
        emit_reg_push(aw, loc);
      }
    }
  }

  // Zero all registers except rsp
  add_comment_line(aw, "Zero all registers except rsp");
  // FIXME: If this will use the System V ABI make sure that the %rsp is aligned
  // before this call
  // FIXME: This call causes a fault in the PLT for some indirect calls
  //add_asm_line(aw, "call __libia2_scrub_registers");

  // Restore used arg regs after zeroing registers
  if (reg_arg_count > 0) {
    add_comment_line(aw, "Restore registers containing arguments");
    for (auto loc = param_locs.rbegin(); loc != param_locs.rend(); loc++) {
      if (!loc->is_stack()) {
        emit_reg_pop(aw, *loc);
      }
    }
  }

  // Change pkru to the compartment's value using rax, r10 and r11 as scratch
  // registers
  add_comment_line(aw, "Set PKRU to the compartment's value");
  if (indirect_wrapper) {
    add_raw_line(aw, "GATE(target_pkey)");
  } else {
    add_raw_line(aw, "GATE_PUSH");
  }

  // Call wrapped function
  add_comment_line(aw, "Call wrapped function");
  if (indirect_wrapper) {
    size_t fn_ptr_offset = compartment_stack_space + stack_alignment - 8;
    add_asm_line(aw, "movq "s + std::to_string(fn_ptr_offset) + "(%rsp), %r10");
    add_asm_line(aw, "call *%r10");
  } else {
    add_asm_line(aw, "call "s + name);
  }

  add_comment_line(aw, "Set PKRU to the caller's value");
  // FIXME: The GATE macros use rax as a scratch register, but it may contain a
  // return value after the call so we save it in r9. We should fix this when we
  // combine PKRU and stack switching for indirect calls.
  add_asm_line(aw, "movq %rax, %r9");
  // Change pkru to the caller's value using rax, r10 and r11 as scratch
  // registers
  if (indirect_wrapper) {
    add_raw_line(aw, "GATE(caller_pkey)");
  } else {
    add_raw_line(aw, "GATE_POP");
  }
  add_asm_line(aw, "movq %r9, %rax");

  // Free stack space used for stack args on the untrusted stack
  if (stack_arg_size > 0) {
    add_comment_line(aw, "Free stack space used for stack args");
    add_asm_line(aw, "addq $"s + std::to_string(stack_arg_size) + ", %rsp");
  }

  // If we inserted 8 bytes to align the stack before calling the wrapped
  // function, free this space.
  if (stack_alignment != 0) {
    assert(stack_alignment == 8);
    add_asm_line(aw, "addq $8, %rsp");
  }

  // Copy any stack returns to caller's stack
  if (stack_return_size > 0) {
    // If the return value is in memory, we pushed the initial rdi (which
    // pointed to the caller's return value memory) onto the compartment's
    // stack. Now we pop it into rax which is where the address of the return
    // value must be on return.
    add_asm_line(aw, "popq %rax");

    // After the pop rsp points to the memory for the return value on the
    // compartment's stack.
    add_comment_line(aw,
                     "Copy "s + std::to_string(stack_return_size) +
                         " bytes for the return value to the caller's stack");
    for (int i = 0; i < stack_return_size; i += 8) {
      add_asm_line(aw, "popq "s + std::to_string(i) + "(%rax)");
    }
  }

  add_asm_line(aw, "movq ia2_trusted_stackptr@GOTPCREL(%rip), %rsp");
  add_asm_line(aw, "movq (%rsp), %rsp");

  add_comment_line(
      aw, "Push return regs to caller's stack before scrubbing registers");
  // Push return regs to the caller's stack. These pushes have matching pops
  // before the return so it has no net effect on the caller's stack pointer.
  for (auto loc : return_locs) {
    if (!loc.is_stack()) {
      emit_reg_push(aw, loc);
    }
  }

  // Zero all registers except rsp
  add_comment_line(aw, "Scrub registers after call");
  // FIXME: If this will use the System V ABI make sure that the %rsp is aligned
  // before this call
  // FIXME: This call causes a fault in the PLT for some indirect calls
  //add_asm_line(aw, "call __libia2_scrub_registers");

  // pop return regs
  add_comment_line(aw, "Pop return regs");
  for (auto loc = return_locs.rbegin(); loc != return_locs.rend(); loc++) {
    if (!loc->is_stack()) {
      emit_reg_pop(aw, *loc);
    }
  }

  // Return to the caller
  add_comment_line(aw, "Return to the caller");
  add_asm_line(aw, "ret");

  if (indirect_wrapper) {
    // Jump to the previous location counter to undo the effect of `.text 1`
    // above
    add_asm_line(aw, ".previous");
  }

  return aw.ss.str();
}
