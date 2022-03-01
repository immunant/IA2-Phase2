#include "clang/AST/AST.h"
#include "clang/CodeGen/ModuleBuilder.h"

clang::CodeGenerator *createCodeGenerator(clang::ASTContext &astContext);