#pragma once
// ============================================================================
//  InstanceReport.h
//  ---------------------------------------------------------------------------
//  Writes the detailed per-instance log file requested for every instance:
//    * a header describing the instance and the game model,
//    * the initial RANDOM strategy profile every run starts from,
//    * the full iteration table - one row per accepted best-response move,
//      showing which job moved, what it did, its completion time before/after,
//      and the resulting makespan,
//    * the final Nash-equilibrium schedule (per-machine sequences, per-job
//      completion times and the full operation timetable),
//    * the comparison with the literature best-known value.
// ============================================================================

#include "Instance.h"
#include "GameSolver.h"
#include "PayoffFunction.h"
#include <string>

using namespace std;

namespace fjs {

class InstanceReport {
public:
    // bestKnown < 0 means "not available". selfish = true when the pure
    // non-cooperative Nash game produced the result (changes the narrative).
    static void write(const string& path, const Instance& inst,
                      const SolveResult& result, const PayoffFunction& payoff,
                      int bestKnown, bool selfish = false);
};

} // namespace fjs
