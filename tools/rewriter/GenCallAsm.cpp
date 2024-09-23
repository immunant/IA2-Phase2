#include "llvm/Support/FormatVariadic.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <numeric>
#include <optional>
#include <sstream>
#include <vector>

#include "CAbi.h"
#include "GenCallAsm.h"

using namespace std::string_literals;
using std::size_t;

struct ParamLocation {
private:
  ParamLocation(const char *reg, unsigned size, unsigned align) : reg(reg), _size(size), _align(align) {}

public:
  const char *reg;

private:
  const unsigned _size;
  const unsigned _align;

public:
  static ParamLocation Register(const char *regname) {
    return ParamLocation(regname, 8, 0);
  }
  static ParamLocation Stack(unsigned size, unsigned align) { return ParamLocation(nullptr, size, align); }

  bool is_stack() const { return reg == nullptr; }
  bool is_128bit_float() const {
    return reg != nullptr && ((reg[0] == 'x' && reg[1] == 'm' && reg[2] == 'm') || reg[0] == 'q');
  }
  const char *as_str() const {
    if (reg) {
      return reg;
    } else {
      return "<stack>";
    }
  }
  operator const char *() const { return as_str(); }
  size_t size() const { return is_128bit_float() ? 16 : _size; }
  size_t align() const { return _align; }
};

const std::vector<const char *> x86_int_param_reg_order = {"rdi", "rsi", "rdx",
                                                           "rcx", "r8", "r9"};

const std::vector<const char *> arm_int_param_reg_order = {"x0", "x1", "x2", "x3",
                                                           "x4", "x5", "x6", "x7"};

const std::vector<const char *> x86_float_reg_order = {"xmm0", "xmm1", "xmm2", "xmm3",
                                                       "xmm4", "xmm5", "xmm6", "xmm7"};

const std::vector<const char *> arm_float_reg_order = {"q0", "q1", "q2", "q3",
                                                       "q4", "q5", "q6", "q7"};

const std::vector<const char *> x86_int_ret_reg_order = {"rax", "rdx"};
// return and parameter registers are the same on AArch64
const std::vector<const char *> arm_int_ret_reg_order = arm_int_param_reg_order;

const std::array<const char *, 3> cabi_arg_kind_names = {"int", "float", "mem"};

// rsp and rbp are also preserved registers, but we handle them separately from
// these since they're the stack and frame pointers
const std::array<const char *, 5> x86_preserved_registers = {"rbx", "r12", "r13",
                                                             "r14", "r15"};

const std::array<const char *, 10> aarch64_preserved_registers = {"x19", "x20", "x21",
                                                                  "x22", "x23", "x24",
                                                                  "x25", "x26", "x27",
                                                                  "x28"};

/// Compute abi locations for parameters of a C-abi function, given its sequence
/// of argument kinds.
std::vector<ParamLocation> param_locations(const CAbiSignature &func, Arch arch) {
  const auto &int_param_reg_order = (arch == Arch::X86) ? x86_int_param_reg_order : arm_int_param_reg_order;
  const auto &float_reg_order = (arch == Arch::X86) ? x86_float_reg_order : arm_float_reg_order;
  std::vector<ParamLocation> locs = {};
  size_t ints_used = 0;
  // if the return is in memory, the first integer argument is the location to
  // write the return value
  size_t memory_return_slots =
      std::count_if(func.ret.begin(), func.ret.end(),
                    [](auto &x) { return x.kind == CAbiArgKind::Memory; });
  if (memory_return_slots > 0) {
    locs.push_back(ParamLocation::Register(int_param_reg_order[0]));
    ints_used++;
  }
  size_t floats_used = 0;
  for (const auto &arg : func.args) {
    switch (arg.kind) {
    case CAbiArgKind::Integral: {
      if (ints_used < int_param_reg_order.size()) {
        locs.push_back(ParamLocation::Register(int_param_reg_order[ints_used]));
        ints_used += 1;
      } else {
        locs.push_back(ParamLocation::Stack(8, 8));
      }
      break;
    }
    case CAbiArgKind::Float: {
      if (floats_used < float_reg_order.size()) {
        locs.push_back(ParamLocation::Register(float_reg_order[floats_used]));
        floats_used += 1;
      } else {
        locs.push_back(ParamLocation::Stack(8, 8));
      }
      break;
    }
    case CAbiArgKind::Memory: {
      locs.push_back(ParamLocation::Stack(arg.size, arg.align));
      break;
    }
    }
  }
  return locs;
}

std::vector<ParamLocation> return_locations(const CAbiSignature &func, Arch arch) {
  const auto &int_ret_reg_order = (arch == Arch::X86) ? x86_int_ret_reg_order : arm_int_ret_reg_order;
  const auto &float_reg_order = (arch == Arch::X86) ? x86_float_reg_order : arm_float_reg_order;

  std::vector<ParamLocation> locs = {};

  if (func.ret.empty()) {
    return locs;
  }

  size_t ints_used = 0;
  size_t floats_used = 0;
  for (const auto &arg : func.ret) {
    switch (arg.kind) {
    case CAbiArgKind::Integral:
      locs.push_back(ParamLocation::Register(int_ret_reg_order[ints_used]));
      ints_used += 1;
      break;
    case CAbiArgKind::Float:
      // TODO: handle x87 in st0 and complex x87 in st0+st1
      locs.push_back(ParamLocation::Register(float_reg_order[floats_used]));
      floats_used += 1;
      break;
    case CAbiArgKind::Memory:
      // memory return also returns address in first return register
      if (locs.empty()) {
        locs.push_back(ParamLocation::Register(int_ret_reg_order[0]));
      }
      locs.push_back(ParamLocation::Stack(arg.size, arg.align));
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
  if (loc.is_128bit_float()) {
    add_asm_line(aw, "subq $16, %rsp");
    add_asm_line(aw, "movdqu %"s + loc.as_str() + ", (%rsp)");
  } else {
    add_asm_line(aw, "pushq %"s + loc.as_str());
  }
}

static void emit_reg_pop(AsmWriter &aw, const ParamLocation &loc) {
  assert(!loc.is_stack());
  if (loc.is_128bit_float()) {
    add_asm_line(aw, "movdqu (%rsp), %"s + loc.as_str());
    add_asm_line(aw, "addq $16, %rsp");
  } else {
    add_asm_line(aw, "popq %"s + loc.as_str());
  }
}

// Emit code to copy `byte_count` bytes from `src` to `dst` using `scratch` as a
// temporary register.
static void emit_memcpy(AsmWriter &aw, unsigned byte_count, const std::string &dst,
                        const std::string &src, const std::string &scratch) {
  add_comment_line(aw, "Copy " + std::to_string(byte_count) + " bytes from " + src + " to " + dst);
  int i = 0;
  for (; i + 8 <= byte_count; i += 8) {
    add_asm_line(aw, "movq "s + std::to_string(i) + "(%" + src + "), %" + scratch);
    add_asm_line(aw, "movq %" + scratch + ", "s + std::to_string(i) + "(%" + dst + ")");
  }
  if (i + 4 <= byte_count) {
    add_asm_line(aw, "movl "s + std::to_string(i) + "(%" + src + "), %" + scratch + "d");
    add_asm_line(aw, "movl %" + scratch + "d, "s + std::to_string(i) + "(%" + dst + ")");
    i += 4;
  }
  if (i + 2 <= byte_count) {
    add_asm_line(aw, "movw "s + std::to_string(i) + "(%" + src + "), %" + scratch + "w");
    add_asm_line(aw, "movw %" + scratch + "w, "s + std::to_string(i) + "(%" + dst + ")");
    i += 2;
  }
  if (i < byte_count) {
    add_asm_line(aw, "movb "s + std::to_string(i) + "(%" + src + "), %" + scratch + "b");
    add_asm_line(aw, "movb %" + scratch + "b, "s + std::to_string(i) + "(%" + dst + ")");
  }
}

// Emit code to set the PKRU. Clobbers eax, ecx and edx.
// \p pkey is a std::string of an assembly literal without a $ prefix.
static void emit_wrpkru(AsmWriter &aw, int pkey) {
  uint32_t pkru = ~((0b11 << (2 * pkey)) | 0b11);
  // wrpkru requires zeroing ecx and edx
  add_asm_line(aw, "xorl %ecx, %ecx");
  add_asm_line(aw, "xorl %edx, %edx");
  add_asm_line(aw, llvm::formatv("movl ${0:x8}, %eax", pkru));
  add_raw_line(aw, "\"wrpkru\\n\"");
}

// Emit code to set the PKRU. Clobbers eax, ecx and edx.
// \p pkey is a std::string of an assembly literal without a $ prefix.
static void emit_mixed_wrpkru(AsmWriter &aw, int pkey0, int pkey1) {
  uint32_t pkru = ~((0b11 << (2 * pkey0)) | (0b11 << (2 * pkey1)) | 0b11);
  // wrpkru requires zeroing ecx and edx
  add_asm_line(aw, "xorl %ecx, %ecx");
  add_asm_line(aw, "xorl %edx, %edx");
  add_asm_line(aw, llvm::formatv("movl ${0:x8}, %eax", pkru));
  add_raw_line(aw, "\"wrpkru\\n\"");
}

// Emit code to load the address of a compartment's stack from ia2_stackptr_##n.
// \p fs_offset_reg is a register name (sans % prefix) that will be used to
// store the offset of the stack pointer from %fs.
//
// Returns an expression that can be prefixed with % to use the stack pointer as
// an instruction operand:
//
//   movq %{retval}, %rsp
//
// or:
//
//   movq %rsp, %{retval}
static std::string emit_load_sp_offset(AsmWriter &aw, int pkey,
                                       const std::string &fs_offset_reg) {
  add_asm_line(
      aw, llvm::formatv("mov ia2_stackptr_{0}@GOTTPOFF(%rip), %{1}",
                        pkey, fs_offset_reg));
  return "fs:(%"s + fs_offset_reg + ")"s;
}

// Emit code to switch stacks, using ia2_stackptr_##{old,new}_pkey as storage
// for the old and new stack pointer.
// \p compartment_offset_reg is a register name (sans % prefix) that will be
// clobbered.
//
// On AArch64, this function leaves the old stack pointer in x12.
static void emit_switch_stacks(AsmWriter &aw, int old_pkey, int new_pkey, Arch arch) {
  if (arch == Arch::X86) {
    const std::string compartment_offset_reg = "r11"s;

    add_comment_line(
        aw,
        llvm::formatv("Compute location to save old stack pointer (using {0})",
                      compartment_offset_reg));
    std::string expr = emit_load_sp_offset(aw, old_pkey, compartment_offset_reg);
    add_comment_line(aw, "Write the old stack pointer to memory");
    add_asm_line(aw, llvm::formatv("movq %rsp, %{0}", expr));

    // After saving the old sp, we switch stacks by loading the new one
    add_comment_line(
        aw,
        llvm::formatv("Compute location to load new stack pointer (using {0})",
                      compartment_offset_reg));
    expr = emit_load_sp_offset(aw, new_pkey, compartment_offset_reg);
    add_comment_line(aw, "Read the new stack pointer from memory");
    add_asm_line(aw, llvm::formatv("movq %{0}, %rsp", expr));
  } else if (arch == Arch::Aarch64) {
    add_comment_line(aw, "Compute location to save old stack pointer in x10");
    add_asm_line(aw, "mrs x9, tpidr_el0");
    add_asm_line(aw, "adrp x10, :gottprel:ia2_stackptr_"s + std::to_string(old_pkey));
    add_asm_line(aw, "ldr x10, [x10, #:gottprel_lo12:ia2_stackptr_"s + std::to_string(old_pkey) + "]");
    add_asm_line(aw, "add x10, x10, x9");
    add_comment_line(aw, "Write old stack pointer to memory");
    // Keep the old stack pointer in x12
    add_asm_line(aw, "mov x12, sp");
    add_asm_line(aw, "str x12, [x10]");

    add_comment_line(aw, "Compute location to load new stack pointer in x10");
    add_asm_line(aw, "adrp x10, :gottprel:ia2_stackptr_"s + std::to_string(new_pkey));
    add_asm_line(aw, "ldr x10, [x10, #:gottprel_lo12:ia2_stackptr_"s + std::to_string(new_pkey) + "]");
    add_asm_line(aw, "add x10, x10, x9");
    add_comment_line(aw, "Read new stack pointer from memory");
    add_asm_line(aw, "ldr x11, [x10]");
    add_asm_line(aw, "mov sp, x11");
  }
}

static void append_arg_kinds(std::stringstream &ss,
                             std::vector<CAbiArgLocation> args) {
  bool first = true;
  for (auto arg : args) {
    if (!first) {
      ss << ", ";
    }
    ss << cabi_arg_kind_names[(int)arg.kind];
    first = false;
  }
}

static std::string sig_string(const CAbiSignature &sig,
                              const std::optional<std::string> name) {
  std::stringstream ss = {};
  if (!name) {
    ss << "indirect call"
       << "(";
  } else {
    ss << name.value() << "(";
  }
  append_arg_kinds(ss, sig.args);
  ss << ")";
  if (!sig.ret.empty()) {
    ss << " -> ";
    append_arg_kinds(ss, sig.ret);
  }
  return ss.str();
}

static void emit_fn_call(
    const std::optional<std::string> target_name,
    WrapperKind kind,
    AsmWriter &aw,
    Arch arch) {

  add_comment_line(aw, "Call wrapped function");

  if (!target_name) {
    // indirect call
    assert(kind == WrapperKind::IndirectCallsite);
    if (arch == Arch::X86) {
      add_asm_line(aw, "call *%r12");
    } else if (arch == Arch::Aarch64) {
      add_asm_line(aw, "blr x9");
    }
  } else {
    // direct call
    if (arch == Arch::X86) {
      add_asm_line(aw, "call "s + target_name.value());
    } else if (arch == Arch::Aarch64) {
      add_asm_line(aw, "bl "s + target_name.value());
    }
  }
}

static AsmWriter get_asmwriter(bool as_macro) {
  std::string terminator = {};
  // Code generated as a macro needs to terminate each line with '\'
  if (as_macro) {
    terminator = "\\"s;
  }
  return {.ss = {}, .terminator = terminator};
}

static void emit_prologue(AsmWriter &aw, uint32_t caller_pkey, uint32_t target_pkey, Arch arch) {
  if (arch == Arch::X86) {
    // Save the old frame pointer and set the frame pointer for the call gate
    add_asm_line(aw, "pushq %rbp");
    add_asm_line(aw, "movq %rsp, %rbp");
    // Save registers that are preserved across function calls before switching to
    // the other compartment's stack. This is on the caller's stack so it's not in
    // the diagram above.
    for (auto &r : x86_preserved_registers) {
      add_asm_line(aw, "pushq %"s + r);
    }
  } else if (arch == Arch::Aarch64) {
    // Frame pointer and link register need to be saved first, to make backtraces work
    add_asm_line(aw, "stp x29, x30, [sp, #-16]!");
    // Set the new frame pointer
    add_asm_line(aw, "mov x29, sp");

    // Calculate total space needed for the callee-saved registers
    int total_space_needed = aarch64_preserved_registers.size() * 8; // Each register requires 8 bytes

    // The stack must be 16-byte aligned. It will be as long as we're saving an
    // even number of registers.
    assert(total_space_needed % 16 == 0);

    // Allocate space on the stack at once
    add_asm_line(aw, "sub sp, sp, #" + std::to_string(total_space_needed));

    // Save callee-saved registers
    for (size_t i = 0; i < aarch64_preserved_registers.size(); i++) {
      // TODO(performance): We could store by pairs (STP)
      add_asm_line(aw, "str "s + aarch64_preserved_registers[i] + ", [sp, #" + std::to_string(i * 8) + "]");
    }
  }
}

static void x86_emit_intermediate_pkru(AsmWriter &aw, uint32_t caller_pkey, uint32_t target_pkey, const char *reg1, const char *reg2) {
  // Change pkru to the intermediate value before copying args
  add_comment_line(aw, "Set PKRU to the intermediate value to move arguments");
  // wrpkru requires zeroing rcx and rdx, but they may have arguments so use r10
  // and r11 as scratch registers.
  // When we're returning from the callee, rax may have a return value so we save that instead of rcx.
  auto saveline1 = "movq %"s + reg1 + ", %r10";
  auto saveline2 = "movq %"s + reg2 + ", %r11";
  add_asm_line(aw, saveline1);
  add_asm_line(aw, saveline2);
  emit_mixed_wrpkru(aw, caller_pkey, target_pkey);
  auto restoreline1 = "movq %r10, %"s + reg1;
  auto restoreline2 = "movq %r11, %"s + reg2;
  add_asm_line(aw, restoreline1);
  add_asm_line(aw, restoreline2);
}

static void emit_copy_args(AsmWriter &aw, size_t stack_return_size, size_t stack_return_padding, int stack_alignment, size_t stack_arg_size, size_t stack_arg_padding, uint32_t caller_pkey, Arch arch) {
  if (arch == Arch::X86) {
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
    if (stack_arg_size > 0) {
      // Set rax to the caller's stack so we can copy the stack args to the
      // compartment's stack.
      add_comment_line(
          aw, "Copy stack arguments from the caller's stack to the compartment");
      // Load addr of top of caller compartment's stack into rax, clobbering r12
      std::string expr = emit_load_sp_offset(aw, caller_pkey, "r12"s);
      add_asm_line(aw, "movq %" + expr + ", %rax");
      // We must take the preserved registers we pushed on the caller's stack
      // into account (including rbp) when determining the location of the stack
      // args
      size_t offset =
          stack_arg_size + stack_arg_padding + ((x86_preserved_registers.size() + 1) * 8);
      for (int i = 0; i < stack_arg_size; i += 8) {
        // The index into the caller's stack is backwards since pushq will copy to
        // the compartment's stack from the highest addresses to the lowest.
        add_asm_line(aw,
                     "pushq " + std::to_string(offset - i) + "(%rax)");
      }
    }
  } else if (arch == Arch::Aarch64) {
    // Same as for X86 above, we allocate space for the return value and push x8
    // onto the stack so we can point it at the new stack return slot.
    if (stack_return_size > 0) {
      add_comment_line(
          aw, "Allocate space on the compartment's stack for the return value (including the slot for the return value pointer)");
      size_t padded_return_size = stack_return_size + stack_return_padding + 8;
      add_asm_line(aw, "sub sp, sp, #" + std::to_string(padded_return_size));
      add_comment_line(aw, "Save address of the caller's return value");
      add_asm_line(aw, "str x8, [sp, #0]");
      add_comment_line(aw, "Set x8 to the compartment's return value memory");
      // The new return value is 8 bytes above the bottom of the stack so we need
      // to add 8 to x8
      add_asm_line(aw, "add x8, sp, #8");
    }

    // Copy stack args to target stack
    if (stack_arg_size > 0) {
      add_comment_line(
          aw, "Copy stack arguments from the caller's stack to the compartment");
      // Load addr of top of caller compartment's stack into x10, clobbering x9
      size_t caller_stack_size =
          stack_arg_size + ((aarch64_preserved_registers.size() + 1) * 8);
      // TODO(security): We copy an extra word here if we are passing an odd
      // number of words, leaking that stack value.
      for (int i = 0; i < stack_arg_size; i += 16) {
        add_asm_line(aw, "ldp x9, x10, [x12, #" + std::to_string(caller_stack_size - i) + "]");
        add_asm_line(aw, "stp x9, x10, [sp, #-16]!");
      }
      add_asm_line(aw, "mov x0, sp");
    }
  }
}

static void emit_load_fn_ptr(AsmWriter &aw, Arch arch) {
  if (arch == Arch::X86) {
    add_asm_line(aw, "movq ia2_fn_ptr@GOTPCREL(%rip), %r12");
    add_asm_line(aw, "movq (%r12), %r12");
  } else if (arch == Arch::Aarch64) {
    add_asm_line(aw, "adrp x9, ia2_fn_ptr");
    add_asm_line(aw, "add x9, x9, #:lo12:ia2_fn_ptr");
    add_asm_line(aw, "ldr x9, [x9]");
  }
}

static void emit_set_pkru(AsmWriter &aw, uint32_t target_pkey, Arch arch) {
  if (arch == Arch::X86) {
    // Change pkru to the compartment's value
    add_comment_line(aw, "Set PKRU to the compartment's value");
    // wrpkru requires zeroing rcx and rdx, but they may have arguments so use r10
    // and r11 as scratch registers
    add_asm_line(aw, "movq %rcx, %r10");
    add_asm_line(aw, "movq %rdx, %r11");
    emit_wrpkru(aw, target_pkey);
    add_asm_line(aw, "movq %r10, %rcx");
    add_asm_line(aw, "movq %r11, %rdx");
  } else if (arch == Arch::Aarch64) {
    // set X18 to the pointer key (compartment number left-shifted 56 bits)
    assert(target_pkey < 16);
    add_asm_line(aw, llvm::formatv("movz x18, #{0:x4}, LSL #48", target_pkey << 8));
  }
}

static void emit_free_stack_space(AsmWriter &aw, size_t stack_size, Arch arch) {
  if (stack_size == 0) {
    return;
  }

  add_comment_line(aw, "Free stack space used for stack args");
  if (arch == Arch::X86) {
    // Free stack space used for stack args on the target stack
    add_asm_line(aw, "addq $"s + std::to_string(stack_size) + ", %rsp");
  } else if (arch == Arch::Aarch64) {
    // TODO ARM free stack space
    llvm::errs() << "TODO stack space freeing not implemented on ARM\n";
  }
}

static void emit_copy_stack_returns(AsmWriter &aw, size_t stack_return_size, size_t stack_return_padding, uint32_t caller_pkey, uint32_t target_pkey, Arch arch) {
  if (arch == Arch::X86) {
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

      emit_memcpy(aw, stack_return_size, "rax", "rsp", "r10");

      add_asm_line(aw, "addq $"s + std::to_string(stack_return_size + stack_return_padding) + ", %rsp");
    }
  } else if (arch == Arch::Aarch64) {
    // TODO ARM stack switch back
    llvm::errs() << "TODO stack switch-back not implemented on ARM\n";
  }
}

static void emit_scrub_regs(AsmWriter &aw, uint32_t pkey, const std::vector<ParamLocation> &locs, bool preserve_regs, Arch arch) {
  // Handles register scrubbing for calls into/out of protected compartments.
  // After call: Preserves return values by pushing them to the stack before scrubbing.
  // Before call: Saves argument registers in a similar manner.
  if (arch == Arch::X86) {
    if (pkey != 0) {
      // Zero out all unused registers. First we save all registers containing args.
      // These pushes have matching pops before calling the wrapped function so this
      // stack space is not shown in the diagram above.
      if (preserve_regs) {
        add_comment_line(aw, "Preserve essential regs on stack");

        for (auto loc : locs) {
          if (!loc.is_stack()) {
            emit_reg_push(aw, loc);
          }
        }
      }

      // Scrub all registers except rsp.
      // FIXME: If this will use the System V ABI make sure that the %rsp is aligned
      // before this call
      add_comment_line(aw, "Scrub non-essential regs");
      add_asm_line(aw, "call __libia2_scrub_registers");

      if (preserve_regs) {
        // Restore saved regs
        add_comment_line(aw, "Restore preserved regs");
        for (auto loc = locs.rbegin(); loc != locs.rend(); loc++) {
          if (!loc->is_stack()) {
            emit_reg_pop(aw, *loc);
          }
        }
      }
    }
  } else if (arch == Arch::Aarch64) {
    auto regs_64bit = std::vector<std::string>();
    auto regs_128bit = std::vector<std::string>();
    for (auto loc : locs) {
      if (!loc.is_stack()) {
        if (loc.is_128bit_float()) {
          regs_128bit.push_back(loc.as_str());
        } else {
          regs_64bit.push_back(loc.as_str());
        }
      }
    }

    if (preserve_regs && (!regs_128bit.empty() || !regs_64bit.empty())) {
      // Save all registers containing args.
      add_comment_line(aw, "Preserve essential regs on stack");
      auto reg = regs_64bit.begin();
      for (; reg != regs_64bit.end(); reg++) {
        auto reg1 = *reg;
        if (reg+1 == regs_64bit.end()) {
          break;
        }
        reg++;
        auto reg2 = *reg;
        add_asm_line(aw, "stp "s + reg1 + ", " + reg2 + ", [sp, #-16]!");
      }
      if (reg != regs_64bit.end()) {
        add_asm_line(aw, "str "s + *reg + ", [sp, #-16]!");
      }

      for (auto reg : regs_128bit) {
        add_asm_line(aw, "str "s + reg + ", [sp, #-16]!");
      }
    }

    add_comment_line(aw, "Scrub non-essential regs");
    add_asm_line(aw, "bl __libia2_scrub_registers");

    if (preserve_regs && (!regs_128bit.empty() || !regs_64bit.empty())) {
      add_comment_line(aw, "Restore preserved regs");
      for (auto reg = regs_128bit.rbegin(); reg != regs_128bit.rend(); reg++) {
        add_asm_line(aw, "ldr "s + *reg + ", [sp], #16");
      }

      auto reg = regs_64bit.rbegin();
      if (regs_64bit.size() % 2 == 1) {
        add_asm_line(aw, "ldr "s + *reg + ", [sp]");
        add_asm_line(aw, "add sp, sp, #16");
        reg++;
      }
      for (; reg != regs_64bit.rend(); reg++) {
        auto reg1 = *reg;
        reg++;
        assert(reg != regs_64bit.rend());
        auto reg2 = *reg;
        add_asm_line(aw, "ldp "s + reg2 + ", " + reg1 + ", [sp], #16");
      }
    }
  }
}

static void emit_set_return_pkru(AsmWriter &aw, uint32_t caller_pkey, Arch arch) {
  if (arch == Arch::X86) {
    // Once again use r10 and r11 as scratch registers
    add_comment_line(aw, "Set PKRU to the caller's value");
    add_asm_line(aw, "movq %rax, %r10");
    add_asm_line(aw, "movq %rdx, %r11");
    emit_wrpkru(aw, caller_pkey);
    add_asm_line(aw, "movq %r10, %rax");
    add_asm_line(aw, "movq %r11, %rdx");
  } else if (arch == Arch::Aarch64) {
    // set X18 to the pointer key (compartment number left-shifted 56 bits)
    assert(caller_pkey < 16);
    add_asm_line(aw, llvm::formatv("movz x18, #{0:x4}, LSL #48", caller_pkey << 8));
  }
}

static void emit_epilogue(AsmWriter &aw, uint32_t caller_pkey, Arch arch) {
  if (arch == Arch::X86) {
    // Load registers that are preserved across function calls after switching
    // back to the caller's compartment's stack. This is on the caller's stack so
    // it's not in the diagram above.
    for (auto r = x86_preserved_registers.rbegin(); r != x86_preserved_registers.rend();
         r++) {
      add_asm_line(aw, "popq %"s + *r);
    }
    // Restore the caller's frame pointer
    add_asm_line(aw, "popq %rbp");
  } else if (arch == Arch::Aarch64) {
    // Restore callee-saved registers
    for (int i = 0; i < aarch64_preserved_registers.size(); ++i) {
      add_asm_line(aw,
                   "ldr "s +
                       aarch64_preserved_registers[i] +
                       ", [sp, #" +
                       std::to_string(i * 8) +
                       "]");
    }
    // Adjust the stack pointer
    add_asm_line(aw, "add sp, sp, #" + std::to_string(aarch64_preserved_registers.size() * 8));
    // Restore frame pointer and link register
    add_asm_line(aw, "ldp x29, x30, [sp], #16");
  }
}

static void emit_return(AsmWriter &aw) {
  add_comment_line(aw, "Return to the caller");
  add_asm_line(aw, "ret");
}

std::string emit_asm_wrapper(const CAbiSignature &sig,
                             const std::string &wrapper_name,
                             const std::optional<std::string> target_name,
                             WrapperKind kind, int caller_pkey, int target_pkey,
                             Arch arch, bool as_macro) {

  // Small sanity check
  assert(caller_pkey != target_pkey);

  AsmWriter aw = get_asmwriter(as_macro);
  auto param_locs = param_locations(sig, arch);
  size_t stack_arg_count = std::count_if(param_locs.begin(), param_locs.end(),
                                         [](auto &x) { return x.is_stack(); });
  size_t stack_arg_size = 0;
  for (auto &x : param_locs) {
    if (x.is_stack()) {
      // All stack arguments must be 8-byte aligned
      size_t align = std::max(x.align(), (size_t)8);
      if (stack_arg_size % align != 0) {
        stack_arg_size += align - (stack_arg_size % align);
      }
      stack_arg_size += x.size();
    }
  }
  size_t unaligned = stack_arg_size % 8;
  size_t stack_arg_padding = unaligned != 0 ? 8 - unaligned : 0;
  size_t reg_arg_count = param_locs.size() - stack_arg_count;

  auto return_locs = return_locations(sig, arch);
  size_t stack_return_size = 0;
  for (auto &x : return_locs) {
    if (x.is_stack()) {
      stack_return_size += x.size();
    }
  }

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

  if (stack_return_size > 0) {
    // If we have a stack return, we also have to save the old ret ptr, which
    // precedes the stack return itself.
    start_of_ret_space += 8;

    // Align the size of the return space to `stack_return_align`.
    size_t unaligned = (start_of_ret_space + stack_return_size) % stack_return_align;
    stack_return_padding = unaligned != 0 ? stack_return_align - unaligned : 0;
  }

  // Count room for for the ret align padding, return value, and our ret ptr.
  size_t compartment_stack_space = start_of_ret_space + stack_return_size +
                                   stack_return_padding + stack_arg_size + stack_arg_padding;
  // Compute what the stack alignment would be before calling the wrapped
  // function to check if we need to insert 8 bytes for alignment. We add 8
  // bytes to the compartment_stack_space since the frame is initially off by 8
  // bytes.
  size_t stack_alignment = (compartment_stack_space + 8) % 16;

  add_comment_line(aw, "Wrapper for "s + sig_string(sig, target_name) + ":");
  add_asm_line(aw, ".text");
  if (!as_macro) {
    add_asm_line(aw, ".global "s + wrapper_name);
  } else {
    add_asm_line(aw, ".local "s + wrapper_name);
  }
  // Set the symbol type
  add_asm_line(aw, ".type "s + wrapper_name + ", @function");
  add_asm_line(aw, wrapper_name + ":");

  emit_prologue(aw, caller_pkey, target_pkey, arch);

  add_raw_line(aw, llvm::formatv("ASSERT_PKRU({0:x8}) \"\\n\"", ~((0b11 << (2 * caller_pkey)) | 0b11)));

  if (arch == Arch::X86) {
    x86_emit_intermediate_pkru(aw, caller_pkey, target_pkey, "rcx", "rdx");
  }

  emit_switch_stacks(aw, caller_pkey, target_pkey, arch);

  emit_copy_args(aw, stack_return_size, stack_return_padding, stack_alignment, stack_arg_size, stack_arg_padding, caller_pkey, arch);

  emit_scrub_regs(aw, caller_pkey, param_locs, reg_arg_count > 0, arch);

  if (kind == WrapperKind::IndirectCallsite) {
    emit_load_fn_ptr(aw, arch);
  }

  emit_set_pkru(aw, target_pkey, arch);

  emit_fn_call(target_name, kind, aw, arch);

  if (arch == Arch::X86) {
    x86_emit_intermediate_pkru(aw, caller_pkey, target_pkey, "rax", "rdx");
  }

  emit_free_stack_space(aw, stack_arg_size + stack_arg_padding + stack_alignment, arch);

  emit_copy_stack_returns(aw, stack_return_size, stack_return_padding, caller_pkey, target_pkey, arch);

  emit_switch_stacks(aw, target_pkey, caller_pkey, arch);

  emit_scrub_regs(aw, target_pkey, return_locs, true, arch);

  emit_set_return_pkru(aw, caller_pkey, arch);

  emit_epilogue(aw, caller_pkey, arch);

  emit_return(aw);

  // Set the symbol size
  add_asm_line(aw, ".size "s + wrapper_name + ", .-" + wrapper_name);

  // Jump to the previous location counter to undo the effect of `.text 1`
  // for indirect wrappers or `.text` for the direct case
  add_asm_line(aw, ".previous");

  return aw.ss.str();
}
