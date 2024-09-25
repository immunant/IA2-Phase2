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
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include <cstddef>
#include <optional>
#include <vector>

#define VERBOSE_DEBUG 0

#if VERBOSE_DEBUG
#define DEBUG(X) do { X; } while (0)
#else
#define DEBUG(X) do { } while (0)
#endif

static CAbiArgLocation classifyScalarType(const llvm::Type &type) {
  if (type.isFloatingPointTy()) {
    return CAbiArgLocation{CAbiArgKind::Float};
  } else if (type.isIntOrPtrTy()) {
    return CAbiArgLocation{CAbiArgKind::Integral};
  } else if (type.isAggregateType()) {
    llvm::report_fatal_error("nested aggregates not currently handled");
  } else {
    llvm::report_fatal_error("unhandled type when computing ABI slots");
  }
  llvm::report_fatal_error("could not classify LLVM type!");
}

// Given a single argument, determine its ABI slots
static std::vector<CAbiArgLocation>
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
      return {CAbiArgLocation{
          .kind = CAbiArgKind::Memory,
          .size = static_cast<unsigned>(layout.getSize().getQuantity()),
          .align = static_cast<unsigned>(layout.getAlignment().getQuantity()),
      }};
    }
  }
  // in register with zext/sext
  case Kind::Extend:
    [[fallthrough]];
  case Kind::Direct: // in register
  {
    auto Ty = argInfo.getCoerceToType();
    llvm::StructType *STy = llvm::dyn_cast<llvm::StructType>(Ty);
    if (STy) {
      // Struct case
      if (argInfo.getCanBeFlattened()) {
        // A struct is "flattenable" if its individual elements can be passed as arguments.
        // In this case we classify each element of the struct and add each to the list.
        auto elems = STy->elements();
        std::vector<CAbiArgLocation> out = {};
        for (const auto &elem : elems) {
          out.push_back(classifyScalarType(*elem));
        }
        return out;
      } else {
        llvm::report_fatal_error("non-flattenable struct cannot be passed directly in registers");
      }
    }
    llvm::ArrayType *ATy = llvm::dyn_cast<llvm::ArrayType>(Ty);
    if (ATy) {
      // Array case
      return std::vector(ATy->getNumElements(), classifyScalarType(*ATy->getElementType()));
    }
    llvm::FixedVectorType *VTy = llvm::dyn_cast<llvm::FixedVectorType>(Ty);
    if (VTy) {
      // Vector case
      return std::vector(VTy->getNumElements(), classifyScalarType(*VTy->getElementType()));
    }
    // We have a scalar type, so classify it.
    return {classifyScalarType(*Ty)};
  }
  case Kind::Ignore:   // no ABI presence
    return {};
  case Kind::InAlloca: // via implicit pointer
    // It looks like inalloca is only valid on Win32
    llvm::report_fatal_error("Cannot handle InAlloca arguments");
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
  DEBUG({
    llvm::dbgs() << "return: ";
    returnInfo.dump();
  });
  sig.ret = abiSlotsForArg(info.getReturnType(), returnInfo, astContext, arch);

  auto is_integral = [](auto &x) { return x.kind == CAbiArgKind::Integral; };

  // num_regs is the number of registers in which the return value is stored.
  // This may be one for a value up to 64 bits or two for a 128-bit value.
  std::size_t num_regs = std::count_if(sig.ret.begin(), sig.ret.end(), is_integral);
  // It's possible that clang gives us back a value of three or more integers,
  // for example a struct full of ints. However, in reality the whole value
  // goes on the stack in this case according to the x64 ABI.
  // We handle that case here.
  if (num_regs > 2) { // TODO do we need to do the same on ARM?
    // Replace the integer slots with an equal number of memory (stack) slots
    std::erase_if(sig.ret, is_integral);
    sig.ret.push_back(CAbiArgLocation{
        .kind = CAbiArgKind::Memory,
        .size = static_cast<unsigned>(num_regs) * 8,
        .align = 8,
    });
  }

  // Now determine the slots for the arguments
  for (auto &argInfo : info.arguments()) {
    DEBUG({
      llvm::dbgs() << "arg: ";
      argInfo.info.dump();
    });
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
  DEBUG(llvm::dbgs() << "determineAbiForDecl: " << name << "\n");

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
