#pragma once
// ============================================================================
//  GameSolver.h
//  ---------------------------------------------------------------------------
//  The engine that PLAYS the critical-path best-response game.
//
//    1. Every run starts from a fully RANDOM strategy profile (random machine
//       per operation + random precedence-feasible dispatch order).
//    2. DESCENT: repeatedly the makespan-critical player deviates by the single
//       best move on the CRITICAL PATH (re-route or re-sequence a critical
//       operation) that lowers the makespan, until none does - a Nash
//       equilibrium / critical-path local optimum.
//    3. ITERATED LOCAL SEARCH: kick the best profile of the run and descend
//       again, keeping the run's best.
//    4. MULTIPLE RUNS: do this from many random starts and simply keep the best
//       schedule found over all the runs.
//
//  The whole search is bounded by an evaluation budget that scales with the
//  instance size.
//
//  OOP: a self-contained CLASS; MoveRecord and SolveResult are companion
//  CLASSES (never structs) carrying the trace and the outcome.
// ============================================================================

#include "Instance.h"
#include "StrategyProfile.h"
#include "PayoffFunction.h"
#include "BeliefModel.h"
#include <vector>
#include <string>
#include <random>

using namespace std;

namespace fjs {

// One accepted critical-path deviation - one row of the per-instance table.
class MoveRecord {
public:
    int         iteration = 0;     // 1-based global move counter
    int         run       = 0;     // which random run produced it
    int         job       = 0;     // the player that moved (0-based)
    string action;            // human-readable description of the move
    double      oldCost   = 0;     // makespan before the move
    double      newCost   = 0;     // makespan after the move
    int         makespan  = 0;     // makespan after the move (== newCost)
    long long   sumCompletion = 0; // sum of all completion times after the move
};

// Everything the reports need about one solved instance.
class SolveResult {
public:
    string name;
    int numJobs = 0, numMachines = 0, numOperations = 0;

    int       initialMakespan = 0;   // makespan of the very first random profile
    StrategyProfile initialState;          // that first random profile (for reporting)

    int       bestMakespan = 0;
    long long bestTotalCompletion = 0;
    StrategyProfile bestState;             // the best profile found over all runs

    bool equilibriumReached = false; // at least one critical-path local optimum
    int  runsRun       = 0;          // number of independent runs performed
    int  acceptedMoves = 0;          // total improving moves over the whole run
    long evaluations   = 0;          // schedule decodes performed
    vector<int> runBests;       // best makespan of each individual run

    vector<MoveRecord> trace;   // capped trace for the per-instance log
};

class GameSolver {
public:
    GameSolver(const Instance& inst, const PayoffFunction& payoff, unsigned seed);

    SolveResult solve();

private:
    StrategyProfile randomProfile();
    void      fillRandomSequence(StrategyProfile& state);        // precedence-feasible order
    StrategyProfile greedyGlobalProfile();   // load-balancing machine selection (Reijnen "Global")
    StrategyProfile greedyLocalProfile();    // shortest-processing-time machine selection ("Local")
    StrategyProfile taskPoolProfile();       // task-pool constructor: ready ops compete (earliest completion)
    StrategyProfile beliefProfile(const BeliefModel& belief);    // fictitious-play seeded start
    Schedule  evaluate(const StrategyProfile& state);            // decode + count

    // Operations lying on a critical path of `sched` (those that can change Cmax).
    vector<int> criticalOperations(const Schedule& sched) const;

    // Critical-path best-response descent: keep applying the best makespan-reducing
    // critical deviation until none remains (a Nash equilibrium / local optimum).
    // Runs to full convergence - there is no budget cut-off.
    void descend(StrategyProfile& state, int run,
                 SolveResult& result, long long& bestFit, int& iteration);

    // Diversification kick for iterated local search; if `belief` is non-null the
    // re-routing part is drawn from the players' beliefs (intensification).
    void perturb(StrategyProfile& state, int strength, const BeliefModel* belief);

    // Consider `cand` as the new global incumbent.
    void considerIncumbent(SolveResult& result, long long& bestFit,
                           const StrategyProfile& state, const Schedule& sched);

    const Instance&       inst_;
    const PayoffFunction& payoff_;
    mt19937          rng_;

    long evals_        = 0;   // schedule decodes performed (reported, not a cap)
    int  maxTraceRows_ = 2500;
};

} // namespace fjs
