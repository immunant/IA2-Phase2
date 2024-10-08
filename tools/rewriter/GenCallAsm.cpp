#include "llvm/Support/FormatVariadic.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <optional>
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
                                                         "rcx", "r8", "r9"};
const std::array<const char *, 8> xmms = {"xmm0", "xmm1", "xmm2", "xmm3",
                                          "xmm4", "xmm5", "xmm6", "xmm7"};

const std::array<const char *, 2> int_ret_reg_order = {"rax", "rdx"};

const std::array<const char *, 3> cabi_arg_kind_names = {"int", "float", "mem"};

// rsp and rbp are also preserved registers, but we handle them separately from
// these since they're the stack and frame pointers
const std::array<const char *, 5> preserved_registers = {"rbx", "r12", "r13",
                                                         "r14", "r15"};

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
    // TODO ARM stack switching
    llvm::errs() << "TODO stack switching not implemented on ARM\n";
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
      llvm::errs() << "TODO indirect calls not implemented on ARM\n";
    }
  } else {
    // direct call
    if (arch == Arch::X86) {
      add_asm_line(aw, "call "s + target_name.value());
    } else if (arch == Arch::Aarch64) {
      add_asm_line(aw, "b "s + target_name.value());
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
    for (auto &r : preserved_registers) {
      add_asm_line(aw, "pushq %"s + r);
    }
  } else if (arch == Arch::Aarch64) {
    // TODO ARM stack switching
    llvm::errs() << "TODO register saving not implemented on ARM\n";
  }
}

static void emit_intermediate_pkru(AsmWriter &aw, uint32_t caller_pkey, uint32_t target_pkey, const char *reg1, const char *reg2, Arch arch) {
  if (arch == Arch::X86) {
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
  } else if (arch == Arch::Aarch64) {
    // TODO ARM stack switching
    llvm::errs() << "TODO intermediate PKRU not implemented on ARM\n";
  }
}

static void emit_copy_args(AsmWriter &aw, size_t stack_return_size, size_t stack_return_padding, int stack_alignment, int stack_arg_count, size_t stack_arg_size, uint32_t caller_pkey, Arch arch) {
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
    if (stack_arg_count > 0) {
      // Set rax to the caller's stack so we can copy the stack args to the
      // compartment's stack.
      add_comment_line(
          aw, "Copy stack arguments from the caller's stack to the compartment");
      // Load addr of top of caller compartment's stack into rax, clobbering r12
      std::string expr = emit_load_sp_offset(aw, caller_pkey, "r12"s);
      add_asm_line(aw, "movq %" + expr + ", %rax");
      // This is effectively a memcpy of size `stack_arg_size` from the caller's
      // stack to the compartment's
      for (int i = 0; i < stack_arg_size; i += 8) {
        // We must take the preserved registers we pushed on the caller's stack
        // into account (including rbp) when determining the location of the stack
        // args
        size_t caller_stack_size =
            stack_arg_size + ((preserved_registers.size() + 1) * 8);
        // The index into the caller's stack is backwards since pushq will copy to
        // the compartment's stack from the highest addresses to the lowest.
        add_asm_line(aw,
                     "pushq " + std::to_string(caller_stack_size - i) + "(%rax)");
      }
    }
  } else if (arch == Arch::Aarch64) {
    // TODO ARM arguments
    llvm::errs() << "TODO arguments not implemented on ARM\n";
  }
}

static void emit_load_fn_ptr(AsmWriter &aw, Arch arch) {
  if (arch == Arch::X86) {
    add_asm_line(aw, "movq ia2_fn_ptr@GOTPCREL(%rip), %r12");
    add_asm_line(aw, "movq (%r12), %r12");
  } else if (arch == Arch::Aarch64) {
    // TODO ARM function pointer save
    llvm::errs() << "TODO indirect calls not implemented on ARM\n";
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
    llvm::errs() << "TODO x18 switching is not implemented\n";
  }
}

static void emit_free_stack_space(AsmWriter &aw, size_t stack_arg_size, int stack_alignment, Arch arch) {
  if (arch == Arch::X86) {
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
      for (int i = 0; i < stack_return_size; i += 8) {
        add_asm_line(aw, "popq "s + std::to_string(i) + "(%rax)");
      }

      if (stack_return_padding > 0) {
        add_asm_line(aw, "addq $8, %rsp");
      }
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
    // TODO ARM reg zero
    llvm::errs() << "TODO ARM reg scrubbing not implemented\n";
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
    // TODO ARM set PKRU on return
    llvm::errs() << "TODO set return PKRU not implemented on ARM\n";
  }
}

static void emit_epilogue(AsmWriter &aw, uint32_t caller_pkey, Arch arch) {
  if (arch == Arch::X86) {
    // Load registers that are preserved across function calls after switching
    // back to the caller's compartment's stack. This is on the caller's stack so
    // it's not in the diagram above.
    for (auto r = preserved_registers.rbegin(); r != preserved_registers.rend();
         r++) {
      add_asm_line(aw, "popq %"s + *r);
    }
    // Restore the caller's frame pointer
    add_asm_line(aw, "popq %rbp");
  } else if (arch == Arch::Aarch64) {
    // TODO ARM return regs
    llvm::errs() << "TODO restore regs not implemented on ARM\n";
  }
}

static void emit_return(AsmWriter &aw, Arch arch) {
  if (arch == Arch::X86) {
    // Return to the caller
    add_comment_line(aw, "Return to the caller");
    add_asm_line(aw, "ret");
  } else if (arch == Arch::Aarch64) {
    // TODO ARM return to called
    llvm::errs() << "TODO return not implemented on ARM\n";
  }
}

std::string emit_asm_wrapper(const CAbiSignature &sig,
                             const std::string &wrapper_name,
                             const std::optional<std::string> target_name,
                             WrapperKind kind, int caller_pkey, int target_pkey,
                             Arch arch, bool as_macro) {

  // Small sanity check
  assert(caller_pkey != target_pkey);

  AsmWriter aw = get_asmwriter(as_macro);
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

  if (stack_return_size > 0) {
    // If we have a stack return, we also have to save the old ret ptr, which
    // precedes the stack return itself.
    start_of_ret_space += 8;

    // See what unpadded alignment would be for the start of the stack return.
    size_t unaligned = start_of_ret_space % stack_return_align;
    stack_return_padding = unaligned != 0 ? stack_return_align - unaligned : 0;
  }

  // Count room for for the ret align padding, return value, and our ret ptr.
  size_t compartment_stack_space = start_of_ret_space + stack_return_size +
                                   stack_return_padding + stack_arg_size;
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

  emit_intermediate_pkru(aw, caller_pkey, target_pkey, "rcx", "rdx", arch);

  emit_switch_stacks(aw, caller_pkey, target_pkey, arch);

  emit_copy_args(aw, stack_return_size, stack_return_padding, stack_alignment, stack_arg_count, stack_arg_size, caller_pkey, arch);

  emit_scrub_regs(aw, caller_pkey, param_locs, reg_arg_count > 0, arch);

  if (kind == WrapperKind::IndirectCallsite) {
    emit_load_fn_ptr(aw, arch);
  }

  emit_set_pkru(aw, target_pkey, arch);

  emit_fn_call(target_name, kind, aw, arch);

  emit_intermediate_pkru(aw, caller_pkey, target_pkey, "rax", "rdx", arch);

  emit_free_stack_space(aw, stack_arg_size, stack_alignment, arch);

  emit_copy_stack_returns(aw, stack_return_size, stack_return_padding, caller_pkey, target_pkey, arch);

  emit_switch_stacks(aw, target_pkey, caller_pkey, arch);

  emit_scrub_regs(aw, target_pkey, return_locs, true, arch);

  emit_set_return_pkru(aw, caller_pkey, arch);

  emit_epilogue(aw, caller_pkey, arch);

  emit_return(aw, arch);

  // Set the symbol size
  add_asm_line(aw, ".size "s + wrapper_name + ", .-" + wrapper_name);

  // Jump to the previous location counter to undo the effect of `.text 1`
  // for indirect wrappers or `.text` for the direct case
  add_asm_line(aw, ".previous");

  return aw.ss.str();
}
