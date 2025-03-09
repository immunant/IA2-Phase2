#pragma once
#include "CAbi.h"
#include "GenCallAsm.h"
#include "clang/AST/AST.h"

FnSignature determineFnSignatureForDecl(
    Context &ctx, const clang::FunctionDecl &fnDecl,
    Arch arch);

FnSignature determineFnSignatureForProtoType(
    Context &ctx,
    const clang::FunctionProtoType &fpt,
    clang::ASTContext &astContext,
    Arch arch);
