#include "llvm/Support/FormatVariadic.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <sstream>
#include <vector>

#include "CAbi.h"
#include "GenCallAsm.h"

using namespace std::string_literals;

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

// rsp is also a preserved register, but we handle it separately from these
// since it's the stack
const std::array<const char *, 6> preserved_registers = {"rbx", "rbp", "r12",
                                                         "r13", "r14", "r15"};

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
    case CAbiArgKind::Integral: {
      if (ints_used < int_param_reg_order.size()) {
        locs.push_back(ParamLocation::Register(int_param_reg_order[ints_used]));
        ints_used += 1;
      } else {
        locs.push_back(ParamLocation::Stack());
      }
      break;
    }
    case CAbiArgKind::Float: {
      if (floats_used < xmms.size()) {
        locs.push_back(ParamLocation::Register(xmms[floats_used]));
        floats_used += 1;
      } else {
        locs.push_back(ParamLocation::Stack());
      }
      break;
    }
    case CAbiArgKind::Memory: {
      locs.push_back(ParamLocation::Stack());
      break;
    }
    }
  }
  return locs;
}

std::vector<ParamLocation> return_locations(const CAbiSignature &func) {
  std::vector<ParamLocation> locs = {};

  if (func.ret.empty()) {
    return locs;
  }

  size_t ints_used = 0;
  size_t floats_used = 0;
  for (const auto &kind : func.ret) {
    switch (kind) {
    case CAbiArgKind::Integral:
      assert(ints_used < 2);
      locs.push_back(ParamLocation::Register(int_ret_reg_order[ints_used]));
      ints_used += 1;
      break;
    case CAbiArgKind::Float:
      // TODO: handle x87 in st0 and complex x87 in st0+st1
      assert(floats_used < 2);
      locs.push_back(ParamLocation::Register(xmms[floats_used]));
      floats_used += 1;
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
  // An optional string to terminate every line with.
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
  assert(!loc.is_stack());
  if (loc.is_xmm()) {
    add_asm_line(aw, "subq $16, %rsp");
    add_asm_line(aw, "movdqu %"s + loc.as_str() + ", (%rsp)");
  } else {
    add_asm_line(aw, "pushq %"s + loc.as_str());
  }
}

static void emit_reg_pop(AsmWriter &aw, const ParamLocation &loc) {
  assert(!loc.is_stack());
  if (loc.is_xmm()) {
    add_asm_line(aw, "movdqu (%rsp), %"s + loc.as_str());
    add_asm_line(aw, "addq $16, %rsp");
  } else {
    add_asm_line(aw, "popq %"s + loc.as_str());
  }
}

// Adapt a macro parameter (for indirect calls) or a macro into a context
// suitable for interpolation into the string literals for asm(), expanding it
// in the process. This involves closing/reopening the asm string and
// stringifying the macro.
static std::string asm_macro_expansion(const std::string &macro) {
  return llvm::formatv("\" XSTR({0}) \"", macro);
}

// Emit code to set the PKRU. Clobbers eax, ecx and edx.
// \p pkey is a std::string of an assembly literal without a $ prefix.
static void emit_wrpkru(AsmWriter &aw, const std::string &pkey) {
  // wrpkru requires zeroing ecx and edx
  add_asm_line(aw, "xorl %ecx, %ecx");
  add_asm_line(aw, "xorl %edx, %edx");
  add_asm_line(aw,
               llvm::formatv("mov_pkru_eax {0}", asm_macro_expansion(pkey)));
  add_raw_line(aw, "IA2_WRPKRU \"\\n\"");
}

// Emit code to set the PKRU. Clobbers eax, ecx and edx.
// \p pkey is a std::string of an assembly literal without a $ prefix.
static void emit_mixed_wrpkru(AsmWriter &aw, const std::string &pkey0,
                              const std::string &pkey1) {
  // wrpkru requires zeroing ecx and edx
  add_asm_line(aw, "xorl %ecx, %ecx");
  add_asm_line(aw, "xorl %edx, %edx");
  add_asm_line(aw, llvm::formatv("mov_mixed_pkru_eax {0}, {1}",
                                 asm_macro_expansion(pkey0),
                                 asm_macro_expansion(pkey1)));
  add_raw_line(aw, "IA2_WRPKRU \"\\n\"");
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
                             WrapperKind kind, const std::string &target_pkey,
                             bool as_macro) {
  // Indirect wrappers and manually defined direct wrappers are always generated
  // as macros
  as_macro = as_macro || (kind == WrapperKind::Indirect);
  std::string terminator = {};
  // Code generated as a macro needs to terminate each line with '\'
  if (as_macro) {
    terminator = "\\"s;
  }
  AsmWriter aw = {.ss = {}, .terminator = terminator};

  std::string caller_pkey;
  if (as_macro) {
    // caller_pkey is the macro param defining the caller's pkey in the
    // IA2_FNPTR_* macros
    caller_pkey = "caller_pkey";
  } else {
    // The CALLER_PKEY macro must be defined to compile the wrapper source files
    // that this is written to. Failure to define it gives a preprocessor error.
    caller_pkey = "CALLER_PKEY";
  }

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

  /*
    Just before calling the wrapped function, its compartment's stack may
    contain any of the following.

    +-----+
    | top |Top of the stack (stack grows down on x86-64). This address - 8 is
    |     |aligned to 16 bytes assuming the wrapper's caller follows the SysV
    |     |ABI.
    +-----+
    |ret  |Padding for alignment prior to the compartment's return value (if it
    |align|has class MEMORY) as such memory must be 16B aligned.
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
  size_t stack_return_align = 16;
  size_t stack_return_padding = 0;
  size_t start_of_ret_space = 0;
  if (kind == WrapperKind::Indirect) {
    // Count the space for the function pointer if the call is indirect.
    start_of_ret_space += 8;
  }

  if (stack_return_size > 0) {
    // If we have a stack return, we also have to save the old ret ptr, which
    // precedes the stack return itself.
    start_of_ret_space += 8;

    // See what unpadded alignment would be for the start of the stack return.
    size_t unaligned = start_of_ret_space % stack_return_align;
    stack_return_padding = unaligned != 0 ? stack_return_align - unaligned : 0;
  }

  // Count room for for the ret align padding, return value, and our ret ptr.
  size_t compartment_stack_space = start_of_ret_space + stack_return_size + stack_return_padding + stack_arg_size;
  // Compute what the stack alignment would be before calling the wrapped
  // function to check if we need to insert 8 bytes for alignment. We add 8
  // bytes to the compartment_stack_space since the frame is initially off by 8
  // bytes.
  size_t stack_alignment = (compartment_stack_space + 8) % 16;

  add_comment_line(aw, "Wrapper for "s + sig_string(sig, name) + ":");
  // Define the wrapper symbol
  if (kind == WrapperKind::Indirect) {
    // This is for IA2_CALL
    // Jump to a subsection of .text to avoid inlining this wrapper function in
    // the function that invoked the macro for indirect wrappers
    add_asm_line(aw, ".text 1");
    // Set the wrapper's symbol to the current location counter
    // We use .equ rather than defining a label since clang has issues with
    // expanding macros into asm string literals.
    add_raw_line(
        aw, "\".equ \" XSTR(PASTE4(__ia2_, ty, _line_, __LINE__)) \", .\\n\"");
  } else if (as_macro) {
    // This is for IA2_DEFINE_WRAPPER
    add_asm_line(aw, ".text 1");
    add_asm_line(aw, llvm::formatv(".global __ia2_{0}_{1}_{2}",
                                   asm_macro_expansion("target"),
                                   asm_macro_expansion(caller_pkey),
                                   asm_macro_expansion(target_pkey)));
    add_asm_line(
        aw, llvm::formatv(".equ __ia2_{0}_{1}_{2}, . ", asm_macro_expansion("target"),
                          asm_macro_expansion(caller_pkey),
                          asm_macro_expansion(target_pkey)));
  } else {
    // This is for wrappers defined in the shims
    add_asm_line(aw, ".text");
    add_asm_line(aw, ".global __wrap_"s + name);
    add_asm_line(aw, "__wrap_"s + name + ":");
  }

  // Save registers that are preserved across function calls before switching to
  // the other compartment's stack. This is on the caller's stack so it's not in
  // the diagram above.
  for (auto &r : preserved_registers) {
    add_asm_line(aw, "pushq %"s + r);
  }

  // Change pkru to the intermediate value before copying args
  add_comment_line(aw, "Set PKRU to the intermediate value to move arguments");
  // wrpkru requires zeroing rcx and rdx, but they may have arguments so use r10
  // and r11 as scratch registers
  add_asm_line(aw, "movq %rcx, %r10");
  add_asm_line(aw, "movq %rdx, %r11");
  emit_mixed_wrpkru(aw, caller_pkey, target_pkey);
  add_asm_line(aw, "movq %r10, %rcx");
  add_asm_line(aw, "movq %r11, %rdx");

  // Save caller stack pointer
  add_comment_line(aw, "Save caller stack pointer");
  add_asm_line(aw, "movq ia2_stackptrs@GOTPCREL(%rip), %rax");
  add_asm_line(aw,
               llvm::formatv("leaq \" STACK({0}) \"(%rax), %rax", caller_pkey));
  add_asm_line(aw, "movq %rsp, (%rax)");

  // Switch to target stack
  add_comment_line(aw, "Switch to target stack");
  add_asm_line(aw, "movq ia2_stackptrs@GOTPCREL(%rip), %rsp");
  add_raw_line(aw, llvm::formatv("\"movq \" STACK({0}) \"(%rsp), %rsp\\n\"",
                                 target_pkey));

  // When returning via memory, the address of the return value is passed in
  // rdi. Since this memory belongs to the caller, we first allocate space for
  // the return value, then save a pointer to the caller's return value and set
  // rdi to the newly allocated space. Allocating space before saving the old
  // pointer makes it easier to undo these operations after the call. The System
  // V ABI only specifies that the memory for the return value must not overlap
  // any other memory available to the target so the order of these two stack
  // items doesn't matter.
  if (stack_return_size > 0) {
    add_comment_line(
        aw, "Allocate space on the compartment's stack for the return value");
    size_t padded_return_size = stack_return_size + stack_return_padding;
    add_asm_line(aw, "subq $"s + std::to_string(padded_return_size) + ", %rsp");
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

  // Copy stack args to target stack
  if (stack_arg_count > 0) {
    // Set rax to the caller's stack so we can copy the stack args to the
    // compartment's stack.
    add_comment_line(
        aw, "Copy stack arguments from the caller's stack to the compartment");
    add_asm_line(aw, "movq ia2_stackptrs@GOTPCREL(%rip), %rax");
    add_asm_line(
        aw, llvm::formatv("movq \" STACK({0}) \"(%rax), %rax", caller_pkey));
    // This is effectively a memcpy of size `stack_arg_size` from the caller's
    // stack to the compartment's
    for (int i = 0; i < stack_arg_size; i += 8) {
      // We must take the preserved registers we pushed on the caller's stack
      // into account when determining the location of the stack args
      size_t caller_stack_size =
          stack_arg_size + (preserved_registers.size() * 8);
      // The index into the caller's stack is backwards since pushq will copy to
      // the compartment's stack from the highest addresses to the lowest.
      add_asm_line(aw,
                   "pushq " + std::to_string(caller_stack_size - i) + "(%rax)");
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
  add_asm_line(aw, "call __libia2_scrub_registers");

  // Restore used arg regs after zeroing registers
  if (reg_arg_count > 0) {
    add_comment_line(aw, "Restore registers containing arguments");
    for (auto loc = param_locs.rbegin(); loc != param_locs.rend(); loc++) {
      if (!loc->is_stack()) {
        emit_reg_pop(aw, *loc);
      }
    }
  }

  if (kind == WrapperKind::Indirect) {
    add_asm_line(
        aw,
        "movq \" XSTR(PASTE4(__ia2_, ty, _target_ptr_line_, __LINE__)) \"@GOTPCREL(%rip), %r12");
    add_asm_line(aw, "movq (%r12), %r12");
  }
  // Change pkru to the compartment's value
  add_comment_line(aw, "Set PKRU to the compartment's value");
  // wrpkru requires zeroing rcx and rdx, but they may have arguments so use r10
  // and r11 as scratch registers
  add_asm_line(aw, "movq %rcx, %r10");
  add_asm_line(aw, "movq %rdx, %r11");
  emit_wrpkru(aw, target_pkey);
  add_asm_line(aw, "movq %r10, %rcx");
  add_asm_line(aw, "movq %r11, %rdx");

  // Call wrapped function
  add_comment_line(aw, "Call wrapped function");
  if (kind == WrapperKind::Indirect) {
    add_asm_line(aw, "call *%r12");
  } else if (as_macro) {
    add_raw_line(aw, "\"call \" #target \"\\n\"");
  } else {
    add_asm_line(aw, "call "s + name);
  }

  // After calling the wrapped function, rax and rdx may contain a return value
  // so use r10 and r11 as scratch registers
  add_comment_line(aw,
                   "Set PKRU to the intermediate value to move return value");
  add_asm_line(aw, "movq %rax, %r10");
  add_asm_line(aw, "movq %rdx, %r11");
  // Change pkru to the intermediate value. This uses rax, r10 and r11 as
  // scratch registers.
  emit_mixed_wrpkru(aw, caller_pkey, target_pkey);
  add_asm_line(aw, "movq %r10, %rax");
  add_asm_line(aw, "movq %r11, %rdx");

  // Free stack space used for stack args on the target stack
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

    if (stack_return_padding > 0) {
      add_asm_line(aw, "addq $8, %rsp");
    }
  }

  // Switch back to the caller's stack
  add_asm_line(aw, "movq ia2_stackptrs@GOTPCREL(%rip), %r12");
  add_asm_line(aw,
               llvm::formatv("leaq \" STACK({0}) \"(%r12), %r12", target_pkey));
  add_asm_line(aw, "movq %rsp, (%r12)");

  add_comment_line(aw, "Switch back to the caller's stack");
  add_asm_line(aw, "movq ia2_stackptrs@GOTPCREL(%rip), %rsp");
  add_asm_line(aw,
               llvm::formatv("movq \" STACK({0}) \"(%rsp), %rsp", caller_pkey));

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
  add_asm_line(aw, "call __libia2_scrub_registers");

  // pop return regs
  add_comment_line(aw, "Pop return regs");
  for (auto loc = return_locs.rbegin(); loc != return_locs.rend(); loc++) {
    if (!loc->is_stack()) {
      emit_reg_pop(aw, *loc);
    }
  }
  // Once again use r10 and r11 as scratch registers
  add_comment_line(aw, "Set PKRU to the caller's value");
  add_asm_line(aw, "movq %rax, %r10");
  add_asm_line(aw, "movq %rdx, %r11");
  emit_wrpkru(aw, caller_pkey);
  add_asm_line(aw, "movq %r10, %rax");
  add_asm_line(aw, "movq %r11, %rdx");

  // Load registers that are preserved across function calls after switching
  // back to the caller's compartment's stack. This is on the caller's stack so
  // it's not in the diagram above.
  for (auto r = preserved_registers.rbegin(); r != preserved_registers.rend();
       r++) {
    add_asm_line(aw, "popq %"s + *r);
  }

  // Return to the caller
  add_comment_line(aw, "Return to the caller");
  add_asm_line(aw, "ret");

  // Jump to the previous location counter to undo the effect of `.text 1`
  // for indirect wrappers or `.text` for the direct case
  add_asm_line(aw, ".previous");

  return aw.ss.str();
}
