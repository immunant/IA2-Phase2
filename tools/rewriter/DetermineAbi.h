#pragma once
#include "CAbi.h"
#include "GenCallAsm.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma GCC diagnostic ignored "-Wnonnull"
#include "clang/AST/AST.h"
#pragma GCC diagnostic pop

auto determineAbiForDecl(const clang::FunctionDecl &fnDecl, Arch arch) -> AbiSignature;

AbiSignature determineAbiForProtoType(const clang::FunctionProtoType &fpt,
                                      clang::ASTContext &astContext,
                                      Arch arch);
