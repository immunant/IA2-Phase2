#include "CAbi.h"
#include "GenCallAsm.h"
#include "clang/AST/AST.h"
#include "clang/AST/RecordLayout.h"
#include "clang/Basic/CodeGenOptions.h"
#if LLVM_VERSION_MAJOR >= 15
#include "clang/Basic/FileManager.h"
#include "clang/Basic/FileSystemOptions.h"
#endif
#include "clang/Basic/SourceManager.h"
#include "clang/CodeGen/CGFunctionInfo.h"
#include "clang/CodeGen/CodeGenABITypes.h"
#include "clang/CodeGen/ModuleBuilder.h"
#include "clang/Lex/HeaderSearchOptions.h"
#include "clang/Lex/Lexer.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include <cstddef>
#include <optional>
#include <vector>

#define VERBOSE_DEBUG 0

#if VERBOSE_DEBUG
#define DEBUG(X) \
  do {           \
    X;           \
  } while (0)
#else
#define DEBUG(X) \
  do {           \
  } while (0)
#endif

static ArgLocation classifyScalarType(const llvm::Type &type) {
  auto Size = type.getScalarSizeInBits() / 8;
  if (type.isFloatingPointTy()) {
    return ArgLocation::Register(ArgLocation::Kind::Float, Size);
  } else if (type.isIntOrPtrTy()) {
    return ArgLocation::Register(ArgLocation::Kind::Integral, Size);
  } else if (type.isAggregateType()) {
    llvm::report_fatal_error("nested aggregates not currently handled");
  } else {
    llvm::report_fatal_error("unhandled type when computing ABI slots");
  }
  llvm::report_fatal_error("could not classify LLVM type!");
}

// Given a single argument, determine its ABI slots
static std::vector<ArgLocation>
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
      return {ArgLocation::Indirect(
          layout.getSize().getQuantity(),
          layout.getAlignment().getQuantity())};
    }
    llvm::errs() << "indirect argument (" << qt.getAsString() << ") not a struct\n";
    return {ArgLocation::Indirect(
        astContext.getTypeSize(qt),
        astContext.getTypeAlign(qt))};
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
        std::vector<ArgLocation> out = {};
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
  case Kind::Ignore: // no ABI presence
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
  } else if (ft->isFunctionNoProtoType()) {
    return clang::CodeGen::arrangeFreeFunctionType(
        cgm, canQualFnTy.castAs<clang::FunctionNoProtoType>());
  } else {
    llvm::errs() << "Function is neither Proto nor NoProto type!";
    abort();
  }
}

AbiSignature determineAbiSignature(const clang::CodeGen::CGFunctionInfo &info,
                                   const clang::ASTContext &astContext, Arch arch) {
  // get ABI for return type and each parameter
  AbiSignature sig;

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

static std::string get_expansion_filename(const clang::SourceLocation loc,
                                          const clang::SourceManager &sm) {
  llvm::SmallString<256> s(sm.getFilename(sm.getExpansionLoc(loc)));
  return s.str().str();
}

AbiSignature determineAbiSignatureForDecl(const clang::FunctionDecl &fnDecl, Arch arch) {
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
  clang::SourceLocation loc = fnDecl.getLocation();
  auto &sm = astContext.getSourceManager();
  auto expansion_file = get_expansion_filename(loc, sm);
  // skip for function decls that can't be made into pointers, as a sanity check
  auto fn_ty = fnDecl.getType()->getAs<clang::FunctionType>();
  if (!fn_ty) {
    llvm::errs() << "no fn ptr type for " << fnDecl.getType().getAsString() << " " << clang::Lexer::getSourceText(clang::CharSourceRange::getTokenRange(fnDecl.getSourceRange()), sm, astContext.getLangOpts()).str() << "\n";
    return {};
  }

  for (auto &param : fnDecl.parameters()) {
    auto paramTy = param->getOriginalType();
    if (paramTy.getTypePtr()->getAsRecordDecl() && paramTy.getTypePtr()->getAsCXXRecordDecl()) {
      llvm::errs() << "Function " << name << " defined at " << loc.printToString(sm) << " takes cxxrecord type " << paramTy.getAsString() << " as arg, skipping\n";
      return {};
    }
  }
  auto retTy = fn_ty->getReturnType();
  if (retTy.getTypePtr()->getAsRecordDecl() && retTy.getTypePtr()->getAsCXXRecordDecl()) {
    llvm::errs() << "Function " << name << " defined at " << loc.printToString(sm) << " returns cxxrecord type " << retTy.getAsString() << ", skipping\n";
    return {};
  }

  if (name == "load" || name == "inspect" || expansion_file.find("dist/include/double-conversion") != std::string::npos // big guns
      || expansion_file.find("dist/include/fmt") != std::string::npos                                                   // big guns
      || expansion_file.find("dist/include/mozilla/") != std::string::npos                                              // big guns
      || expansion_file.find("js/") != std::string::npos                                                                // big guns
      || expansion_file.find("BaseProfilingCategory.h") != std::string::npos || expansion_file.find("nsStreamUtils.h") != std::string::npos || expansion_file.find("xpcom") != std::string::npos || expansion_file.find("mozilla/Result.h") != std::string::npos || expansion_file.find("mozilla/Atomics.h") != std::string::npos) {
    llvm::errs() << "Function " << name << " defined at " << loc.printToString(sm) << ", skipping\n";
    return {};
  }

  if (fnDecl.getTemplatedKind() != clang::FunctionDecl::TK_NonTemplate) {
    llvm::errs() << "Function " << name << " has TemplatedKind " << (int)fnDecl.getTemplatedKind() << "\n";
  }
  if (auto *TempInfo = fnDecl.getTemplateSpecializationInfo()) {
    llvm::errs() << "Function " << name << " has template info\n";
    if (!TempInfo->isExplicitInstantiationOrSpecialization()) {
      llvm::errs() << "Function " << name << " is a template\n";
      return {};
    }
    return {};
  }

  const auto &info = cgFunctionInfo(cgm, fnDecl);
  DEBUG(llvm::dbgs() << "determineAbiSignatureForDecl: " << name << "\n");

  const auto &convention = info.getEffectiveCallingConvention();

  // our ABI computation assumes SysV ABI; bail if another ABI is explicitly
  // selected
  if (convention != clang::CallingConv::CC_X86_64SysV &&
      convention != clang::CallingConv::CC_C) {
    printf("calling convention of %s was not SysV (%d)\n", name.c_str(),
           info.getASTCallingConvention());
    abort();
  }
  return determineAbiSignature(info, astContext, arch);
}

AbiSignature determineAbiSignatureForProtoType(const clang::FunctionProtoType &fpt,
                                               clang::ASTContext &astContext, Arch arch) {
  // FIXME: This is copied verbatim from determineAbiSignatureForDecl and could be
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

  return determineAbiSignature(info, astContext, arch);
}

// Not thread safe.
std::unordered_map<std::string, uint32_t> type_ids;

uint32_t get_type_id(clang::QualType type) {
  auto canonical_name = type.getCanonicalType().getAsString();
  // Constructs the default value, 0, if the key doesn't exist yet.
  auto type_id = type_ids[canonical_name];
  if (type_id == 0) {
    type_id = type_ids.size() + 1;
    type_ids[canonical_name] = type_id;
  }
  return type_id;
}

Param determineParam(std::string name, clang::QualType type) {
  return (Param) {
      .name = name,
      .type_name = type.getAsString(),
      .canonical_type_name = type.getCanonicalType().getAsString(),
      .type_id = get_type_id(type),
  };
}

ApiSignature determineApiSignatureForDecl(const clang::FunctionDecl &fn_decl) {
  ApiSignature api;

  for (auto param_ptr : fn_decl.parameters()) {
    auto &param = *param_ptr;
    api.args.emplace_back(determineParam(param.getNameAsString(), param.getOriginalType()));
  }

  api.ret = determineParam("return", fn_decl.getReturnType());

  return api;
}

ApiSignature determineApiSignatureForProtoType(const clang::FunctionProtoType &fpt,
                                               clang::ASTContext &astContext) {
  ApiSignature api;

  auto i = 0;
  for (auto param_type : fpt.param_types()) {
    auto name = "arg_" + std::to_string(i++);
    api.args.emplace_back(determineParam(name, param_type));
  }
  api.ret = determineParam("return", fpt.getReturnType());

  return api;
}

FnSignature determineFnSignatureForDecl(const clang::FunctionDecl &fnDecl, Arch arch) {
  return (FnSignature){
      .abi = determineAbiSignatureForDecl(fnDecl, arch),
      .api = determineApiSignatureForDecl(fnDecl),
      .variadic = fnDecl.isVariadic(),
  };
}

FnSignature determineFnSignatureForProtoType(const clang::FunctionProtoType &fpt,
                                             clang::ASTContext &astContext, Arch arch) {
  return (FnSignature){
      .abi = determineAbiSignatureForProtoType(fpt, astContext, arch),
      .api = determineApiSignatureForProtoType(fpt, astContext),
      .variadic = fpt.isVariadic(),
  };
}
