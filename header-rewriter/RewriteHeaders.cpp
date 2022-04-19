#include "DetermineAbi.h"
#include "GenCallAsm.h"
//#include "RewriteHeaders.h"
#include "TypeOps.h"
#include "clang/AST/AST.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/RefactoringCallbacks.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/raw_ostream.h"

extern llvm::cl::list<std::string> SharedHeaders;
extern llvm::cl::opt<uint32_t> CompartmentPkey;
extern std::vector<std::string> FunctionAllowlist;

extern llvm::cl::opt<std::string> OutputHeader;
extern llvm::cl::opt<bool> OmitWrappers;
extern llvm::cl::opt<std::string> FunctionAllowlistFilename;

using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace std::string_literals;

static std::string get_expansion_filename(const clang::Decl *decl,
                                          const clang::SourceManager *sm) {
  if (!decl || !sm) {
    return "";
  }
  return sm->getFilename(sm->getExpansionLoc(decl->getLocation())).str();
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

struct FunctionWrapper {
  std::string name;
  std::string mangled_name;
  std::string definition;

  FunctionWrapper(const clang::FunctionDecl *fn_decl) {
    auto fn_name = fn_decl->getNameInfo().getAsString();

    auto ret_type = fn_decl->getReturnType();
    if (ret_type->isFunctionPointerType()) {
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
    this->name = "__ia2_" + fn_name;
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
    std::string header_name =
        get_expansion_filename(fn_decl, Result.SourceManager);

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
      // Make sure to include any macro expansions in the SourceRange being
      // rewritten
      clang::CharSourceRange expansion_range =
          Result.SourceManager->getExpansionRange(fn_decl->getSourceRange());
      Replacement decl_replacement{*Result.SourceManager, expansion_range, ""};

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

    // Insert a symbol versioning attribute prior to the function's decl
    // to redirect it to the wrapper
    auto fn_name = fn_decl->getNameInfo().getAsString();
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

    // Generate a wrapper for this function
    WrappedFunctions.push_back(FunctionWrapper(fn_decl));
  }

public:
  const std::vector<FunctionWrapper> &wrapped_functions() const {
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

std::shared_ptr<FnDecl>
createFnDecl(ASTMatchRefactorer &refactorer,
             std::map<std::string, Replacements> &FileReplacements) {
  return std::make_shared<FnDecl>(refactorer, FileReplacements);
}
const std::vector<FunctionWrapper> &getWrappedFunctions(const FnDecl &fn_decl) {
  return fn_decl.wrapped_functions();
}

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

std::shared_ptr<FnPtrPrinter>
createFnPtrPrinter(ASTMatchRefactorer &refactorer,
                   std::map<std::string, Replacements> &FileReplacements) {
  return std::make_shared<FnPtrPrinter>(refactorer, FileReplacements);
}
const std::map<std::string, FunctionInfo> &
getFunctionInfo(const FnPtrPrinter &printer) {
  return printer.function_info();
}

std::string generate_output_header(
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

    // IA2_WRAPPER_* takes a function name and declares a wrapped function
    // which we can be used to initialize a function pointer
    os << "#define IA2_WRAPPER_" << mangled_type
       << "(target, caller_pkey, target_pkey) \\\n";
    // This argument must be a valid asm identifier for direct calls
    auto direct_wrapper =
        emit_asm_wrapper(fi.sig, "target"s, WrapperKind::Direct, "target_pkey"s,
                         true /* as_macro */);
    os << direct_wrapper << "\n";

    // IA2_FNPTR_WRAPPER_* takes a function pointer and returns an opaque type
    // which we can pass to other compartments. This means that the wrapper will
    // be called from an untrusted compartment.
    os << "#define IA2_FNPTR_WRAPPER_" << mangled_type
       << "(target, caller_pkey, target_pkey) \\\n";
    // target_pkey is the macro param defining the callee's pkey
    auto wrapper_from_trusted =
        emit_asm_wrapper(fi.sig, fi.new_type,
                         WrapperKind::IndirectFromUntrusted, "target_pkey"s);
    os << wrapper_from_trusted << "\n";

    // IA2_FNPTR_UNWRAPPER_* takes an opaque pointer and returns a function
    // pointer so the wrapper will be called from the trusted compartment.
    os << "#define IA2_FNPTR_UNWRAPPER_" << mangled_type
       << "(target, caller_pkey, target_pkey) \\\n";
    // target_pkey is the macro param defining the callee's pkey
    auto wrapper_from_untrusted = emit_asm_wrapper(
        fi.sig, fi.new_type, WrapperKind::IndirectFromTrusted, "target_pkey"s);
    os << wrapper_from_untrusted << "\n";
  }

  return result;
}

void emit_wrappers(llvm::raw_ostream &WrapperOut, llvm::raw_ostream &SymsOut,
                   const std::vector<FunctionWrapper> &WrappedFunctions) {
  WrapperOut << "#include <ia2.h>\n"
             << "#ifndef CALLER_PKEY\n"
             << "#error CALLER_PKEY must be defined to compile this file\n"
             << "#endif\n";

  SymsOut << "IA2 {\n"
          << "  global:\n";

  for (const auto &wrapper : WrappedFunctions) {
    // Add the wrapper function definition to the wrapper file
    WrapperOut << wrapper.definition;

    // Add the wrapper to the list of symbols to redirect
    SymsOut << "    " << wrapper.name << ";\n";
  }

  SymsOut << "};\n";
}
