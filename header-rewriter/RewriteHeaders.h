#include "CAbi.h"
#include "clang/Tooling/RefactoringCallbacks.h"
#include <string>

using namespace clang::ast_matchers;
using namespace clang::tooling;

struct FunctionWrapper {
  std::string name;
  std::string mangled_name;
  std::string definition;

  FunctionWrapper(const clang::FunctionDecl *fn_decl);
};

class FnDecl;
std::shared_ptr<FnDecl>
createFnDecl(ASTMatchRefactorer &refactorer,
             std::map<std::string, Replacements> &FileReplacements);
const std::vector<FunctionWrapper> &getWrappedFunctions(const FnDecl &);

struct FunctionInfo {
  // The new type for this function pointer
  std::string new_type;

  // The return type of the function
  std::string return_type;

  // The list of parameter types, e.g. "void f(int a, float b)" would be
  // ["int", "float"]
  std::vector<std::string> parameter_types;

  CAbiSignature sig;
};

class FnPtrPrinter;
std::shared_ptr<FnPtrPrinter>
createFnPtrPrinter(ASTMatchRefactorer &refactorer,
                   std::map<std::string, Replacements> &FileReplacements);
const std::map<std::string, FunctionInfo> &
getFunctionInfo(const FnPtrPrinter &);

std::string
generate_output_header(const std::map<std::string, FunctionInfo> &FunctionInfo);

void emit_wrappers(llvm::raw_ostream &WrapperOut, llvm::raw_ostream &SymsOut,
                   const std::vector<FunctionWrapper> &WrappedFunctions);
