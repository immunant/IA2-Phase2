#include "CAbi.h"
#include "GenCallAsm.h"
#include "clang/AST/AST.h"
#include "clang/AST/RecordLayout.h"
#include "clang/Basic/CodeGenOptions.h"
#if LLVM_VERSION_MAJOR >= 15
#include "clang/Basic/FileManager.h"
#include "clang/Basic/FileSystemOptions.h"
#endif
#include "clang/CodeGen/CGFunctionInfo.h"
#include "clang/CodeGen/CodeGenABITypes.h"
#include "clang/CodeGen/ModuleBuilder.h"
#include "clang/Lex/HeaderSearchOptions.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "llvm/IR/LLVMContext.h"
#include <optional>

// Merge the classifications of an aggregate type.
// If any classifiction is memory, the whole type must go to memory.
// Otherwise, preserve the classifications (registers) as they are.
static std::vector<CAbiArgKind> mergeKinds(const std::vector<CAbiArgKind> &kinds) {
  if (std::find(kinds.begin(), kinds.end(), CAbiArgKind::Memory) != kinds.end()) {
    return std::vector<CAbiArgKind>(kinds.size(), CAbiArgKind::Memory);
  } else {
    return kinds;
  }
}

// Detect a homogenous floating-point aggregate.
// ABI: "A Homogeneous Floating-point Aggregate (HFA) is a Homogeneous Aggregate
// with a Fundamental Data Type that is a Floating-Point type and at most four
// uniquely addressable members."
static bool isHFA(const clang::RecordDecl *decl, const clang::ASTContext &astContext) {
  if (decl->field_empty() || decl->isUnion())
    return false;

  unsigned int num_fields = 0;
  for (auto field : decl->fields()) {
    const clang::Type *field_type = field->getType().getTypePtr();
    if (!field_type->isRealFloatingType())
      return false;
    num_fields++;
  }

  return num_fields <= 4;
}

// Implements the rules described in sections 5.9 and 6.8 of the ARM ABI
std::vector<CAbiArgKind> classifyARMAggregate(const clang::Type &type, const clang::ASTContext &astContext) {
  const clang::RecordType *rec = type.getAsStructureType();
  const clang::RecordDecl *decl = rec->getDecl();

  // HFAs (5.9.5.1)
  if (isHFA(decl, astContext)) {
    // HFAs (homogenous floating point aggregates) are structs of all floats.
    // We can pass these in registers.
    return std::vector<CAbiArgKind>(std::distance(decl->field_begin(), decl->field_end()), CAbiArgKind::Float);
  }

  // TODO detect homogenous vector aggregates (5.9.5.2) which are unsupported

  // size is in bits
  uint64_t size = astContext.getTypeSize(&type);
  uint64_t alignment = astContext.getTypeAlign(&type);

  // If the aggregate size is less than or equal to 16 bytes we might fit in registers
  if (size <= 128) {
    std::vector<CAbiArgKind> kinds;
    uint64_t offset = 0;

    // Iterate over the fields of the aggregate
    for (auto field : decl->fields()) {
      uint64_t field_offset = astContext.getFieldOffset(field);
      const clang::Type *field_type = field->getType().getTypePtr();
      uint64_t field_size = astContext.getTypeSize(field_type);

      // If there is padding between the previous field and the current field,
      // add a Memory classification for the padding bytes
      if (offset < field_offset) {
        kinds.push_back(CAbiArgKind::Memory);
        offset = field_offset;
      }

      // Classify the field type
      if (field_type->isFundamentalType()) {
        CAbiArgKind kind;
        if (type.isIntegralOrEnumerationType() || type.isPointerType() || type.isReferenceType()) {
          kind = CAbiArgKind::Integral;
        } else if (type.isRealFloatingType()) {
          kind = CAbiArgKind::Float;
        } else {
          kind = CAbiArgKind::Memory;
        }
        kinds.push_back(kind);
      } else {
        kinds.push_back(CAbiArgKind::Memory);
      }

      // Update the offset to the end of the current field
      offset += field_size;

      // Align the offset to the minimum of the aggregate alignment and 8 bytes
      offset = llvm::alignTo(offset, std::min(alignment, uint64_t(64)));
    }

    // If one type is Memory, all must go to memory
    return mergeKinds(kinds);
  } else {
    // If the aggregate size exceeds 16 bytes, return a vector of Memory kinds
    return std::vector<CAbiArgKind>((size + 63) / 64, CAbiArgKind::Memory);
  }
}

// Compute sequence of eightbyte classifications for a type that Clang has
// chosen to pass directly in registers
static std::vector<CAbiArgKind> classifyDirectType(const clang::Type &type,
                                                   const clang::ASTContext &astContext, Arch arch) {
  if (type.isVoidType())
    return {};
  if (type.isScalarType()) {
    switch (type.getScalarTypeKind()) {
    case clang::Type::ScalarTypeKind::STK_CPointer:
    case clang::Type::ScalarTypeKind::STK_Bool:
    case clang::Type::ScalarTypeKind::STK_Integral:
    case clang::Type::ScalarTypeKind::STK_FixedPoint:
      return {CAbiArgKind::Integral};
    case clang::Type::ScalarTypeKind::STK_Floating:
      return {CAbiArgKind::Float};
    case clang::Type::ScalarTypeKind::STK_IntegralComplex:
    case clang::Type::ScalarTypeKind::STK_FloatingComplex:
      llvm::report_fatal_error(
          "complex types not yet supported for ABI computation");
    case clang::Type::ScalarTypeKind::STK_BlockPointer:
    case clang::Type::ScalarTypeKind::STK_ObjCObjectPointer:
    case clang::Type::ScalarTypeKind::STK_MemberPointer:
      // these may have special ABI handling due to containing code
      // pointers or having special calling convention
      llvm::report_fatal_error(
          "unsupported scalar type (obj-C object, Clang block, or C++ member) found during ABI computation");
    }
  } else {
    if (arch == Arch::Aarch64) {
      return classifyARMAggregate(type, astContext);
    } else {
      // TODO maybe break this out into a function classifyX86Aggregate
      // Slightly annoying because we call classifyDirectType recursively

      // Handle the case where we pass a struct directly in a register.
      // The strategy here is to iterate through each field of the struct
      // and record the ABI type each time we exit an eightbyte chunk.

      const clang::RecordType *rec = type.getAsStructureType();
      const clang::RecordDecl *decl = rec->getDecl();

      if (decl->canPassInRegisters()) {
        std::vector<CAbiArgKind> out; // Classifications for the entire record
        std::optional<CAbiArgKind> pending_kind;
        int64_t prev_end_offset = 0; // Initially, first eightbyte

        const clang::ASTRecordLayout &layout =
            astContext.getASTRecordLayout(decl);

        // Consider the classification of each field, but only push the current
        // classification each time we leave an eightbyte
        for (auto field : decl->fields()) {
          int64_t offset = layout.getFieldOffset(field->getFieldIndex());

          // Save the classification of the last eightbyte if we've left it
          bool same_eightbyte = offset / 64 == prev_end_offset / 64;
          if (!same_eightbyte) {
            assert(pending_kind.has_value());
            out.push_back(*pending_kind);
            pending_kind.reset();
          }

          // Update pending_kind based on ABI rules.
          // We expect the field to fit in a single register (any larger and we
          // should pass the entire struct on the stack). The field may be another
          // struct, so we have to call this recursively.
          auto recur = classifyDirectType(*field->getType(), astContext, arch);
          if (recur.size() != 1) {
            llvm::report_fatal_error(
                "unexpectedly classified register-passable field as multiple eightbytes");
          }
          auto new_kind = recur[0];
          // This block sets pending_kind = new_kind regardless.
          // However we format it this way to match §3.2.3.4 of the x86_64 ABI.
          // In the future, if we add support for X87 types etc, this logic will
          // be more complex.
          if (pending_kind) {
            if (*pending_kind != new_kind) {
              if (new_kind == CAbiArgKind::Memory) {
                pending_kind = CAbiArgKind::Memory;
              } else if (new_kind == CAbiArgKind::Integral) {
                pending_kind = CAbiArgKind::Integral;
                // TODO: handle X87/X87UP/COMPLEX_X87
              } else {
                pending_kind = new_kind;
              }
            }
          } else {
            pending_kind = new_kind;
          }

          // Update prev_end_offset for next iteration
          if (field->isBitField()) {
            prev_end_offset = offset + field->getBitWidthValue(astContext);
          } else {
            prev_end_offset = offset + astContext.getTypeSize(field->getType());
          }
        }
        // Store any pending kind if the last field did not an eightbyte
        if (pending_kind) {
          out.push_back(*pending_kind);
          pending_kind.reset();
        }
        return out;
      }
      llvm::report_fatal_error(
          "classifyDirectType called on non-scalar, non-canPassInRegisters type");
    }
  }
}

static CAbiArgKind classifyLlvmType(const llvm::Type &type) {
  if (type.isFloatingPointTy()) {
    return CAbiArgKind::Float;
  } else if (type.isSingleValueType()) {
    return CAbiArgKind::Integral;
  } else if (type.isAggregateType()) {
    llvm::report_fatal_error("nested aggregates not currently handled");
  } else {
    llvm::report_fatal_error("unhandled type when computing ABI slots");
  }
  llvm::report_fatal_error("could not classify LLVM type!");
}

// Given a single argument, determine its ABI slots
static std::vector<CAbiArgKind>
abiSlotsForArg(const clang::QualType &qt,
               const clang::CodeGen::ABIArgInfo &argInfo,
               const clang::ASTContext &astContext,
               Arch arch) {
  // this function is most similar to Clang's `ClangToLLVMArgMapping::construct`
  typedef enum clang::CodeGen::ABIArgInfo::Kind Kind;
  switch (argInfo.getKind()) {
#if LLVM_VERSION_MAJOR >= 12
  // ptr in register
  case Kind::IndirectAliased:
    [[fallthrough]];
#endif
  // ptr in register
  case Kind::Indirect: {
    const clang::RecordType *rec = qt->getAsStructureType();
    if (rec != nullptr) {
      const clang::RecordDecl *decl = rec->getDecl();
      const clang::ASTRecordLayout &layout =
          astContext.getASTRecordLayout(decl);
      int64_t size = layout.getSize().getQuantity();
      std::vector<CAbiArgKind> out;
      // argument itself is in memory, so create enough memory-classified eightbytes to hold it
      for (int64_t i = 0; i < size; i += 8) {
        out.push_back(CAbiArgKind::Memory);
      }
      return out;
    }
  }
  // in register with zext/sext
  case Kind::Extend:
    [[fallthrough]];
  case Kind::Direct: // in register
  {
    llvm::StructType *STy =
        dyn_cast<llvm::StructType>(argInfo.getCoerceToType());
    if (STy) {
      // Struct case
      if (argInfo.getCanBeFlattened()) {
        // A struct is "flattenable" if its individual elements can be passed as arguments.
        // In this case we classify each element of the struct and add each to the list.
        auto elems = STy->elements();
        std::vector<CAbiArgKind> out = {};
        for (const auto &elem : elems) {
          out.push_back(classifyLlvmType(*elem));
        }
        return out;
      } else {
        // A non-flattenable direct (passed in register) type is a pointer.
        // see clang's ClangToLLVMArgMapping::construct
        return {CAbiArgKind::Integral};
      }
    }
    // We have a scalar type, so classify it.
    return classifyDirectType(*qt.getCanonicalType(), astContext, arch);
  }
  case Kind::Ignore:   // no ABI presence
                       // fall through
  case Kind::InAlloca: // via implicit pointer
    return {};
  case Kind::Expand: // split aggregate into multiple registers -- already
                     // handled before our logic runs?
    llvm::report_fatal_error(
        "unhandled \"Expand\" type when computing ABI slots");
  case Kind::CoerceAndExpand: // same as Expand for our concerns
    // this code is, afaict, dead
    llvm::report_fatal_error(
        "unhandled \"CoerceAndExpand\" type when computing ABI slots");
  }
  llvm::report_fatal_error("could not compute ABI slots for arg!\n");
}

/// Get CGFunctionInfo for a given function declaration.
const clang::CodeGen::CGFunctionInfo &
cgFunctionInfo(clang::CodeGen::CodeGenModule &cgm,
               const clang::FunctionDecl &fnDecl) {
  const clang::QualType &ft = fnDecl.getType();
  const auto &canQualFnTy = ft->getCanonicalTypeUnqualified();
  assert(ft->isFunctionType());
  assert(ft->isFunctionProtoType() || ft->isFunctionNoProtoType());
  if (ft->isFunctionProtoType()) {
    return clang::CodeGen::arrangeFreeFunctionType(
        cgm, canQualFnTy.castAs<clang::FunctionProtoType>());
  } else {
    return clang::CodeGen::arrangeFreeFunctionType(
        cgm, canQualFnTy.castAs<clang::FunctionNoProtoType>());
  }
}

CAbiSignature determineAbi(const clang::CodeGen::CGFunctionInfo &info,
                           const clang::ASTContext &astContext, Arch arch) {
  // get ABI for return type and each parameter
  CAbiSignature sig;
  sig.variadic = info.isVariadic();

  // We want to find the layout of the parameter and return value "slots."
  // We can store a certain number of slots in registers, while the rest
  // will go on the stack.
  // These are "slots" and not parameters because some parameters larger
  // than 64 bits can occupy multiple slots.
  // Each slot is of one of the types described in CAbiArgKind.

  // Get the slots for the return value.
  auto &returnInfo = info.getReturnInfo();
  sig.ret = abiSlotsForArg(info.getReturnType(), returnInfo, astContext, arch);

  auto is_integral = [](auto &x) { return x == CAbiArgKind::Integral; };

  // num_regs is the number of registers in which the return value is stored.
  // This may be one for a value up to 64 bits or two for a 128-bit value.
  size_t num_regs = std::count_if(sig.ret.begin(), sig.ret.end(), is_integral);
  // It's possible that clang gives us back a value of three or more integers,
  // for example a struct full of ints. However, in reality the whole value
  // goes on the stack in this case according to the x64 ABI.
  // We handle that case here.
  if (num_regs > 2) { // TODO do we need to do the same on ARM?
    // Replace the integer slots with an equal number of memory (stack) slots
    std::erase_if(sig.ret, is_integral);
    for (int i = 0; i < num_regs; i++) {
        sig.ret.push_back(CAbiArgKind::Memory);
    }
  }

  // Now determine the slots for the arguments
  for (auto &argInfo : info.arguments()) {
    clang::QualType paramType = argInfo.type;
    auto slots = abiSlotsForArg(paramType, argInfo.info, astContext, arch);
    sig.args.insert(sig.args.end(), slots.begin(), slots.end());
  }

  return sig;
}

CAbiSignature determineAbiForDecl(const clang::FunctionDecl &fnDecl, Arch arch) {
  clang::ASTContext &astContext = fnDecl.getASTContext();

  // set up context for codegen so we can ask about function ABI
  // XXX: these should probably be constructed at the top-level and
  // just `clang::CodeGen::CodeGenModule& cgm` could be passed in.
  // doing so would add the precondition that the clang::AstContext of
  // the passed fnDecl matches that of the passed CGM's CodeGenerator.
#if LLVM_VERSION_MAJOR >= 15
  clang::FileSystemOptions fso{"."};
  clang::FileManager fm(fso);
  llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> vfs =
      &fm.getVirtualFileSystem();
#endif

  clang::HeaderSearchOptions hso;
  clang::PreprocessorOptions ppo;
  clang::CodeGenOptions cgo;
  llvm::LLVMContext llvmCtx;
  clang::CodeGenerator *codeGenerator =
      CreateLLVMCodeGen(astContext.getDiagnostics(), llvm::StringRef(),
#if LLVM_VERSION_MAJOR >= 15
                        vfs,
#endif
                        hso, ppo, cgo, llvmCtx);

  codeGenerator->Initialize(astContext);
  clang::CodeGen::CodeGenModule &cgm = codeGenerator->CGM();

  auto name = fnDecl.getNameInfo().getAsString();
  const auto &info = cgFunctionInfo(cgm, fnDecl);

  const auto &convention = info.getEffectiveCallingConvention();

  // our ABI computation assumes SysV ABI; bail if another ABI is explicitly
  // selected
  if (convention != clang::CallingConv::CC_X86_64SysV &&
      convention != clang::CallingConv::CC_C) {
    printf("calling convention of %s was not SysV (%d)\n", name.c_str(),
           info.getASTCallingConvention());
    abort();
  }
  return determineAbi(info, astContext, arch);
}

CAbiSignature determineAbiForProtoType(const clang::FunctionProtoType &fpt,
                                       clang::ASTContext &astContext, Arch arch) {
  // FIXME: This is copied verbatim from determineAbiForDecl and could be
  // factored out. This depends on what we do with PR #78 so I'm leaving it as
  // is for now.
#if LLVM_VERSION_MAJOR >= 15
  clang::FileSystemOptions fso{"."};
  clang::FileManager fm(fso);
  llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> vfs =
      &fm.getVirtualFileSystem();
#endif

  clang::HeaderSearchOptions hso;
  clang::PreprocessorOptions ppo;
  clang::CodeGenOptions cgo;
  llvm::LLVMContext llvmCtx;
  clang::CodeGenerator *codeGenerator =
      CreateLLVMCodeGen(astContext.getDiagnostics(), llvm::StringRef(),
#if LLVM_VERSION_MAJOR >= 15
                        vfs,
#endif
                        hso, ppo, cgo, llvmCtx);

  codeGenerator->Initialize(astContext);
  clang::CodeGen::CodeGenModule &cgm = codeGenerator->CGM();

  const auto &info = clang::CodeGen::arrangeFreeFunctionType(
      cgm,
      fpt.getCanonicalTypeUnqualified().castAs<clang::FunctionProtoType>());

  return determineAbi(info, astContext, arch);
}
