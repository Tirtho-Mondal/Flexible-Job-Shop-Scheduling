#pragma once
// ============================================================================
//  StrategicCoordinationLayer.h   (Layer 1)
//  ---------------------------------------------------------------------------
//  GLOBAL optimization: searches the space of ROUTING PLANS for the one whose
//  operational-layer Nash equilibrium has the lowest makespan,
//        a* = argmin_a  Cmax( E(a) ),
//  where E(a) is the equilibrium returned by the Operational Dispatching Layer
//  for the routing plan a. It proposes routing plans (random or fictitious-play
//  beliefs or crossover - NO greedy/dispatch-rule construction), hands each to Layer 2
//  to be played to a Nash equilibrium, keeps the best by makespan, and feeds the
//  result back into the beliefs - a bilevel game wrapped in multi-start + ILS.
// ============================================================================

#include "Instance.h"
#include "StrategyProfile.h"
#include "PayoffFunction.h"
#include "AlgorithmConfig.h"
#include "SolveResult.h"
#include "FictitiousPlay.h"
#include "OperationalDispatchingLayer.h"
#include <random>
#include <iosfwd>

using namespace std;

namespace fjs {

class StrategicCoordinationLayer {
public:
    StrategicCoordinationLayer(const Instance& inst, const PayoffFunction& payoff,
                               unsigned seed, const AlgorithmConfig& cfg = {});

    SolveResult solve();

    // Optional LIVE trace: stream every accepted move (routing AND sequencing) to this
    // stream as it happens, so the per-instance file updates in real time during solve().
    void setLiveTrace(std::ostream* os) { liveOut = os; op.setLiveTrace(os); }

private:
    // ---- routing-plan proposals (the strategic decisions; NO greedy construction) ----
    StrategyProfile randomProfile();
    void            fillRandomSequence(StrategyProfile& state);
    StrategyProfile beliefProfile(const FictitiousPlay& belief);  // fictitious-play seed

    // Keep `state` if its schedule is the new global best (by makespan, then sumC).
    void considerIncumbent(SolveResult& result, long long& bestFit,
                           const StrategyProfile& state, const Schedule& sched);

    // GLOBAL ROUTING GAME (bilevel upper level): jobs re-route their CRITICAL
    // operations (solo, or two rivals jointly = mutual) whenever it lowers the global
    // potential Phi, anticipating that the LOCAL sequencing game re-equilibrates for
    // the new routing (the two games ALTERNATE). Iterates to a routing Nash
    // equilibrium - a subgame-perfect equilibrium of the two-stage game. Both layers.
    void playRoutingGame(StrategyProfile& state, int run,
                         SolveResult& result, long long& bestFit, int& iteration);

    // Stream one accepted routing move to the live trace (no-op if liveOut is null).
    void logLive(const MoveRecord& rec) const;

    const Instance&       inst;
    const PayoffFunction& payoff;
    mt19937               rng;
    AlgorithmConfig       cfg;
    OperationalDispatchingLayer op;   // Layer 2: the local conflict game
    std::ostream*         liveOut = nullptr;   // live per-move trace sink (not owned)
};

} // namespace fjs
