#pragma once

#include "clang/AST/AST.h"
#include <string>

extern const std::string kTypePlaceHolder;
extern const std::string kFnPtrTypePrefix;

std::string type_string(const clang::QualType &ty);
std::string type_string_with_placeholder(const clang::QualType &ty);
std::string replace_type_placeholder(std::string s, const std::string &with);

std::string mangle_type(clang::ASTContext &ctx, const clang::QualType &ty);
std::string mangle_name(const clang::FunctionDecl *decl);

typedef uint32_t TypeId;
