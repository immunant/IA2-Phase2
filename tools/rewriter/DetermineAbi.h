#pragma once
#include "CAbi.h"
#include "GenCallAsm.h"
#include "clang/AST/AST.h"

auto determineSignatureForDecl(const clang::FunctionDecl &fnDecl, Arch arch) -> FnSignature;

FnSignature determineSignatureForProtoType(const clang::FunctionProtoType &fpt,
                                       clang::ASTContext &astContext,
                                       Arch arch);
