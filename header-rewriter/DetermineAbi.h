#pragma once
#include "CAbi.h"
#include "clang/AST/AST.h"

auto determineAbiForDecl(const clang::FunctionDecl &fnDecl) -> CAbiSignature;
