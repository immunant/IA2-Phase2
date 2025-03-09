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

struct TypeInfo {
  TypeId id;
  std::string name;
  std::string canonical_name;
};

class TypeInfoInterner {
private:
  /// Key is canonical type name.
  /// Value is type ID.
  std::unordered_map<std::string, TypeId> ids;

  /// Index is type ID.
  std::vector<TypeInfo> infos;

public:
  std::vector<TypeInfo>::const_iterator begin() const;
  std::vector<TypeInfo>::const_iterator end() const;

  /// Looks up the `TypeInfo` for `type`
  /// based on the canonical name, interned as `TypeId`.
  TypeId intern(clang::QualType type);

  const TypeInfo &get(TypeId index) const;

  TypeInfo &get(TypeId index);
};
