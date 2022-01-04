#pragma once
#include "clang/AST/AST.h"
#include "CAbi.h"

auto determineAbiForDecl(const clang::FunctionDecl& fnDecl) -> CAbiSignature;
