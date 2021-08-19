#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/RefactoringCallbacks.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FormatVariadic.h"

using namespace clang::ast_matchers;
using namespace clang::tooling;

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static llvm::cl::OptionCategory HeaderRewriterCategory("header rewriter options");

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

class FnPtrPrinter : public RefactoringCallback {
public:
  virtual void run(const MatchFinder::MatchResult &Result) {
    if (const clang::ParmVarDecl *parm_var_decl =
        Result.Nodes.getNodeAs<clang::ParmVarDecl>("fnPtrParam")) {
        auto new_param =
            llvm::formatv("struct IA2_fnptr_{0} {1}", Replace.size(),
                          parm_var_decl->getName()).str();

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
    RefactoringTool tool(options_parser.getCompilations(),
                         options_parser.getSourcePathList());

    ASTMatchRefactorer refactorer(tool.getReplacements());
    FnPtrPrinter printer;
    refactorer.addMatcher(fn_ptr_matcher, &printer);

    return tool.runAndSave(newFrontendActionFactory(&refactorer).get());
}
