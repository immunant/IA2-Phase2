#include "TypeOps.h"
#include "clang/AST/GlobalDecl.h"
#include "clang/AST/Mangle.h"
#include "clang/Basic/Version.h"

// For types that have both a left and right side, this is what
// we emit for the name between the two sides, e.g.,
// int (*$$$IA2_PLACEHOLDER$$$)(int).
// We can replace this placeholder with any identifier to produce
// a new variable with that same type.
const std::string kTypePlaceHolder = "$$$IA2_PLACEHOLDER$$$";

// Prefix we prepend to each rewritten function pointer type
const std::string kFnPtrTypePrefix = "struct IA2_fnptr_";

// Convert a QualType to a string.
std::string type_string(const clang::QualType &ty) {
  std::string result;
  llvm::raw_string_ostream os{result};
  ty.print(os, clang::LangOptions());
  return result;
}

// Convert a QualType to a string that contains kTypePlaceholder
std::string type_string_with_placeholder(const clang::QualType &ty) {
  std::string result;
  llvm::raw_string_ostream os{result};
  ty.print(os, clang::LangOptions(), kTypePlaceHolder);
  return result;
}

// Replace kTypePlaceholder in a string produced by
// type_string_with_placeholder with an actual given name
std::string replace_type_placeholder(std::string s, const std::string &with) {
  auto placeholder_pos = s.find(kTypePlaceHolder);
  if (placeholder_pos != std::string::npos) {
    s.replace(placeholder_pos, kTypePlaceHolder.size(), with);
  }
  return s;
}

std::string mangle_type(clang::ASTContext &ctx, const clang::QualType &ty) {
  std::unique_ptr<clang::MangleContext> mctx{
      clang::ItaniumMangleContext::create(ctx, ctx.getDiagnostics())};

  std::string os;
  llvm::raw_string_ostream out{os};
#if CLANG_VERSION_MAJOR <= 17
  mctx->mangleTypeName(ty.getCanonicalType(), out);
#else
  mctx->mangleCanonicalTypeName(ty.getCanonicalType(), out);
#endif
  return os;
}

std::string mangle_name(const clang::FunctionDecl *decl) {
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
