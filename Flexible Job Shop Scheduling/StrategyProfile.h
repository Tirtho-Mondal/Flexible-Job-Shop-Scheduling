#pragma once
// ============================================================================
//  StrategyProfile.h
//  ---------------------------------------------------------------------------
//  A STRATEGY PROFILE is the game-theoretic term for "every player's strategy
//  bundled together" - the complete joint decision of all the job-players. It is
//  the working unit the solver clones, perturbs and restores while the players
//  explore their best responses; one decoded profile becomes a Solution.
//
//  Two vectors describe a profile:
//      routing_  : for each operation (addressed by its globalId) the index of
//                  the eligible alternative the owning job currently picks (MAV);
//      sequence_ : a precedence-feasible global dispatch order of all operations
//                  - the order a list-scheduler hands them to machines (OSV).
//
//  "Precedence-feasible" means the operations of any single job appear in their
//  route order.  Every move the solver makes preserves that invariant, so a
//  StrategyProfile always decodes into a valid schedule.
//
//  (One single job-player's slice of this profile - its own machine choices and
//  queue positions - is modelled by the Strategy class; see Strategy.h.)
//
//  It is a CLASS (not a struct) with encapsulated storage exposed through
//  accessors, in keeping with the project's strict OOP style.
// ============================================================================

#include <vector>

using namespace std;

namespace fjs {

class StrategyProfile {
public:
    StrategyProfile() = default;
    StrategyProfile(int operationCount)
        : routing_(operationCount, 0), sequence_() {
        sequence_.reserve(operationCount);
    }

    vector<int>&       routing()        { return routing_; }
    const vector<int>& routing()  const { return routing_; }
    vector<int>&       sequence()       { return sequence_; }
    const vector<int>& sequence() const { return sequence_; }

    int  alternativeOf(int globalId) const { return routing_[globalId]; }
    void setAlternativeOf(int globalId, int alt) { routing_[globalId] = alt; }

    // ---- strategy moves (named operations on the strategy data) --------
    // A move is one player changing its strategy. These edit the profile's two
    // vectors; the solver (GameSolver) decides WHICH move to make (best response)
    // and a swap / mutual reroute is just two of these applied to a rival pair.

    // ROUTING move (MAV): pick a different eligible machine for one operation.
    void reroute(int globalId, int alt) { routing_[globalId] = alt; }

    // SEQUENCING move (OSV): a copy of the dispatch order with the operation at
    // index `fromPos` moved to index `toPos`. Used for re-sequence and for the
    // pairwise swap (move the later rival just ahead of the earlier one).
    vector<int> resequenced(int fromPos, int toPos) const {
        vector<int> r = sequence_;
        int g = r[fromPos];
        r.erase(r.begin() + fromPos);
        int t = (toPos > fromPos) ? toPos - 1 : toPos;
        r.insert(r.begin() + t, g);
        return r;
    }

private:
    vector<int> routing_;
    vector<int> sequence_;
};

} // namespace fjs
