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

static size_t stack_arg_count(const std::vector<ArgLocation> &args) {
  return std::count_if(args.begin(), args.end(),
                       [](auto &x) { return x.is_stack(); });
}

static size_t reg_arg_count(const std::vector<ArgLocation> &args) {
  return std::count_if(args.begin(), args.end(),
                       [](auto &x) { return !x.is_stack(); });
}

/// Compute abi locations for parameters of a C-abi function, given its sequence
/// of argument kinds.
std::vector<ArgLocation> allocate_param_locations(const AbiSignature &func, Arch arch, size_t *stack_arg_size_out) {
  const auto &int_param_reg_order = (arch == Arch::X86) ? x86_int_param_reg_order : arm_int_param_reg_order;
  const auto &float_reg_order = (arch == Arch::X86) ? x86_float_reg_order : arm_float_reg_order;
  std::vector<ArgLocation> args;
  size_t ints_used = 0;
  size_t memory_return_slots =
      std::count_if(func.ret.begin(), func.ret.end(),
                    [](auto &x) { return x.is_stack() || x.is_indirect(); });
  if (memory_return_slots > 0) {
    if (arch == Arch::X86) {
      // if the return is in memory, the first integer argument is the location to
      // write the return value
      auto ptr_arg = ArgLocation::Register(ArgLocation::Kind::Integral, 8);
      ptr_arg.allocate_reg(int_param_reg_order[ints_used]);
      args.push_back(ptr_arg);
      ints_used += 1;
    } else if (arch == Arch::Aarch64) {
      // memory return region address goes in x8
      auto ptr_arg = ArgLocation::Register(ArgLocation::Kind::Integral, 8);
      ptr_arg.allocate_reg("x8");
      args.push_back(ptr_arg);
    }
  }
  size_t stack_offset = 0;
  size_t floats_used = 0;
  for (auto arg : func.args) {
    if (arch == Arch::X86 && arg.is_indirect()) {
      // "indirect" arguments are just passed on the stack in x86, not
      // indirectly in a register.
      arg = ArgLocation::Stack(arg.size(), arg.align());
    }
    switch (arg.kind()) {
    case ArgLocation::Kind::Integral: {
      if ((arg.size() <= 8 || arg.is_indirect()) && ints_used < int_param_reg_order.size()) {
        arg.allocate_reg(int_param_reg_order[ints_used]);
        args.push_back(arg);
        ints_used += 1;
      } else if (ints_used + 1 < int_param_reg_order.size()) {
        assert(arg.size() == 16);
        assert(!arg.is_indirect());
        for (int i = 0; i < 2; i++) {
          auto half_arg = ArgLocation::Register(ArgLocation::Kind::Integral, 8);
          half_arg.allocate_reg(int_param_reg_order[ints_used]);
          args.push_back(half_arg);
          ints_used += 1;
        }
      } else {
        assert(arch != Arch::X86 || !arg.is_indirect());
        arg.allocate_stack(stack_offset);
        args.push_back(arg);
        stack_offset += 8;
      }
      break;
    }
    case ArgLocation::Kind::Float: {
      if (floats_used < float_reg_order.size()) {
        arg.allocate_reg(float_reg_order[floats_used]);
        args.push_back(arg);
        floats_used += 1;
      } else {
        arg.allocate_stack(stack_offset);
        args.push_back(arg);
        stack_offset += arg.size();
      }
      break;
    }
    case ArgLocation::Kind::Memory: {
      assert(arg.is_stack());
      if (stack_offset % arg.align() != 0) {
        stack_offset += arg.align() - stack_offset % arg.align();
      }
      arg.allocate_stack(stack_offset);
      args.push_back(arg);
      stack_offset += arg.size();
      if (stack_offset % 8 != 0) {
        stack_offset += 8 - stack_offset % 8;
      }
      break;
    }
    }
  }
  *stack_arg_size_out = stack_offset;
  return args;
}

std::vector<ArgLocation> allocate_return_locations(const AbiSignature &func, Arch arch) {
  const auto &int_ret_reg_order = (arch == Arch::X86) ? x86_int_ret_reg_order : arm_int_ret_reg_order;
  const auto &float_reg_order = (arch == Arch::X86) ? x86_float_reg_order : arm_float_reg_order;

  std::vector<ArgLocation> args;
  size_t ints_used = 0;
  size_t floats_used = 0;
  for (auto arg : func.ret) {
    switch (arg.kind()) {
    case ArgLocation::Kind::Integral:
      if (arg.size() <= 8 || arg.is_indirect()) {
        assert(ints_used < int_ret_reg_order.size());
        if (!arg.is_indirect() || arch == Arch::X86) {
          // indirect return values addresses are expected to be in return
          // registers on X86
          arg.allocate_reg(int_ret_reg_order[ints_used]);
          ints_used += 1;
        }
        args.push_back(arg);
        break;
      } else {
        assert(arg.size() == 16);
        assert(!arg.is_indirect());
        assert(ints_used + 1 < int_ret_reg_order.size());
        for (int i = 0; i < 2; i++) {
          auto half_arg = ArgLocation::Register(ArgLocation::Kind::Integral, 8);
          half_arg.allocate_reg(int_ret_reg_order[ints_used]);
          args.push_back(half_arg);
          ints_used += 1;
        }
      }
    case ArgLocation::Kind::Float:
      // TODO: handle x87 in st0 and complex x87 in st0+st1
      arg.allocate_reg(float_reg_order[floats_used]);
      args.push_back(arg);
      floats_used += 1;
      break;
    case ArgLocation::Kind::Memory:
      // Memory return also returns address in first return register on x86.
      // Memory returns on AArch64 are classified as indirect and handled above.
      assert(arch == Arch::X86);
      assert(ints_used == 0);
      arg.allocate_reg(int_ret_reg_order[ints_used]);
      arg.set_indirect_on_stack();
      args.push_back(arg);
      break;
    }
  }
  return args;
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

static void emit_reg_push(AsmWriter &aw, const ArgLocation &loc) {
  assert(!loc.is_stack());
  if (loc.is_128bit_float()) {
    add_asm_line(aw, "subq $16, %rsp");
    add_asm_line(aw, "movdqu %"s + loc.as_str() + ", (%rsp)");
  } else {
    add_asm_line(aw, "pushq %"s + loc.as_str());
  }
}

static void emit_reg_pop(AsmWriter &aw, const ArgLocation &loc) {
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
                        const std::string &src, const std::string &scratch, size_t offset, Arch arch) {
  add_comment_line(aw, "Copy " + std::to_string(byte_count) + " bytes from " + src + " to " + dst);
  if (arch == Arch::X86) {
    int i = 0;
    for (; i + 8 <= byte_count; i += 8) {
      add_asm_line(aw, "movq "s + std::to_string(i + offset) + "(%" + src + "), %" + scratch);
      add_asm_line(aw, "movq %" + scratch + ", "s + std::to_string(i) + "(%" + dst + ")");
    }
    if (i + 4 <= byte_count) {
      add_asm_line(aw, "movl "s + std::to_string(i + offset) + "(%" + src + "), %" + scratch + "d");
      add_asm_line(aw, "movl %" + scratch + "d, "s + std::to_string(i) + "(%" + dst + ")");
      i += 4;
    }
    if (i + 2 <= byte_count) {
      add_asm_line(aw, "movw "s + std::to_string(i + offset) + "(%" + src + "), %" + scratch + "w");
      add_asm_line(aw, "movw %" + scratch + "w, "s + std::to_string(i) + "(%" + dst + ")");
      i += 2;
    }
    if (i < byte_count) {
      add_asm_line(aw, "movb "s + std::to_string(i + offset) + "(%" + src + "), %" + scratch + "b");
      add_asm_line(aw, "movb %" + scratch + "b, "s + std::to_string(i) + "(%" + dst + ")");
    }
  } else if (arch == Arch::Aarch64) {
    auto halfreg = "W"s + scratch.substr(1);
    int i = 0;
    for (; i + 8 <= byte_count; i += 8) {
      add_asm_line(aw, "ldr "s + scratch + ", [" + src + ", #" + std::to_string(i + offset) + "]");
      add_asm_line(aw, "str "s + scratch + ", [" + dst + ", #" + std::to_string(i) + "]");
    }
    if (i + 4 <= byte_count) {
      add_asm_line(aw, "ldr "s + halfreg + ", [" + src + ", #" + std::to_string(i + offset) + "]");
      add_asm_line(aw, "str "s + halfreg + ", [" + dst + ", #" + std::to_string(i) + "]");
      i += 4;
    }
    if (i + 2 <= byte_count) {
      add_asm_line(aw, "ldrh "s + halfreg + ", [" + src + ", #" + std::to_string(i + offset) + "]");
      add_asm_line(aw, "strh "s + halfreg + ", [" + dst + ", #" + std::to_string(i) + "]");
      i += 2;
    }
    if (i < byte_count) {
      add_asm_line(aw, "ldrb "s + halfreg + ", [" + src + ", #" + std::to_string(i + offset) + "]");
      add_asm_line(aw, "strb "s + halfreg + ", [" + dst + ", #" + std::to_string(i) + "]");
    }
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
                             std::vector<ArgLocation> args) {
  bool first = true;
  for (auto arg : args) {
    if (!first) {
      ss << ", ";
    }
    if (arg.is_indirect()) {
      ss << "[mem]";
    } else if (arg.is_stack()) {
      ss << "mem";
    } else {
      ss << cabi_arg_kind_names[(int)arg.kind()];
    }
    first = false;
  }
}

static std::string sig_string(const AbiSignature &sig,
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

static void emit_copy_args(AsmWriter &aw, const std::vector<ArgLocation> &args,
                           const std::optional<std::vector<ArgLocation>> &wrapper_args,
                           size_t stack_return_size, size_t stack_return_padding, int stack_alignment, 
                           size_t stack_arg_size, size_t stack_arg_padding, size_t wrapper_stack_arg_size, 
                           uint32_t caller_pkey, Arch arch) {
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
      offset += wrapper_stack_arg_size - stack_arg_size;
      for (int i = 0; i < stack_arg_size; i += 8) {
        // The index into the caller's stack is backwards since pushq will copy to
        // the compartment's stack from the highest addresses to the lowest.
        add_asm_line(aw,
                     "pushq " + std::to_string(offset - i) + "(%rax)");
      }
    }

    if (wrapper_args) {
      add_comment_line(aw, "Copy arguments into the correct registers");
      auto src_arg = wrapper_args->begin();
      auto dest_arg = args.begin();
      if (stack_return_size > 0) {
        src_arg++;
        dest_arg++;
      }
      
      add_asm_line(aw, "movq %"s + src_arg->as_str() + ", %r12");
      src_arg++;
      for (; dest_arg != args.end(); src_arg++, dest_arg++) {
        if (!dest_arg->is_stack()) {
          if (src_arg->is_stack()) {
            size_t offset = (x86_preserved_registers.size() + 1) * 8 + stack_arg_padding + src_arg->stack_offset() + src_arg->size();
            size_t align = std::max(src_arg->align(), (size_t)8);
            if (offset % align != 0) {
              offset += align - (offset % align);
            }
            add_asm_line(aw, "movq "s + std::to_string(offset) + "(%rax), %" + dest_arg->as_str());
          } else {
            add_asm_line(aw, "movq %"s + src_arg->as_str() + ", %"s + dest_arg->as_str());
          }
        }
      }
    }
  } else if (arch == Arch::Aarch64) {
    size_t indirect_arg_size = 0;
    for (auto &arg : args) {
      if (arg.is_indirect()) {
        size_t align = std::max(arg.align(), (size_t)8);
        if (stack_arg_size % align != 0) {
          indirect_arg_size += align - (stack_arg_size % align);
        }
        indirect_arg_size += arg.size();
      }
    }
    size_t total_stack_size = stack_return_size + stack_return_padding + indirect_arg_size + stack_arg_size + stack_arg_padding;
    if (stack_return_size > 0) {
      // Reserve space to save the original return value pointer. We only want
      // this slot if we are returning a value on the stack.
      total_stack_size += 8;
    }
    if (total_stack_size > 0) {
      add_comment_line(
          aw, "Allocate space on the compartment's stack for stack return and/or args");
      add_asm_line(aw, "sub sp, sp, #" + std::to_string(total_stack_size));
    }
    size_t stack_return_saved_ptr_offset = stack_arg_size + stack_arg_padding;
    size_t stack_return_start = stack_return_saved_ptr_offset + 8;
    size_t indirect_args_start = stack_return_start + stack_return_size + stack_return_padding;

    // Same as for X86 above, we allocate space for the return value and push x8
    // onto the stack so we can point it at the new stack return slot.
    if (stack_return_size > 0) {
      // total_stack_size already includes space to save the old address
      add_comment_line(aw, "Save address of the caller's return value");
      add_asm_line(aw, "str x8, [sp, #" + std::to_string(stack_return_saved_ptr_offset) + "]");
      add_comment_line(aw, "Set x8 to the compartment's return value memory");
      // The new return value is 8 bytes above the bottom of the stack so we need
      // to add 8 to x8
      add_asm_line(aw, "add x8, sp, #" + std::to_string(stack_return_start));
    }

    // Copy stack args to target stack
    if (stack_arg_size > 0) {
      add_comment_line(
          aw, "Copy stack arguments from the caller's stack to the compartment");
      size_t src_offset = ((aarch64_preserved_registers.size() + 2) * 8);
      src_offset += wrapper_stack_arg_size - stack_arg_size;
      emit_memcpy(aw, stack_arg_size, "sp", "x12", "x9", src_offset, arch);
    }

    size_t indirect_arg_current_offset = indirect_args_start;
    for (auto &arg : args) {
      if (arg.is_indirect()) {
        // We can't guarantee more than 16-byte alignment for the stack.
        assert(arg.align() <= 16);
        if (indirect_arg_current_offset % arg.align() != 0) {
          indirect_arg_current_offset += arg.align() - (indirect_arg_current_offset % arg.align());
        }
        if (arg.is_stack()) {
          add_asm_line(aw, "ldr x10, [sp, #" + std::to_string(arg.stack_offset()) + "]");
          add_asm_line(aw, "add x9, sp, #" + std::to_string(indirect_arg_current_offset));
          emit_memcpy(aw, arg.size(), "x9", "x10", "x11", 0, arch);
          add_asm_line(aw, "str x9, [sp, #" + std::to_string(arg.stack_offset()) + "]");
        } else {
          add_asm_line(aw, "mov x10, "s + arg.as_str());
          add_asm_line(aw, "add "s + arg.as_str() + ", sp, #" + std::to_string(indirect_arg_current_offset));
          emit_memcpy(aw, arg.size(), arg.as_str(), "x10", "x11", 0, arch);
        }
        indirect_arg_current_offset += arg.size();
      }
    }

    if (wrapper_args) {
      add_comment_line(aw, "Copy arguments into the correct registers");
      auto src_arg = wrapper_args->begin();
      auto dest_arg = args.begin();
      if (stack_return_size > 0) {
        src_arg++;
        dest_arg++;
      }

      add_asm_line(aw, "mov x9, "s + src_arg->as_str());
      src_arg++;
      for (; dest_arg != args.end(); src_arg++, dest_arg++) {
        if (!dest_arg->is_stack()) {
          if (src_arg->is_stack()) {
            size_t offset = (aarch64_preserved_registers.size() + 2) * 8 + src_arg->stack_offset();
            add_asm_line(aw, "ldr "s + dest_arg->as_str() + ", [x12, #" + std::to_string(offset) + "]");
          } else {
            add_asm_line(aw, "mov "s + dest_arg->as_str() + ", "s + src_arg->as_str());
          }
        }
      }
    }
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
    add_asm_line(aw, "add sp, sp, #" + std::to_string(stack_size));
  }
}

static void emit_copy_stack_returns(AsmWriter &aw, size_t stack_return_size, size_t stack_return_padding, size_t stack_arg_size, size_t stack_arg_padding, uint32_t caller_pkey, uint32_t target_pkey, Arch arch) {
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

      emit_memcpy(aw, stack_return_size, "rax", "rsp", "r10", 0, arch);

      add_asm_line(aw, "addq $"s + std::to_string(stack_return_size + stack_return_padding) + ", %rsp");
    }
  } else if (arch == Arch::Aarch64) {
    if (stack_return_size > 0) {
      add_comment_line(aw, "Copy stack returns to caller's stack");

      size_t stack_return_saved_ptr_offset = stack_arg_size + stack_arg_padding;
      size_t stack_return_start = stack_return_saved_ptr_offset + 8;

      // Pop the original return value address into x8
      add_asm_line(aw, "ldr x8, [sp, #"s + std::to_string(stack_return_saved_ptr_offset) + "]");
      // Copy the value back to the caller's stack
      emit_memcpy(aw, stack_return_size, "x8", "sp", "x9", stack_return_start, arch);
      // Free the space used for the return value (including the return value address slot)
      add_asm_line(aw, "add sp, sp, #" + std::to_string(stack_return_size + 8 + stack_return_padding));
    }
  }
}

static void emit_scrub_regs(AsmWriter &aw, uint32_t pkey, const std::vector<ArgLocation> &locs, bool indirect, Arch arch) {
  bool preserve_regs = reg_arg_count(locs) > 0 || indirect;

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

        if (indirect) {
          // Save the function pointer argument
          add_asm_line(aw, "pushq %r12");
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
        if (indirect) {
          // Restore the function pointer argument
          add_asm_line(aw, "popq %r12");
        }

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
    if (indirect) {
      regs_64bit.push_back("x9");
    }
    for (auto loc : locs) {
      if (!loc.is_stack() && loc.is_allocated()) {
        // loc is not allocated to a register if it is an indirect return on
        // AArch64. x8 does not have to be preserved by the callee, see AArch64
        // Procedure Call Standard 6.9: "(there is no requirement for the callee
        // to preserve the value stored in x8)".
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
        if (reg + 1 == regs_64bit.end()) {
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

std::string emit_asm_wrapper(AbiSignature sig,
                             std::optional<AbiSignature> wrapper_sig,
                             const std::string &wrapper_name,
                             const std::optional<std::string> target_name,
                             WrapperKind kind, int caller_pkey, int target_pkey,
                             Arch arch, bool as_macro) {

  // Small sanity check
  assert(caller_pkey != target_pkey);

  AsmWriter aw = get_asmwriter(as_macro);

  size_t stack_arg_size = 0;
  auto args = allocate_param_locations(sig, arch, &stack_arg_size);
  std::optional<std::vector<ArgLocation>> wrapper_args;
  size_t wrapper_stack_arg_size = stack_arg_size;
  if (wrapper_sig) {
    wrapper_args = {allocate_param_locations(*wrapper_sig, arch, &wrapper_stack_arg_size)};
  }
  size_t unaligned = stack_arg_size % 8;
  size_t stack_arg_padding = unaligned != 0 ? 8 - unaligned : 0;

  llvm::errs() << "Generating wrapper for " << sig_string(sig, target_name) << "\n";
  auto rets = allocate_return_locations(sig, arch);

  // std::stringstream ss;
  // append_arg_kinds(ss, rets);
  // llvm::errs() << "Return kinds: " << ss.str() << "\n";
  size_t stack_return_size = 0;
  for (const auto &x : rets) {
    if (x.is_stack() || x.is_indirect()) {
      stack_return_size += x.size();
    }
  }

  /*
    Just before calling the wrapped function, its compartment's stack may
    contain any of the following.

    For X86-64:
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
    +-----+ <- Base of reserved space
    |stack|Space required for stack arguments. This is initialized from the
    |args |analogous part of the caller's stack. If all arguments are passed in
    |     |registers this space isn't allocated.
    +-----+
    |ret  |Return address for wrapped function. This is implicitly placed on the
    |addr |stack by the call to the wrapped function.
    +-----+

    For AArch64:
    +-----+
    | top |Top of the stack (stack grows down on AArch64). This address is
    |     |aligned to 16 bytes.
    +-----+
    |ind  |Space for arguments passed indirectly (i.e. in memory). These arguments
    |args |will point to this region.
    +-----+
    |     |Space for the compartment's return value if it has class MEMORY. This
    |ret  |space is only allocated if the pointer to the caller's return value
    |space|memory is also placed on the compartment's stack.
    +-----+
    |ret  |A pointer saving the caller's address for return value. Only added if
    |ptr  |using ret space. Saves the previous value of x8.
    +-----+
    |align|8 bytes for alignment if the total size of ind args + ret space 
    |     |  + ret ptr + stack args is not a multiple of 16.
    +-----+
    |stack|Space required for stack arguments. This is initialized from the
    |args |analogous part of the caller's stack. If all arguments are passed in
    |     |registers this space isn't allocated.
    |     |
    +-----+ <- Base of reserved space, Start of callee stack frame, 16-byte aligned
    |     |
    |frame|Previous link register contents.
    |ptr  |
    +-----+
    |ret  |Return address into the wrapper function.
    |addr |
    +-----+

    ind args, ret space, and stack args may each have up to 15 bytes of alignment 
    above the section on the stack so that the start of the first item in each section is
    properly aligned. This is necessary if the size of the section is not a multiple 
    of the alignment of the first item.
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
  size_t stack_alignment = 0;
  if (arch == Arch::X86) {
    // Compute what the stack alignment would be before calling the wrapped
    // function to check if we need to insert 8 bytes for alignment. We add 8
    // bytes to the compartment_stack_space since the frame is initially off by 8
    // bytes.
    stack_alignment = (compartment_stack_space + 8) % 16;
  }

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

  if (arch == Arch::X86) {
    add_raw_line(aw, llvm::formatv("ASSERT_PKRU({0:x8}) \"\\n\"", ~((0b11 << (2 * caller_pkey)) | 0b11)));
  }

  if (arch == Arch::X86) {
    x86_emit_intermediate_pkru(aw, caller_pkey, target_pkey, "rcx", "rdx");
  }

  emit_switch_stacks(aw, caller_pkey, target_pkey, arch);

  emit_copy_args(aw, args, wrapper_args, stack_return_size, stack_return_padding, stack_alignment, stack_arg_size, stack_arg_padding, wrapper_stack_arg_size, caller_pkey, arch);

  emit_scrub_regs(aw, caller_pkey, args, kind == WrapperKind::IndirectCallsite, arch);

  emit_set_pkru(aw, target_pkey, arch);

  emit_fn_call(target_name, kind, aw, arch);

  if (arch == Arch::X86) {
    x86_emit_intermediate_pkru(aw, caller_pkey, target_pkey, "rax", "rdx");
  }

  emit_free_stack_space(aw, stack_arg_size + stack_arg_padding + stack_alignment, arch);

  emit_copy_stack_returns(aw, stack_return_size, stack_return_padding, stack_arg_size, stack_arg_padding, caller_pkey, target_pkey, arch);

  emit_switch_stacks(aw, target_pkey, caller_pkey, arch);

  emit_scrub_regs(aw, target_pkey, rets, false, arch);

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
