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
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include <fstream>
#include <optional>

using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace std::string_literals;

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static llvm::cl::OptionCategory
    SourceRewriterCategory("Source rewriter options");

static llvm::cl::opt<std::string>
    OutputFilename("output-filename", llvm::cl::Required,
                   llvm::cl::cat(SourceRewriterCategory),
                   llvm::cl::desc("<prefix for output files>"));

// Specifies the compartment's protection key index, if any.
static llvm::cl::opt<uint32_t>
    CompartmentPkey("compartment-pkey", llvm::cl::Required,
                    llvm::cl::desc("The compartment's protection key index"),
                    llvm::cl::cat(SourceRewriterCategory));

static std::string get_filename(const clang::Decl *decl) {
  assert(decl != nullptr);
  const auto &sm = decl->getASTContext().getSourceManager();
  return sm.getFilename(sm.getSpellingLoc(decl->getLocation())).str();
}

static std::string get_expansion_filename(const clang::Decl *decl) {
  assert(decl != nullptr);
  const auto &sm = decl->getASTContext().getSourceManager();
  return sm.getFilename(sm.getExpansionLoc(decl->getLocation())).str();
}

static bool ignore_file(const std::string &file_name) {
  auto file = llvm::StringRef(file_name);
  // TODO: Add a proper blacklist with libia2 headers in it by default
  return file.startswith("/usr/") || file.endswith("ia2.h") ||
         file.endswith("test_fault_handler.h");
}

static std::string append_name_if_nonempty(const std::string &new_type,
                                           const std::string &name) {
  return new_type + (name.empty() ? "" : " ") + name;
};

struct FnPtrTy {
  std::string fn_ptr;
  std::string opaque_struct;

  bool operator==(const FnPtrTy &other) const {
    return (this->fn_ptr == other.fn_ptr) &&
           (this->opaque_struct == other.opaque_struct);
  };
  bool operator<(const FnPtrTy &other) const {
    return (this->fn_ptr < other.fn_ptr) &&
           (this->opaque_struct < other.opaque_struct);
  };
};

// Replaces function pointer types with opaque structs specific to the
// function-signature of the pointee.
class FnPtrTypes : public RefactoringCallback {
public:
  FnPtrTypes(ASTMatchRefactorer &refactorer,
             std::map<std::string, Replacements> &file_replacements)
      : file_replacements(file_replacements) {
    TypeMatcher fn_ptr_matcher =
        pointerType(pointee(ignoringParens(functionType())));

    // Bind to declarations of fn ptrs in parameters, fields, typedefs and
    // variable declarations
    DeclarationMatcher fn_ptr_param_matcher =
        parmVarDecl(hasType(fn_ptr_matcher)).bind("fnPtrParam");
    DeclarationMatcher fn_ptr_field_matcher =
        fieldDecl(hasType(fn_ptr_matcher)).bind("fnPtrField");
    DeclarationMatcher fn_ptr_typedef_matcher =
        typedefNameDecl(hasType(fn_ptr_matcher)).bind("fnPtrTypedef");
    DeclarationMatcher fn_ptr_var_matcher =
        varDecl(hasType(fn_ptr_matcher)).bind("fnPtrVar");
    refactorer.addMatcher(fn_ptr_param_matcher, this);
    refactorer.addMatcher(fn_ptr_field_matcher, this);
    refactorer.addMatcher(fn_ptr_typedef_matcher, this);
    refactorer.addMatcher(fn_ptr_var_matcher, this);
  }
  virtual void run(const MatchFinder::MatchResult &result) {
    const clang::Decl *old_decl = nullptr;
    clang::QualType old_type;
    std::string mangled_type;
    std::function<std::string(std::string &, std::string &)> generate_decl;
    // fnPtrVar should only replace a subset of its source range in some cases.
    // In this case it assigns a value to this optional which is then used for
    // the replacement.
    std::optional<clang::SourceRange> range = std::nullopt;
    // Depending on which matcher hit, determine what the fn ptr type is and how
    // to make a new, wrapped declaration of the same kind from it
    if (auto *parm_var_decl =
            result.Nodes.getNodeAs<clang::ParmVarDecl>("fnPtrParam")) {
      old_decl = llvm::cast<clang::Decl>(parm_var_decl);
      old_type = parm_var_decl->getOriginalType();
      generate_decl = append_name_if_nonempty;
    } else if (auto *field_decl =
                   result.Nodes.getNodeAs<clang::FieldDecl>("fnPtrField")) {
      old_decl = llvm::cast<clang::Decl>(field_decl);
      old_type = field_decl->getType();
      generate_decl = append_name_if_nonempty;
    } else if (auto *typedef_decl =
                   result.Nodes.getNodeAs<clang::TypedefDecl>("fnPtrTypedef")) {
      old_decl = llvm::cast<clang::Decl>(typedef_decl);
      old_type = typedef_decl->getUnderlyingType();
      generate_decl = [](const auto &new_type, const auto &name) {
        return "typedef " + new_type + ' ' + name;
      };
    } else if (auto *type_alias_decl =
                   result.Nodes.getNodeAs<clang::TypeAliasDecl>(
                       "fnPtrTypedef")) {
      old_decl = llvm::cast<clang::Decl>(type_alias_decl);
      old_type = typedef_decl->getUnderlyingType();
      generate_decl = [](const auto &new_type, const auto &name) {
        return "using "s + name + " = " + new_type;
      };
    } else if (auto *var_decl =
                   result.Nodes.getNodeAs<clang::VarDecl>("fnPtrVar")) {
      old_decl = llvm::cast<clang::Decl>(var_decl);
      old_type = var_decl->getType();
      // When function pointer variables are initialized
      // (e.g. int(*ptr)(int) = INITIAL_VALUE;) the initial value will be part
      // of the SourceRange. Since FnPtrAssign may need to rewrite
      // INITIAL_VALUE, we reduce the SourceRange to only include the LHS.
      if (var_decl->hasInit()) {
        // VarDecl doesn't expose a way to get the SourceRange without the
        // initializer, so this is mostly taken from
        // DeclaratorDecl::getSourceRange
        auto start = var_decl->getOuterLocStart();
        // The type extends past the variable name so we need to manually set
        // the SourceRange end
        auto *ty_info = var_decl->getTypeSourceInfo();
        assert(ty_info != nullptr);
        auto end = ty_info->getTypeLoc().getSourceRange().getEnd();
        range = clang::SourceRange(start, end);
      }
      generate_decl = append_name_if_nonempty;
    }
    if (!range.has_value()) {
      range = old_decl->getSourceRange();
    }

    if (result.SourceManager == nullptr || result.Context == nullptr) {
      return;
    }
    if (old_decl == nullptr) {
      return;
    }
    const auto &sm = *(result.SourceManager);
    auto &ctxt = *(result.Context);
    mangled_type = mangle_type(ctxt, old_type);
    std::string new_type = kFnPtrTypePrefix + mangled_type;
    std::string name = llvm::cast<clang::NamedDecl>(old_decl)->getName().str();
    std::string new_decl = generate_decl(new_type, name);

    if (ignore_file(get_filename(old_decl))) {
      return;
    }
    // TODO: Factor out these checks and add them to the FnPtrAssign pass
    auto *fpt = old_type->castAs<clang::PointerType>()
                    ->getPointeeType()
                    ->getAsAdjusted<clang::FunctionProtoType>();
    if (fpt == nullptr) {
      llvm::errs() << "K&R function pointer is not supported, location:"
                   << old_decl->getSourceRange().printToString(sm) << "\n";
      return;
    }
    if (fpt->isVariadic()) {
      llvm::errs() << "Variadic function pointer is not supported, location: "
                   << old_decl->getSourceRange().printToString(sm) << "\n";
      return;
    }
    std::string file_name = get_expansion_filename(old_decl);

    needed_fn_ptr_types.insert(
        {.fn_ptr = old_type.getAsString(), .opaque_struct = new_type});

    fn_ptr_abi_sig[new_type] =
        determineAbiForProtoType(*fpt, old_decl->getASTContext());

    // Replace the old decl with the new one
    // Make sure to include any macro expansions in the SourceRange being
    // replaced
    clang::CharSourceRange expansion_range = sm.getExpansionRange(*range);
    Replacement r{sm, expansion_range, new_decl};
    auto err = file_replacements[file_name].add(r);
    if (err) {
      llvm::errs() << "Error adding replacement: " << err << '\n';
    }
    return;
  }

  // The set of function pointer types found
  std::set<FnPtrTy> needed_fn_ptr_types;
  std::map<std::string, CAbiSignature> fn_ptr_abi_sig;

private:
  std::map<std::string, Replacements> &file_replacements;
};

// Replaces function pointer comparisons
class FnPtrCall : public RefactoringCallback {
public:
  FnPtrCall(ASTMatchRefactorer &refactorer,
            std::map<std::string, Replacements> &file_replacements)
      : file_replacements(file_replacements) {
    auto is_direct_call = declRefExpr(hasDeclaration(functionDecl()));
    StatementMatcher fn_ptr_call = callExpr(callee(
        expr(unless(ignoringImplicit(is_direct_call))).bind("fnPtrCall")));

    refactorer.addMatcher(fn_ptr_call, this);
  }
  virtual void run(const MatchFinder::MatchResult &result) {
    if (auto *fn_ptr_call = result.Nodes.getNodeAs<clang::Expr>("fnPtrCall")) {
      if (result.SourceManager == nullptr || result.Context == nullptr) {
        return;
      }
      const auto &sm = *result.SourceManager;
      const auto &ctxt = *result.Context;

      clang::SourceLocation loc = fn_ptr_call->getExprLoc();
      if (ignore_file(sm.getFilename(sm.getSpellingLoc(loc)).str())) {
        return;
      }

      std::string new_expr;
      if (*sm.getCharacterData(loc) == '(') {
        new_expr = "IA2_CALL("s;
      } else {
        // TODO: This doesn't rewrite expressions like `expr->field(args)`
        // correctly
        auto *decl = fn_ptr_call->getReferencedDeclOfCallee();
        assert(decl != nullptr);
        auto ptr_identifier =
            llvm::cast<clang::NamedDecl>(decl)->getName().str();
        new_expr = "IA2_CALL("s + ptr_identifier + ")";
      }

      std::string file_name = sm.getFilename(sm.getExpansionLoc(loc)).str();
      clang::CharSourceRange expansion_range = sm.getExpansionRange(loc);
      Replacement r{sm, expansion_range, new_expr};
      auto err = file_replacements[file_name].add(r);
      if (err) {
        llvm::errs() << "Error adding replacement: " << err << '\n';
      }
      return;
    }
  }

private:
  std::map<std::string, Replacements> &file_replacements;
};

// Replaces function pointer assignments
class FnPtrAssign : public RefactoringCallback {
public:
  FnPtrAssign(ASTMatchRefactorer &refactorer,
              std::map<std::string, Replacements> &file_replacements)
      : file_replacements(file_replacements) {

    auto fn_expr =
        declRefExpr(hasDeclaration(functionDecl())).bind("fnPtrExpr");
    auto call_expr = hasAncestor(
        callExpr(callee(expr(ignoringImplicit(equalsBoundNode("fnPtrExpr"))))));
    StatementMatcher fn_ptr_expr = expr(fn_expr, unless(call_expr));

    refactorer.addMatcher(fn_ptr_expr, this);
  }
  virtual void run(const MatchFinder::MatchResult &result) {
    if (auto *fn_ptr_expr =
            result.Nodes.getNodeAs<clang::DeclRefExpr>("fnPtrExpr")) {
      if (result.SourceManager == nullptr || result.Context == nullptr) {
        return;
      }
      const auto &sm = *result.SourceManager;
      const auto &ctxt = *result.Context;

      clang::SourceLocation loc = fn_ptr_expr->getExprLoc();
      if (ignore_file(sm.getFilename(sm.getSpellingLoc(loc)).str())) {
        return;
      }

      auto *decl = llvm::cast<clang::NamedDecl>(
          fn_ptr_expr->getReferencedDeclOfCallee());
      assert(decl != nullptr);

      std::string fn_name = decl->getName().str();
      std::string new_expr = "IA2_FN("s + fn_name + ")";
      std::string addr_taken;

      std::string file_name = sm.getFilename(sm.getExpansionLoc(loc)).str();

      auto linkage = decl->getFormalLinkage();
      if (linkage == clang::Linkage::ExternalLinkage) {
        addr_taken_fn.insert(fn_name);
      } else if ((linkage == clang::Linkage::InternalLinkage) ||
                 (linkage == clang::Linkage::NoLinkage)) {
        internal_addr_taken_by_file[file_name].insert(fn_name);
      } else {
        // TODO: I don't know how to handle other types of linkage
        llvm::errs()
            << "Found declRefExpr in FnPtrAssign pass with unsupported linkage\n";
        return;
      }

      clang::CharSourceRange expansion_range = sm.getExpansionRange(loc);
      Replacement r{sm, expansion_range, new_expr};
      auto err = file_replacements[file_name].add(r);
      if (err) {
        llvm::errs() << "Error adding replacement: " << err << '\n';
      }
      return;
    }
  }

  // The set of all functions that had their address taken
  std::set<std::string> addr_taken_fn;
  // Maps file names to the set of static functions that had their address taken
  // in each file
  std::map<std::string, std::set<std::string>> internal_addr_taken_by_file;

private:
  std::map<std::string, Replacements> &file_replacements;
};

class FnDecl : public RefactoringCallback {
public:
  FnDecl(ASTMatchRefactorer &refactorer,
         std::map<std::string, Replacements> &replacements) {
    DeclarationMatcher fn_def_matcher =
        functionDecl(isDefinition()).bind("DefinedFunction");
    DeclarationMatcher fn_decl_matcher =
        functionDecl(unless(isDefinition())).bind("DeclaredFunction");
    refactorer.addMatcher(fn_def_matcher, this);
    refactorer.addMatcher(fn_decl_matcher, this);
  }

  virtual void run(const MatchFinder::MatchResult &result) {
    const clang::FunctionDecl *fn = nullptr;
    std::set<std::string> *fn_set = nullptr;
    if (auto *fn_def =
            result.Nodes.getNodeAs<clang::FunctionDecl>("DefinedFunction")) {
      fn = fn_def;
      fn_set = &defined_fns;
    } else if (auto *fn_decl = result.Nodes.getNodeAs<clang::FunctionDecl>(
                   "DeclaredFunction")) {
      fn = fn_decl;
      fn_set = &declared_fns;
    } else {
      // Type-casts failed, so we don't care to handle this decl
      return;
    }
    if (ignore_file(get_filename(fn))) {
      return;
    }

    std::string fn_name = fn->getNameAsString();
    CAbiSignature fn_sig = determineAbiForDecl(*fn);

    fn_signatures[fn_name] = fn_sig;
    fn_set->insert(fn_name);
  }

  // The set of all functions defined in the shared object
  std::set<std::string> defined_fns;
  // The set of all functions declared in the shared object's sources. This
  // includes all of the defined functions as well.
  std::set<std::string> declared_fns;
  // Map from defined function name to C ABI signature
  // TODO: Make this external linkage only?
  std::map<std::string, CAbiSignature> fn_signatures;

private:
};

int main(int argc, const char **argv) {
#if LLVM_VERSION_MAJOR >= 13
  auto parser_ptr =
      CommonOptionsParser::create(argc, argv, SourceRewriterCategory);
  if (!parser_ptr) {
    auto error = parser_ptr.takeError();
    llvm::errs() << error;
    return 1;
  }
  CommonOptionsParser &options_parser = *parser_ptr;
#else
  CommonOptionsParser options_parser(argc, argv, SourceRewriterCategory);
#endif

  /* Make a copy of each original input sources */
  for (auto s : options_parser.getSourcePathList()) {
    std::ifstream src(s, std::ios::binary);
    std::ofstream dst(s + ".orig", std::ios::binary);
    dst << src.rdbuf();
  }

  RefactoringTool tool(options_parser.getCompilations(),
                       options_parser.getSourcePathList());

  // Add the rewrite passes
  ASTMatchRefactorer refactorer(tool.getReplacements());
  FnDecl fn_decl(refactorer, tool.getReplacements());
  FnPtrTypes fn_ptr_types(refactorer, tool.getReplacements());
  FnPtrAssign fn_ptr_assign(refactorer, tool.getReplacements());
  FnPtrCall fn_ptr_call(refactorer, tool.getReplacements());

  auto rc = tool.runAndSave(newFrontendActionFactory(&refactorer).get());

  /* Create the wrapper source, linker args and function pointer header */
  std::error_code EC;
  llvm::raw_fd_ostream wrapper_out(OutputFilename + ".c", EC);
  if (EC) {
    llvm::errs() << "Error opening output wrapper file: " << EC.message()
                 << "\n";
    return EC.value();
  }
  llvm::raw_fd_ostream args_out(OutputFilename + ".args", EC);
  if (EC) {
    llvm::errs() << "Error opening output arguments file: " << EC.message()
                 << "\n";
    return EC.value();
  }
  llvm::raw_fd_ostream header_out(OutputFilename + ".h", EC);
  if (EC) {
    llvm::errs() << "Error opening output header file: " << EC.message()
                 << "\n";
    return EC.value();
  }

  wrapper_out << "#include \"internal_ia2.h\"\n";
  wrapper_out << "#ifndef TARGET_PKEY\n";
  wrapper_out << "#error Define TARGET_PKEY to compile this file\n";
  wrapper_out << "#endif\n";

  header_out << "#include \"internal_ia2.h\"\n";

  /*
   * Create call gates for direct calls to other comparments
   *
   * Undefined functions must be defined in another DSO which we assume
   * corresponds to another compartment.
   */
  std::set<std::string> undefined_fns;
  std::set_difference(fn_decl.declared_fns.begin(), fn_decl.declared_fns.end(),
                      fn_decl.defined_fns.begin(), fn_decl.defined_fns.end(),
                      std::inserter(undefined_fns, undefined_fns.begin()));

  auto compartment_pkey = std::to_string(CompartmentPkey);
  for (const auto &fn : undefined_fns) {
    CAbiSignature c_abi_sig = fn_decl.fn_signatures[fn];
    /* The target pkey is set by the TARGET_PKEY macro definition when compiling
     * the output wrapper source */
    std::string asm_wrapper = emit_asm_wrapper(
        c_abi_sig, fn, WrapperKind::Direct, compartment_pkey, "TARGET_PKEY");

    wrapper_out << "asm(\n";
    wrapper_out << asm_wrapper;
    wrapper_out << ");\n";

    args_out << "--wrap=" << fn << "\n";
  }

  /* Define IA2_FN for referencing call gate wrappers for specific functions */
  if (!fn_ptr_types.needed_fn_ptr_types.empty()) {
    std::string wrapper_name;
    if (compartment_pkey == "0") {
      /* If this compartment is untrusted, IA2_FN just needs do a type
       * conversion */
      wrapper_name = "func";
    } else {
      wrapper_name = "&__ia2_##func";
    }
    header_out << "#define IA2_FN(func) _Generic(func, \\\n";
    for (const auto &fn_ptr_ty : fn_ptr_types.needed_fn_ptr_types) {
      header_out << "    " << fn_ptr_ty.fn_ptr << " : ("
                 << fn_ptr_ty.opaque_struct << ") " << wrapper_name << ", \\\n";
    }
    /*
     * generic selection requires a default, but if the type is not found this
     * should not compile so we arbitrarily make the expression 0 to ensure this
     */
    header_out << "    default: 0)\n";

    /* Define IA2_CALL for calling pointers received from other compartments.
     * Unlike the wrappers in IA2_FN which are specific to a function, there is
     * only one of these for each function signature
     */
    /* This `void *` is defined in libia2/ia2.c */
    header_out << "extern void *ia2_fn_ptr;\n";
    header_out << "#define IA2_CALL(opaque) ({\\\n";
    header_out << "    ia2_fn_ptr = opaque;   \\\n";
    header_out << "    _Generic(opaque,       \\\n";
    for (const auto &fn_ptr_ty : fn_ptr_types.needed_fn_ptr_types) {
      header_out << "    " << fn_ptr_ty.opaque_struct << " : \\\n";
      if (compartment_pkey == "0") {
        /* If the compartment is untrusted, IA2_CALL just needs to do a type
         * conversion */
        header_out << "        (" << fn_ptr_ty.fn_ptr << ")opaque, \\\n";
      } else {
        header_out << "        (" << fn_ptr_ty.fn_ptr << ")&__ia2_"
                   << fn_ptr_ty.opaque_struct << ", \\\n";
      }
    }
    /*
     * generic selection requires a default, but if the type is not found this
     * should not compile so we arbitrarily make the expression 0 to ensure this
     */
    header_out << "    default: 0); \\\n";
    header_out << "})\n";
  }

  /* Define opaque function pointer types */
  for (const auto &fn_ptr_ty : fn_ptr_types.needed_fn_ptr_types) {
    // TODO: This shouldn't be a typedef to a pointer since you can cast it
    // to/from `void *` which is unsafe
    header_out << "struct " << fn_ptr_ty.opaque_struct << "_inner;\n";
    header_out << "typedef struct " << fn_ptr_ty.opaque_struct << "_inner *"
               << fn_ptr_ty.opaque_struct << ";\n";
  }

  /* Declare the wrappers for IA2_CALL */
  for (const auto &fn_ptr_ty : fn_ptr_types.needed_fn_ptr_types) {
    header_out << "extern void *__ia2_" << fn_ptr_ty.opaque_struct << ";\n";
  }

  /* Declare the wrappers for IA2_FN (i.e. functions that had their address
   * taken) */
  for (const auto &fn_decl : fn_ptr_assign.addr_taken_fn) {
    header_out << "extern char __ia2_" << fn_decl << ";\n";
  }

  /* If the compartment is untrusted, we're done */
  if (compartment_pkey == "0") {
    return rc;
  }

  /* Create call gate wrappers for pointers sent to other compartments. These
   * are the ones referenced by IA2_FN and are specific to each function */
  for (const auto &fn : fn_ptr_assign.addr_taken_fn) {
    CAbiSignature c_abi_sig = fn_decl.fn_signatures[fn];
    std::string asm_wrapper;
    std::string target_pkey = compartment_pkey;
    /*
     * TODO: This tool assumes that there are two trusted compartments.
     * To support more compartments we need a way to specify/determine where
     * undefined functions that have their address taken are defined.
     */
    if (undefined_fns.contains(fn)) {
      /* If this compartment has pkey 1 assume the undefined function is in
       * compartment 2 and call it with that pkey */
      if (compartment_pkey == "1") {
        target_pkey = "2"s;
        /* Otherwise (i.e. if this compartment has pkey 2) assume the undefined
         * function is in compartment 1 and call it with that pkey */
      } else {
        target_pkey = "1"s;
      }
    }
    asm_wrapper = emit_asm_wrapper(c_abi_sig, fn, WrapperKind::SentPointer,
                                   "0"s, target_pkey);
    wrapper_out << "asm(\n";
    wrapper_out << asm_wrapper;
    wrapper_out << ");\n";
  }

  /* Create call gate wrappers for pointers received from other compartments.
   * These are the ones referenced by IA2_CALL and there is only one per
   * function signature */
  for (const auto &[fn_ptr, c_abi_sig] : fn_ptr_types.fn_ptr_abi_sig) {
    std::string asm_wrapper =
        emit_asm_wrapper(c_abi_sig, fn_ptr, WrapperKind::ReceivedPointer,
                         compartment_pkey, "0"s);
    wrapper_out << "asm(\n";
    wrapper_out << asm_wrapper;
    wrapper_out << ");\n";
  }

  /* Create call gate wrappers for static functions sent to other compartments.
   * In this case the call gate wrapper must be defined in the translation unit
   * that defined the static function, so we define a IA2_DEFINE_WRAPPER_*
   * macros in the output header and invoke these macros in the corresponding
   * source files.
   */
  for (const auto &[file, functions] :
       fn_ptr_assign.internal_addr_taken_by_file) {
    std::ofstream source_file(file, std::ios::app);
    for (const auto &fn : functions) {
      CAbiSignature c_abi_sig = fn_decl.fn_signatures[fn];
      // TODO: This assumes that static functions have unique names
      /* Define a macro for defining the function wrapper */
      header_out << "#define IA2_DEFINE_WRAPPER_" << fn << " \\\n";
      std::string asm_wrapper =
          emit_asm_wrapper(c_abi_sig, fn, WrapperKind::SentPointer, "0"s,
                           compartment_pkey, true /* as_macro */);
      header_out << "asm(\\\n";
      header_out << asm_wrapper;
      header_out << ");\n";

      /* Declare the wrapper function */
      header_out << "extern char __ia2_" << fn << ";\n";

      /* Invoke the macro we just defined in the corresponding source file */
      source_file << "IA2_DEFINE_WRAPPER_" << fn << "\n";
    }
  }

  return rc;
}
