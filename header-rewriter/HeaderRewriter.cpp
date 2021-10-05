#include <fstream>
#include <map>
#include "clang/AST/AST.h"
#include "clang/AST/Mangle.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/RefactoringCallbacks.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang::ast_matchers;
using namespace clang::tooling;

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static llvm::cl::OptionCategory
    HeaderRewriterCategory("Header rewriter options");

static llvm::cl::opt<std::string>
    OutputHeader("output-header", llvm::cl::desc("Output header file"),
                 llvm::cl::cat(HeaderRewriterCategory), llvm::cl::Optional);

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

// For types that have both a left and right side, this is what
// we emit for the name between the two sides, e.g.,
// int (*$$$IA2_PLACEHOLDER$$$)(int).
// We can replace this placeholder with any identifier to produce
// a new variable with that same type.
static const std::string kTypePlaceHolder = "$$$IA2_PLACEHOLDER$$$";

// Prefix we prepend to each rewritten function pointer type
static const std::string kFnPtrTypePrefix = "struct IA2_fnptr_";

// Convert a QualType to a string that contains kTypePlaceholder
static std::string type_string_with_placeholder(clang::QualType ty) {
  std::string result;
  llvm::raw_string_ostream os{result};
  ty.print(os, clang::LangOptions(), kTypePlaceHolder);
  return result;
}

// Replace kTypePlaceholder in a string produced by
// type_string_with_placeholder with an actual given name
template <typename T>
static std::string replace_type_placeholder(std::string s, const T &with) {
  auto placeholder_pos = s.find(kTypePlaceHolder);
  if (placeholder_pos != std::string::npos) {
    s.replace(placeholder_pos, kTypePlaceHolder.size(), with);
  }
  return s;
}

static std::string mangle_type(clang::ASTContext &ctx, clang::QualType ty) {
  std::unique_ptr<clang::MangleContext> mctx{
      clang::ItaniumMangleContext::create(ctx, ctx.getDiagnostics())};

  std::string os;
  llvm::raw_string_ostream out{os};
  mctx->mangleTypeName(ty.getCanonicalType(), out);
  return os;
}

static DeclarationMatcher fn_decl_matcher =
    functionDecl(unless(isDefinition())).bind("exportedFn");

static std::string mangle_name(const clang::FunctionDecl *decl) {
    clang::ASTContext& ctx = decl->getASTContext();
    std::unique_ptr<clang::MangleContext> mctx{
        clang::ItaniumMangleContext::create(ctx, ctx.getDiagnostics())};
    std::string os;
    llvm::raw_string_ostream out{os};
    mctx->mangleName(clang::GlobalDecl(decl), out);
    return os;
}

class FnDecl : public RefactoringCallback {
public:
  FnDecl(llvm::raw_ostream &WrapperOut, llvm::raw_ostream &SymsOut,
         std::map<std::string, Replacements> &FileReplacements)
      : WrapperOut(WrapperOut), SymsOut(SymsOut),
        FileReplacements(FileReplacements) {}

  virtual void run(const MatchFinder::MatchResult &Result) {
    if (const clang::FunctionDecl *fn_decl =
            Result.Nodes.getNodeAs<clang::FunctionDecl>("exportedFn")) {
      // This is an absolute path to the header with the fn decl
      llvm::StringRef header_name =
          Result.SourceManager->getFilename(fn_decl->getLocation());

      // Avoid wrapping functions declared in system headers or from macro expansions
      if (header_name.startswith("/usr/") || header_name.empty()) {
          return;
      }

      auto header_ref_result =
          Result.SourceManager->getFileManager().getFileRef(header_name);
      if (auto err = header_ref_result.takeError()) {
        llvm::errs() << "Error getting FileEntryRef: " << err << '\n';
        return;
      }
      clang::FileEntryRef header_ref = *header_ref_result;
      auto fn_name = fn_decl->getNameInfo().getAsString();

      // Deleting variadic functions from the rewritten header for now
      bool delete_decl = false;
      // See https://github.com/immunant/IA2-Phase2/issues/18
      if (fn_decl->isVariadic()) {
          delete_decl = true;
      }
      // Also skipping functions with va_list arguments
      for (auto &p : fn_decl->parameters()) {
          // TODO: Find a better way to check for `va_list` if this becomes necessary
          if (!p->getType().getAsString().compare("struct __va_list_tag *")) {
              delete_decl = true;
              break;
          }
      }
      if (delete_decl) {
          // Make sure to include any macro expansions in the SourceRange being rewritten
          clang::CharSourceRange expansion_range = Result.SourceManager->getExpansionRange(fn_decl->getSourceRange());
          Replacement decl_replacement{*Result.SourceManager, expansion_range, ""};

          auto err = FileReplacements[header_name.str()].add(decl_replacement);
          if (err) {
            llvm::errs() << "Error adding replacement: " << err << '\n';
            return;
          }
          return;
      }

      // This callback may find a fn decl multiple times so only wrap it the
      // first time it's encountered in an input header
      if (!functionIsWrapped(fn_decl)) {
          WrappedFunctions.push_back(mangle_name(fn_decl));

        if (!isInitialized(header_ref)) {
          if (addHeaderImport(header_name)) {
            return;
          }
          if (!OutputHeader.empty()) {
            WrapperOut << llvm::formatv("#include \"{0}\"\n", OutputHeader);
          }
          WrapperOut << llvm::formatv("#include \"{0}\"\n", header_name);
          InitializedHeaders.push_back(header_ref);
        }

        std::string wrapper_macro = "IA2_WRAP_FUNCTION(" + fn_name + ");\n";
        clang::SourceLocation expansion_loc = Result.SourceManager->getExpansionLoc(fn_decl->getBeginLoc());
        Replacement decl_replacement{*Result.SourceManager, expansion_loc, 0, wrapper_macro};

        auto err = FileReplacements[header_name.str()].add(decl_replacement);
        if (err) {
          llvm::errs() << "Error adding replacement: " << err << '\n';
          return;
        }

        auto wrapper_name = "__ia2_" + fn_name;
        auto ret_type = fn_decl->getReturnType();
        if (ret_type->isFunctionPointerType()) {
          auto &sm = fn_decl->getASTContext().getSourceManager();
          llvm::errs() << "Function that returns a function pointer "
                          "is not supported, location:"
                       << fn_decl->getSourceRange().printToString(sm) << "\n";
          return;
        }

        std::string param_decls;
        std::string param_names;

        for (auto &p : fn_decl->parameters()) {
          if (!param_names.empty()) {
            param_decls.append(", ");
            param_names.append(", ");
          }
          auto name = p->getNameAsString();
          if (name.empty()) {
            auto n = &p - fn_decl->param_begin();
            name = llvm::formatv("__ia2_arg_{0}", n);
          }

          auto param_type = p->getType();
          if (param_type->isFunctionPointerType()) {
            // Parameter is a function pointer, so we need to rewrite it
            // into the internal mangled structure type
            auto mangled_type = mangle_type(fn_decl->getASTContext(), param_type);
            auto param_decl = llvm::formatv("{0}{1} {2}", kFnPtrTypePrefix,
                                            mangled_type, name);
            param_decls.append(param_decl);
          } else {
            auto param_type_string = type_string_with_placeholder(param_type);
            param_decls.append(replace_type_placeholder(param_type_string, name));
          }
          param_names.append(name);
        }

        auto ret_type_string = type_string_with_placeholder(ret_type);
        std::string ret_val;
        auto ret_stmt = "";
        if (!ret_type.isCForbiddenLValueType()) {
          auto ret_var = replace_type_placeholder(ret_type_string, "res");
          ret_val = ret_var + " = ";
          ret_stmt = "    return res;\n";
        }

        WrapperOut << llvm::formatv(
            "{0}({1}) {\n"
            "    // call_gate_push();\n"
            "    {2}{3}({4});\n"
            "    // call_gate_pop();\n"
            "{5}"
            "}\n",
            replace_type_placeholder(ret_type_string, wrapper_name),
            param_decls, ret_val, fn_name, param_names, ret_stmt);

        SymsOut << "    " << wrapper_name << ";\n";
      }
    }
  }

private:
  llvm::raw_ostream &WrapperOut;
  llvm::raw_ostream &SymsOut;
  std::map<std::string, Replacements> &FileReplacements;
  // Headers that have included the IA2 header
  std::vector<clang::FileEntryRef> InitializedHeaders;
  // The mangled names of all functions that have been wrapped so far.
  std::vector<std::string> WrappedFunctions;

  bool functionIsWrapped(const clang::FunctionDecl *fn_decl) {
      return std::find(WrappedFunctions.begin(), WrappedFunctions.end(),
        mangle_name(fn_decl)) != WrappedFunctions.end();
  }

  bool isInitialized(clang::FileEntryRef InputHeader) {
    return std::find(InitializedHeaders.begin(), InitializedHeaders.end(),
                     InputHeader) != InitializedHeaders.end();
  }

  bool addHeaderImport(llvm::StringRef Filename) {
    auto err = FileReplacements[Filename.str()].add(
        Replacement(Filename, 0, 0, "#include <ia2.h>\n"));
    if (err) {
      llvm::errs() << "Error adding ia2 header import: " << err << '\n';
      return true;
    }
    return false;
  }
};

static TypeMatcher fn_ptr_matcher =
    pointerType(pointee(ignoringParens(functionType())));
static DeclarationMatcher fn_ptr_param_matcher =
    parmVarDecl(hasType(fn_ptr_matcher)).bind("fnPtrParam");
static DeclarationMatcher fn_ptr_field_matcher =
    fieldDecl(hasType(fn_ptr_matcher)).bind("fnPtrField");
static DeclarationMatcher fn_ptr_typedef_matcher =
    typedefNameDecl(hasType(fn_ptr_matcher)).bind("fnPtrTypedef");

struct FunctionInfo {
  // The new type for this function pointer
  std::string new_type;

  // The return type of the function
  std::string return_type;

  // The list of parameters, e.g., "int a, int b"
  std::vector<std::string> parameters;
};

class FnPtrPrinter : public RefactoringCallback {
public:
  virtual void run(const MatchFinder::MatchResult &Result) {
    const clang::Decl *old_decl = nullptr;
    clang::QualType old_type;
    std::string mangled_type;
    std::string new_type = kFnPtrTypePrefix;
    std::string new_decl;
    if (auto *parm_var_decl =
            Result.Nodes.getNodeAs<clang::ParmVarDecl>("fnPtrParam")) {
      old_decl = llvm::cast<clang::Decl>(parm_var_decl);
      old_type = parm_var_decl->getOriginalType();
      mangled_type = mangle_type(old_decl->getASTContext(), old_type);

      new_type += mangled_type;
      new_decl = new_type;
      if (!parm_var_decl->getName().empty()) {
        new_decl += ' ';
        new_decl += parm_var_decl->getName().str();
      }
    } else if (auto *field_decl =
                   Result.Nodes.getNodeAs<clang::FieldDecl>("fnPtrField")) {
      old_decl = llvm::cast<clang::Decl>(field_decl);
      old_type = field_decl->getType();
      mangled_type = mangle_type(old_decl->getASTContext(), old_type);

      new_type += mangled_type;
      new_decl = new_type;
      if (!field_decl->getName().empty()) {
        new_decl += ' ';
        new_decl += field_decl->getName().str();
      }
    } else if (auto *typedef_decl =
                   Result.Nodes.getNodeAs<clang::TypedefDecl>("fnPtrTypedef")) {
      old_decl = llvm::cast<clang::Decl>(typedef_decl);
      old_type = typedef_decl->getUnderlyingType();
      mangled_type = mangle_type(old_decl->getASTContext(), old_type);

      new_type += mangled_type;
      new_decl = "typedef " + new_type + ' ' + typedef_decl->getName().str();
    } else if (auto *type_alias_decl =
                   Result.Nodes.getNodeAs<clang::TypeAliasDecl>(
                       "fnPtrTypedef")) {
      old_decl = llvm::cast<clang::Decl>(type_alias_decl);
      old_type = typedef_decl->getUnderlyingType();
      mangled_type = mangle_type(old_decl->getASTContext(), old_type);

      new_type += mangled_type;
      new_decl = "using " + typedef_decl->getName().str() + " = " + new_type;
    }

    if (old_decl != nullptr) {
      auto *fpt = old_type->castAs<clang::PointerType>()
                      ->getPointeeType()
                      ->getAsAdjusted<clang::FunctionProtoType>();
      if (fpt == nullptr) {
        auto &sm = old_decl->getASTContext().getSourceManager();
        llvm::errs() << "K&R function pointer is not supported, location:"
                     << old_decl->getSourceRange().printToString(sm) << "\n";
        return;
      }
      if (fpt->isVariadic()) {
        auto &sm = old_decl->getASTContext().getSourceManager();
        llvm::errs() << "Variadic function pointer is not supported, location: "
                     << old_decl->getSourceRange().printToString(sm) << "\n";
        return;
      }

      Replacement r{*Result.SourceManager, old_decl, new_decl};
      auto err = Replace.add(r);
      if (err) {
        llvm::errs() << "Error adding replacement: " << err << '\n';
        return;
      }

      if (m_function_info.find(mangled_type) == m_function_info.end()) {
        FunctionInfo fi;
        fi.new_type = std::move(new_type);

        auto return_type = fpt->getReturnType();
        if (!return_type.isCForbiddenLValueType()) {
          fi.return_type = type_string_with_placeholder(return_type);
        }

        for (auto param_type : fpt->param_types()) {
          auto s = type_string_with_placeholder(param_type);
          auto i = fi.parameters.size();
          auto arg_name = llvm::formatv("__ia2_arg_{0}", i).str();
          fi.parameters.push_back(
              replace_type_placeholder(std::move(s), arg_name));
        }

        m_function_info.insert({mangled_type, fi});
      }
    }
  }

  const std::map<std::string, FunctionInfo> &function_info() const {
    return m_function_info;
  }

private:
  std::map<std::string, FunctionInfo> m_function_info;
};

static int emit_output_header(const FnPtrPrinter &printer) {
  if (OutputHeader.empty()) {
    return EXIT_SUCCESS;
  }

  std::error_code err;
  llvm::raw_fd_ostream os(OutputHeader, err, llvm::sys::fs::OF_Text);
  if (err) {
    llvm::errs() << "Error opening file " << OutputHeader << ": "
                 << err.message() << '\n';
    return EXIT_FAILURE;
  }

  os << "#pragma once\n";
  for (auto &p : printer.function_info()) {
    auto &mangled_type = p.first;
    auto &fi = p.second;

    os << fi.new_type << " { char *ptr; };\n";

    if (!fi.return_type.empty()) {
      std::string variable_type =
          replace_type_placeholder(fi.return_type, "__ia2_variable");
      os << "#define IA2_FNPTR_RETURN_" << mangled_type << "(__ia2_variable) "
         << variable_type << '\n';
    }

    std::string fn_sig;
    if (fi.return_type.empty()) {
      fn_sig = "void __ia2_target(" + llvm::join(fi.parameters, ", ") + ')';
    } else {
      // The arguments go right after the name inside the placeholder,
      // not at the end of the return type, e.g., for this declaration
      // int (*f(float))(char) {} float is the type of f's argument
      // and char is the type of the argument of the returned function
      auto fn_with_args =
          "__ia2_target(" + llvm::join(fi.parameters, ", ") + ')';
      fn_sig =
          replace_type_placeholder(fi.return_type, std::move(fn_with_args));
    }
    os << "#define IA2_FNPTR_WRAPPER_" << mangled_type << "(__ia2_target) "
       << fn_sig << '\n';

    os << "#define IA2_FNPTR_ARG_NAMES_" << mangled_type;
    for (size_t i = 0; i < fi.parameters.size(); i++) {
      if (i > 0) {
        os << ',';
      }
      os << " __ia2_arg_" << i;
    }
    os << '\n';
  }

  return EXIT_SUCCESS;
}

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
  FnDecl decl_replacement(wrapper_out, syms_out, tool.getReplacements());
  refactorer.addMatcher(fn_decl_matcher, &decl_replacement);
  refactorer.addMatcher(fn_ptr_param_matcher, &printer);
  refactorer.addMatcher(fn_ptr_field_matcher, &printer);
  refactorer.addMatcher(fn_ptr_typedef_matcher, &printer);

  auto rc = tool.runAndSave(newFrontendActionFactory(&refactorer).get());

  syms_out << "};\n";
  if (rc != EXIT_SUCCESS) {
    return rc;
  }

  rc = emit_output_header(printer);
  if (rc != EXIT_SUCCESS) {
    return rc;
  }

  return EXIT_SUCCESS;
}
