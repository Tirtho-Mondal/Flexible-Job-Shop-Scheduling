#pragma once
// ============================================================================
//  Job.h
//  ---------------------------------------------------------------------------
//  A Job is the PLAYER of our non-cooperative scheduling game.
//
//  Each job owns an ordered list of operations (its route, which must run in
//  order).  A player's STRATEGY has two parts:
//      1. routing   - which eligible machine each of its operations uses
//                     (stored inside the Operation objects), and
//      2. priority  - where its operations sit in the global dispatch order
//                     (managed by the solver through the shared GameState).
//
//  A player is "selfish": it only cares about its OWN completion time C_i, the
//  moment its last operation finishes.  The payoff it tries to maximise is
//  therefore -C_i (see PayoffFunction).
//
//  OOP pillar shown here: ENCAPSULATION - the route is private and is read
//  through const accessors.
// ============================================================================

#include "Operation.h"
#include <vector>
#include <string>

using namespace std;

namespace fjs {

class Job {
public:
    explicit Job(int index);

    // Append the next operation of this job's route and hand back a reference
    // so the reader can register its eligible machines.
    Operation& addOperation(int globalId);

    int index() const { return index_; }

    int                           operationCount() const { return (int)ops_.size(); }
    Operation&                    operation(int k)        { return ops_[k]; }
    const Operation&              operation(int k)  const { return ops_[k]; }
    vector<Operation>&       operations()            { return ops_; }
    const vector<Operation>& operations()      const { return ops_; }

    // Total processing time of the route under the currently chosen routing -
    // a natural lower bound on this player's completion time.
    int routeWorkload() const;

    string label() const { return "J" + to_string(index_ + 1); }

private:
    int index_;
    vector<Operation> ops_;
};

} // namespace fjs
