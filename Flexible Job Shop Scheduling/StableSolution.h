#pragma once
// ============================================================================
//  StableSolution.h
//  ---------------------------------------------------------------------------
//  A STABLE SOLUTION is reached when no job can improve the schedule by changing
//  its own strategy alone. That is the game-theoretic meaning of a solution: a
//  pure-strategy Nash equilibrium of the scheduling game.
//
//  This class decodes a StrategyProfile into a Solution and then verifies
//  stability by testing every unilateral single-operation re-route (the routing
//  half of a job-player's strategy): if any job could lower the fitness by
//  moving one of its operations to another eligible machine on its own, the
//  profile is NOT stable and the first such profitable deviation is recorded.
//
//  (The solver's best-response descent additionally guarantees no improving
//  re-sequence or pairwise rival move remains; this class checks the routing
//  deviations explicitly so a report can state the equilibrium property.)
// ============================================================================

#include "Solution.h"
#include <string>

using namespace std;

namespace fjs {

class Instance;
class StrategyProfile;
class PayoffFunction;

class StableSolution {
public:
    StableSolution(const Instance& inst, const StrategyProfile& profile,
                   const PayoffFunction& payoff);

    // ---- public data: the decoded solution and its stability verdict ----
    Solution solution;                  // the decoded Nash candidate
    bool     isStable = true;           // true when no job can improve alone
    string   profitableDeviation;       // an improving deviation, else empty

    int       makespan() const { return solution.makespan(); }
    long long fitness()  const { return solution.fitness; }
};

} // namespace fjs
