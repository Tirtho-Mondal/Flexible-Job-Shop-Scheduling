#pragma once
// ============================================================================
//  CodeExplanation.h
//  ---------------------------------------------------------------------------
//  Writes output/code_explanation.md - a guided tour of how the source code is
//  organised and, above all, how the game-theoretic model and its payoff
//  function work.  Generated at the end of every run so the explanation always
//  ships next to the results.
// ============================================================================

#include "PayoffFunction.h"
#include <string>

using namespace std;

namespace fjs {

class CodeExplanation {
public:
    static void write(const string& path, const PayoffFunction& payoff);
};

} // namespace fjs
