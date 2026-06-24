#pragma once
// ============================================================================
//  BeliefModel.h   (fictitious play / belief learning for the game)
//  ---------------------------------------------------------------------------
//  This is the game-theoretic version of the "long-term memory" that lets the
//  Kasapidis et al. (2025) ALNS-CP close the gap to optimal: it keeps a pool of
//  the best equilibria seen so far and measures, for every operation, HOW OFTEN
//  each eligible machine is used across those good equilibria.
//
//  In game theory this is FICTITIOUS PLAY / BELIEF LEARNING: a player forms a
//  belief about which of its actions tend to appear in high-payoff outcomes,
//  and biases its next strategy toward those actions.  Concretely:
//
//      belief(op, machine) = fraction of elite equilibria in which `op` is
//                            routed to `machine`  (Laplace-smoothed)
//
//  The solver uses these beliefs to SEED new runs and to aim its kicks, so the
//  search concentrates on the machine assignments that good solutions agree on
//  - exactly the mechanism that drives the literature methods toward optimal,
//  expressed purely through the players' learned beliefs.  It does NOT change
//  the payoff: the single payoff (makespan-aligned selfish completion) still
//  decides every move and which schedule is kept.
// ============================================================================

#include "Instance.h"
#include "GameState.h"
#include <vector>
#include <random>

using namespace std;

namespace fjs {

class BeliefModel {
public:
    BeliefModel(const Instance& inst, int capacity);

    // Offer an equilibrium to the elite pool (kept if good and not a duplicate).
    void consider(const GameState& state, int makespan);

    bool ready()       const { return !pool_.empty(); }
    int  eliteCount()  const { return (int)pool_.size(); }
    int  bestMakespan() const;

    // A routing vector sampled from the current beliefs (one machine per op).
    vector<int> sampleRouting(mt19937& rng) const;
    // The single most-believed alternative for one operation.
    int argmaxAlternative(int globalId) const;
    // Sample one alternative for an operation from its belief distribution.
    int sampleAlternative(int globalId, mt19937& rng) const;

private:
    void rebuild();   // recompute the frequency maps from the pool

    const Instance&                  inst_;
    int                              capacity_;
    vector<GameState>           pool_;       // elite equilibria
    vector<int>                 makespans_;  // their makespans
    vector<vector<double>> freq_;       // gid -> alt -> probability
};

} // namespace fjs
