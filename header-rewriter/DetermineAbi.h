#pragma once
#include "CAbi.h"
#include "clang/AST/AST.h"

auto determineAbiForDecl(const clang::FunctionDecl &fnDecl) -> CAbiSignature;

CAbiSignature determineAbiForProtoType(const clang::FunctionProtoType &fpt,
                                       clang::ASTContext &astContext);
