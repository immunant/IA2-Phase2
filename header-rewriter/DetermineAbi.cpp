#include "CAbi.h"
#include "clang/AST/AST.h"
#include "clang/CodeGen/CodeGenABITypes.h"
#include "clang/CodeGen/ModuleBuilder.h"
#include "clang/Lex/HeaderSearchOptions.h"
#include "clang/Lex/PreprocessorOptions.h"

static auto classifyType(const clang::Type &type) -> std::vector<CAbiArgKind> {
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
      puts("complex types not yet supported for ABI computation");
      abort();
    case clang::Type::ScalarTypeKind::STK_BlockPointer:
    case clang::Type::ScalarTypeKind::STK_ObjCObjectPointer:
    case clang::Type::ScalarTypeKind::STK_MemberPointer:
      // these may have special ABI handling due to containing code
      // pointers or having special calling convention
      puts(
          "unsupported scalar type (obj-C object, Clang block, or C++ member) found during ABI computation");
      abort();
    }
  } else {
    return {CAbiArgKind::Integral};
    // assert(0 && "classifyType called on non-scalar type");
  }
  printf("could not classify type!\n");
  abort();
}

static auto classifyLlvmType(const llvm::Type &type) -> CAbiArgKind {
  if (type.isFloatingPointTy()) {
    return CAbiArgKind::Float;
  } else if (type.isSingleValueType()) {
    return CAbiArgKind::Integral;
  } else if (type.isAggregateType()) {
    assert(0 && "nested aggregates not currently handled");
  } else {
    assert(0 && "unhandled type when computing ABI slots");
  }
  printf("could not classify LLVM type!\n");
  abort();
}

static auto abiSlotsForArg(const clang::QualType &qt,
                           const clang::CodeGen::ABIArgInfo &argInfo,
                           const clang::ASTContext &astContext)
    -> std::vector<CAbiArgKind> {
  // this function is most similar to Clang's `ClangToLLVMArgMapping::construct`
  using enum clang::CodeGen::ABIArgInfo::Kind;
  switch (argInfo.getKind()) {
  case Extend:                      // in register with zext/sext
                                    // fall through
  case Indirect:                    // ptr in register
                                    // fall through
  case IndirectAliased:             // ptr in register
    return {CAbiArgKind::Integral}; //{classifyType(*qt)};
  case Direct:                      // in register
  {
    llvm::StructType *STy =
        dyn_cast<llvm::StructType>(argInfo.getCoerceToType());
    if (STy) {
      if (argInfo.getCanBeFlattened()) {
        auto elems = STy->elements();
        std::vector<CAbiArgKind> out = {};
        for (const auto &elem : elems) {
          out.push_back(classifyLlvmType(*elem));
        }
        return out;
      } else {
        return {CAbiArgKind::Integral}; // one pointer in register, see `clang's
                                        // ClangToLLVMArgMapping::construct`
      }
    }
    // if not flattenable, classify the single-register value
    return classifyType(*qt.getCanonicalType());
  }
  case Ignore:   // no ABI presence
                 // fall through
  case InAlloca: // via implicit pointer
    return {};
  case Expand: // split aggregate into multiple registers
    assert(0 && "unhandled \"Expand\" type when computing ABI slots");
    // return getExpansionSize(qt, astContext);
  case CoerceAndExpand: // same as Expand for our concerns
  {
    const auto &seq = argInfo.getCoerceAndExpandTypeSequence();
    std::vector<CAbiArgKind> out = {};
    for (const auto &elem : seq) {
      out.push_back(classifyLlvmType(*elem));
    }
    return out;
  }
  }
  printf("could not compute ABI slots for arg!\n");
  abort();
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

auto determineAbiForDecl(const clang::FunctionDecl &fnDecl) -> CAbiSignature {
  clang::ASTContext &astContext = fnDecl.getASTContext();

  // set up context for codegen so we can ask about function ABI
  // XXX: these should probably be constructed at the top-level and
  // just `clang::CodeGen::CodeGenModule& cgm` could be passed in.
  // doing so would add the precondition that the clang::AstContext of
  // the passed fnDecl matches that of the passed CGM's CodeGenerator.
  clang::HeaderSearchOptions hso;
  clang::PreprocessorOptions ppo;
  clang::CodeGenOptions cgo;
  llvm::LLVMContext llvmCtx;
  clang::CodeGenerator *codeGenerator = CreateLLVMCodeGen(
      astContext.getDiagnostics(), llvm::StringRef(), hso, ppo, cgo, llvmCtx);

  codeGenerator->Initialize(astContext);
  clang::CodeGen::CodeGenModule &cgm = codeGenerator->CGM();

  auto name = fnDecl.getNameInfo().getAsString();
  printf("computing abi for %s\n", name.c_str());
  const auto &info = cgFunctionInfo(cgm, fnDecl);

  // get ABI for return type and each parameter
  CAbiSignature sig;
  sig.variadic = fnDecl.isVariadic();
  auto &returnInfo = info.getReturnInfo();
  sig.ret = abiSlotsForArg(fnDecl.getReturnType(), returnInfo, astContext);
  for (auto &argInfo : info.arguments()) {
    clang::QualType paramType = argInfo.type;
    auto slots = abiSlotsForArg(paramType, argInfo.info, astContext);
    sig.args.insert(sig.args.end(), slots.begin(), slots.end());
  }
  return sig;
}
