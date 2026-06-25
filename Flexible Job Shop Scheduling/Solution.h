#pragma once
// ============================================================================
//  Solution.h
//  ---------------------------------------------------------------------------
//  A SOLUTION is the complete schedule generated after all jobs choose their
//  strategies. It bundles everything that defines one realised outcome of the
//  game:
//      * the operation sequence vector (OSV) and machine assignment vector (MAV),
//      * the start time and finish time of every operation,
//      * the final makespan,
//      * the final fitness and total payoff value,
//      * each job-player's own Strategy (its slice of the profile).
//
//  A Solution is produced by decoding a StrategyProfile against an Instance with
//  the active schedule builder; a Solution that no player can improve alone is a
//  StableSolution (see StableSolution.h).
// ============================================================================

#include "StrategyProfile.h"
#include "Schedule.h"
#include "Strategy.h"
#include <vector>

using namespace std;

namespace fjs {

class Instance;
class PayoffFunction;

class Solution {
public:
    // Decode a strategy profile into a complete, timed solution.
    static Solution decode(const Instance& inst, const StrategyProfile& profile,
                           const PayoffFunction& payoff);

    // ---- public data: the realised outcome of the game ----------------
    StrategyProfile  profile;           // OSV + MAV (alternative indices)
    Schedule         schedule;          // start/finish/makespan
    long long        fitness     = 0;   // makespan-dominated selection key
    double           totalPayoff = 0.0; // sum of every U_i

    const vector<int>&     operationSequence() const { return profile.sequence; }  // OSV
    const vector<int>&     machineAssignment() const { return machine; }           // MAV (machine ids)

    // ---- the timing ----------------------------------------------------
    int  startTime(int gid)         const { return schedule.startOf(gid); }
    int  finishTime(int gid)        const { return schedule.endOf(gid); }

    // ---- the objective value computed from the schedule ----------------
    int       makespan()    const { return schedule.makespan(); }

    // ---- the per-player view ------------------------------------------
    int                     numJobs()             const { return (int)jobStrategies.size(); }
    const Strategy&         jobStrategy(int job)  const { return jobStrategies[job]; }

private:
    Solution(const StrategyProfile& profile, const Schedule& sched);

    vector<int>      machine;           // MAV decoded to actual machine ids
    vector<Strategy> jobStrategies;     // one Strategy per job-player
};

} // namespace fjs
