#pragma once
#include "CAbi.h"
#include "clang/AST/AST.h"
#include "clang/CodeGen/ModuleBuilder.h"

auto determineAbiForDecl(const clang::FunctionDecl &fnDecl, clang::CodeGen::CodeGenModule &cgm) -> CAbiSignature;
