#pragma once
// ============================================================================
//  RandomKick.h
//  ---------------------------------------------------------------------------
//  The diversification kick used by iterated local search. It perturbs a
//  strategy profile: a few randomly chosen operations either re-route to another
//  eligible machine (drawn from the players' learned beliefs when available) or
//  jump to a new legal position in the dispatch order. This is how the search
//  escapes a local optimum / weak equilibrium before descending again.
// ============================================================================

#include "Instance.h"
#include "StrategyProfile.h"
#include "FictitiousPlay.h"
#include <random>

using namespace std;

namespace fjs {

class RandomKick {
public:
    RandomKick(const Instance& instance, mt19937& rng);

    // Apply `strength` random moves to `profile`. If `belief` is non-null, the
    // re-routing part is drawn from the players' beliefs (intensified kick).
    void apply(StrategyProfile& profile, int strength, const FictitiousPlay* belief);

private:
    const Instance& instance;
    mt19937&        rng;
};

} // namespace fjs
