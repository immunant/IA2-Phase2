#include "DetermineAbi.h"
#include "GenCallAsm.h"
#include "TypeOps.h"
#include "clang/AST/AST.h"
#include "clang/AST/Type.h"
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
// compartment we are generating a wrapper for, it must be marked as shared to
// avoid adding incorrect .symver statements.
static llvm::cl::list<std::string>
    SharedHeaders("shared-headers",
                  llvm::cl::desc("Headers which are only needed for types"),
                  llvm::cl::cat(HeaderRewriterCategory), llvm::cl::Optional);

// Limit wrapped functions to those whose names are in the given file.
static llvm::cl::opt<std::string> FunctionAllowlistFilename(
    "function-allowlist",
    llvm::cl::desc("File containing list of function names to wrap"),
    llvm::cl::cat(HeaderRewriterCategory), llvm::cl::Optional);

static std::vector<std::string> FunctionAllowlist;

// Skip outputting a .c file with wrappers
static llvm::cl::opt<bool> OmitWrappers(
    "omit-wrappers",
    llvm::cl::desc("Do not emit a source file containing function wrappers"),
    llvm::cl::cat(HeaderRewriterCategory), llvm::cl::Optional);

static std::string get_expansion_filename(const clang::Decl *decl) {
  if (!decl) {
    return "";
  }
  const auto &sm = decl->getASTContext().getSourceManager();
  return sm.getFilename(sm.getExpansionLoc(decl->getLocation())).str();
}

static bool skip_fn_decls(llvm::StringRef header_name) {
  if (header_name.empty()) {
    return false;
  }
  return std::find_if(SharedHeaders.begin(), SharedHeaders.end(),
                      [&](std::string &s) {
                        return header_name.endswith(s);
                      }) != SharedHeaders.end();
}

static bool
replace_decl(const clang::Decl *decl, const std::string &replacement,
             std::map<std::string, Replacements> &FileReplacements) {
  const auto &sm = decl->getASTContext().getSourceManager();
  std::string header_name = get_expansion_filename(decl);
  // Make sure to include any macro expansions in the SourceRange being replaced
  clang::CharSourceRange expansion_range =
      sm.getExpansionRange(decl->getSourceRange());
  Replacement r{sm, expansion_range, replacement};
  auto err = FileReplacements[header_name].add(r);
  if (err) {
    llvm::errs() << "Error adding replacement: " << err << '\n';
    return false;
  }
  return true;
}

struct FunctionWrapper {
  std::string name;
  std::string mangled_name;
  std::string definition;

  FunctionWrapper(const clang::FunctionDecl *fn_decl) {
    auto fn_name = fn_decl->getNameInfo().getAsString();

    auto ret_type = fn_decl->getReturnType();
    if (ret_type->isFunctionPointerType() && !ret_type->getAs<clang::TypedefType>()) {
      auto &sm = fn_decl->getASTContext().getSourceManager();
      llvm::errs() << "Function that returns a non-typedefed function pointer"
                      " is not supported, location:"
                   << fn_decl->getSourceRange().printToString(sm) << "\n";
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
        auto mangled_type = mangle_type(fn_decl->getASTContext(), param_type);
        auto param_decl =
            llvm::formatv("{0}{1} {2}", kFnPtrTypePrefix, mangled_type, name);
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

    auto cAbiSig = determineAbiForDecl(*fn_decl);
    // Default to 0 if --compartment-pkey is not passed
    auto PkeyValue =
        (CompartmentPkey.getNumOccurrences() == 0) ? 0 : CompartmentPkey;
    std::string asm_wrapper = emit_asm_wrapper(
        cAbiSig, fn_name, WrapperKind::Direct, std::to_string(PkeyValue));

    // Generate wrapper symbol definition, invoking call gates around call
    this->name = fn_name;
    this->mangled_name = mangle_name(fn_decl);
    this->definition = llvm::formatv(
        "// {0}({1});\n"
        "asm(\n"
        "{2}\n"
        ");\n\n",
        replace_type_placeholder(ret_type_string, this->name), param_decls,
        asm_wrapper);
  }
};

class FnDecl : public RefactoringCallback {
public:
  FnDecl(ASTMatchRefactorer &refactorer,
         std::map<std::string, Replacements> &FileReplacements)
      : FileReplacements(FileReplacements) {
    // Bind to all function declarations
    DeclarationMatcher fn_decl_matcher =
        functionDecl(unless(isDefinition())).bind("exportedFn");
    refactorer.addMatcher(fn_decl_matcher, this);
  }

  virtual void run(const MatchFinder::MatchResult &Result) {
    const clang::FunctionDecl *fn_decl =
        Result.Nodes.getNodeAs<clang::FunctionDecl>("exportedFn");
    if (fn_decl == nullptr) {
      // Type-cast failed, so we don't care to handle this decl
      return;
    }

    // This is an absolute path to the header with the fn decl
    std::string header_name = get_expansion_filename(fn_decl);

    // Avoid wrapping functions declared in system headers or shared headers
    if (llvm::StringRef(header_name).startswith("/usr/") ||
        skip_fn_decls(header_name)) {
      return;
    }

    // Calls to compiler builtins produce an inline declaration that should
    // not be wrapped; we also don't want to wrap explicit decls of builtins
    if (fn_decl->getBuiltinID() != 0) {
      return;
    }

    if (!functionShouldBeWrapped(fn_decl)) {
      return;
    }

    // If we're emitting wrappers, we should remove declarations of functions
    // present in the library that we would wrap but cannot presently. The only
    // remaining class of functions for which this holds is variadics; see:
    // https://github.com/immunant/IA2-Phase2/issues/18
    if (fn_decl->isVariadic()) {
      llvm::errs() << "Warning: not wrapping variadic function "
                   << fn_decl->getNameAsString() << '\n';
      return;
    }

    // This callback may find a fn decl multiple times so only wrap it the
    // first time it's encountered in an input header
    if (functionIsWrapped(fn_decl)) {
      return;
    }

    // At this point we know we need to wrap this function

    // Get a reference to the header file so we can ensure we add our headers to
    // its imports only the first time we modify it
    auto header_ref_result =
        Result.SourceManager->getFileManager().getFileRef(header_name);
    if (auto err = header_ref_result.takeError()) {
      llvm::errs() << "Error getting FileEntryRef for '" << header_name << "' ("
                   << fn_decl->getLocation().printToString(
                          *Result.SourceManager)
                   << "): " << err << '\n';
      return;
    }
    clang::FileEntryRef header_ref = *header_ref_result;

    // Add ia2.h and the output header to the header being rewritten
    if (!isInitialized(header_ref)) {
      if (addHeaderImport(header_name, OutputHeader)) {
        return;
      }
      InitializedHeaders.push_back(header_ref);
    }

    // Generate a wrapper for this function
    WrappedFunctions.push_back(FunctionWrapper(fn_decl));
  }

public:
  const std::vector<FunctionWrapper> &wrapped_functions() {
    return WrappedFunctions;
  }

private:
  std::map<std::string, Replacements> &FileReplacements;
  // Headers that have included the IA2 header
  std::vector<clang::FileEntryRef> InitializedHeaders;
  // The wrappers for functions that have been wrapped so far.
  std::vector<FunctionWrapper> WrappedFunctions;

  bool functionIsWrapped(const clang::FunctionDecl *fn_decl) {
    auto mangled = mangle_name(fn_decl);
    return std::find_if(WrappedFunctions.begin(), WrappedFunctions.end(),
                        [&](const auto &wrapped) {
                          return wrapped.mangled_name == mangled;
                        }) != WrappedFunctions.end();
  }

  bool functionShouldBeWrapped(const clang::FunctionDecl *fn_decl) {
    if (OmitWrappers) {
      return false;
    }
    if (FunctionAllowlistFilename.empty()) {
      return true;
    }
    return std::find(FunctionAllowlist.begin(), FunctionAllowlist.end(),
                     fn_decl->getName().str()) != FunctionAllowlist.end();
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

static std::string append_name_if_nonempty(const std::string &new_type,
                                           const std::string &name) {
  return new_type + (name.empty() ? "" : " ") + name;
};

class FnPtrPrinter : public RefactoringCallback {
public:
  FnPtrPrinter(ASTMatchRefactorer &refactorer,
               std::map<std::string, Replacements> &FileReplacements)
      : FileReplacements(FileReplacements) {
    TypeMatcher fn_ptr_matcher =
        pointerType(pointee(ignoringParens(functionType())));

    // Bind to declarations of fn ptrs in parameters, fields, and typedefs
    DeclarationMatcher fn_ptr_param_matcher =
        parmVarDecl(hasType(fn_ptr_matcher)).bind("fnPtrParam");
    DeclarationMatcher fn_ptr_field_matcher =
        fieldDecl(hasType(fn_ptr_matcher)).bind("fnPtrField");
    DeclarationMatcher fn_ptr_typedef_matcher =
        typedefNameDecl(hasType(fn_ptr_matcher)).bind("fnPtrTypedef");
    refactorer.addMatcher(fn_ptr_param_matcher, this);
    refactorer.addMatcher(fn_ptr_field_matcher, this);
    refactorer.addMatcher(fn_ptr_typedef_matcher, this);
  }
  virtual void run(const MatchFinder::MatchResult &Result) {
    const clang::Decl *old_decl = nullptr;
    clang::QualType old_type;
    std::string mangled_type;
    std::function<std::string(std::string &, std::string &)> generate_decl;
    // Depending on which matcher hit, determine what the fn ptr type is and how
    // to make a new, wrapped declaration of the same kind from it
    if (auto *parm_var_decl =
            Result.Nodes.getNodeAs<clang::ParmVarDecl>("fnPtrParam")) {
      old_decl = llvm::cast<clang::Decl>(parm_var_decl);
      old_type = parm_var_decl->getOriginalType();
      generate_decl = append_name_if_nonempty;
    } else if (auto *field_decl =
                   Result.Nodes.getNodeAs<clang::FieldDecl>("fnPtrField")) {
      old_decl = llvm::cast<clang::Decl>(field_decl);
      old_type = field_decl->getType();
      generate_decl = append_name_if_nonempty;
    } else if (auto *typedef_decl =
                   Result.Nodes.getNodeAs<clang::TypedefDecl>("fnPtrTypedef")) {
      old_decl = llvm::cast<clang::Decl>(typedef_decl);
      old_type = typedef_decl->getUnderlyingType();
      generate_decl = [](const auto &new_type, const auto &name) {
        return "typedef "s + new_type + ' ' + name;
      };
    } else if (auto *type_alias_decl =
                   Result.Nodes.getNodeAs<clang::TypeAliasDecl>(
                       "fnPtrTypedef")) {
      old_decl = llvm::cast<clang::Decl>(type_alias_decl);
      old_type = typedef_decl->getUnderlyingType();
      generate_decl = [](const auto &new_type, const auto &name) {
        return "using "s + name + " = " + new_type;
      };
    }

    if (old_decl != nullptr) {
      mangled_type = mangle_type(old_decl->getASTContext(), old_type);
      std::string new_type = kFnPtrTypePrefix + mangled_type;
      std::string name =
          llvm::cast<clang::NamedDecl>(old_decl)->getName().str();
      std::string new_decl = generate_decl(new_type, name);

      std::string header_name = get_expansion_filename(old_decl);
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

      // Replace the old decl with the new one
      if (!replace_decl(old_decl, new_decl, FileReplacements)) {
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

static std::string generate_output_header(
    const std::map<std::string, FunctionInfo> &FunctionInfo) {
  std::string result;
  llvm::raw_string_ostream os{result};

  os << "#pragma once\n";

  for (auto &p : FunctionInfo) {
    auto &mangled_type = p.first;
    auto &fi = p.second;

    os << fi.new_type << "_inner_t;\n";
    os << fi.new_type << " { " << fi.new_type << "_inner_t *ptr; };\n";

    os << "#define IA2_FNPTR_TYPE_" << mangled_type << " ";
    if (!fi.return_type.empty()) {
      os << replace_type_placeholder(fi.return_type, "(*)");
    } else {
      os << "void(*)";
    }
    os << "(" << llvm::join(fi.parameter_types, ", ") << ")\n";

    // IA2_DEFINE_WRAPPER_* takes a function name and declares a wrapped
    // function which we can be used to initialize a function pointer
    os << "#define IA2_DEFINE_WRAPPER_" << mangled_type
       << "(target, caller_pkey, target_pkey) \\\n";
    // This argument must be a valid asm identifier for direct calls
    auto direct_wrapper =
        emit_asm_wrapper(fi.sig, "target"s, WrapperKind::Direct, "target_pkey"s,
                         true /* as_macro */);
    os << direct_wrapper << "\n";

    // IA2_CALL_* takes an opaque pointer and returns a function
    // pointer so the wrapper will be called from the trusted compartment.
    os << "#define IA2_CALL_" << mangled_type
       << "(target, ty, caller_pkey, target_pkey) \\\n";
    // target_pkey is the macro param defining the callee's pkey
    auto wrapper_from_untrusted = emit_asm_wrapper(
        fi.sig, fi.new_type, WrapperKind::Indirect, "target_pkey"s);
    os << wrapper_from_untrusted << "\n";
  }

  return result;
}

static void
emit_wrappers(llvm::raw_ostream &WrapperOut, llvm::raw_ostream &ArgsOut,
              const std::vector<FunctionWrapper> &WrappedFunctions) {
  WrapperOut << "#include <ia2.h>\n"
             << "#ifndef CALLER_PKEY\n"
             << "#error CALLER_PKEY must be defined to compile this file\n"
             << "#endif\n";

  for (const auto &wrapper : WrappedFunctions) {
    // Add the wrapper function definition to the wrapper file
    WrapperOut << wrapper.definition;

    // Add the wrapper to the list of symbols to redirect
    ArgsOut << "--wrap=" << wrapper.name << "\n";
  }
}

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
  FnPtrPrinter printer(refactorer, tool.getReplacements());
  FnDecl decl_replacement(refactorer, tool.getReplacements());

  auto rc = tool.runAndSave(newFrontendActionFactory(&refactorer).get());

  if (!OmitWrappers) {
    std::error_code EC;
    llvm::raw_fd_ostream wrapper_out(WrapperOutputFilename, EC);
    if (EC) {
      llvm::errs() << "Error opening output wrapper file: " << EC.message()
                   << "\n";
      return EC.value();
    }
    llvm::raw_fd_ostream args_out(WrapperOutputFilename + ".args", EC);
    if (EC) {
      llvm::errs() << "Error opening output arguments file: " << EC.message()
                   << "\n";
      return EC.value();
    }
    emit_wrappers(wrapper_out, args_out, decl_replacement.wrapped_functions());
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

    os << generate_output_header(printer.function_info());
  }

  return EXIT_SUCCESS;
}
