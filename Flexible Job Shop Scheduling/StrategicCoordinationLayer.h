#pragma once
// ============================================================================
//  StrategicCoordinationLayer.h   (Layer 1)
//  ---------------------------------------------------------------------------
//  GLOBAL optimization: searches the space of ROUTING PLANS for the one whose
//  operational-layer Nash equilibrium has the lowest makespan,
//        a* = argmin_a  Cmax( E(a) ),
//  where E(a) is the equilibrium returned by the Operational Dispatching Layer
//  for the routing plan a. It proposes routing plans (random / greedy / task-pool
//  / fictitious-play beliefs / crossover), hands each to Layer 2 to be played to a
//  Nash equilibrium, keeps the best by makespan, and feeds the result back into the
//  beliefs - a bilevel (Stackelberg-style) game wrapped in multi-start + ILS.
// ============================================================================

#include "Instance.h"
#include "StrategyProfile.h"
#include "PayoffFunction.h"
#include "AlgorithmConfig.h"
#include "SolveResult.h"
#include "FictitiousPlay.h"
#include "OperationalDispatchingLayer.h"
#include <random>

using namespace std;

namespace fjs {

class StrategicCoordinationLayer {
public:
    StrategicCoordinationLayer(const Instance& inst, const PayoffFunction& payoff,
                               unsigned seed, const AlgorithmConfig& cfg = {});

    SolveResult solve();

private:
    // ---- routing-plan proposals (the strategic decisions) ----
    StrategyProfile randomProfile();
    void            fillRandomSequence(StrategyProfile& state);
    StrategyProfile greedyGlobalProfile();   // Reijnen "Global": load-balancing routing
    StrategyProfile greedyLocalProfile();    // Reijnen "Local": shortest-processing-time
    StrategyProfile beliefProfile(const FictitiousPlay& belief);  // fictitious-play seed

    // Keep `state` if its schedule is the new global best (by makespan, then sumC).
    void considerIncumbent(SolveResult& result, long long& bestFit,
                           const StrategyProfile& state, const Schedule& sched);

    const Instance&       inst;
    const PayoffFunction& payoff;
    mt19937               rng;
    AlgorithmConfig       cfg;
    OperationalDispatchingLayer op;   // Layer 2: the local conflict game
};

} // namespace fjs
