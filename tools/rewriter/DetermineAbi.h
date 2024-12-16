#pragma once
#include "CAbi.h"
#include "GenCallAsm.h"
#include "clang/AST/AST.h"

FnSignature determineSignatureForDecl(const clang::FunctionDecl &fnDecl, const clang::ASTContext &ast_context, Arch arch);

FnSignature determineSignatureForProtoType(const clang::FunctionProtoType &fpt,
                                           clang::ASTContext &astContext,
                                           Arch arch);
