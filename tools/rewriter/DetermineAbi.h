#pragma once
#include "CAbi.h"
#include "GenCallAsm.h"
#include "clang/AST/AST.h"

auto determineAbiForDecl(const clang::FunctionDecl &fnDecl, Arch arch) -> AbiSignature;

AbiSignature determineAbiForProtoType(const clang::FunctionProtoType &fpt,
                                       clang::ASTContext &astContext,
                                       Arch arch);
