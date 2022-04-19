#include "RewriteHeaders.h"
#include "TypeOps.h"
#include "clang/AST/AST.h"
#include "clang/Basic/FileManager.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <fstream>
#include <map>

using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace std::string_literals;

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static llvm::cl::OptionCategory
    HeaderRewriterCategory("Header rewriter options");

llvm::cl::opt<std::string> OutputHeader("output-header",
                                        llvm::cl::desc("Output header file"),
                                        llvm::cl::cat(HeaderRewriterCategory),
                                        llvm::cl::Optional);

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

// Specifies the compartment's protection key index, if any.
llvm::cl::opt<uint32_t>
    CompartmentPkey("compartment-pkey",
                    llvm::cl::desc("The compartment's protection key index"),
                    llvm::cl::cat(HeaderRewriterCategory), llvm::cl::Optional);

// Headers with both functions and type definitions may be shared between
// compartments. If a shared header declares functions that do not belong to the
// compartment we are generating a wrapper for, it must be marked as shared to
// avoid adding incorrect .symver statements.
llvm::cl::list<std::string>
    SharedHeaders("shared-headers",
                  llvm::cl::desc("Headers which are only needed for types"),
                  llvm::cl::cat(HeaderRewriterCategory), llvm::cl::Optional);

// Limit wrapped functions to those whose names are in the given file.
llvm::cl::opt<std::string> FunctionAllowlistFilename(
    "function-allowlist",
    llvm::cl::desc("File containing list of function names to wrap"),
    llvm::cl::cat(HeaderRewriterCategory), llvm::cl::Optional);

std::vector<std::string> FunctionAllowlist;

// Skip outputting a .c file with wrappers
llvm::cl::opt<bool> OmitWrappers(
    "omit-wrappers",
    llvm::cl::desc("Do not emit a source file containing function wrappers"),
    llvm::cl::cat(HeaderRewriterCategory), llvm::cl::Optional);

int main(int argc, const char **argv) {
#if LLVM_VERSION_MAJOR >= 13
  auto parser_ptr =
      CommonOptionsParser::create(argc, argv, HeaderRewriterCategory);
  if (!parser_ptr) {
    auto error = parser_ptr.takeError();
    llvm::errs() << error;
    return 1;
  }
  CommonOptionsParser &options_parser = *parser_ptr;
#else
  CommonOptionsParser options_parser(argc, argv, HeaderRewriterCategory);
#endif

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
      llvm::errs() << "Error getting FileEntryRef for " << s << ": " << err
                   << '\n';
    }
    clang::FileEntryRef input_ref = *input_ref_result;
    input_files.push_back(input_ref);
  }

  // Load the allowlist of functions to wrap, if specified
  if (!FunctionAllowlistFilename.empty()) {
    // Forbid using --function-allowlist and --omit-wrappers together
    if (OmitWrappers) {
      llvm::errs()
          << "--function-allowlist and --omit-wrappers flags may not be "
             "specified together. The former indicates intent to wrap the "
             "listed functions while the latter indicates intent to generate "
             "no wrapper library at all.\n";
      return 1;
    }

    std::ifstream allowlist_file(FunctionAllowlistFilename);
    if (!allowlist_file) {
      llvm::errs() << "Could not open specified function allowlist file "
                   << FunctionAllowlistFilename << '\n';
      return 1;
    }
    std::string func_name;
    while (std::getline(allowlist_file, func_name)) {
      FunctionAllowlist.push_back(func_name);
    }
  }

  ASTMatchRefactorer refactorer(tool.getReplacements());
  auto printer(createFnPtrPrinter(refactorer, tool.getReplacements()));
  auto decl_replacement(createFnDecl(refactorer, tool.getReplacements()));

  auto rc = tool.runAndSave(newFrontendActionFactory(&refactorer).get());

  if (!OmitWrappers) {
    std::error_code EC;
    llvm::raw_fd_ostream wrapper_out(WrapperOutputFilename, EC);
    if (EC) {
      llvm::errs() << "Error opening output wrapper file: " << EC.message()
                   << "\n";
      return EC.value();
    }
    llvm::raw_fd_ostream syms_out(WrapperOutputFilename + ".syms", EC);
    if (EC) {
      llvm::errs() << "Error opening output syms file: " << EC.message()
                   << "\n";
      return EC.value();
    }
    emit_wrappers(wrapper_out, syms_out,
                  getWrappedFunctions(*decl_replacement));
  }

  if (rc != EXIT_SUCCESS) {
    return rc;
  }

  // Emit output header
  if (!OutputHeader.empty()) {
    std::error_code err;
    llvm::raw_fd_ostream os(OutputHeader, err, llvm::sys::fs::OF_Text);
    if (err) {
      llvm::errs() << "Error opening file " << OutputHeader << ": "
                   << err.message() << '\n';
      return EXIT_FAILURE;
    }

    os << generate_output_header(getFunctionInfo(*printer));
  }

  return EXIT_SUCCESS;
}
