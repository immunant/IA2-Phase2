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
#include <clang/AST/Decl.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/Basic/SourceLocation.h>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>

#define PKEY_DEFINE "-DPKEY="
#define MAX_PKEYS 16

using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace std::string_literals;

static constexpr llvm::StringLiteral SKIP_WRAP_ATTR("ia2_skip_wrap");

typedef std::string Function;
typedef std::string Filename;
typedef int Pkey;
typedef std::string OpaqueStruct;

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static llvm::cl::OptionCategory
    SourceRewriterCategory("Source rewriter options");

static llvm::cl::opt<Arch> Target("arch",
                                  llvm::cl::init(Arch::X86),
                                  llvm::cl::Optional,
                                  llvm::cl::cat(SourceRewriterCategory),
                                  llvm::cl::desc("<aarch64 or x86>"),
                                  llvm::cl::values(
                                      clEnumValN(Arch::X86, "x86", "Generate code for compartmentalization on x86 using MPK"),
                                      clEnumValN(Arch::Aarch64, "aarch64", "Generate code for compartmentalization on Aarch64 using MTE")));

static llvm::cl::opt<std::string>
    RootDirectory("root-directory", llvm::cl::Required,
                  llvm::cl::cat(SourceRewriterCategory),
                  llvm::cl::desc("<root directory for input files>"));

static llvm::cl::opt<std::string>
    OutputDirectory("output-directory", llvm::cl::Required,
                    llvm::cl::cat(SourceRewriterCategory),
                    llvm::cl::desc("<root directory for output files>"));

static llvm::cl::opt<std::string>
    OutputPrefix("output-prefix", llvm::cl::Required,
                 llvm::cl::cat(SourceRewriterCategory),
                 llvm::cl::desc("<prefix for output files>"));

// Map each translation unit's filename to its pkey.
static std::map<Filename, Pkey> file_pkeys;

static std::map<Filename, Filename> rel_path_to_full;

static Filename get_expansion_filename(const clang::SourceLocation loc,
                                       const clang::SourceManager &sm) {
  llvm::SmallString<256> s(sm.getFilename(sm.getExpansionLoc(loc)));
  if (llvm::sys::path::is_relative(s) && s != "") {
    try {
      return rel_path_to_full.at(s.str().str());
    } catch (std::out_of_range const &exc) {
      llvm::errs() << "get_filename failed to find full path for " << s.str().str() << '\n';
      abort();
    }
  }
  llvm::sys::path::replace_path_prefix(s, RootDirectory, OutputDirectory);
  return s.str().str();
}

static Filename get_filename(const clang::SourceLocation loc,
                             const clang::SourceManager &sm) {
  llvm::SmallString<256> s(sm.getFilename(sm.getSpellingLoc(loc)));
  if (llvm::sys::path::is_relative(s) && s != "") {
    try {
      return rel_path_to_full.at(s.str().str());
    } catch (std::out_of_range const &exc) {
      llvm::errs() << "get_filename failed to find full path for " << s.str().str() << '\n';
      abort();
    }
  }
  llvm::sys::path::replace_path_prefix(s, RootDirectory, OutputDirectory);
  return s.str().str();
}

static bool in_macro_expansion(const clang::SourceLocation loc,
                               const clang::SourceManager &sm) {
  return sm.getExpansionLoc(loc) != sm.getSpellingLoc(loc);
}

static bool in_fn_like_macro(const clang::SourceLocation loc,
                             const clang::SourceManager &sm) {
  if (!in_macro_expansion(loc, sm)) {
    return false;
  }
  const clang::SrcMgr::ExpansionInfo &exp =
      sm.getSLocEntry(sm.getFileID(loc)).getExpansion();
  return exp.isFunctionMacroExpansion() || exp.isMacroArgExpansion();
}

static Replacement replace_new_file(const Filename &filename,
                                    const Replacement &old_r) {
  return {llvm::StringRef(filename), old_r.getOffset(), old_r.getLength(),
          old_r.getReplacementText()};
}

static Pkey get_file_pkey(const clang::SourceManager &sm) {
  auto file_entry = sm.getFileEntryForID(sm.getMainFileID());
  auto filename = file_entry->tryGetRealPathName().str();
  try {
    return file_pkeys.at(filename);
  } catch (std::out_of_range const &exc) {
    llvm::errs() << "Source file " << filename.c_str()
                 << " has no entry with -DPKEY in compile_commands.json\n";
    abort();
  }
}

static bool ignore_file(const Filename &filename) {
  return !filename.starts_with(OutputDirectory) &&
         !filename.starts_with(RootDirectory);
}

static bool ignore_function(const clang::Decl &decl,
                            const std::optional<clang::SourceLocation> &loc,
                            const clang::SourceManager &sm) {
  if (const auto *named_decl = dyn_cast<clang::NamedDecl>(&decl)) {
    if (named_decl->getNameAsString().starts_with(
            "ia2_compartment_destructor")) {
      return false;
    }
  }

  auto annotation = decl.getAttr<clang::AnnotateAttr>();
  if (annotation && annotation->getAnnotation() == SKIP_WRAP_ATTR) {
    return true;
  }

  if (loc) {
    return ignore_file(get_filename(*loc, sm));
  }

  return false;
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

    // This pass modifies source files so it should not change files with pkey 0
    if (get_file_pkey(sm) == 0) {
      return;
    }

    auto loc = old_decl->getLocation();
    auto filename = get_filename(loc, sm);
    if (ignore_function(*old_decl, loc, sm)) {
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
    if (in_fn_like_macro(loc, sm)) {
      auto expansion_file = get_expansion_filename(loc, sm);
      auto expansion_line = sm.getExpansionLineNumber(loc);
      auto spelling_line = sm.getSpellingLineNumber(loc);
      llvm::errs() << "FnPtrTypes: " << expansion_file << ":" << expansion_line
                   << " ";
      if (expansion_line != spelling_line) {
        llvm::errs() << filename << ":" << spelling_line << " ";
      }
      llvm::errs() << "must be rewritten manually\n";
      return;
    }

    // Replace the old decl with the new one. Make sure to include any macro
    // expansions in the SourceRange being replaced
    clang::CharSourceRange expansion_range = sm.getExpansionRange(*range);
    Replacement old_r{sm, expansion_range, new_decl};
    Replacement r = replace_new_file(filename, old_r);

    auto err = file_replacements[filename].add(r);
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
        isAssignmentOperator(), hasRHS(null_expr),
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

    // This pass modifies source files so it should not change files with pkey 0
    if (get_file_pkey(sm) == 0) {
      return;
    }

    clang::SourceLocation loc = null_fn_ptr->getExprLoc();
    Filename filename = get_expansion_filename(loc, sm);

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
    Replacement old_r{sm, expansion_range, new_expr};
    Replacement r = replace_new_file(filename, old_r);
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
 * Rewrites indirect function calls as `IA2_CALL(fn_ptr, ID)`. fn_ptr is
 * the original function pointer expression. ID is an integer assigned by this
 * pass and specific to each function pointer signature.
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
                                                expr(unless(ignoringImplicit(declared_function))).bind("fnPtrExpr")))
                                       .bind("fnPtrCall");

    refactorer.addMatcher(fn_ptr_call, this);
  }
  virtual void run(const MatchFinder::MatchResult &result) {
    auto *fn_ptr_expr = result.Nodes.getNodeAs<clang::Expr>("fnPtrExpr");
    assert(fn_ptr_expr != nullptr);
    auto *fn_ptr_call = result.Nodes.getNodeAs<clang::CallExpr>("fnPtrCall");
    assert(fn_ptr_call != nullptr);

    assert(result.SourceManager != nullptr);
    auto &sm = *result.SourceManager;

    assert(result.Context != nullptr);
    auto &ctxt = *result.Context;

    // This pass modifies source files so it should not change files with pkey 0
    if (get_file_pkey(sm) == 0) {
      return;
    }

    clang::SourceLocation loc = fn_ptr_expr->getExprLoc();
    Filename filename = get_filename(loc, sm);
    if (ignore_file(filename)) {
      return;
    }

    auto callee_decl = fn_ptr_call->getCalleeDecl();
    if (callee_decl && ignore_function(*callee_decl, {}, sm)) {
      return;
    }

    // Check if the function pointer type already has an ID
    auto expr_ty = fn_ptr_expr->getType()->getCanonicalTypeInternal();
    auto expr_mangled_ty = mangle_type(ctxt, expr_ty);
    auto expr_ty_str = expr_ty.getAsString();
    fn_ptr_types[expr_ty_str] = expr_mangled_ty;

    auto *fpt = expr_ty->castAs<clang::PointerType>()
                    ->getPointeeType()
                    ->getAsAdjusted<clang::FunctionProtoType>();
    fn_ptr_abi_sig[expr_ty_str] = determineAbiForProtoType(*fpt, ctxt, Target);

    // This check must come after modifying the maps in this pass but before the
    // Replacement is added
    if (in_fn_like_macro(loc, sm)) {
      auto expansion_file = get_expansion_filename(loc, sm);
      auto expansion_line = sm.getExpansionLineNumber(loc);
      auto spelling_line = sm.getSpellingLineNumber(loc);
      llvm::errs() << "FnPtrCall: " << expansion_file << ":" << expansion_line
                   << " ";
      if (spelling_line != expansion_line) {
        llvm::errs() << filename << ":" << spelling_line << " ";
      }
      llvm::errs() << "must be rewritten manually (" << expr_mangled_ty << ")\n";
      return;
    }

    // getTokenRange is required to replace the entire callee expression
    auto char_range =
        clang::CharSourceRange::getTokenRange(fn_ptr_expr->getSourceRange());

    auto old_expr =
        clang::Lexer::getSourceText(char_range, sm, ctxt.getLangOpts());

    std::string new_expr =
        "IA2_CALL("s + old_expr.str() + ", " + expr_mangled_ty + ")";

    if (in_macro_expansion(loc, sm)) {
      filename = get_expansion_filename(loc, sm);
      char_range = sm.getExpansionRange(loc);
    }

    Replacement old_r{sm, char_range, new_expr};
    Replacement r = replace_new_file(filename, old_r);
    auto err = file_replacements[filename].add(r);
    if (err) {
      llvm::errs() << "Error adding replacements: " << err << '\n';
    }
    return;
  }

  std::map<OpaqueStruct, std::string> fn_ptr_types;
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
    auto call_expr = hasParent(implicitCastExpr(hasParent(callExpr(
        callee(expr(ignoringImplicit(equalsBoundNode("fnPtrExpr"))))))));

    auto in_bin_op = hasAncestor(
        binaryOperator(anyOf(hasOperatorName("=="), hasOperatorName("!=")),
                       hasEitherOperand(expr(
                           ignoringImplicit(equalsBoundNode("fnPtrExpr"))))));

    auto param_expr = hasParent(implicitCastExpr(hasParent(callExpr(
        forEachArgumentWithParam(equalsBoundNode("fnPtrExpr"),
                                 parmVarDecl().bind("fnPtrParamDecl"))))));

    StatementMatcher fn_ptr_expr =
        expr(fn_expr, unless(call_expr), unless(in_bin_op),
             optionally(param_expr));

    refactorer.addMatcher(fn_ptr_expr, this);
  }
  virtual void run(const MatchFinder::MatchResult &result) {
    auto *fn_ptr_expr = result.Nodes.getNodeAs<clang::DeclRefExpr>("fnPtrExpr");
    assert(fn_ptr_expr != nullptr);

    assert(result.SourceManager != nullptr);
    auto &sm = *result.SourceManager;

    assert(result.Context != nullptr);
    auto &ctxt = *result.Context;

    // This pass modifies source files so it should not change files with pkey 0
    if (get_file_pkey(sm) == 0) {
      return;
    }

    clang::SourceLocation loc = fn_ptr_expr->getExprLoc();
    auto *fn_decl =
        llvm::cast<clang::NamedDecl>(fn_ptr_expr->getReferencedDeclOfCallee());
    assert(fn_decl != nullptr);

    if (ignore_function(*fn_decl, loc, sm)) {
      return;
    }

    auto *param_decl = result.Nodes.getNodeAs<clang::ParmVarDecl>("fnPtrParamDecl");
    if (param_decl && ignore_function(*param_decl, {}, sm)) {
      return;
    }

    Function fn_name = fn_decl->getName().str();

    // Unlike the macro expansion check below, this check should go before
    // modifying the maps in this pass because we don't want to treat the signal
    // handlers as functions that have their address-taken and thus require
    // wrappers. We modify the PKRU register directly in the signal handler and
    // can't assume that the PKRU starts with pkey 0.
    if (fn_name.starts_with("ia2_sighandler_")) {
      return;
    }
    std::string new_expr = "IA2_FN("s + fn_name + ")";

    Filename filename = get_expansion_filename(loc, sm);

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

      auto [it, new_fn] = internal_addr_taken_fns[filename].insert(
          std::make_pair(fn_name, new_type));

      // TODO: Note that this only checks if a function is added to the
      // internal_addr_taken_fns map. To make the rewriter idempotent we should
      // check for an existing used attribute.
      if (new_fn) {
        auto decl_start = fn_decl->getBeginLoc();
        Replacement old_used_attr(sm, decl_start, 0,
                                  llvm::StringRef("__attribute__((used)) "));
        Replacement used_attr = replace_new_file(filename, old_used_attr);
        auto err = file_replacements[filename].add(used_attr);
        if (err) {
          llvm::errs() << "Error adding replacements: " << err << '\n';
        }
      }

    } else {
      llvm::errs()
          << "Found declRefExpr in FnPtrExpr pass with unsupported linkage\n";
      return;
    }

    // This check must come after modifying the maps in this pass but before the
    // Replacement is added
    if (in_fn_like_macro(loc, sm)) {
      auto expansion_file = get_expansion_filename(loc, sm);
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
    Replacement old_r{sm, expansion_range, new_expr};
    Replacement r = replace_new_file(filename, old_r);
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

class FnPtrEq : public RefactoringCallback {
public:
  FnPtrEq(ASTMatchRefactorer &refactorer,
          std::map<std::string, Replacements> &file_replacements)
      : file_replacements(file_replacements) {
    auto fn_ptr = pointerType(pointee(ignoringParens(functionType())));

    auto fn_ptr_typedef = hasType(typedefNameDecl(hasType(fn_ptr)));

    auto fn_expr =
        declRefExpr(hasDeclaration(functionDecl())).bind("comparedFn");

    auto ptr = expr(unless(fn_expr), anyOf(fn_ptr_typedef, hasType(fn_ptr)))
                   .bind("ifPtr");

    auto not_ptr = unaryOperator(hasOperatorName("!"), hasUnaryOperand(ptr));

    auto ptr_as_bool = anyOf(ptr, not_ptr);

    auto if_stmt = ifStmt(hasCondition(ptr_as_bool));
    // These need to be two separate matchers since a binary comparison may
    // match ptr_as_bool twice, but we can only bind "ifPtr" once per matcher
    auto lhs_ptr_op = binaryOperator(unless(isAssignmentOperator()),
                                     hasLHS(ignoringImpCasts(ptr_as_bool)));
    auto rhs_ptr_op = binaryOperator(unless(isAssignmentOperator()),
                                     hasRHS(ignoringImpCasts(ptr_as_bool)));

    auto lhs_cmp_fn =
        binaryOperator(anyOf(hasOperatorName("=="), hasOperatorName("!=")),
                       hasLHS(ignoringImpCasts(fn_expr)));
    auto rhs_cmp_fn =
        binaryOperator(anyOf(hasOperatorName("=="), hasOperatorName("!=")),
                       hasRHS(ignoringImpCasts(fn_expr)));

    refactorer.addMatcher(if_stmt, this);
    refactorer.addMatcher(lhs_ptr_op, this);
    refactorer.addMatcher(rhs_ptr_op, this);
    refactorer.addMatcher(lhs_cmp_fn, this);
    refactorer.addMatcher(rhs_cmp_fn, this);
  }
  virtual void run(const MatchFinder::MatchResult &result) {
    const clang::Expr *expr = nullptr;
    bool rewriting_fn;
    if (auto *cmp_fn_expr = result.Nodes.getNodeAs<clang::Expr>("comparedFn")) {
      rewriting_fn = true;
      expr = cmp_fn_expr;
    } else if (auto *if_ptr_expr =
                   result.Nodes.getNodeAs<clang::Expr>("ifPtr")) {
      rewriting_fn = false;
      expr = if_ptr_expr;
    }
    assert(expr != nullptr);

    assert(result.SourceManager != nullptr);
    auto &sm = *result.SourceManager;

    assert(result.Context != nullptr);
    auto &ctxt = *result.Context;

    // This pass modifies source files so it should not change files with pkey 0
    if (get_file_pkey(sm) == 0) {
      return;
    }

    clang::SourceLocation loc = expr->getExprLoc();
    Filename filename = get_filename(loc, sm);
    if (ignore_file(filename)) {
      return;
    }

    if (in_fn_like_macro(loc, sm)) {
      auto expansion_file = get_expansion_filename(loc, sm);
      auto expansion_line = sm.getExpansionLineNumber(loc);
      auto spelling_line = sm.getSpellingLineNumber(loc);
      llvm::errs() << "FnPtrEq: " << expansion_file << ":" << expansion_line
                   << " ";
      if (spelling_line != expansion_line) {
        llvm::errs() << filename << ":" << spelling_line << " ";
      }
      llvm::errs() << "must be rewritten manually\n";
      return;
    }

    auto char_range =
        clang::CharSourceRange::getTokenRange(expr->getSourceRange());
    auto orig_expr =
        clang::Lexer::getSourceText(char_range, sm, ctxt.getLangOpts());
    std::string new_expr;
    if (rewriting_fn) {
      new_expr = "IA2_FN_ADDR(" + orig_expr.str() + ")";
    } else {
      new_expr = "IA2_ADDR(" + orig_expr.str() + ")";
    }

    if (in_macro_expansion(loc, sm)) {
      filename = get_expansion_filename(loc, sm);
      char_range = sm.getExpansionRange(loc);
    }

    Replacement old_r{sm, char_range, new_expr};
    Replacement r = replace_new_file(filename, old_r);
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

    Function fn_name = fn_node->getNameAsString();

    // Ignore declarations in libc and libia2 headers
    if (ignore_function(*fn_node, fn_node->getLocation(), sm)) {
      return;
    }

    // Calls to compiler builtins produce an inline declaration that should
    // not be wrapped; we also don't want to wrap explicit decls of builtins
    if (fn_node->getBuiltinID() != 0) {
      return;
    }

    if (fn_node->isVariadic()) {
      static std::set<Function> variadic_warnings_printed = {};
      if (variadic_warnings_printed.contains(fn_name)) {
        return;
      }
      variadic_warnings_printed.insert(fn_name);
      llvm::errs()
          << "Wrapping variadic functions is not yet supported, skipping ";
      fn_node->getNameForDiagnostic(
          llvm::errs(), fn_node->getASTContext().getPrintingPolicy(), true);
      llvm::errs() << "\n";
      return;
    }

    CAbiSignature fn_sig = determineAbiForDecl(*fn_node, Target);
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

static void create_ld_file(llvm::raw_fd_ostream *file[MAX_PKEYS], int i) {
  if (file[i] == nullptr) {
    std::error_code EC;
    file[i] = new llvm::raw_fd_ostream(
        OutputPrefix + "_" + std::to_string(i) + ".ld", EC);
    assert(file[i] != nullptr);
  }
}

static void write_to_ld_file(llvm::raw_fd_ostream *file[MAX_PKEYS], int i,
                             const std::string &contents) {
  create_ld_file(file, i);
  *file[i] << contents;
}

/* Copy files to the output directory.
 * Populates `rel_path_to_full` which is used by `get_expansion_filename` and
 * `get_filename`.
 */
std::set<llvm::SmallString<256>> copy_files(std::vector<std::unique_ptr<clang::ASTUnit>> &asts,
                                            const CompilationDatabase &comp_db,
                                            const std::string &input_root,
                                            const std::string &output_dir) {
  std::set<llvm::SmallString<256>> copied_files;
  for (auto &ast : asts) {
    auto &sm = ast->getSourceManager();
    for (auto file_it = sm.fileinfo_begin(); file_it != sm.fileinfo_end();
         file_it++) {

      llvm::SmallString<256> input_file(file_it->getFirst()->getName());
      llvm::SmallString<256> output_file(input_file);
      bool needs_copy = false;
      if (llvm::sys::path::is_relative(input_file)) {
        // FileManager::makeAbsolutePath just prepends the working directory
        // which is not what we want here. Luckily we only have to
        // canonicalize paths once then we can reuse the same results.

        auto main_c_file = ast->getMainFileName().str();
        auto cc_cmd = comp_db.getCompileCommands(main_c_file);

        assert(cc_cmd.size() == 1);

        auto rel_path = file_it->getFirst()->getName().str();
        input_file = cc_cmd[0].Directory;
        input_file.append("/" + rel_path);
        output_file = input_file;
        llvm::sys::path::replace_path_prefix(output_file, input_root, output_dir);

        rel_path_to_full[rel_path] = output_file.str().str();
        needs_copy = true;
      } else {
        needs_copy = llvm::sys::path::replace_path_prefix(output_file, input_root, output_dir);
      }
      if (needs_copy && !copied_files.contains(input_file)) {
        copied_files.insert(input_file);

        std::vector<std::string> subdirs = {};
        auto parent_dir = llvm::sys::path::parent_path(output_file.str());

        while (!opendir(parent_dir.str().c_str())) {
          subdirs.push_back(parent_dir.str());
          parent_dir = llvm::sys::path::parent_path(parent_dir);
        }

        for (auto subdir_it = subdirs.rbegin(); subdir_it != subdirs.rend();
             subdir_it++) {
          int mkdir_rc = mkdir(subdir_it->c_str(), 0766);
          if (mkdir_rc != 0) {
            llvm::errs() << "Failed to create dir " << subdir_it->c_str() << ": " << strerror(errno) << '\n';
          }
          assert(mkdir_rc == 0);
        }

        std::ifstream src(input_file.str().str(), std::ios::binary);
        std::ofstream dst(output_file.str().str(), std::ios::binary);
        dst << src.rdbuf();
      }
    }
  }
  return copied_files;
}

std::optional<Pkey> pkey_from_commands(std::function<std::optional<std::vector<CompileCommand>>(std::string &)> get_commands, std::string s) {
  std::cout << "looking at commands for " << s << std::endl;
  // Get the compile commands for each input source
  auto commands = get_commands(s);
  if (commands == std::nullopt) {
    llvm::errs() << "No compilation command found for " << s.c_str()
                 << '\n';
    return {};
  }

  auto comp_cmds = *commands;
  auto is_pkey_define = [](const std::string &s) {
    return s.starts_with(PKEY_DEFINE);
  };

  // Files may be compiled more than once, but we don't support that yet. If
  // the file is not found in the compilation database then there's a problem
  // with it.
  auto has_pkey_define = [=](const CompileCommand &cc) {
    for (const auto &flag : cc.CommandLine) {
      if (is_pkey_define(flag)) {
        return true;
      }
    }
    return false;
  };
  // All files must be compiled with -DPKEY=N
  auto comp_cmd_with_pkey =
      std::find_if(comp_cmds.begin(), comp_cmds.end(), has_pkey_define);
  if (comp_cmd_with_pkey == comp_cmds.end()) {
    llvm::errs() << "No compilation command with -DPKEY found for " << s.c_str()
                 << '\n';
    llvm::errs() << "Modify compile_commands.json to specify the compartment "
                 << "for this file by adding -DPKEY=N (for compartment N) to "
                 << "its compilation command line."
                 << "\n";
    return {};
  }

  assert(comp_cmds.size() == 1);
  auto cc_cmd = *comp_cmd_with_pkey;

  auto pkey_define = std::find_if(cc_cmd.CommandLine.begin(),
                                  cc_cmd.CommandLine.end(), is_pkey_define);

  if (pkey_define == cc_cmd.CommandLine.end()) {
    llvm::errs() << cc_cmd.Filename.c_str()
                 << " was not compiled with -DPKEY=\n";
    for (const auto &flag : cc_cmd.CommandLine) {
      llvm::errs() << flag.c_str() << " ";
    }
    llvm::errs() << '\n';
    return {};
  }

  assert(s == cc_cmd.Filename);

  // Using sizeof - 1 avoids counting the null terminator in PKEY_DEFINE
  Pkey pkey = std::stoi(pkey_define->substr(sizeof(PKEY_DEFINE) - 1));

  return pkey;
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
  tool.appendArgumentsAdjuster([&](const CommandLineArguments &args, llvm::StringRef filename) {
    CommandLineArguments new_args(args);
    // Try to remove existing definition from command line to avoid warnings.
    new_args.erase(std::remove_if(new_args.begin(), new_args.end(),
                                  [](std::string &x) { return x.starts_with("-DIA2_ENABLE="); }),
                   new_args.end());
    new_args.push_back("-DIA2_ENABLE=0"s);
    if (Target == Arch::Aarch64) {
      new_args.push_back("--target=aarch64-linux-gnu"s);
    }
    return new_args;
  });
  CompilationDatabase &comp_db = options_parser.getCompilations();

  // Copy files to output directory before modifying them in place
  auto asts = std::vector<std::unique_ptr<clang::ASTUnit>>();
  tool.buildASTs(asts);
  auto copied_files = copy_files(asts, comp_db, RootDirectory, OutputDirectory);

  auto all_files = comp_db.getAllFiles();
  auto all_files_set = std::set(all_files.begin(), all_files.end());
  auto get_commands = [&](std::string &s) -> std::optional<std::vector<CompileCommand>> {
    // getCompileCommands returns bogus results instead of nothing for files not
    // present in the compile_commands.json, so filter them out explicitly
    if (!all_files_set.contains(s)) {
      return std::nullopt;
    }
    return {comp_db.getCompileCommands(llvm::StringRef(s))};
  };

  // Collect mapping of filenames to pkeys and used pkeys
  std::set<Pkey> pkeys_used;
  for (auto s : options_parser.getSourcePathList()) {
    auto pkey = pkey_from_commands(get_commands, s);
    if (!pkey) {
      return -1;
    }

    file_pkeys[s] = *pkey;
    pkeys_used.insert(*pkey);
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
  FnPtrEq eq_ptr_pass(refactorer, tool.getReplacements());

  auto rc = tool.runAndSave(newFrontendActionFactory(&refactorer).get());
  if (rc != 0) {
    return rc;
  }

  header_out << "#include <ia2.h>\n";
  header_out << "#include <scrub_registers.h>\n";

  header_out << '\n';

  wrapper_out << "#include <ia2.h>\n";
  wrapper_out << "#include <scrub_registers.h>\n";

  /*
   * Define wrappers for IA2_CALL. These switch from the caller's pkey to pkey 0
   * so we don't need to generate them for caller_pkey = 0. When IA2_CALL has
   * caller pkey = 0, it just becomes a cast that calls the fn ptr.
   */
  std::cout << "Generating indirect callsite wrappers\n";
  std::string wrapper_decls;
  std::set<OpaqueStruct> type_id_macros_generated = {};
  for (int caller_pkey = 1; caller_pkey < num_pkeys; caller_pkey++) {
    for (const auto &[ty, mangled_ty] : ptr_call_pass.fn_ptr_types) {
      CAbiSignature c_abi_sig;
      try {
        c_abi_sig = ptr_call_pass.fn_ptr_abi_sig.at(ty);
      } catch (std::out_of_range const &exc) {
        llvm::errs() << "Opaque struct " << ty.c_str()
                     << " not found by FnPtrCall pass\n";
        abort();
      }
      std::string wrapper_name = "__ia2_indirect_callgate_"s +
                                 mangled_ty + "_pkey_" +
                                 std::to_string(caller_pkey);

      std::string asm_wrapper =
          emit_asm_wrapper(c_abi_sig, wrapper_name, std::nullopt,
                           WrapperKind::IndirectCallsite, caller_pkey, 0, Target);
      wrapper_out << "asm(\n";
      wrapper_out << asm_wrapper;
      wrapper_out << ");\n";

      if (!type_id_macros_generated.contains(mangled_ty)) {
        header_out << "#define IA2_TYPE_"s << mangled_ty << " " << ty << "\n";
        type_id_macros_generated.insert(mangled_ty);
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
    create_ld_file(ld_args_out, caller_pkey);
    // For each compartment find the functions that are declared but not defined
    std::set<Function> undefined_fns = {};
    std::set_difference(fn_decl_pass.declared_fns[caller_pkey].begin(),
                        fn_decl_pass.declared_fns[caller_pkey].end(),
                        fn_decl_pass.defined_fns[caller_pkey].begin(),
                        fn_decl_pass.defined_fns[caller_pkey].end(),
                        std::inserter(undefined_fns, undefined_fns.begin()));
    for (const auto &fn_name : undefined_fns) {
      CAbiSignature c_abi_sig;
      try {
        c_abi_sig = fn_decl_pass.abi_signatures.at(fn_name);
      } catch (std::out_of_range const &exc) {
        llvm::errs() << "C ABI signature for function " << fn_name.c_str()
                     << " not found by FnDecl pass\n";
        abort();
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
          emit_asm_wrapper(c_abi_sig, wrapper_name, fn_name,
                           WrapperKind::Direct, caller_pkey, target_pkey, Target);
      wrapper_out << "asm(\n";
      wrapper_out << asm_wrapper;
      wrapper_out << ");\n";

      write_to_ld_file(ld_args_out, caller_pkey, "--wrap="s + fn_name + '\n');
    }
  }

  // Create wrapper for compartment destructor
  for (int compartment_pkey = 1; compartment_pkey < num_pkeys; compartment_pkey++) {
    std::string fn_name = "ia2_compartment_destructor_" + std::to_string(compartment_pkey);
    CAbiSignature c_abi_sig;
    try {
      c_abi_sig = fn_decl_pass.abi_signatures.at(fn_name);
    } catch (std::out_of_range const &exc) {
      llvm::errs() << "Could not find ia2_compartment_destructor_" << compartment_pkey << '\n'
                   << "Make sure to #include ia2_compartment_init.inc for this compartment\n";
      abort();
    }
    std::string wrapper_name = "__wrap_"s + fn_name;
    std::string asm_wrapper =
        emit_asm_wrapper(c_abi_sig, wrapper_name, fn_name, WrapperKind::Direct,
                         0, compartment_pkey, Target);
    wrapper_out << "asm(\n";
    wrapper_out << asm_wrapper;
    wrapper_out << ");\n";

    write_to_ld_file(ld_args_out, compartment_pkey,
                     "--wrap="s + fn_name + '\n');
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
          emit_asm_wrapper(c_abi_sig, wrapper_name, fn_name,
                           WrapperKind::Pointer, 0, target_pkey, Target);
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
    for (const auto [fn_name, opaque] : addr_taken_fns) {
      std::string wrapper_name = "__ia2_"s + fn_name;
      // TODO: These wrapper go from pkey 0 to the target pkey so if the target
      // also has pkey 0 then it just needs call the original function
      Pkey target_pkey = fn_decl_pass.fn_pkeys[fn_name];
      if (target_pkey != 0) {
        CAbiSignature c_abi_sig = fn_decl_pass.abi_signatures[fn_name];

        std::string asm_wrapper = emit_asm_wrapper(
            c_abi_sig, wrapper_name, fn_name, WrapperKind::Pointer, 0,
            target_pkey, Target, true /* as_macro */);
        static_wrappers += "#define IA2_DEFINE_WRAPPER_"s + fn_name + " \\\n";
        static_wrappers += "asm(\\\n";
        static_wrappers += asm_wrapper;
        static_wrappers += ");\n";

        header_out << "extern " << opaque << " " << wrapper_name << ";\n";

        source_file << "IA2_DEFINE_WRAPPER(" << fn_name << ")\n";
      }
    }
  }
  const char *undef_insn_x86 = "ud2";
  const char *undef_insn_arm = "udf #0";
  const char *undef_insn;
  if (Target == Arch::Aarch64) {
    undef_insn = undef_insn_arm;
  } else {
    undef_insn = undef_insn_x86;
  }
  header_out << "asm(\"__libia2_abort:\\n\"\n"
             << "    \"" << undef_insn << "\");\n";
  header_out << static_wrappers.c_str();

  for (int i = 0; i < num_pkeys; i++) {
    if (ld_args_out[i] != nullptr) {
      ld_args_out[i]->close();
    }
  }

  return EXIT_SUCCESS;
}
