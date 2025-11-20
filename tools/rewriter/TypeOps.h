#pragma once

#include "clang/AST/AST.h"
#include <optional>
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
  std::optional<std::string> constructor;
  std::optional<std::string> destructor;

  /// Check if one was already set.
  void set_constructor(std::string name);

  /// Check if one was already set.
  void set_destructor(std::string name);

  bool has_structors() const;

  /// Check if there is both a constructor and destructor, or neither.
  void check() const;
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

  /// Manual interning helper for synthetic types (e.g., ld.so stubs) that
  /// don't originate from a clang::QualType.
  TypeId intern_from_strings(const std::string &canonical_name, const std::string &name);

  const TypeInfo &get(TypeId index) const;

  TypeInfo &get(TypeId index);

  void check() const;
};
