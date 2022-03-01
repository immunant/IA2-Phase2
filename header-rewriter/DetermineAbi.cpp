#include "CAbi.h"
#include "clang/AST/AST.h"
#include "clang/AST/RecordLayout.h"
#include "clang/Basic/CodeGenOptions.h"
#include "clang/CodeGen/CGFunctionInfo.h"
#include "clang/CodeGen/CodeGenABITypes.h"
#include "clang/CodeGen/ModuleBuilder.h"
#include "llvm/IR/LLVMContext.h"

static std::vector<CAbiArgKind> classifyType(const clang::Type &type) {
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
    const clang::RecordType *rec = type.getAsStructureType();
    const clang::RecordDecl *decl = rec->getDecl();

    std::vector<CAbiArgKind> out;
    if (decl->canPassInRegisters()) {
      for (const auto &x : decl->fields()) {
        auto recur = classifyType(*x->getType());
        if (recur.size() != 1) {
          llvm::report_fatal_error("size of register-passable field not 1!");
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
  llvm::report_fatal_error("could not classify type!\n");
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

static std::vector<CAbiArgKind>
abiSlotsForArg(const clang::QualType &qt,
               const clang::CodeGen::ABIArgInfo &argInfo,
               const clang::ASTContext &astContext) {
  // this function is most similar to Clang's `ClangToLLVMArgMapping::construct`
  typedef enum clang::CodeGen::ABIArgInfo::Kind Kind;
  switch (argInfo.getKind()) {
  // in register with zext/sext
  case Kind::Extend:
    [[fallthrough]];
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
        // FIXME: for now, just assume all arguments are integral
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
  case Kind::Direct: // in register
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

// Determine the ABI-level signature for a function, given its declaration.
// Note that the `.getAstContext()` of the passed fnDecl must match the
// astContext of the clang::CodeGenerator used by the passed CodeGenModule.
CAbiSignature determineAbiForDecl(const clang::FunctionDecl &fnDecl,
  clang::CodeGen::CodeGenModule &cgm) {
  clang::ASTContext &astContext = fnDecl.getASTContext();

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
