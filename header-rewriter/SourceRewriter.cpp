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
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/RefactoringCallbacks.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>

#define PKEY_DEFINE "-DPKEY="
#define MAX_PKEYS 16

using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace std::string_literals;

typedef std::string Function;
typedef std::string Filename;
typedef int Pkey;
typedef std::string OpaqueStruct;

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static llvm::cl::OptionCategory
    SourceRewriterCategory("Source rewriter options");

static llvm::cl::opt<std::string>
    OutputPrefix("output-prefix", llvm::cl::Required,
                 llvm::cl::cat(SourceRewriterCategory),
                 llvm::cl::desc("<prefix for output files>"));

// Map each translation unit's filename to its pkey.
static std::map<Filename, Pkey> file_pkeys;

static Filename get_filename(const clang::SourceLocation loc,
                             const clang::SourceManager &sm) {
  return sm.getFilename(sm.getSpellingLoc(loc)).str();
}

static bool in_macro_expansion(const clang::SourceLocation loc,
                               const clang::SourceManager &sm) {
  return sm.getExpansionLoc(loc) != sm.getSpellingLoc(loc);
}

static Pkey get_file_pkey(const clang::SourceManager &sm) {
  auto file_entry = sm.getFileEntryForID(sm.getMainFileID());
  auto filename = file_entry->tryGetRealPathName().str();
  try {
    return file_pkeys.at(filename);
  } catch (std::out_of_range const &exc) {
    llvm::errs() << "Source file " << filename.c_str()
                 << " has no entry with -DPKEY in compile_commands.json\n";
    assert(0);
  }
}

static bool ignore_file(const Filename &filename) {
  return filename.starts_with("/usr/") || filename.ends_with("ia2.h") ||
         filename.ends_with("test_fault_handler.h") || filename == "";
}

static std::string append_name_if_nonempty(const std::string &new_type,
                                           const std::string &name) {
  return new_type + (name.empty() ? "" : " ") + name;
};

/*
 * Rewrites function pointer types as opaque structs. Also keeps track of the
 * opaque structs used
 */
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

    // We only need to replace a subset of the source range for some fnPtrVar
    // nodes. In these cases this optional is assigned a value which is used for
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
      // of the SourceRange. Since the FnPtrExpr pass may need to rewrite
      // INITIAL_VALUE, we reduce the SourceRange to only include the LHS.
      if (var_decl->hasInit()) {
        // VarDecl doesn't expose a way to get the SourceRange without the
        // initializer, so this is taken almost verbatim from
        // DeclaratorDecl::getSourceRange
        auto start = var_decl->getOuterLocStart();
        // The type extends past the variable name (i.e. `ptr` above) so we need
        // to manually set the SourceRange end
        auto *ty_info = var_decl->getTypeSourceInfo();
        assert(ty_info != nullptr);
        auto end = ty_info->getTypeLoc().getSourceRange().getEnd();
        range = clang::SourceRange(start, end);
      }
      generate_decl = append_name_if_nonempty;
    }
    // Get the node's SourceRange if we are not just rewriting a subset of it
    if (!range.has_value()) {
      range = old_decl->getSourceRange();
    }

    assert(result.SourceManager != nullptr);
    assert(result.Context != nullptr);
    assert(old_decl != nullptr);

    const auto &sm = *result.SourceManager;
    auto &ctxt = *result.Context;

    if (get_file_pkey(sm) == 0) {
      return;
    }

    auto loc = old_decl->getLocation();
    Filename file_name = get_filename(loc, sm);
    if (ignore_file(file_name)) {
      return;
    }

    auto *fpt = old_type->castAs<clang::PointerType>()
                    ->getPointeeType()
                    ->getAsAdjusted<clang::FunctionProtoType>();
    // TODO: Factor out these checks and add them to the FnPtrExpr pass
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

    mangled_type = mangle_type(ctxt, fpt->getCanonicalTypeInternal());
    OpaqueStruct new_type = kFnPtrTypePrefix + mangled_type;
    std::string name = llvm::cast<clang::NamedDecl>(old_decl)->getName().str();
    std::string new_decl = generate_decl(new_type, name);

    fn_ptr_types.insert(new_type);

    // This check must come after inserting new_type into fn_ptr_types but
    // before the Replacement is added
    if (in_macro_expansion(loc, sm)) {
      auto expansion_file = sm.getFilename(sm.getExpansionLoc(loc)).str();
      auto expansion_line = sm.getExpansionLineNumber(loc);
      auto spelling_line = sm.getSpellingLineNumber(loc);
      llvm::errs() << "FnPtrTypes: " << expansion_file << ":" << expansion_line
                   << " ";
      if (expansion_line != spelling_line) {
        llvm::errs() << file_name << ":" << spelling_line << " ";
      }
      llvm::errs() << "must be rewritten manually\n";
      return;
    }

    // Replace the old decl with the new one. Make sure to include any macro
    // expansions in the SourceRange being replaced
    clang::CharSourceRange expansion_range = sm.getExpansionRange(*range);
    Replacement r{sm, expansion_range, new_decl};
    auto err = file_replacements[file_name].add(r);
    if (err) {
      llvm::errs() << "Error adding replacement: " << err << '\n';
    }
    return;
  }

  std::set<OpaqueStruct> fn_ptr_types;

private:
  std::map<std::string, Replacements> &file_replacements;
};

/*
 * Rewrites NULL fn ptr expressions as NULL opaque structs (i.e. { NULL }).
 * This matches variable declarations and assignments. For varDecls the opaque
 * struct's type can be inferred from the LHS, but for assignment the type must
 * be specified in our replacement expression e.g.  `ptr = (fn_type) { NULL };`
 */
class FnPtrNull : public RefactoringCallback {
public:
  FnPtrNull(ASTMatchRefactorer &refactorer,
            std::map<std::string, Replacements> &file_replacements)
      : file_replacements(file_replacements) {

    auto fn_ptr = pointerType(pointee(ignoringParens(functionType())));

    auto fn_ptr_typedef = hasType(typedefNameDecl(hasType(fn_ptr)));

    auto null_expr = implicitCastExpr(ignoringParenCasts(nullPointerConstant()))
                         .bind("nullExpr");

    auto null_fn_ptr = varDecl(hasInitializer(null_expr),
                               anyOf(fn_ptr_typedef, hasType(fn_ptr)));

    auto assign_null = binaryOperator(
        hasRHS(null_expr),
        hasLHS(expr(anyOf(fn_ptr_typedef, hasType(fn_ptr))).bind("ptrLHS")));

    refactorer.addMatcher(null_fn_ptr, this);
    refactorer.addMatcher(assign_null, this);
  }
  virtual void run(const MatchFinder::MatchResult &result) {
    // The two matchers both have a nullExpr node so this getNodeAs can't fail
    auto *null_fn_ptr = result.Nodes.getNodeAs<clang::Expr>("nullExpr");
    assert(null_fn_ptr != nullptr);

    assert(result.SourceManager != nullptr);
    assert(result.Context != nullptr);
    auto &sm = *result.SourceManager;
    auto &ctxt = *result.Context;

    if (get_file_pkey(sm) == 0) {
      return;
    }

    clang::SourceLocation loc = null_fn_ptr->getExprLoc();
    Filename filename = sm.getFilename(sm.getExpansionLoc(loc)).str();

    std::string new_expr = "{ NULL }";
    // If the matcher found an assignment add the type of the LHS variable to
    // new_expr
    if (auto *lhs_ptr = result.Nodes.getNodeAs<clang::Expr>("ptrLHS")) {
      auto char_range =
          clang::CharSourceRange::getTokenRange(lhs_ptr->getSourceRange());
      auto lhs_binding =
          clang::Lexer::getSourceText(char_range, sm, ctxt.getLangOpts());
      new_expr = "(typeof("s + lhs_binding.str() + ")) { NULL }";
    }

    clang::CharSourceRange expansion_range = sm.getExpansionRange(loc);
    Replacement r{sm, expansion_range, new_expr};
    auto err = file_replacements[filename].add(r);
    if (err) {
      llvm::errs() << "Error adding replacements: " << err << '\n';
    }
    return;
  }

private:
  std::map<std::string, Replacements> &file_replacements;
};

/*
 * Rewrites indirect function calls as `IA2_CALL(fn_ptr, ID, pkey)`. fn_ptr is
 * the original function pointer expression. ID is an integer assigned by this
 * pass and specific to each function pointer signature. pkey is the caller's
 * pkey.
 */
class FnPtrCall : public RefactoringCallback {
public:
  FnPtrCall(ASTMatchRefactorer &refactorer,
            std::map<std::string, Replacements> &file_replacements)
      : file_replacements(file_replacements) {
    // Initialize the function pointer signature ID counter
    id_counter = 0;

    // This matches expressions that reference declared functions and we use
    // this to filter out direct calls in the following matcher
    auto declared_function = declRefExpr(hasDeclaration(functionDecl()));

    // Matches function calls excluding direct calls. Only the callee nodes are
    // bound to "fnPtrCall"
    StatementMatcher fn_ptr_call = callExpr(callee(
        expr(unless(ignoringImplicit(declared_function))).bind("fnPtrCall")));

    refactorer.addMatcher(fn_ptr_call, this);
  }
  virtual void run(const MatchFinder::MatchResult &result) {
    auto *fn_ptr_call = result.Nodes.getNodeAs<clang::Expr>("fnPtrCall");
    assert(fn_ptr_call != nullptr);

    assert(result.SourceManager != nullptr);
    auto &sm = *result.SourceManager;

    assert(result.Context != nullptr);
    auto &ctxt = *result.Context;

    if (get_file_pkey(sm) == 0) {
      return;
    }

    clang::SourceLocation loc = fn_ptr_call->getExprLoc();
    Filename filename = get_filename(loc, sm);
    if (ignore_file(filename)) {
      return;
    }

    // Check if the function pointer type already has an ID
    auto expr_ty = fn_ptr_call->getType()->getCanonicalTypeInternal();
    auto expr_ty_str = expr_ty.getAsString();
    if (!fn_ptr_ids.contains(expr_ty_str)) {
      fn_ptr_ids[expr_ty_str] = id_counter;
      id_counter += 1;
    }
    auto fn_ptr_id = std::to_string(fn_ptr_ids.at(expr_ty_str));

    auto *fpt = expr_ty->castAs<clang::PointerType>()
                    ->getPointeeType()
                    ->getAsAdjusted<clang::FunctionProtoType>();
    fn_ptr_abi_sig[expr_ty_str] = determineAbiForProtoType(*fpt, ctxt);

    // This check must come after modifying the maps in this pass but before the
    // Replacement is added
    if (in_macro_expansion(loc, sm)) {
      auto expansion_file = sm.getFilename(sm.getExpansionLoc(loc)).str();
      auto expansion_line = sm.getExpansionLineNumber(loc);
      auto spelling_line = sm.getSpellingLineNumber(loc);
      llvm::errs() << "FnPtrCall: " << expansion_file << ":" << expansion_line
                   << " ";
      if (spelling_line != expansion_line) {
        llvm::errs() << filename << ":" << spelling_line << " ";
      }
      llvm::errs() << "must be rewritten manually (ID = " << fn_ptr_id << ")\n";
      return;
    }

    // getTokenRange is required to replace the entire callee expression
    auto char_range =
        clang::CharSourceRange::getTokenRange(fn_ptr_call->getSourceRange());

    auto old_expr =
        clang::Lexer::getSourceText(char_range, sm, ctxt.getLangOpts());

    // Get the translation unit's filename to figure out the pkey
    std::string pkey = std::to_string(get_file_pkey(sm));

    std::string new_expr =
        "IA2_CALL("s + old_expr.str() + ", " + fn_ptr_id + ", " + pkey + ")";

    Replacement r{sm, char_range, new_expr};
    auto err = file_replacements[filename].add(r);
    if (err) {
      llvm::errs() << "Error adding replacements: " << err << '\n';
    }
    return;
  }

  std::map<OpaqueStruct, int> fn_ptr_ids;
  std::map<OpaqueStruct, CAbiSignature> fn_ptr_abi_sig;

private:
  std::map<std::string, Replacements> &file_replacements;
  int id_counter;
};

/*
 * Rewrites function pointer expressions that reference functions as
 * IA2_FN(fn_name). This pass also keeps track of functions that have their
 * address taken. For functions that are visible to the linker we only need to
 * store their name and corresponding opaque struct while for static functions
 * we also need the translation unit's name. To simplify the former case this
 * pass stores this in two separate maps.
 */
class FnPtrExpr : public RefactoringCallback {
public:
  FnPtrExpr(ASTMatchRefactorer &refactorer,
            std::map<std::string, Replacements> &file_replacements)
      : file_replacements(file_replacements) {

    // This matches a superset of the nodes we want to replace. In addition to
    // function pointer expressions that reference functions, this also matches
    // direct function calls which are handled by the FnPtrCall pass.
    auto fn_expr =
        declRefExpr(hasDeclaration(functionDecl())).bind("fnPtrExpr");

    // Matches function calls to filter them out of the previous matcher
    auto call_expr = hasAncestor(
        callExpr(callee(expr(ignoringImplicit(equalsBoundNode("fnPtrExpr"))))));

    StatementMatcher fn_ptr_expr = expr(fn_expr, unless(call_expr));

    refactorer.addMatcher(fn_ptr_expr, this);
  }
  virtual void run(const MatchFinder::MatchResult &result) {
    auto *fn_ptr_expr = result.Nodes.getNodeAs<clang::DeclRefExpr>("fnPtrExpr");
    assert(fn_ptr_expr != nullptr);

    assert(result.SourceManager != nullptr);
    auto &sm = *result.SourceManager;

    assert(result.Context != nullptr);
    auto &ctxt = *result.Context;

    if (get_file_pkey(sm) == 0) {
      return;
    }

    clang::SourceLocation loc = fn_ptr_expr->getExprLoc();
    if (ignore_file(get_filename(loc, sm))) {
      return;
    }

    auto *fn_decl =
        llvm::cast<clang::NamedDecl>(fn_ptr_expr->getReferencedDeclOfCallee());
    assert(fn_decl != nullptr);

    Function fn_name = fn_decl->getName().str();
    std::string new_expr = "IA2_FN("s + fn_name + ")";

    Filename filename = sm.getFilename(sm.getExpansionLoc(loc)).str();

    auto old_type = fn_decl->getFunctionType();
    assert(old_type != nullptr);
    std::string mangled_type =
        mangle_type(ctxt, old_type->getCanonicalTypeInternal());
    OpaqueStruct new_type = kFnPtrTypePrefix + mangled_type;

    auto linkage = fn_decl->getFormalLinkage();
    if (linkage == clang::Linkage::ExternalLinkage) {
      addr_taken_fns[fn_name] = new_type;
    } else if ((linkage == clang::Linkage::InternalLinkage) ||
               (linkage == clang::Linkage::NoLinkage)) {
      internal_addr_taken_fns[filename].insert(
          std::make_pair(fn_name, new_type));
    } else {
      llvm::errs()
          << "Found declRefExpr in FnPtrExpr pass with unsupported linkage\n";
      return;
    }

    // This check must come after modifying the maps in this pass but before the
    // Replacement is added
    if (in_macro_expansion(loc, sm)) {
      auto expansion_file = sm.getFilename(sm.getExpansionLoc(loc)).str();
      auto expansion_line = sm.getExpansionLineNumber(loc);
      auto spelling_line = sm.getSpellingLineNumber(loc);
      llvm::errs() << "FnPtrExpr: " << expansion_file << ":" << expansion_line
                   << " ";
      if (spelling_line != expansion_line) {
        llvm::errs() << filename << ":" << spelling_line << " ";
      }
      llvm::errs() << "must be rewritten manually\n";
      return;
    }

    clang::CharSourceRange expansion_range = sm.getExpansionRange(loc);
    Replacement r{sm, expansion_range, new_expr};
    auto err = file_replacements[filename].add(r);
    if (err) {
      llvm::errs() << "Error adding replacements: " << err << '\n';
    }
    return;
  }

  std::map<Function, OpaqueStruct> addr_taken_fns;
  std::map<Filename, std::set<std::pair<Function, OpaqueStruct>>>
      internal_addr_taken_fns;

private:
  std::map<std::string, Replacements> &file_replacements;
};

/*
 * Finds function declarations and definitions to determine in which
 * compartment functions are defined in. This is later used to determine what
 * wrappers to generate and what PKRUs to use. This pass does not rewrite any
 * code.
 */
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
    bool definition = false;
    const clang::FunctionDecl *fn_node = nullptr;
    if (auto *fn_def =
            result.Nodes.getNodeAs<clang::FunctionDecl>("DefinedFunction")) {
      definition = true;
      fn_node = fn_def;
    } else if (auto *fn_decl = result.Nodes.getNodeAs<clang::FunctionDecl>(
                   "DeclaredFunction")) {
      fn_node = fn_decl;
    } else {
      llvm::errs() << "Type-cast in FnDecl pass failed\n";
      return;
    }

    assert(result.SourceManager != nullptr);
    auto &sm = *result.SourceManager;

    // Ignore declarations in libc and libia2 headers
    if (ignore_file(get_filename(fn_node->getLocation(), sm))) {
      return;
    }

    // Calls to compiler builtins produce an inline declaration that should
    // not be wrapped; we also don't want to wrap explicit decls of builtins
    if (fn_node->getBuiltinID() != 0) {
      return;
    }

    Function fn_name = fn_node->getNameAsString();
    CAbiSignature fn_sig = determineAbiForDecl(*fn_node);
    abi_signatures[fn_name] = fn_sig;

    // Get the translation unit's filename to figure out the pkey
    Pkey pkey = get_file_pkey(sm);
    assert(pkey < MAX_PKEYS);

    if (definition) {
      defined_fns[pkey].insert(fn_name);
      fn_pkeys[fn_name] = pkey;
    } else {
      declared_fns[pkey].insert(fn_name);
    }
  }

  std::set<Function> defined_fns[MAX_PKEYS];
  std::set<Function> declared_fns[MAX_PKEYS];
  std::map<Function, CAbiSignature> abi_signatures;
  std::map<Function, Pkey> fn_pkeys;
};

void write_to_ld_file(llvm::raw_fd_ostream *file[MAX_PKEYS], int i,
                      const std::string &contents) {
  if (file[i] == nullptr) {
    std::error_code EC;
    file[i] = new llvm::raw_fd_ostream(
        OutputPrefix + "_" + std::to_string(i) + ".ld", EC);
    assert(file[i] != nullptr);
  }
  *file[i] << contents;
}

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

  RefactoringTool tool(options_parser.getCompilations(),
                       options_parser.getSourcePathList());
  CompilationDatabase &comp_db = options_parser.getCompilations();

  std::set<Pkey> pkeys_used;
  for (auto s : options_parser.getSourcePathList()) {
    // Get the compile commands for each input source
    std::vector<CompileCommand> comp_cmds =
        comp_db.getCompileCommands(llvm::StringRef(s));

    // Files may be compiled more than once, but we don't support that yet. If
    // the file is not found in the compilation database then there's a problem
    // with it.
    auto has_pkey_define = [](const CompileCommand &cc) {
      bool pre_rewriter = false;
      bool has_pkey_define = false;
      for (const auto &flag : cc.CommandLine) {
        if (flag.starts_with(PKEY_DEFINE)) {
          has_pkey_define = true;
        }
      }
      return has_pkey_define;
    };
    auto comp_cmd_with_pkey =
        std::find_if(comp_cmds.begin(), comp_cmds.end(), has_pkey_define);
    if (comp_cmd_with_pkey == comp_cmds.end()) {
      llvm::errs() << "No compile commands with -DPKEY found for " << s.c_str()
                   << '\n';
      return -1;
    }

    assert(comp_cmds.size() == 1);
    auto cc_cmd = *comp_cmd_with_pkey;

    auto is_pkey_define = [](const std::string &s) {
      return s.starts_with(PKEY_DEFINE);
    };

    auto pkey_define = std::find_if(cc_cmd.CommandLine.begin(),
                                    cc_cmd.CommandLine.end(), is_pkey_define);

    // All files must be compiled with -DPKEY=N
    if (pkey_define == cc_cmd.CommandLine.end()) {
      llvm::errs() << cc_cmd.Filename.c_str()
                   << " was not compiled with -DPKEY=\n";
      for (const auto &flag : cc_cmd.CommandLine) {
        llvm::errs() << flag.c_str() << " ";
      }
      llvm::errs() << '\n';
      return -1;
    }

    // Using sizeof - 1 avoids counting the null terminator in PKEY_DEFINE
    Pkey pkey = std::stoi(pkey_define->substr(sizeof(PKEY_DEFINE) - 1));

    file_pkeys[cc_cmd.Filename] = pkey;
    pkeys_used.insert(pkey);
  }

  // Check the number of pkeys used to avoid generating more output than
  // necessary in the next parts
  size_t num_pkeys = pkeys_used.size();
  assert(num_pkeys < MAX_PKEYS);

  // Ensure only the lowest pkeys are used. The two allowed configurations for N
  // pkeys are pkeys 0 to N - 1 or 1 to N.
  Pkey min_pkey = *std::min_element(pkeys_used.begin(), pkeys_used.end());
  Pkey max_pkey = *std::max_element(pkeys_used.begin(), pkeys_used.end());
  bool config_0 = (min_pkey == 0) && (max_pkey == num_pkeys - 1);
  bool config_1 = (min_pkey == 1) && (max_pkey == num_pkeys);
  assert(config_0 || config_1);
  if (min_pkey == 1) {
    num_pkeys += 1;
  }

  for (auto s : options_parser.getSourcePathList()) {
    // Make a copy of each original input sources
    std::ifstream src(s, std::ios::binary);
    std::ofstream dst(s + ".orig", std::ios::binary);
    dst << src.rdbuf();
  }

  /* Create the wrapper source and function pointer header */
  std::error_code EC;
  llvm::raw_fd_ostream wrapper_out(OutputPrefix + ".c", EC);
  if (EC) {
    llvm::errs() << "Error opening output wrapper file: " << EC.message()
                 << "\n";
    return EC.value();
  }
  llvm::raw_fd_ostream header_out(OutputPrefix + ".h", EC);
  if (EC) {
    llvm::errs() << "Error opening output header file: " << EC.message()
                 << "\n";
    return EC.value();
  }

  ASTMatchRefactorer refactorer(tool.getReplacements());
  FnDecl fn_decl_pass(refactorer, tool.getReplacements());
  FnPtrTypes ptr_types_pass(refactorer, tool.getReplacements());
  FnPtrExpr ptr_expr_pass(refactorer, tool.getReplacements());
  FnPtrCall ptr_call_pass(refactorer, tool.getReplacements());
  FnPtrNull null_ptr_pass(refactorer, tool.getReplacements());

  auto rc = tool.runAndSave(newFrontendActionFactory(&refactorer).get());

  header_out << "#include \"scrub_registers.h\"\n";
  header_out << "#ifdef LIBIA2_INSECURE\n";
  header_out << "#define IA2_WRPKRU\n";
  header_out << "#else\n";
  header_out << "#define IA2_WRPKRU \"wrpkru\"\n";
  header_out << "#endif\n";

  header_out << '\n';

  header_out
      << "#define IA2_FN(func) (typeof(__ia2_##func)) { (void*)&__ia2_##func }\n";

  /*
   * When IA2_CALL has caller pkey = 0, it just casts the opaque struct to an fn
   * ptr. Otherwise it sets ia2_fn_ptr to the opaque struct's value then calls
   * an indirect call gate depending on the opaque struct's type.
   */
  header_out << "#define IA2_CALL(opaque, id, pkey) ({ \\\n";
  header_out << "  ia2_fn_ptr = opaque.ptr; \\\n";
  header_out
      << "  (IA2_TYPE_##id)&__ia2_indirect_callgate_##id##_pkey_##pkey; \\\n";
  header_out << "})\n";

  wrapper_out << "#include \"scrub_registers.h\"\n";
  wrapper_out << "#ifdef LIBIA2_INSECURE\n";
  wrapper_out << "#define IA2_WRPKRU\n";
  wrapper_out << "#else\n";
  wrapper_out << "#define IA2_WRPKRU \"wrpkru\"\n";
  wrapper_out << "#endif\n";

  /*
   * Define wrappers for IA2_CALL. These switch from the caller's pkey to pkey 0
   * so we don't need to generate them for caller_pkey = 0. When IA2_CALL has
   * caller pkey = 0, it just becomes a cast that calls the fn ptr.
   */
  std::cout << "Generating indirect callsite wrappers\n";
  std::string wrapper_decls;
  std::set<int> type_id_macros_generated = {};
  for (int caller_pkey = 1; caller_pkey < num_pkeys; caller_pkey++) {
    for (const auto &[ty, id] : ptr_call_pass.fn_ptr_ids) {
      CAbiSignature c_abi_sig;
      try {
        c_abi_sig = ptr_call_pass.fn_ptr_abi_sig.at(ty);
      } catch (std::out_of_range const &exc) {
        llvm::errs() << "Opaque struct " << ty.c_str()
                     << " not found by FnPtrCall pass\n";
        assert(0);
      }
      std::string wrapper_name = "__ia2_indirect_callgate_"s +
                                 std::to_string(id) + "_pkey_" +
                                 std::to_string(caller_pkey);
      std::string asm_wrapper =
          emit_asm_wrapper(c_abi_sig, wrapper_name, nullptr,
                           WrapperKind::IndirectCallsite, caller_pkey, 0);
      wrapper_out << "asm(\n";
      wrapper_out << asm_wrapper;
      wrapper_out << ");\n";

      if (!type_id_macros_generated.contains(id)) {
        header_out << "#define IA2_TYPE_"s << id << " " << ty << "\n";
        type_id_macros_generated.insert(id);
      }
      wrapper_decls += "extern char " + wrapper_name + ";\n";
    }
  }
  header_out << wrapper_decls.c_str();

  std::cout << "Generating opaque function pointer types\n";
  // Define opaque struct types for function pointers e.g.
  // struct IA2_fnptr__ZTSFiiE { char *ptr; };
  for (const auto &opaque : ptr_types_pass.fn_ptr_types) {
    header_out << opaque << " { char *ptr; };\n";
  }

  // Declare the pointer read by the indirect call gates
  header_out << "extern void *ia2_fn_ptr;\n";
  // Define the pointer read by indirect call gates
  wrapper_out << "void *ia2_fn_ptr;\n";

  std::cout << "Generating direct call gate wrappers\n";
  // Create wrappers for direct calls. These wrappers are inserted by ld --wrap
  // so the wrapper name cannot be changed.
  llvm::raw_fd_ostream *ld_args_out[MAX_PKEYS] = {};
  for (int caller_pkey = 0; caller_pkey < num_pkeys; caller_pkey++) {
    // For each compartment find the functions that are declared but not defined
    std::set<Function> undefined_fns = {};
    std::set_difference(fn_decl_pass.declared_fns[caller_pkey].begin(),
                        fn_decl_pass.declared_fns[caller_pkey].end(),
                        fn_decl_pass.defined_fns[caller_pkey].begin(),
                        fn_decl_pass.defined_fns[caller_pkey].end(),
                        std::inserter(undefined_fns, undefined_fns.begin()));
    // create the file, even if it's going to be empty
    write_to_ld_file(ld_args_out, caller_pkey, "");
    for (const auto &fn_name : undefined_fns) {
      CAbiSignature c_abi_sig;
      try {
        c_abi_sig = fn_decl_pass.abi_signatures.at(fn_name);
      } catch (std::out_of_range const &exc) {
        llvm::errs() << "C ABI signature for function " << fn_name.c_str()
                     << " not found by FnDecl pass\n";
        assert(0);
      }
      // TODO: Add the option to append "_from_PKEY" to the wrapper name and use
      // objcopy --redefine-syms to support calling a function from multiple
      // compartments
      std::string wrapper_name = "__wrap_"s + fn_name;
      Pkey target_pkey;
      try {
        target_pkey = fn_decl_pass.fn_pkeys.at(fn_name);
      } catch (std::out_of_range const &exc) {
        llvm::errs() << "Assuming pkey for function " << fn_name.c_str()
                     << " is same as the caller (" << caller_pkey
                     << ") since its definition was not found by FnDecl pass\n";
        continue;
      }
      std::string asm_wrapper =
          emit_asm_wrapper(c_abi_sig, wrapper_name, &fn_name,
                           WrapperKind::Direct, caller_pkey, target_pkey);
      wrapper_out << "asm(\n";
      wrapper_out << asm_wrapper;
      wrapper_out << ");\n";

      write_to_ld_file(ld_args_out, caller_pkey, "--wrap="s + fn_name + '\n');
    }
  }

  std::cout << "Generating function pointer wrappers\n";
  // Define wrappers for function pointers (i.e. those referenced by IA2_FN)
  for (const auto &[fn_name, opaque] : ptr_expr_pass.addr_taken_fns) {
    /*
     * Declare these wrapper in the output header so that IA2_FN can reference
     * them. e.g. extern struct IA2_fnptr_ZTSFiiE __ia2_foo;
     *
     * The type used in these declarations is arbitrary and chosen to make it
     * easy to go from a function's name to its mangled type in IA2_FN.
     */
    std::string wrapper_name = "__ia2_"s + fn_name;
    header_out << "extern " << opaque << " " << wrapper_name << ";\n";

    // TODO: These wrapper go from pkey 0 to the target pkey so if the target
    // also has pkey 0 then it just needs call the original function
    Pkey target_pkey = fn_decl_pass.fn_pkeys[fn_name];
    if (target_pkey != 0) {
      CAbiSignature c_abi_sig = fn_decl_pass.abi_signatures[fn_name];
      std::string asm_wrapper =
          emit_asm_wrapper(c_abi_sig, wrapper_name, &fn_name,
                           WrapperKind::Pointer, 0, target_pkey);
      wrapper_out << "asm(\n";
      wrapper_out << asm_wrapper;
      wrapper_out << ");\n";
    }
  }

  std::cout << "Generating function pointer wrappers for static functions\n";
  // Define wrappers for pointers to static functions (also those referenced by
  // IA2_FN)
  std::string static_wrappers;
  for (const auto &[filename, addr_taken_fns] :
       ptr_expr_pass.internal_addr_taken_fns) {

    // Open each file that took the address of a static function
    std::ofstream source_file(filename, std::ios::app);

    // For each static function, define a macro to define the wrapper in the
    // output header and invoke it in the source file
    for (const auto &[fn_name, opaque] : addr_taken_fns) {
      std::string wrapper_name = "__ia2_"s + fn_name;
      // TODO: These wrapper go from pkey 0 to the target pkey so if the target
      // also has pkey 0 then it just needs call the original function
      Pkey target_pkey = fn_decl_pass.fn_pkeys[fn_name];
      if (target_pkey != 0) {
        CAbiSignature c_abi_sig = fn_decl_pass.abi_signatures[fn_name];

        std::string asm_wrapper = emit_asm_wrapper(
            c_abi_sig, wrapper_name, &fn_name, WrapperKind::Pointer, 0,
            target_pkey, true /* as_macro */);
        static_wrappers += "#define IA2_DEFINE_WRAPPER_"s + fn_name + " \\\n";
        static_wrappers += "asm(\\\n";
        static_wrappers += asm_wrapper;
        static_wrappers += ");\n";

        header_out << "extern " << opaque << " " << wrapper_name << ";\n";

        source_file << "IA2_DEFINE_WRAPPER_" << fn_name << "\n";
      }
    }
  }
  header_out << static_wrappers.c_str();

  for (int i = 0; i < num_pkeys; i++) {
    if (ld_args_out[i] != nullptr) {
      ld_args_out[i]->close();
    }
  }

  return EXIT_SUCCESS;
}
