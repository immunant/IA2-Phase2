#include "CAbi.h"
#include "clang/AST/AST.h"
#include "clang/AST/RecordLayout.h"
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
    const clang::RecordType *rec = type.getAsStructureType();
    const clang::RecordDecl *decl = rec->getDecl();

    std::vector<CAbiArgKind> out;
    if (decl->canPassInRegisters()) {
      for (const auto &x : decl->fields()) {
        auto recur = classifyType(*x->getType());
        if (recur.size() != 1) {
          puts("size of register-passable field not 1!");
          abort();
        }
        out.push_back(recur[0]);
      }
      return out;
    }

    for (const auto &x : decl->fields()) {
      out.push_back(CAbiArgKind::Memory);
    }
    return out;
  }
  puts("could not classify type!\n");
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
  case Extend:          // in register with zext/sext
                        // fall through
  case Indirect:        // ptr in register
                        // fall through
  case IndirectAliased: // ptr in register
  {
    const clang::RecordType *rec = qt->getAsStructureType();
    if (rec != nullptr) {
      const clang::RecordDecl *decl = rec->getDecl();
      const clang::ASTRecordLayout &layout =
          astContext.getASTRecordLayout(decl);
      int64_t size = layout.getSize().getQuantity();
      std::vector<CAbiArgKind> out;
      if (size > 64) {
        // aggregates larger than 8 eightbytes are passed in memory
        for (int64_t i = 0; i < size; i += 8) {
          out.push_back(CAbiArgKind::Memory);
        }
        return out;
      }

      // aggregates of 1-8 eightbytes have each eightbyte classified
      // individually. we have to iterate over fields to determine which fields
      // are present in each eightbyte, then resolve conflicts with the logic
      // from SysV ABI §3.3.3 (step 4):
      auto field_iter = decl->field_begin();
      auto field_end = decl->field_end();
      while (field_iter != field_end) {
        // for now, just assume all arguments are integral
        out.push_back(CAbiArgKind::Integral);
        field_iter++;
      }

      // apply the "post-merger cleanup" described in SysV ABI §3.3.3 (step 5)
      // we do not yet handle x87up, so ignore 5b
      // 5c (n.b. that we ignore the sseup condition as we do not yet handle
      // sseup)
      if (out.size() > 2 && out[0] != CAbiArgKind::Float) {
        for (auto &i : out) {
          i = CAbiArgKind::Memory;
        }
        return out;
      }
      // we do not yet handle sseup, so ignore 5d

      return out;
    }
  }
  case Direct: // in register
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
  case Expand: // split aggregate into multiple registers -- already handled
               // before our logic runs?
    puts("unhandled \"Expand\" type when computing ABI slots");
    abort();
  case CoerceAndExpand: // same as Expand for our concerns
    // this code is, afaict, dead
    puts("unhandled \"CoerceAndExpand\" type when computing ABI slots");
    abort();
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

  const auto &convention = info.getEffectiveCallingConvention();

  // our ABI computation assumes SysV ABI; bail if another ABI is explicitly
  // selected
  if (convention != clang::CallingConv::CC_X86_64SysV &&
      convention != clang::CallingConv::CC_C) {
    printf("calling convention of %s was not SysV (%d)\n", name.c_str(),
           info.getASTCallingConvention());
    abort();
  }

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
