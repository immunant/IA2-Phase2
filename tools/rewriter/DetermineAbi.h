#pragma once
#include "CAbi.h"
#include "GenCallAsm.h"
#include "clang/AST/AST.h"

FnSignature determineFnSignatureForDecl(const clang::FunctionDecl &fnDecl, Arch arch);

FnSignature determineFnSignatureForProtoType(const clang::FunctionProtoType &fpt,
                                      clang::ASTContext &astContext,
                                      Arch arch);
