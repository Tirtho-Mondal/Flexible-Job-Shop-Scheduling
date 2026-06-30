#pragma once
// ============================================================================
//  Crossover.h
//  ---------------------------------------------------------------------------
//  The recombination operators used by the memetic search. Each takes two parent
//  strategy profiles (each = routing MAV + dispatch OSV) and returns ONE feasible
//  child. All three keep every job's operations in route order, so the child is
//  always precedence-feasible.
//
//      POX - uniform crossover on the routing + random-partition POX on the order.
//      OUX - payoff-guided order-based uniform crossover: each job inherits its
//            whole strategy from the parent where it is individually happier - i.e.
//            where its OWN-interest cost own_j is lower (partition decided by payoff,
//            not a fixed mixing ratio). own_j is used instead of the full U_i because
//            the stable U_i is makespan-aligned and barely differs between jobs.
//      OOX - order-based one-point crossover: a random cut point; the left part is
//            copied from parent 1 (operations + machines), the remainder filled
//            from parent 2 in order (skipping operations already placed).
//
//  The operator is chosen at run time by AlgorithmConfig.crossoverType
//  (0 = POX, 1 = OUX, 2 = OOX) via recombine().
// ============================================================================

#include "Instance.h"
#include "StrategyProfile.h"
#include "PayoffFunction.h"
#include <random>

using namespace std;

namespace fjs {

class Crossover {
public:
    Crossover(const Instance& inst, const PayoffFunction& payoff)
        : inst(inst), payoff(payoff) {}

    // Dispatch to the chosen operator (0 = POX, 1 = OUX, 2 = OOX).
    StrategyProfile recombine(int type, const StrategyProfile& a,
                              const StrategyProfile& b, mt19937& rng) const;

    StrategyProfile pox(const StrategyProfile& a, const StrategyProfile& b, mt19937& rng) const;
    StrategyProfile oux(const StrategyProfile& a, const StrategyProfile& b, mt19937& rng) const;
    StrategyProfile oox(const StrategyProfile& a, const StrategyProfile& b, mt19937& rng) const;

private:
    const Instance&       inst;
    const PayoffFunction& payoff;
};

} // namespace fjs
