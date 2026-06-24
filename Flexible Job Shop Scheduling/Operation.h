#pragma once
// ============================================================================
//  Operation.h
//  ---------------------------------------------------------------------------
//  One operation O(job, k) - the k-th operation in a job's route.
//
//  In the flexible job shop an operation may be processed on a SET of eligible
//  machines, each with its own processing time.  Choosing which eligible
//  machine to use is exactly the routing part of a job-player's strategy, so
//  this class is where one "slice" of a strategy lives:
//
//      * the eligible machines + their processing times are immutable data,
//      * the currently chosen alternative is the mutable strategic decision.
//
//  Every operation also carries a globalId so the solver can address all
//  operations of the instance with a single flat index (handy for the global
//  dispatch sequence and the decoder).
//
//  OOP pillar shown here: ENCAPSULATION - the eligibility table is private and
//  the chosen alternative is changed only through the validated chooseAlternative().
// ============================================================================

#include <vector>
#include <string>

using namespace std;

namespace fjs {

class Operation {
public:
    Operation(int globalId, int jobIndex, int positionInJob);

    // Called by the reader while loading: this operation may run on `machine`
    // (0-based) taking `processingTime` time units.
    void addAlternative(int machine, int processingTime);

    // ---- Identity -------------------------------------------------------
    int globalId()      const { return globalId_; }
    int jobIndex()      const { return jobIndex_; }
    int positionInJob() const { return positionInJob_; }

    // ---- Eligible alternatives (immutable problem data) ------------------
    int alternativeCount()        const { return (int)machines_.size(); }
    int machineOfAlternative(int a)  const { return machines_[a]; }
    int timeOfAlternative(int a)     const { return times_[a]; }

    // ---- Current strategic choice (mutable) -----------------------------
    int chosenAlternative()       const { return chosen_; }
    void chooseAlternative(int a)       { chosen_ = a; }
    int assignedMachine()         const { return machines_[chosen_]; }
    int assignedProcessingTime()  const { return times_[chosen_]; }

    string label() const;   // e.g. "O(2,3)"

private:
    int globalId_;
    int jobIndex_;
    int positionInJob_;
    vector<int> machines_;   // eligible machine ids (parallel arrays)
    vector<int> times_;      // processing time on the matching machine
    int chosen_ = 0;              // index into the parallel arrays
};

} // namespace fjs
