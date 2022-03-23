#include "DetermineAbi.h"
#include "GenCallAsm.h"
#include "clang/AST/AST.h"
#include "clang/AST/GlobalDecl.h"
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
#include <fstream>
#include <map>

using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace std::string_literals;

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

// Specifies the compartment's protection key index, if any.
static llvm::cl::opt<uint32_t>
    CompartmentPkey("compartment-pkey",
                   llvm::cl::desc("The compartment's protection key index"),
                   llvm::cl::cat(HeaderRewriterCategory), llvm::cl::Optional);

// Headers with both functions and type definitions may be shared between
// compartments. If a shared header declares functions that do not belong to the
// compartment we are generating a wrapper for, it must be blacklisted to avoid
// adding incorrect .symver statements.
static llvm::cl::list<std::string>
    HeaderBlacklist("header-blacklist",
                    llvm::cl::desc("Headers which are only needed for types"),
                    llvm::cl::cat(HeaderRewriterCategory), llvm::cl::Optional);

// For types that have both a left and right side, this is what
// we emit for the name between the two sides, e.g.,
// int (*$$$IA2_PLACEHOLDER$$$)(int).
// We can replace this placeholder with any identifier to produce
// a new variable with that same type.
static const std::string kTypePlaceHolder = "$$$IA2_PLACEHOLDER$$$";

// Prefix we prepend to each rewritten function pointer type
static const std::string kFnPtrTypePrefix = "struct IA2_fnptr_";

// Convert a QualType to a string.
static std::string type_string(clang::QualType ty) {
  std::string result;
  llvm::raw_string_ostream os{result};
  ty.print(os, clang::LangOptions());
  return result;
}

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
  clang::ASTContext &ctx = decl->getASTContext();
  std::unique_ptr<clang::MangleContext> mctx{
      clang::ItaniumMangleContext::create(ctx, ctx.getDiagnostics())};
  std::string os;
  llvm::raw_string_ostream out{os};
#if CLANG_VERSION_MAJOR <= 10
  mctx->mangleName(decl, out);
#else
  mctx->mangleName(clang::GlobalDecl(decl), out);
#endif
  return os;
}

static std::string get_expansion_filename(const clang::Decl *decl,
                                          const clang::SourceManager *sm) {
  if (!decl || !sm) {
    return "";
  }
  return sm->getFilename(sm->getExpansionLoc(decl->getLocation())).str();
}

static bool is_blacklisted(llvm::StringRef header_name) {
  if (header_name.empty()) {
    return false;
  }
  return std::find_if(HeaderBlacklist.begin(), HeaderBlacklist.end(),
                      [&](std::string &s) {
                        return header_name.endswith(s);
                      }) != HeaderBlacklist.end();
}

class FnDecl : public RefactoringCallback {
public:
  FnDecl(llvm::raw_ostream &WrapperOut, llvm::raw_ostream &SymsOut,
         std::map<std::string, Replacements> &FileReplacements)
      : WrapperOut(WrapperOut), SymsOut(SymsOut),
        FileReplacements(FileReplacements) {}

  virtual void run(const MatchFinder::MatchResult &Result) {
    const clang::FunctionDecl *fn_decl =
        Result.Nodes.getNodeAs<clang::FunctionDecl>("exportedFn");
    if (fn_decl == nullptr) {
      // Type-cast failed, so we don't care to handle this decl
      return;
    }

    // This is an absolute path to the header with the fn decl
    std::string header_name =
        get_expansion_filename(fn_decl, Result.SourceManager);

    // Avoid wrapping functions declared in system headers or blacklisted
    // headers
    if (llvm::StringRef(header_name).startswith("/usr/") ||
        is_blacklisted(header_name)) {
      return;
    }
    auto header_ref_result =
        Result.SourceManager->getFileManager().getFileRef(header_name);
    if (auto err = header_ref_result.takeError()) {
      llvm::errs() << "Error getting FileEntryRef for '" << header_name
                   << "' ("
                   << fn_decl->getLocation().printToString(
                          *Result.SourceManager)
                   << "): " << err << '\n';
      return;
    }
    clang::FileEntryRef header_ref = *header_ref_result;
    auto fn_name = fn_decl->getNameInfo().getAsString();

    // Calls to compiler builtins produce an inline declaration that should
    // not be wrapped; we also don't want to wrap explicit decls of builtins
    if (fn_decl->getBuiltinID() != 0) {
      return;
    }

    // Deleting variadic functions from the rewritten header for now
    // See https://github.com/immunant/IA2-Phase2/issues/18
    if (fn_decl->isVariadic()) {
      // Make sure to include any macro expansions in the SourceRange being
      // rewritten
      clang::CharSourceRange expansion_range =
          Result.SourceManager->getExpansionRange(fn_decl->getSourceRange());
      Replacement decl_replacement{*Result.SourceManager, expansion_range,
                                   ""};

      auto err = FileReplacements[header_name].add(decl_replacement);
      if (err) {
        llvm::errs() << "Error adding replacement: " << err << '\n';
        return;
      } else {
        llvm::errs() << "Warning: deleting variadic function "
                     << fn_decl->getNameAsString() << '\n';
      }
      return;
    }

    // This callback may find a fn decl multiple times so only wrap it the
    // first time it's encountered in an input header
    if (!functionIsWrapped(fn_decl)) {
      WrappedFunctions.push_back(mangle_name(fn_decl));

      if (!isInitialized(header_ref)) {
        if (addHeaderImport(header_name, OutputHeader)) {
          return;
        }
        InitializedHeaders.push_back(header_ref);
      }

      std::string wrapper_macro = "IA2_WRAP_FUNCTION(" + fn_name + ");\n";
      clang::SourceLocation expansion_loc =
          Result.SourceManager->getExpansionLoc(fn_decl->getBeginLoc());
      Replacement decl_replacement{*Result.SourceManager, expansion_loc, 0,
                                   wrapper_macro};

      auto err = FileReplacements[header_name].add(decl_replacement);
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

        auto param_type = p->getOriginalType();
        if (param_type->isFunctionPointerType()) {
          // Parameter is a function pointer, so we need to rewrite it
          // into the internal mangled structure type
          auto mangled_type =
              mangle_type(fn_decl->getASTContext(), param_type);
          auto param_decl = llvm::formatv("{0}{1} {2}", kFnPtrTypePrefix,
                                          mangled_type, name);
          param_decls.append(param_decl);
        } else {
          auto param_type_string = type_string_with_placeholder(param_type);
          param_decls.append(
              replace_type_placeholder(param_type_string, name));
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

      auto cAbiSig = determineAbiForDecl(*fn_decl);
      std::string asm_wrapper;
      if (CompartmentPkey.getNumOccurrences() == 0) {
        asm_wrapper = emit_asm_wrapper(cAbiSig, fn_name, WrapperKind::Direct, "UNTRUSTED"s);
      } else {
        asm_wrapper = emit_asm_wrapper(cAbiSig, fn_name, WrapperKind::Direct, std::to_string(CompartmentPkey));
      }

      // Generate wrapper symbol definition, invoking call gates around call
      WrapperOut << llvm::formatv(
          "// {0}({1});\n"
          "asm(\n"
          "{2}\n"
          ");\n\n",
          replace_type_placeholder(ret_type_string, wrapper_name),
          param_decls, asm_wrapper);

      SymsOut << "    " << wrapper_name << ";\n";
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

  bool addHeaderImport(llvm::StringRef Filename,
                       llvm::cl::opt<std::string> &OutputHeader) {
    std::string include = "#include <ia2.h>\n";
    if (!OutputHeader.empty()) {
      auto include_output_header =
          llvm::formatv("#include \"{0}\"\n", OutputHeader);
      include.append(include_output_header);
    }
    auto err = FileReplacements[Filename.str()].add(
        Replacement(Filename, 0, 0, include));
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

  // The list of parameter types, e.g. "void f(int a, float b)" would be
  // ["int", "float"]
  std::vector<std::string> parameter_types;

  CAbiSignature sig;
};

class FnPtrPrinter : public RefactoringCallback {
public:
  FnPtrPrinter(std::map<std::string, Replacements> &FileReplacements)
      : FileReplacements(FileReplacements) {}
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
      std::string header_name =
          get_expansion_filename(old_decl, Result.SourceManager);
      if (llvm::StringRef(header_name).startswith("/usr/")) {
        return;
      }
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

      clang::CharSourceRange expansion_range =
          Result.SourceManager->getExpansionRange(old_decl->getSourceRange());
      Replacement r{*Result.SourceManager, expansion_range, new_decl};
      auto err = FileReplacements[header_name].add(r);
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
          fi.parameter_types.push_back(type_string(param_type));
        }
        fi.sig = determineAbiForProtoType(*fpt, old_decl->getASTContext());

        m_function_info.insert({mangled_type, fi});
      }
    }
  }

  const std::map<std::string, FunctionInfo> &function_info() const {
    return m_function_info;
  }

private:
  std::map<std::string, FunctionInfo> m_function_info;
  std::map<std::string, Replacements> &FileReplacements;
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
  // The rewritten headers include the output header to avoid having to make
  // changes to the user's source code. However, the wrapper source (which
  // defines a stack for direct calls) also has to include the rewritten
  // header since it may define types used in the wrapper function
  // declarations. Since we currently don't allocate stacks at init (see #67),
  // we need to define a separate stack for indirect calls in the output
  // header and the ifndef is necessary since the wrapper source also
  // indirectly includes the output header.
  // TODO: Remove this ifndef when #67 gets fixed to make the code easier to follow.
  // TODO: Remove the IA2_SHARED_DATA attribute on the stack (#68).
  os << "#ifndef IA2_WRAPPER\n"
    << "static char untrusted_stack[8 * 1024 * 1024] __attribute__((aligned(16))) __attribute__((used)) IA2_SHARED_DATA;\n"
    << "static void* ia2_untrusted_stackptr __attribute__((used)) IA2_SHARED_DATA = &untrusted_stack[4 * 1024 * 1024];\n"
    << "static void* ia2_trusted_stackptr __attribute__((used)) IA2_SHARED_DATA;\n"
    << "#endif\n";

  for (auto &p : printer.function_info()) {
    auto &mangled_type = p.first;
    auto &fi = p.second;

    os << fi.new_type << " { char *ptr; };\n";

    os << "#define IA2_FNPTR_TYPE_" << mangled_type << " ";
    if (!fi.return_type.empty()) {
      os << replace_type_placeholder(fi.return_type, "(*)");
    } else {
      os << "void(*)";
    }
    os << "(" << llvm::join(fi.parameter_types, ", ") << ")\n";

    // IA2_FNPTR_WRAPPER_* takes a function pointer and returns an opaque type
    // which we can pass to other compartments. This means that the wrapper will
    // be called from an untrusted compartment.
    os << "#define IA2_FNPTR_WRAPPER_" << mangled_type << "(target, caller_pkey, target_pkey) \\\n";
    // target_pkey is the macro param defining the callee's pkey
    auto wrapper_from_trusted = emit_asm_wrapper(fi.sig, fi.new_type, WrapperKind::IndirectFromUntrusted, "target_pkey"s);
    os << wrapper_from_trusted <<  "\n";

    // IA2_FNPTR_UNWRAPPER_* takes an opaque pointer and returns a function
    // pointer so the wrapper will be called from the trusted compartment.
    os << "#define IA2_FNPTR_UNWRAPPER_" << mangled_type << "(target, caller_pkey, target_pkey) \\\n";
    // target_pkey is the macro param defining the callee's pkey
    auto wrapper_from_untrusted = emit_asm_wrapper(fi.sig, fi.new_type, WrapperKind::IndirectFromTrusted, "target_pkey"s);
    os << wrapper_from_untrusted <<  "\n";
  }

  return EXIT_SUCCESS;
}

int main(int argc, const char **argv) {
#if LLVM_VERSION_MAJOR >= 13
  auto parser_ptr =
      CommonOptionsParser::create(argc, argv, HeaderRewriterCategory);
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

  wrapper_out << "#include <ia2.h>\n"
              << "#ifndef CALLER_PKEY\n"
              << "#error CALLER_PKEY must be defined to compile this file\n"
              << "#endif\n";

  // Add dummy definitions of the untrusted stack, trusted TLS storage
  // for stack pointers, and the __libia2_scrub_registers function
  wrapper_out
      << "static char untrusted_stack[8 * 1024 * 1024] __attribute__((aligned(16))) __attribute__((used));\n"
      << "static void* ia2_untrusted_stackptr __attribute__((used)) = &untrusted_stack[4 * 1024 * 1024];\n"
      << "static void* ia2_trusted_stackptr __attribute__((used));\n";
  syms_out << "IA2 {\n"
           << "  global:\n";

  ASTMatchRefactorer refactorer(tool.getReplacements());
  FnPtrPrinter printer(tool.getReplacements());
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
