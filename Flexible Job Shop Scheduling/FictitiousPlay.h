#pragma once
// ============================================================================
//  FictitiousPlay.h   (fictitious-play belief learning for the game)
//  ---------------------------------------------------------------------------
//  FICTITIOUS PLAY is the classical game-theoretic learning model: each player
//  forms a BELIEF about which of its actions tend to appear in high-payoff
//  outcomes and biases its next strategy toward those actions. Here that belief
//  is built from a pool of the best equilibria seen so far - for every operation
//  it measures HOW OFTEN each eligible machine is used across those good
//  equilibria:
//
//      belief(op, machine) = fraction of elite equilibria in which `op` is
//                            routed to `machine`  (Laplace-smoothed)
//
//  The solver uses these beliefs to SEED new runs and to aim its kicks, so the
//  search concentrates on the machine assignments that good equilibria agree on.
//  It does NOT change the payoff: the single payoff still decides every move and
//  which schedule is kept.  (Formerly named BeliefModel.)
// ============================================================================

#include "Instance.h"
#include "StrategyProfile.h"
#include <vector>
#include <random>

using namespace std;

namespace fjs {

class FictitiousPlay {
public:
    FictitiousPlay(const Instance& inst, int capacity);

    // Offer an equilibrium to the elite pool (kept if good and not a duplicate).
    void consider(const StrategyProfile& state, int makespan);

    bool ready()       const { return !pool.empty(); }
    int  eliteCount()  const { return (int)pool.size(); }
    const StrategyProfile& elite(int i) const { return pool[i]; }  // a stored elite equilibrium
    int  bestMakespan() const;

    // A routing vector sampled from the current beliefs (one machine per op).
    vector<int> sampleRouting(mt19937& rng) const;
    // The single most-believed alternative for one operation.
    int argmaxAlternative(int globalId) const;
    // Sample one alternative for an operation from its belief distribution.
    int sampleAlternative(int globalId, mt19937& rng) const;

private:
    void rebuild();   // recompute the frequency maps from the pool

    const Instance&                  inst;
    int                              capacity;
    vector<StrategyProfile>           pool;        // elite equilibria
    vector<int>                 makespans;   // their makespans
    vector<vector<double>> freq;        // gid -> alt -> probability
};

} // namespace fjs
