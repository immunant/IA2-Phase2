#include "clang/AST/AST.h"
#include "clang/Basic/CodeGenOptions.h"
#include "clang/CodeGen/ModuleBuilder.h"
#include "clang/Lex/HeaderSearchOptions.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "llvm/IR/LLVMContext.h"

clang::CodeGenerator *createCodeGenerator(clang::ASTContext &astContext) {
  // Set up context in order to create a CodeGenerator, which will be used
  // to query function ABI
  clang::HeaderSearchOptions hso;
  clang::PreprocessorOptions ppo;
  clang::CodeGenOptions cgo;
  llvm::LLVMContext llvmCtx;
  clang::CodeGenerator *codeGenerator = CreateLLVMCodeGen(
    astContext.getDiagnostics(), llvm::StringRef(), hso, ppo, cgo, llvmCtx);

  codeGenerator->Initialize(astContext);
  return codeGenerator;
}
