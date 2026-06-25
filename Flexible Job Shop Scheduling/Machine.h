#pragma once
// ============================================================================
//  Machine.h
//  ---------------------------------------------------------------------------
//  A single machine of the flexible job shop.  In this game-theoretic model the
//  machines are the SHARED RESOURCES the job-players compete for: every job
//  wants its operations to run on a machine that lets it finish early, but the
//  more jobs that pick the same machine the more congested (and therefore slow)
//  that machine becomes.
//
//  A Machine is immutable problem data.  Its time-varying state (when it next
//  becomes free) is NOT kept here - that belongs to a decoded Schedule, so the
//  very same Machine object can be reused while evaluating thousands of
//  candidate strategy profiles.
//
//  OOP pillar shown here: ENCAPSULATION - all fields are private and exposed
//  only through const accessors.
// ============================================================================

#include <string>

using namespace std;

namespace fjs {

class Machine {
public:
    explicit Machine(int index);

    int index = 0;                             // 0-based machine id (public data)
    string label() const;                      // "M3" for the report
};

} // namespace fjs
