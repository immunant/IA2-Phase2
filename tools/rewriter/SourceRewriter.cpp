#include "CLI11.hpp"
#include "Context.h"
#include "DetermineAbi.h"
#include "GenCallAsm.h"
#ifdef IA2_LIBC_COMPARTMENT
#include "ia2_compartment_ids.h"
#include "LdsoRegistry.h"
#endif
#include "TypeOps.h"
#include "clang/AST/AST.h"
#include "clang/AST/Type.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/Linkage.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/Version.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/RefactoringCallbacks.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/JSON.h"
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
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>

#define PKEY_DEFINE "-DPKEY="
#define MAX_PKEYS 16

using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace std::string_literals;

static constexpr llvm::StringLiteral SKIP_WRAP_ATTR("ia2_skip_wrap");
static constexpr llvm::StringLiteral PRE_CONDITION_ATTR_PREFIX("ia2_pre_condition:");
static constexpr llvm::StringLiteral POST_CONDITION_ATTR_PREFIX("ia2_post_condition:");
static constexpr llvm::StringLiteral CONSTRUCTOR_ATTR("ia2_constructor");
static constexpr llvm::StringLiteral DESTRUCTOR_ATTR("ia2_destructor");

typedef std::string Filename;
typedef int Pkey;
typedef std::string OpaqueStruct;

static Arch Target = Arch::X86;
static std::string RootDirectory;
static std::string OutputDirectory;
static std::string OutputPrefix;

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
  // if we could not get a filename for the spelling loc, use the expansion filename
  if (s == "") {
    s = sm.getFilename(sm.getExpansionLoc(loc));
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

// Return whether a file should not be modified, due to being an immutable
// system header or otherwise outside the output directory.
static bool should_not_modify_file(const Filename &filename) {
  bool is_empty = filename.empty();
  if (is_empty) {
    return false;
  }

  // We shouldn't query if we should modify files in the root directory. But if
  // the output directory itself is inside the root directory, this will
  // (benignly) happen, and isn't actually a case of trying to modify files not
  // inside the output directory.
  if (filename.starts_with(RootDirectory) && !filename.starts_with(OutputDirectory)) {
    llvm::errs() << "internal error: querying if we should modify file under root directory (this should not happen): " << filename << "\n";
    exit(1);
  }

  return !filename.starts_with(OutputDirectory);
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
    return should_not_modify_file(get_filename(*loc, sm));
  }

  return false;
}

static std::string append_name_if_nonempty(const std::string &new_type,
                                           const std::string &name) {
  return new_type + (name.empty() ? "" : " ") + name;
};

/// Collects in a multimap (`funcs`) all of the function names
/// with an annotation starting with `prefix`.
/// The annotation minus the prefix is the key,
/// and the function name is the value.
///
/// This is, for example, used for
/// pre- and post-condition function annotations.
class AnnotationPrefixFunctions : public MatchFinder::MatchCallback {

public:
  const std::string_view prefix;
  /// Key is suffix, value is function name.
  std::unordered_multimap<std::string, Function> funcs;

  AnnotationPrefixFunctions(std::string_view prefix) : prefix(prefix) {}

  void run(const MatchFinder::MatchResult &result) override {
    if (const auto *func = result.Nodes.getNodeAs<clang::FunctionDecl>("annotatedFunc")) {
      for (const auto *attr : func->attrs()) {
        if (const auto *annotate_attr = llvm::dyn_cast<clang::AnnotateAttr>(attr)) {
          llvm::StringRef annotation = annotate_attr->getAnnotation();
          if (!annotation.consume_front(prefix)) {
            continue;
          }
          const auto func_name = func->getNameInfo().getName().getAsString();
          funcs.emplace(annotation, func_name);
        }
      }
    }
  }
};

/// Finds all constructors (`IA2_CONSTRUCTOR`) and destructors (`IA2_DESTRUCTOR`),
/// checks that the signature matches `void f(T*)`,
/// interns the type `T`, and sets its {con,de}structor.
class Structors : public MatchFinder::MatchCallback {

private:
  TypeInfoInterner &types;

public:
  Structors(TypeInfoInterner &types) : types(types) {}

  void run(const MatchFinder::MatchResult &result) override {
    if (const auto *func = result.Nodes.getNodeAs<clang::FunctionDecl>("annotatedFunc")) {
      for (const auto *attr : func->attrs()) {
        if (const auto *annotate_attr = llvm::dyn_cast<clang::AnnotateAttr>(attr)) {
          const llvm::StringRef annotation = annotate_attr->getAnnotation();
          const auto is_constructor = annotation == CONSTRUCTOR_ATTR;
          const auto is_destructor = annotation == DESTRUCTOR_ATTR;
          assert(!(is_constructor && is_destructor)); // TODO better error message
          if (!(is_constructor || is_destructor)) {
            continue;
          }
          const auto func_name = func->getNameInfo().getName().getAsString();
          const auto &params = func->parameters();
          if (is_constructor) {
            assert(params.size() >= 1); // TODO better error message
          }
          if (is_destructor) {
            assert(params.size() == 1); // TODO better error message
          }
          // Should be `T*`.
          const auto type = params[0]->getOriginalType();
          assert(type.getTypePtr()->isPointerType());
          // Intern the `T*` ptr type, not `T` itself.
          // This makes it simpler to re-look it up.
          auto &info = types.get(types.intern(type));
          if (is_constructor) {
            info.set_constructor(func_name);
          }
          if (is_destructor) {
            info.set_destructor(func_name);
          }
        }
      }
    }
  }
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
      old_type = type_alias_decl->getUnderlyingType();
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
  FnPtrCall(
      Context &ctx,
      ASTMatchRefactorer &refactorer,
      std::map<std::string, Replacements> &file_replacements)
      : ctx(ctx), file_replacements(file_replacements) {
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
    if (should_not_modify_file(filename)) {
      return;
    }

    auto callee_decl = fn_ptr_call->getCalleeDecl();
    if (callee_decl && ignore_function(*callee_decl, {}, sm)) {
      return;
    }

    // Check if the function pointer type already has an ID
    auto expr_ty = fn_ptr_expr->getType()->getCanonicalTypeInternal();
    auto mangled_ty = mangle_type(ctxt, expr_ty);

    auto info = fn_ptr_info.find(mangled_ty);
    if (info == fn_ptr_info.end()) {
      // Add the hidden pointer argument to the function pointer arguments
      auto fn_ptr_ty = expr_ty->getAs<clang::PointerType>();
      assert(fn_ptr_ty);
      auto dest_fn_ty = fn_ptr_ty->getPointeeType()->getAs<clang::FunctionProtoType>();
      assert(dest_fn_ty);
      std::vector<clang::QualType> args = {dest_fn_ty->param_type_begin(), dest_fn_ty->param_type_end()};
      args.insert(args.begin(), ctxt.VoidPtrTy);
      auto wrap_fn_ty = ctxt.getFunctionType(dest_fn_ty->getReturnType(), args, dest_fn_ty->getExtProtoInfo());
      auto wrap_fn_ptr_ty = ctxt.getPointerType(wrap_fn_ty);
      auto wrap_fn_prototype = wrap_fn_ty->getAs<clang::FunctionProtoType>();

      auto expr_ty_str = wrap_fn_ptr_ty.getAsString();

      auto sig = determineFnSignatureForProtoType(ctx, *dest_fn_ty, ctxt, Target);
      auto wrapper_sig = determineFnSignatureForProtoType(ctx, *wrap_fn_prototype, ctxt, Target);

      auto res = fn_ptr_info.emplace(mangled_ty, FnPtrInfo({expr_ty_str, sig, wrapper_sig}));
      info = res.first;
    }

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
      llvm::errs() << "must be rewritten manually (" << mangled_ty << ")\n";
      return;
    }

    // getTokenRange is required to replace the entire callee expression
    auto callee_char_range =
        clang::CharSourceRange::getTokenRange(fn_ptr_expr->getSourceRange());

    auto old_callee =
        clang::Lexer::getSourceText(callee_char_range, sm, ctxt.getLangOpts());

    std::string new_expr =
        "IA2_CALL("s + old_callee.str() + ", " + mangled_ty;

    for (auto const &arg : fn_ptr_call->arguments()) {
      auto char_range = clang::CharSourceRange::getTokenRange(arg->getSourceRange());
      new_expr += ", " + clang::Lexer::getSourceText(char_range, sm, ctxt.getLangOpts()).str();
    }
    new_expr += ")";

    auto char_range =
        clang::CharSourceRange::getTokenRange(fn_ptr_call->getSourceRange());

    if (in_macro_expansion(char_range.getBegin(), sm)) {
      filename = get_expansion_filename(char_range.getBegin(), sm);
      auto expanded_char_range = sm.getExpansionRange(char_range.getBegin());
      char_range = clang::CharSourceRange::getTokenRange(expanded_char_range.getBegin(), char_range.getEnd());
    }

    Replacement old_r{sm, char_range, new_expr};
    Replacement r = replace_new_file(filename, old_r);
    auto err = file_replacements[filename].add(r);
    if (err) {
      llvm::errs() << "Error adding replacements: " << err << '\n';
    }
    return;
  }

  struct FnPtrInfo {
    std::string type_str;
    FnSignature sig;
    FnSignature wrapper_sig;
  };

  // Mapping from mangled type name to function pointer info
  std::map<OpaqueStruct, FnPtrInfo> fn_ptr_info;

private:
  Context &ctx;

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
    if (clang::isExternallyVisible(linkage)) {
      addr_taken_fns[fn_name] = new_type;
    } else {

      auto [it, new_fn] = internal_addr_taken_fns[filename].insert(
          std::make_pair(fn_name, new_type));

      // TODO: Note that this only checks if a function is added to the
      // internal_addr_taken_fns map. To make the rewriter idempotent we should
      // check for an existing used attribute.
      if (new_fn) {
        auto static_fn_range = fn_decl->getSourceRange();
        auto expansion_range = sm.getExpansionRange(static_fn_range);
        if (!expansion_range.getBegin().isFileID()) {
          llvm::errs() << "Error: non-file loc for function " << fn_name << '\n';
        } else {
          auto decl_start = expansion_range.getBegin();
          Filename decl_filename = get_filename(decl_start, sm);
          Replacement old_used_attr(sm, decl_start, 0,
                                    llvm::StringRef("__attribute__((used)) "));
          Replacement used_attr = replace_new_file(decl_filename, old_used_attr);
          auto err = file_replacements[decl_filename].add(used_attr);
          if (err) {
            llvm::errs() << "Error adding replacements: " << err << '\n';
          }
        }
      }
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

    auto ternary_condition = conditionalOperator(hasCondition(ptr_as_bool));

    refactorer.addMatcher(if_stmt, this);
    refactorer.addMatcher(lhs_ptr_op, this);
    refactorer.addMatcher(rhs_ptr_op, this);
    refactorer.addMatcher(lhs_cmp_fn, this);
    refactorer.addMatcher(rhs_cmp_fn, this);
    refactorer.addMatcher(ternary_condition, this);
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
    if (should_not_modify_file(filename)) {
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
  FnDecl(Context &ctx,
         ASTMatchRefactorer &refactorer,
         std::map<std::string, Replacements> &replacements)
      : ctx(ctx) {
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

    // For system headers, we want to track the declarations but not modify them
    bool is_system_header = sm.isInSystemHeader(fn_node->getLocation());

    // Skip if we should ignore (but still track system library declarations)
    if (!is_system_header && ignore_function(*fn_node, fn_node->getLocation(), sm)) {
      return;
    }

    // Calls to compiler builtins produce an inline declaration that should
    // not be wrapped; we also don't want to wrap explicit decls of builtins
    if (fn_node->getBuiltinID() != 0) {
      return;
    }

    if (fn_node->isVariadic()) {
      // Track variadic system functions for special handling
      if (sm.isInSystemHeader(fn_node->getLocation())) {
        variadic_system_fns.insert(fn_name);
      }

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

    FnSignature fn_sig = determineFnSignatureForDecl(ctx, *fn_node, Target);
    fn_signatures[fn_name] = fn_sig;

    // Get the translation unit's filename to figure out the pkey
    Pkey pkey = get_file_pkey(sm);
    assert(pkey < MAX_PKEYS);

    if (definition) {
      defined_fns[pkey].insert(fn_name);
      fn_pkeys[fn_name] = pkey;
      Filename filename = get_expansion_filename(fn_node->getLocation(), sm);
      fn_definitions[fn_name] = filename;
    } else {
      declared_fns[pkey].insert(fn_name);

      // Track if this is from a system header for later use
      if (fn_node && sm.isInSystemHeader(fn_node->getLocation())) {
        system_header_fns.insert(fn_name);
      }
      // Don't automatically assign system functions to compartment 1 here
      // We'll only assign them if they're actually called (tracked by CallTracker)
    }
  }

private:
  Context &ctx;

public:
  std::set<Function> defined_fns[MAX_PKEYS];
  std::set<Function> declared_fns[MAX_PKEYS];
  std::set<Function> called_fns[MAX_PKEYS];  // Track functions that are actually called
  std::set<Function> system_header_fns;  // Track functions declared in system headers
  std::set<Function> variadic_system_fns;  // Track variadic functions from system headers
  std::map<Function, FnSignature> fn_signatures;
  std::map<Function, Pkey> fn_pkeys;
  std::map<Function, Filename> fn_definitions;
};

// Track function calls to determine which functions are actually used
class CallTracker : public RefactoringCallback {
public:
  CallTracker(ASTMatchRefactorer &refactorer, FnDecl &fn_decl_pass)
      : fn_decl_pass(fn_decl_pass) {
    // Match direct function calls
    StatementMatcher call_matcher =
        callExpr(callee(functionDecl().bind("calledFunction"))).bind("call");
    refactorer.addMatcher(call_matcher, this);
  }

  virtual void run(const MatchFinder::MatchResult &result) {
    auto *call = result.Nodes.getNodeAs<clang::CallExpr>("call");
    auto *fn = result.Nodes.getNodeAs<clang::FunctionDecl>("calledFunction");

    if (!call || !fn) return;

    auto &sm = *result.SourceManager;

    // Get the compartment of the caller
    Pkey pkey = get_file_pkey(sm);

    // Track this function as being called from this compartment
    std::string fn_name = fn->getNameAsString();
    fn_decl_pass.called_fns[pkey].insert(fn_name);

    // Also track if this is a variadic system function call
    if (fn->isVariadic() && sm.isInSystemHeader(fn->getLocation())) {
      fn_decl_pass.variadic_system_fns.insert(fn_name);
    }
  }

private:
  FnDecl &fn_decl_pass;
};

static void create_file(llvm::raw_fd_ostream *file[MAX_PKEYS], int i, const char *extension) {
  if (file[i] == nullptr) {
    std::error_code EC;
    file[i] = new llvm::raw_fd_ostream(
        OutputPrefix + "_" + std::to_string(i) + extension, EC);
    assert(file[i] != nullptr);
  }
}

static void create_ld_file(llvm::raw_fd_ostream *file[MAX_PKEYS], int i) {
  create_file(file, i, ".ld");
}

static void write_to_file(llvm::raw_fd_ostream *file[MAX_PKEYS], int i,
                          const std::string &contents, const char *extension) {
  create_file(file, i, extension);
  *file[i] << contents;
}

static std::string file_contents(const std::string &path) {
  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> fileOrError = llvm::MemoryBuffer::getFile(path);
  if (!fileOrError) {
    llvm::errs() << "Error reading file: " << fileOrError.getError().message();
    abort();
  }

  llvm::MemoryBuffer *buffer = fileOrError->get();
  llvm::StringRef fileContents = buffer->getBuffer();
  return fileContents.str();
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

#if CLANG_VERSION_MAJOR <= 17
      auto name = file_it->getFirst()->getName();
#else
      auto name = file_it->getFirst().getName();
#endif
      llvm::SmallString<256> input_file(name);
      llvm::SmallString<256> output_file(input_file);
      bool needs_copy = false;
      if (llvm::sys::path::is_relative(input_file)) {
        // FileManager::makeAbsolutePath just prepends the working directory
        // which is not what we want here. Luckily we only have to
        // canonicalize paths once then we can reuse the same results.

        auto main_c_file = ast->getMainFileName().str();
        auto cc_cmd = comp_db.getCompileCommands(main_c_file);

        assert(cc_cmd.size() == 1);

        auto rel_path = name.str();
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
        if (llvm::sys::fs::equivalent(input_file, output_file)) {
          llvm::errs() << "skipping copying file to itself: " << input_file << "\n";
          continue;
        }

        copied_files.insert(input_file);

        auto ignore_existing = true;
        using llvm::sys::fs::perms;
        llvm::sys::fs::create_directories(llvm::sys::path::parent_path(output_file), ignore_existing, perms::all_all & ~perms::group_exe & ~perms::others_exe);

        llvm::sys::fs::copy_file(input_file, output_file);
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

  if (comp_cmds.size() != 1) {
    llvm::errs() << "warning: multiple compile commands for a single source file" << '\n'
                 << "         truncating to first compile command" << '\n'
                 << "         may not work properly" << '\n';
    comp_cmds.resize(1);
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

// There's no way to create a CompilationDatabase from JSON in-memory or even from a file path.
// Instead, write the JSON to a correctly-named file in a temp directory and load from there.
std::unique_ptr<CompilationDatabase> cc_db_from_json(const std::string &json) {
  using namespace llvm::sys::fs;
  llvm::SmallVector<char> unique_dir;
  auto err = createUniqueDirectory("ia2-rewriter-temp-%%%%%%%", unique_dir);
  if (err) {
    llvm::errs() << "Could not create unique directory for temporary compile_commands.json";
    abort();
  }
  int fd = -1;
  err = openFileForWrite(unique_dir + "/compile_commands.json", fd, CD_CreateAlways, OF_None);
  if (err) {
    llvm::errs() << "Could not open temporary compile_commands.json file for write";
    abort();
  }
  llvm::raw_fd_ostream ostream(fd, true);
  ostream << json;
  ostream.flush();
  ostream.close();
  std::string error;
  auto db = CompilationDatabase::loadFromDirectory(llvm::Twine(unique_dir).getSingleStringRef(), error);
  if (!error.empty()) {
    llvm::errs() << error;
  }
  err = remove_directories(unique_dir);
  return db;
}

auto ValidateCcDbPathOrDirectory = CLI::Validator(
    [](std::string &input) {
      if (CLI::ExistingDirectory(input) == ""s) {
        input += "/compile_commands.json";
        llvm::errs() << "warning: Accepting directory containing compile_commands.json. Please pass the path to the JSON directly.\n";
        return ""s;
      } else if (CLI::ExistingFile(input) == ""s) {
        return ""s;
      } else {
        return "Argument should be a path to an existing compiler_commands.json compilation database: "s + input;
      }
    },
    "COMPILE_COMMANDS database", "compile_commands.json or containing dir");

auto ValidateDirectory = CLI::Validator(
    [](std::string &input) {
      auto e = CLI::ExistingDirectory(input);
      if (e != "") {
        return e;
      }
      llvm::SmallString<PATH_MAX> real_path;
      auto ec = llvm::sys::fs::real_path(input, real_path);
      if (ec) {
        return ec.message();
      }
      input = std::string(real_path) + "/";
      return ""s;
    },
    "Directory", "path of a directory");

const std::map<std::string, int> arch_map{{"x86", (int)Arch::X86}, {"aarch64", (int)Arch::Aarch64}};

std::string CompilationDbPath;
std::vector<std::string> SourceFiles;
std::vector<std::string> ExtraArgs;

int main(int argc, const char **argv) {
  CLI::App app{"IA2 Source Rewriter"};
  app.add_option("--arch", Target, "Target architecture (x86 or aarch64), x86 by default")
      ->option_text("x86|aarch64")
      ->transform(CLI::Transformer(arch_map));
  app.add_option("--output-directory", OutputDirectory, "Root directory for output files")
      ->option_text("<DIR> (REQUIRED)")
      ->transform(ValidateDirectory)
      ->required();
  app.add_option("--root-directory", RootDirectory, "Root directory for input files")
      ->option_text("<DIR> (REQUIRED)")
      ->transform(ValidateDirectory)
      ->required();
  app.add_option("--output-prefix", OutputPrefix, "Path prefix for output files")
      ->option_text("<FILENAME-PREFIX> (REQUIRED)")
      ->required();
  app.add_option("-p,--cc-db", CompilationDbPath, "Path to compile_commands.json")
      ->option_text("compile_commands.json (REQUIRED)")
      ->required()
      ->transform(ValidateCcDbPathOrDirectory);
  app.add_option("--extra-arg", ExtraArgs, "Arguments to add to compilation of each source file")
      ->option_text("<CFLAG>")
      ->allow_extra_args(false); // Do not consume positional args
  app.add_option("source-files", SourceFiles, "List of source files to process")
      ->option_text("<FILES>")
      ->check(CLI::ExistingFile);

  CLI11_PARSE(app, argc, argv);

  auto MaybeCmds = cc_db_from_json(file_contents(CompilationDbPath));
  if (!MaybeCmds) {
    llvm::errs() << "Could not parse compilation database from JSON.";
    return -1;
  }
  CompilationDatabase &comp_db = *MaybeCmds;

  // Ensure that all files to process are in the compilation db; if not, we don't know how to process them!
  auto all_files = comp_db.getAllFiles();
  auto all_files_set = std::set(all_files.begin(), all_files.end());
  for (auto &s : SourceFiles) {
    if (!all_files_set.contains(s)) {
      llvm::errs() << "File to process not present in compilation DB: " << s << "\n";
      return -1;
    }
  }

  RefactoringTool tool(comp_db, SourceFiles);

  // Ensure that exactly one -DIA2_ENABLE is present, defined to zero, so we do
  // not try to rewrite our own implementation details
  tool.appendArgumentsAdjuster([&](const CommandLineArguments &args, llvm::StringRef filename) {
    CommandLineArguments new_args(args);
    // Try to remove existing definition from command line to avoid warnings.
    std::erase_if(new_args, [](const std::string &arg) { return arg.starts_with("-DIA2_ENABLE="); });
    // Insert prior to the "end of flags" double hyphen; if not present, append
    auto double_hyphen_pos = std::find(new_args.begin(), new_args.end(), "--");
    double_hyphen_pos = new_args.insert(double_hyphen_pos, "-DIA2_ENABLE=0"s) + 1;
    if (Target == Arch::Aarch64) {
      double_hyphen_pos = new_args.insert(double_hyphen_pos, "--target=aarch64-linux-gnu"s) + 1;
    }
    // Insert extra args from command line
    double_hyphen_pos = new_args.insert(double_hyphen_pos, ExtraArgs.begin(), ExtraArgs.end()) + ExtraArgs.size();
    return new_args;
  });

  // Copy files to output directory before modifying them in place
  auto asts = std::vector<std::unique_ptr<clang::ASTUnit>>();
  tool.buildASTs(asts);
  auto copied_files = copy_files(asts, comp_db, RootDirectory, OutputDirectory);

  auto get_commands = [&](std::string &s) -> std::optional<std::vector<CompileCommand>> {
    // getCompileCommands returns bogus results instead of nothing for files not
    // present in the compile_commands.json, so filter them out explicitly
    // See JSONCompilationDatabasePlugin which is documented in LLVM as "also infers
    // compile commands for files not present in the database"
    if (!all_files_set.contains(s)) {
      return std::nullopt;
    }
    return {comp_db.getCompileCommands(llvm::StringRef(s))};
  };

  // Collect mapping of filenames to pkeys and used pkeys
  std::set<Pkey> pkeys_used;
  for (auto s : SourceFiles) {
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

  if (!(config_0 || config_1)) {
    llvm::errs() << "Error in pkey configuration. The following pkeys were used: [";
    bool first = true;
    for (const Pkey &pkey : pkeys_used) {
      if (!first) {
        llvm::errs() << ", ";
      }
      llvm::errs() << pkey;
      first = false;
    }
    llvm::errs() << "]\npkeys must be in the range 0 to N - 1 or 1 to N\n";
    abort();
  }

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

  Context ctx;

  {
    auto annotation_matcher = functionDecl(hasAttr(clang::attr::Annotate)).bind("annotatedFunc");
    AnnotationPrefixFunctions pre_condition(PRE_CONDITION_ATTR_PREFIX);
    AnnotationPrefixFunctions post_condition(POST_CONDITION_ATTR_PREFIX);
    Structors structors(ctx.types);
    MatchFinder annotation_finder;
    annotation_finder.addMatcher(annotation_matcher, &pre_condition);
    annotation_finder.addMatcher(annotation_matcher, &post_condition);
    annotation_finder.addMatcher(annotation_matcher, &structors);
    const auto rc = tool.run(newFrontendActionFactory(&annotation_finder).get());
    if (rc != 0) {
      return rc;
    }

    ctx.pre_condition_funcs = std::move(pre_condition.funcs);
    ctx.post_condition_funcs = std::move(post_condition.funcs);
    ctx.types.check();
    for (const auto &type : ctx.types) {
      if (!type.has_structors()) {
        continue;
      }
      // Let us look them up by function name, too.
      ctx.constructors[*type.constructor] = type.id;
      ctx.destructors[*type.destructor] = type.id;
    }
  }

  ASTMatchRefactorer refactorer(tool.getReplacements());
  FnDecl fn_decl_pass(ctx, refactorer, tool.getReplacements());
  CallTracker call_tracker_pass(refactorer, fn_decl_pass);
  FnPtrTypes ptr_types_pass(refactorer, tool.getReplacements());
  FnPtrExpr ptr_expr_pass(refactorer, tool.getReplacements());
  FnPtrCall ptr_call_pass(ctx, refactorer, tool.getReplacements());
  FnPtrNull null_ptr_pass(refactorer, tool.getReplacements());
  FnPtrEq eq_ptr_pass(refactorer, tool.getReplacements());

  auto rc = tool.runAndSave(newFrontendActionFactory(&refactorer).get());
  if (rc != 0) {
    return rc;
  }

  header_out << "#ifndef __ASSEMBLER__\n";
  header_out << '\n';

  header_out << "#include <ia2.h>\n";
  header_out << "#include <scrub_registers.h>\n";
  header_out << '\n';

  wrapper_out << "#include <ia2.h>\n";
  wrapper_out << "#include <scrub_registers.h>\n";
  wrapper_out << '\n';
  wrapper_out << "/* Priority-101 constructor to exit loader gate before user constructors */\n";
  wrapper_out << "#include <stdbool.h>\n";
  wrapper_out << '\n';
  wrapper_out << "#ifdef __cplusplus\n";
  wrapper_out << "extern \"C\" thread_local bool ia2_in_loader_gate __attribute__((weak));\n";
  wrapper_out << "extern \"C\" void ia2_loader_gate_exit(void) __attribute__((weak));\n";
  wrapper_out << "#else\n";
  wrapper_out << "extern _Thread_local bool ia2_in_loader_gate __attribute__((weak));\n";
  wrapper_out << "extern void ia2_loader_gate_exit(void) __attribute__((weak));\n";
  wrapper_out << "#endif\n";
  wrapper_out << '\n';
  wrapper_out << "__attribute__((constructor(101)))\n";
  wrapper_out << "static void __ia2_pre_user_constructors(void) {\n";
  wrapper_out << "    if (ia2_loader_gate_exit && ia2_in_loader_gate) {\n";
  wrapper_out << "        ia2_loader_gate_exit();\n";
  wrapper_out << "    }\n";
  wrapper_out << "}\n";
  wrapper_out << '\n';

  /*
   * Define wrappers for IA2_CALL. These switch from the caller's pkey to pkey 0
   * so we don't need to generate them for caller_pkey = 0. When IA2_CALL has
   * caller pkey = 0, it just becomes a cast that calls the fn ptr.
   */
  std::cout << "Generating indirect callsite wrappers\n";
  std::string wrapper_decls;
  std::set<OpaqueStruct> type_id_macros_generated = {};
  for (int caller_pkey = 1; caller_pkey < num_pkeys; caller_pkey++) {
    for (const auto &[mangled_ty, info] : ptr_call_pass.fn_ptr_info) {
      std::string wrapper_name = "__ia2_indirect_callgate_"s +
                                 mangled_ty + "_pkey_" +
                                 std::to_string(caller_pkey);

      std::string asm_wrapper =
          emit_asm_wrapper(ctx, info.sig, {info.wrapper_sig}, wrapper_name, std::nullopt,
                           WrapperKind::IndirectCallsite, caller_pkey, 0, Target);
      wrapper_out << asm_wrapper;

      if (!type_id_macros_generated.contains(mangled_ty)) {
        header_out << "#define IA2_TYPE_"s << mangled_ty << " " << info.type_str << "\n";
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

  std::cout << "Generating direct call gate wrappers\n";
  // Create wrappers for direct calls. These wrappers are inserted by ld --wrap
  // so the wrapper name cannot be changed.
  llvm::raw_fd_ostream *ld_args_out[MAX_PKEYS] = {};
  llvm::raw_fd_ostream *objcopy_redefine_syms_args[MAX_PKEYS] = {};

  // The set of functions that are called from multiple compartments without the pkey suffix
  std::map<Function, std::set<Pkey>> multicaller_fns = {};
  // This is the set of names for direct call wrappers without the leading
  // __wrap_. This set is ultimately used to generate all the call gates, but
  // it's filled out in two phases: first the functions that only have a single
  // caller compartment then those that are called from multiple compartments.
  std::map<Function, Pkey> direct_call_wrappers = {};

  for (int caller_pkey = 0; caller_pkey < num_pkeys; caller_pkey++) {
    create_ld_file(ld_args_out, caller_pkey);
    create_file(objcopy_redefine_syms_args, caller_pkey, ".objcopy");
    // Find the functions that are declared but defined in another compartment
    std::set<Function> undefined_fns = {};
    std::set_difference(fn_decl_pass.declared_fns[caller_pkey].begin(),
                        fn_decl_pass.declared_fns[caller_pkey].end(),
                        fn_decl_pass.defined_fns[caller_pkey].begin(),
                        fn_decl_pass.defined_fns[caller_pkey].end(),
                        std::inserter(undefined_fns, undefined_fns.begin()));

    for (const auto &fn_name : undefined_fns) {
      // Only process functions that are actually called from this compartment
      if (!fn_decl_pass.called_fns[caller_pkey].contains(fn_name)) {
        continue;  // Skip functions that aren't actually called
      }

      // First check if this function has a pkey assigned
      if (!fn_decl_pass.fn_pkeys.contains(fn_name)) {
        // Check if this is a variadic system function - don't assign to compartment 1
        if (fn_decl_pass.variadic_system_fns.contains(fn_name)) {
          llvm::errs() << "Variadic system function " << fn_name
                       << " (called from compartment " << caller_pkey
                       << ") will NOT be assigned to compartment 1\n";
          // Don't assign a pkey - let it run unprotected
        }
        // Check if this is a system header function that should go to compartment 1
        else if (fn_decl_pass.system_header_fns.contains(fn_name)) {
          fn_decl_pass.fn_pkeys[fn_name] = 1;  // Assign to compartment 1
          llvm::errs() << "System library function " << fn_name
                       << " (called from compartment " << caller_pkey
                       << ") assigned to compartment 1\n";
        }
        // Otherwise, let it fall through to existing logic
      }
#ifdef IA2_LIBC_COMPARTMENT
      // Check if this is an ld.so function that needs a callgate to compartment 1
      if (LdsoFunctionRegistry::is_ldso_function(fn_name) && caller_pkey != 1) {
        // Force create a callgate from this compartment to compartment 1
        // where ld.so functions should run
        direct_call_wrappers[fn_name] = caller_pkey; // Caller's compartment

        // Add to ld args for wrapping
        write_to_file(ld_args_out, caller_pkey,
                      "--wrap="s + fn_name + '\n', ".ld");
        continue; // Skip normal processing
      }
#endif

      if (direct_call_wrappers.contains(fn_name)) {
        // If previous iterations found only one compartment that calls
        // fn_name, make sure it's not already in the multicaller set
        assert(!multicaller_fns.contains(fn_name));
        Pkey other_caller = direct_call_wrappers.at(fn_name);

        // Remove from the single-caller set and add fn_name to the
        // multicaller set, making sure to include the two callers found so
        // far
        direct_call_wrappers.erase(fn_name);
        multicaller_fns.insert({fn_name, std::set{caller_pkey, other_caller}});

      } else if (multicaller_fns.contains(fn_name)) {
        // If fn_name already has multiple callers just add to the set of
        // pkeys
        multicaller_fns.at(fn_name).insert(caller_pkey);
      } else {
        // If fn_name has no callers so far, add it to the single-caller set
        direct_call_wrappers.insert({fn_name, caller_pkey});
      }
    }
  }

  // This maps multicaller function names (ending in _pkey_N) to the original target function
  std::map<Function, Function> targets_for_multicaller_fns = {};

  // At this point direct_call_wrappers has all the single-caller functions, so
  // now we'll add the multicaller ones (with their renamed symbols) to allow
  // generating call gates with a single loop through direct_call_wrappers
  for (const auto &[fn_name, caller_pkey_set] : multicaller_fns) {
    FnSignature fn_sig;
    try {
      fn_sig = fn_decl_pass.fn_signatures.at(fn_name);
    } catch (std::out_of_range const &exc) {
      llvm::errs() << "ABI signature not known for function " << fn_name << "\n";
      abort();
    }

    Pkey target_pkey;
    try {
      target_pkey = fn_decl_pass.fn_pkeys.at(fn_name);
    } catch (std::out_of_range const &exc) {
      llvm::errs() << "pkey not known for function " << fn_name << "\n";
      abort();
    }

    for (int caller_pkey : caller_pkey_set) {
      if (caller_pkey == target_pkey) {
        // Same-compartment calls don't need renamed symbols or wrappers.
        continue;
      }

      std::string new_fn_name = fn_name + "_from_" + std::to_string(caller_pkey);
      // The contents of the objcopy args file
      std::string contents = fn_name + " " + new_fn_name + '\n';

      write_to_file(objcopy_redefine_syms_args, caller_pkey, contents, ".objcopy");

      // Add the renamed symbols to the direct_call_wrappers set
      direct_call_wrappers.insert({new_fn_name, caller_pkey});

      // The loop that generates the direct call wrappers depends on maps in
      // FnDecl that are filled out as we parse the source files. Since the
      // renamed symbols don't appear in these source files, we duplicate the
      // original entries in these maps for the renamed symbols. While we could
      // avoid this duplication if necessary, this simplifies the call gate
      // generation.
      fn_decl_pass.fn_signatures.insert({new_fn_name, fn_sig});
      fn_decl_pass.fn_pkeys.insert({new_fn_name, target_pkey});

      // The target functions for multicaller function call gates don't have
      // matching symbol names (since their symbols are renamed) so let's keep
      // track of the original function names for these symbols.
      targets_for_multicaller_fns.insert({new_fn_name, fn_name});
    }
  }

#ifdef IA2_LIBC_COMPARTMENT
  // Add function signatures for ld.so functions that may not have been parsed from source
  for (const std::string& ldso_fn : LdsoFunctionRegistry::get_ldso_functions()) {
    if (direct_call_wrappers.contains(ldso_fn)) {
      // Always set pkey for ld.so functions to compartment 1
      fn_decl_pass.fn_pkeys[ldso_fn] = 1; // ld.so functions run in compartment 1

      if (!fn_decl_pass.fn_signatures.contains(ldso_fn)) {
        FnSignature ldso_sig;
        if (ldso_fn == "_dl_debug_state") {
          // void _dl_debug_state(void) - simple function with no parameters
          ldso_sig.api.ret.name = ""; // Return value has no name
          ldso_sig.api.ret.type = 0; // Use TypeId 0 for void (basic type)
          ldso_sig.abi.ret = {}; // No return slots for void
          ldso_sig.variadic = false; // Not variadic
          // No parameters so args remain empty
        }
        fn_decl_pass.fn_signatures[ldso_fn] = ldso_sig;
      }
    }
  }
#endif

  // At this point direct_call_wrappers has both single-caller and multicaller
  // functions so we just need to generate the callgates
  for (const auto &[fn_name, caller_pkey] : direct_call_wrappers) {
    FnSignature fn_sig;
    try {
      fn_sig = fn_decl_pass.fn_signatures.at(fn_name);
    } catch (std::out_of_range const &exc) {
      llvm::errs() << "C ABI signature for function " << fn_name.c_str()
                   << " not found by FnDecl pass\n";
      abort();
    }
    std::string wrapper_name = "__wrap_"s + fn_name;
    // The target function just matches the callgate name without the leading
    // __wrap_ unless it's a multicaller function in which case we check the
    // function name before the symbol was renamed.
    std::string target_fn = fn_name;
    if (targets_for_multicaller_fns.contains(fn_name)) {
      target_fn = targets_for_multicaller_fns.at(fn_name);
    }
    Pkey target_pkey;
    try {
      target_pkey = fn_decl_pass.fn_pkeys.at(fn_name);
    } catch (std::out_of_range const &exc) {
      llvm::errs() << "Assuming pkey for function " << fn_name.c_str()
                   << " is same as the caller (" << caller_pkey
                   << ") since its definition was not found by FnDecl pass\n";
      continue;
    }

    // Skip generating wrapper if caller and target are in the same compartment
    if (caller_pkey == target_pkey) {
      continue;
    }

    std::string asm_wrapper =
        emit_asm_wrapper(ctx, fn_sig, std::nullopt, wrapper_name, target_fn,
                         WrapperKind::Direct, caller_pkey, target_pkey, Target);
    wrapper_out << asm_wrapper;

    write_to_file(ld_args_out, caller_pkey, "--wrap="s + fn_name + '\n', ".ld");
  }

  // Generate destructor wrappers for every compartment.
  // IA2_LIBC_COMPARTMENT is defined in runtime/libia2/include/ia2_compartment_ids.h
  // so tooling and runtime agree on the libc compartment index.
  // Each ia2_compartment_destructor_N() does the real teardown, and
  // ia2_compartment_init.inc rewrites DT_FINI/.fini_array to call the matching
  // __wrap_ symbol instead.
  // Compartment 1 (the exit slot) already runs on pkey 1 via __wrap___cxa_finalize,
  // so we keep a one-instruction jmp wrapper to satisfy the header’s symbol
  // expectations (see ia2_compartment_init.inc:95,167-172) without tripping the
  // emit_asm_wrapper same-pkey assert. Other compartments still need full call
  // gates that jump from the exit compartment into their own pkey before cleanup.
  for (int compartment_pkey = 1; compartment_pkey < num_pkeys; compartment_pkey++) {
    std::string fn_name = "ia2_compartment_destructor_" + std::to_string(compartment_pkey);
    FnSignature fn_sig;
    try {
      fn_sig = fn_decl_pass.fn_signatures.at(fn_name);
    } catch (std::out_of_range const &exc) {
      llvm::errs() << "Could not find ia2_compartment_destructor_" << compartment_pkey << '\n'
                   << "Make sure to #include ia2_compartment_init.inc for this compartment\n";
      abort();
    }
    std::string wrapper_name = "__wrap_"s + fn_name;

#ifdef IA2_LIBC_COMPARTMENT
    const bool is_libc_compartment = compartment_pkey == IA2_LIBC_COMPARTMENT;

    if (is_libc_compartment) {
      // __wrap___cxa_finalize already switches into pkey 1, so the exit-compartment
      // destructor runs with the right PKRU/stack. emit_asm_wrapper would assert on
      // caller == target, yet ia2_compartment_init.inc still points both
      // compartment_destructor_ptr (line 95) and the DT_FINI rewrites (lines 167-172)
      // at __wrap_ia2_compartment_destructor_1. Keep the symbol alive with a single jmp.
      std::ostringstream trivial;
      trivial << "asm(\n";
      trivial << "    \".text\\n\"\n";
      trivial << "    \".global " << wrapper_name << "\\n\"\n";
      trivial << "    \".type " << wrapper_name << ", @function\\n\"\n";
      trivial << "    \"" << wrapper_name << ":\\n\"\n";
      trivial << "    \"jmp " << fn_name << "\\n\"\n";
      trivial << "    \".size " << wrapper_name << ", .-" << wrapper_name << "\\n\"\n";
      trivial << "    \".previous\\n\"\n";
      trivial << ");\n";
      wrapper_out << trivial.str();
    } else {
      // Non-exit compartments need full call gates to switch from the libc compartment
      std::string asm_wrapper =
          emit_asm_wrapper(ctx, fn_sig, std::nullopt, wrapper_name, fn_name, WrapperKind::Direct,
                           IA2_LIBC_COMPARTMENT, compartment_pkey, Target, false);
      wrapper_out << asm_wrapper;
    }
#else
    // Original behavior: all destructors get call gates from compartment 0
    std::string asm_wrapper =
        emit_asm_wrapper(ctx, fn_sig, std::nullopt, wrapper_name, fn_name,
                        WrapperKind::Direct, 0, compartment_pkey, Target);
    wrapper_out << asm_wrapper;
#endif

    write_to_file(ld_args_out, compartment_pkey,
                  "--wrap="s + fn_name + '\n', ".ld");
  }

  std::cout << "Generating function pointer wrappers\n";
  std::string macros_defining_wrappers;
  /*
   * This loops over all non-static address-taken functions but we only want to
   * define one call gate for each so we need to track them in case a function
   * has its address taken in multiple places
   */
  std::set<Function> generated_wrappers = {};
  // Define wrappers for function pointers (i.e. those referenced by IA2_FN)
  for (const auto &[fn_name, opaque] : ptr_expr_pass.addr_taken_fns) {
    if (generated_wrappers.contains(fn_name)) {
      continue;
    }
    llvm::errs() << " inserting " << fn_name << " into set\n";
    generated_wrappers.insert(fn_name);
    /*
     * Declare these wrapper in the output header so that IA2_FN can reference
     * them. e.g. extern struct IA2_fnptr_ZTSFiiE __ia2_foo;
     *
     * The type used in these declarations is arbitrary and chosen to make it
     * easy to go from a function's name to its mangled type in IA2_FN.
     */
    std::string wrapper_name = "__ia2_"s + fn_name;
    header_out << "extern " << opaque << " " << wrapper_name << ";\n";

    Pkey target_pkey = fn_decl_pass.fn_pkeys[fn_name];
    if (target_pkey != 0) {
      FnSignature fn_sig = fn_decl_pass.fn_signatures[fn_name];
      std::string asm_wrapper =
          emit_asm_wrapper(ctx, fn_sig, std::nullopt, wrapper_name, fn_name,
                           WrapperKind::Pointer, 0, target_pkey, Target,
                           true /* as_macro */);
      macros_defining_wrappers += "#define IA2_DEFINE_WRAPPER_"s + fn_name + " \\\n";
      macros_defining_wrappers += asm_wrapper;

      /*
       * Invoke IA2_DEFINE_WRAPPER from ia2.h in the source file defining the
       * target function. This expands to the IA2_DEFINE_WRAPPER_* macro we just
       * defined
       */
      auto filename = fn_decl_pass.fn_definitions[fn_name];
      std::ofstream source_file(filename, std::ios::app);
      source_file << "IA2_DEFINE_WRAPPER(" << fn_name << ")\n";

    } else {
      header_out << "asm(\n";
      header_out << "  \".set " << wrapper_name << ", __real_" << fn_name << "\\n\"\n";
      header_out << ");\n";
    }
  }

  std::cout << "Generating function pointer wrappers for static functions\n";
  // Define wrappers for pointers to static functions (also those referenced by
  // IA2_FN)
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
        FnSignature fn_sig = fn_decl_pass.fn_signatures[fn_name];

        std::string asm_wrapper = emit_asm_wrapper(
            ctx, fn_sig, std::nullopt, wrapper_name, fn_name, WrapperKind::PointerToStatic, 0,
            target_pkey, Target, true /* as_macro */);
        macros_defining_wrappers += "#define IA2_DEFINE_WRAPPER_"s + fn_name + " \\\n";
        macros_defining_wrappers += asm_wrapper;

        header_out << "extern " << opaque << " " << wrapper_name << ";\n";

        /*
         * Invoke IA2_DEFINE_WRAPPER from ia2.h in the source file defining the
         * target function. This expands to the IA2_DEFINE_WRAPPER_* macro we just
         * defined
         */
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
  header_out << macros_defining_wrappers.c_str();

  header_out << '\n';
  header_out << "#endif\n";

  for (int i = 0; i < num_pkeys; i++) {
    if (ld_args_out[i] != nullptr) {
      ld_args_out[i]->close();
    }
    if (objcopy_redefine_syms_args[i] != nullptr) {
      objcopy_redefine_syms_args[i]->close();
    }
  }

  return EXIT_SUCCESS;
}
