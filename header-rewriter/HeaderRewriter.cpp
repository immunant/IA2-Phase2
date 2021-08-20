#include "clang/AST/AST.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/RefactoringCallbacks.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FormatVariadic.h"
#include <fstream>

using namespace clang::ast_matchers;
using namespace clang::tooling;

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static llvm::cl::OptionCategory
    HeaderRewriterCategory("header rewriter options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static llvm::cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static llvm::cl::extrahelp MoreHelp("\nMore help text...\n");

static DeclarationMatcher fn_ptr_matcher =
    parmVarDecl(hasType(pointerType(pointee(ignoringParens(functionType())))))
        .bind("fnPtrParam");
// TODO: struct field matcher

static DeclarationMatcher fn_decl_matcher = functionDecl().bind("exportedFn");

class FnDecl : public RefactoringCallback {
public:
  virtual void run(const MatchFinder::MatchResult &Result) {
    if (const clang::FunctionDecl *fn_decl =
            Result.Nodes.getNodeAs<clang::FunctionDecl>("exportedFn")) {
      std::string original_decl;
      llvm::raw_string_ostream os(original_decl);
      fn_decl->print(os);

      // TODO: Handle attributes on wrapper
      auto fn_name = fn_decl->getNameInfo().getAsString();
      auto wrapper_name = "__libia2_" + fn_name;
      auto ret = fn_decl->getReturnType().getAsString();

      std::string param_decls;
      std::string param_names;

      for (auto &p : fn_decl->parameters()) {
        if (!param_names.empty()) {
          param_decls.append(", ");
          param_names.append(", ");
        }
        param_decls.append(p->getType().getAsString() + " ");
        auto name = p->getNameAsString();
        if (name.empty()) {
          auto n = &p - fn_decl->param_begin();
          name = llvm::formatv("__libia2_arg_{0}", n);
        }
        param_decls.append(name);
        param_names.append(name);
      }

      auto body =
          llvm::formatv("{5};\n{0} {1}({2}) {\n    return {3}({4});\n}\n#undef "
                        "{3}\n#define {3} {1}\n",
                        ret, wrapper_name, param_decls, fn_name, param_names,
                        original_decl)
              .str();
      Replacement wrap_decl{
          *Result.SourceManager,
          Result.SourceManager->getExpansionRange(fn_decl->getSourceRange()),
          body};

      auto err = Replace.add(wrap_decl);
      if (err) {
        llvm::errs() << "Error adding replacement: " << err << '\n';
      }
    }
  }
};

class FnPtrPrinter : public RefactoringCallback {
public:
  virtual void run(const MatchFinder::MatchResult &Result) {
    if (const clang::ParmVarDecl *parm_var_decl =
            Result.Nodes.getNodeAs<clang::ParmVarDecl>("fnPtrParam")) {
      auto new_param = llvm::formatv("struct IA2_fnptr_{0} {1}", Replace.size(),
                                     parm_var_decl->getName())
                           .str();

      Replacement r{*Result.SourceManager, parm_var_decl, new_param};
      auto err = Replace.add(r);
      if (err) {
        llvm::errs() << "Error adding replacement: " << err << '\n';
      }
    }
  }
};

int main(int argc, const char **argv) {
  CommonOptionsParser options_parser(argc, argv, HeaderRewriterCategory);
  for (auto s : options_parser.getSourcePathList()) {
    std::ifstream src(s, std::ios::binary);
    std::ofstream dst(s.append(".orig"), std::ios::binary);
    dst << src.rdbuf();
  }
  RefactoringTool tool(options_parser.getCompilations(),
                       options_parser.getSourcePathList());

  ASTMatchRefactorer refactorer(tool.getReplacements());
  FnPtrPrinter printer;
  FnDecl decl_replacement;
  // refactorer.addMatcher(fn_ptr_matcher, &printer);
  refactorer.addMatcher(fn_decl_matcher, &decl_replacement);

  return tool.runAndSave(newFrontendActionFactory(&refactorer).get());
}
