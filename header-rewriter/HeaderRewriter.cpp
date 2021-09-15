#include <clang/AST/AST.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Basic/FileManager.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Refactoring.h>
#include <clang/Tooling/RefactoringCallbacks.h>
#include <clang/Tooling/Tooling.h>
#include <fstream>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FormatVariadic.h>
#include <llvm/Support/raw_ostream.h>
#include <map>

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

static llvm::cl::opt<std::string>
    WrapperOutputFilename(llvm::cl::Positional, llvm::cl::Required,
                          llvm::cl::cat(HeaderRewriterCategory),
                          llvm::cl::desc("<wrapper output filename>"));

static DeclarationMatcher fn_ptr_matcher =
    parmVarDecl(hasType(pointerType(pointee(ignoringParens(functionType())))))
        .bind("fnPtrParam");
// TODO: struct field matcher

static DeclarationMatcher fn_decl_matcher =
    functionDecl(unless(isDefinition())).bind("exportedFn");

class FnDecl : public RefactoringCallback {
public:
  FnDecl(llvm::raw_ostream &WrapperOut, llvm::raw_ostream &SymsOut,
         std::map<std::string, Replacements> &FileReplacements,
         const std::vector<clang::FileEntryRef> &Sources)
      : WrapperOut(WrapperOut), SymsOut(SymsOut),
        FileReplacements(FileReplacements), Sources(Sources) {}

  virtual void run(const MatchFinder::MatchResult &Result) {
    if (const clang::FunctionDecl *fn_decl =
            Result.Nodes.getNodeAs<clang::FunctionDecl>("exportedFn")) {
      // This is an absolute path to the header with the fn decl
      llvm::StringRef header_name =
          Result.SourceManager->getFilename(fn_decl->getLocation());
      auto header_ref_result =
          Result.SourceManager->getFileManager().getFileRef(header_name);
      if (auto err = header_ref_result.takeError()) {
        llvm::errs() << "Error getting FileEntryRef: " << err << '\n';
      }
      clang::FileEntryRef header_ref = *header_ref_result;
      auto fn_name = fn_decl->getNameInfo().getAsString();

      // This callback may find a fn decl multiple times so only wrap it the
      // first time it's encountered in an input header
      if (isCanonicalDecl(fn_decl) && inSources(header_ref)) {

        if (!isInitialized(header_ref)) {
          addHeaderImport(header_name);
          WrapperOut << llvm::formatv("#include \"{0}\"\n", header_name);
          InitializedHeaders.push_back(header_ref);
        }

        std::string original_decl;
        llvm::raw_string_ostream os(original_decl);
        fn_decl->print(os);

        std::string new_decl =
            "IA2_WRAP_FUNCTION(" + fn_name + ");\n" + original_decl;
        Replacement decl_replacement{*Result.SourceManager, fn_decl, new_decl};

        auto err = FileReplacements[header_name.str()].add(decl_replacement);
        if (err) {
          llvm::errs() << "Error adding replacement: " << err << '\n';
        }

        // TODO: Handle attributes on wrapper
        auto wrapper_name = "__ia2_" + fn_name;
        auto ret_type = fn_decl->getReturnType();

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
            name = llvm::formatv("__ia2_arg_{0}", n);
          }
          param_decls.append(name);
          param_names.append(name);
        }

        std::string ret_val;
        auto ret_stmt = "";
        if (!ret_type.isCForbiddenLValueType()) {
          ret_val = llvm::formatv("{0} res = ", ret_type.getAsString()).str();
          ret_stmt = "    return res;\n";
        }

        WrapperOut << llvm::formatv(
            "{0} {1}({2}) {\n"
            "    // call_gate_push();\n"
            "    {3}{4}({5});\n"
            "    // call_gate_pop();\n"
            "{6}"
            "}\n",
            ret_type.getAsString(), wrapper_name, param_decls, ret_val, fn_name,
            param_names, ret_stmt);

        SymsOut << "    " << wrapper_name << ";\n";
        WrappedFnDecls.push_back(fn_name);
      }
    }
  }

private:
  llvm::raw_ostream &WrapperOut;
  llvm::raw_ostream &SymsOut;
  std::map<std::string, Replacements> &FileReplacements;
  // Headers passed as inputs
  const std::vector<clang::FileEntryRef> &Sources;
  // Headers that have included the IA2 header
  std::vector<clang::FileEntryRef> InitializedHeaders;
  // Function declarations that have already been wrapped
  std::vector<std::string> WrappedFnDecls;

  bool isCanonicalDecl(const clang::FunctionDecl *fn_decl) {
      return (fn_decl == fn_decl->getCanonicalDecl());
  }
  bool inSources(clang::FileEntryRef InputHeader) {
    return std::find(Sources.begin(), Sources.end(), InputHeader) !=
           Sources.end();
  }

  bool isInitialized(clang::FileEntryRef InputHeader) {
    return std::find(InitializedHeaders.begin(), InitializedHeaders.end(),
                     InputHeader) != InitializedHeaders.end();
  }

  bool functionDeclWrapped(llvm::StringRef FnDeclName) {
    return std::find(WrappedFnDecls.begin(), WrappedFnDecls.end(),
                     FnDeclName) != WrappedFnDecls.end();
  }

  void addHeaderImport(llvm::StringRef Filename) {
    auto err = FileReplacements[Filename.str()].add(
        Replacement(Filename, 0, 0, "#include <ia2.h>\n"));
    if (err) {
      llvm::errs() << "Error adding ia2 header import: " << err << '\n';
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

  RefactoringTool tool(options_parser.getCompilations(),
                       options_parser.getSourcePathList());
  clang::FileManager &file_mgr = tool.getFiles();
  std::vector<clang::FileEntryRef> input_files;
  for (auto s : options_parser.getSourcePathList()) {
    // Make a copy of each original input headers
    std::ifstream src(s, std::ios::binary);
    std::ofstream dst(s + ".orig", std::ios::binary);
    dst << src.rdbuf();

    // Get a `FileEntryRef` for each input header
    auto input_ref_result = file_mgr.getFileRef(s);
    if (auto err = input_ref_result.takeError()) {
      llvm::errs() << "Error getting FileEntryRef: " << err << '\n';
    }
    clang::FileEntryRef input_ref = *input_ref_result;
    input_files.push_back(input_ref);
  }

  std::error_code EC;
  llvm::raw_fd_ostream wrapper_out(WrapperOutputFilename, EC);
  if (EC) {
    llvm::errs() << "Error opening output wrapper file: " << EC.message()
                 << "\n";
    return EC.value();
  }
  llvm::raw_fd_ostream syms_out(WrapperOutputFilename + ".syms", EC);
  if (EC) {
    llvm::errs() << "Error opening output syms file: " << EC.message() << "\n";
    return EC.value();
  }

  wrapper_out << "#define IA2_WRAPPER\n";
  syms_out << "IA2 {\n"
           << "  global:\n";

  ASTMatchRefactorer refactorer(tool.getReplacements());
  FnPtrPrinter printer;
  FnDecl decl_replacement(wrapper_out, syms_out, tool.getReplacements(),
                          input_files);
  // refactorer.addMatcher(fn_ptr_matcher, &printer);
  refactorer.addMatcher(fn_decl_matcher, &decl_replacement);

  auto rc = tool.runAndSave(newFrontendActionFactory(&refactorer).get());

  syms_out << "};\n";
  return rc;
}
