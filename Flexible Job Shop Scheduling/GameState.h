#pragma once
// ============================================================================
//  GameState.h
//  ---------------------------------------------------------------------------
//  The COMPLETE strategy profile of the game - i.e. every player's current
//  decision bundled together.  Keeping it separate from the (immutable)
//  Instance means we can clone, perturb and restore whole profiles cheaply
//  while the players explore their best responses.
//
//  Two vectors describe a profile:
//      routing_  : for each operation (addressed by its globalId) the index of
//                  the eligible alternative the owning job currently picks;
//      sequence_ : a precedence-feasible global dispatch order of all operations
//                  - the order in which a list-scheduler hands them to machines.
//
//  "Precedence-feasible" means the operations of any single job appear in their
//  route order.  Every move the solver makes preserves that invariant, so a
//  GameState always decodes into a valid schedule.
//
//  It is a CLASS (not a struct) with encapsulated storage exposed through
//  accessors, in keeping with the project's strict OOP style.
// ============================================================================

#include <vector>

using namespace std;

namespace fjs {

class GameState {
public:
    GameState() = default;
    GameState(int operationCount)
        : routing_(operationCount, 0), sequence_() {
        sequence_.reserve(operationCount);
    }

    vector<int>&       routing()        { return routing_; }
    const vector<int>& routing()  const { return routing_; }
    vector<int>&       sequence()       { return sequence_; }
    const vector<int>& sequence() const { return sequence_; }

    int  alternativeOf(int globalId) const { return routing_[globalId]; }
    void setAlternativeOf(int globalId, int alt) { routing_[globalId] = alt; }

private:
    vector<int> routing_;
    vector<int> sequence_;
};

} // namespace fjs
