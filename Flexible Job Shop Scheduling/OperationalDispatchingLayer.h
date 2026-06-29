#pragma once
// ============================================================================
//  OperationalDispatchingLayer.h   (Layer 2)
//  ---------------------------------------------------------------------------
//  LOCAL optimization: resolves the machine-level conflicts of a FIXED routing
//  plan. Given a strategy profile, the jobs play a NON-COOPERATIVE best-response
//  game - reroute, resequence, swap, or jointly re-route on the critical path -
//  until no job (and no rival pair) can improve: a pure-strategy NASH EQUILIBRIUM.
//
//    descend()        - coordinated engine: critical-path + two-player moves
//                       accepted by the makespan potential (best makespan).
//    descendSelfish() - pure non-cooperative game: unilateral then pairwise
//                       (Pareto) best response on each job's OWN payoff U_i.
//
//  The STRATEGIC layer (StrategicCoordinationLayer) supplies the routing plans
//  and selects the best resulting equilibrium.
// ============================================================================

#include "Instance.h"
#include "StrategyProfile.h"
#include "PayoffFunction.h"
#include "AlgorithmConfig.h"
#include "SolveResult.h"
#include <vector>

using namespace std;

namespace fjs {

class OperationalDispatchingLayer {
public:
    OperationalDispatchingLayer(const Instance& inst, const PayoffFunction& payoff,
                                const AlgorithmConfig& cfg);

    // Decode a profile into a timed schedule (and count the decode).
    Schedule evaluate(const StrategyProfile& state);

    // Operations on a critical path of `sched` (those that can change Cmax).
    vector<int> criticalOperations(const Schedule& sched) const;

    // Coordinated best-response descent (two-player + critical-path moves accepted
    // by the makespan potential). Updates the global incumbent in `bestFit`/result.
    void descend(StrategyProfile& state, int run,
                 SolveResult& result, long long& bestFit, int& iteration);

    // Pure selfish non-cooperative game: unilateral then pairwise (Pareto) best
    // response on each job's OWN payoff. Returns true on a certified Nash endpoint.
    bool descendSelfish(StrategyProfile& state, int run,
                        SolveResult& result, int& iteration);

    // LOCAL SEQUENCING GAME (bilevel lower level): with the ROUTING FIXED, each job
    // best-responds on its OWN payoff by re-sequencing one of its operations, until
    // no job can improve - a sequencing Nash equilibrium E2(a). No reroute moves.
    bool sequencingGame(StrategyProfile& state, int run,
                        SolveResult& result, int& iteration);

    long evaluations() const { return evals; }

private:
    const Instance&       inst;
    const PayoffFunction& payoff;
    AlgorithmConfig       cfg;

    long evals        = 0;   // schedule decodes performed
    int  maxTraceRows = 2500; // set from cfg.traceRows in the constructor
    int  detailRows   = 2500;
};

} // namespace fjs
