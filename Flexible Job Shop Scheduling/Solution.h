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

    // ---- the chosen strategies ----------------------------------------
    const StrategyProfile& profile()           const { return profile_; }
    const vector<int>&     operationSequence() const { return profile_.sequence(); } // OSV
    const vector<int>&     machineAssignment() const { return machine_; }            // MAV (machine ids)

    // ---- the timing ----------------------------------------------------
    const Schedule& schedule()      const { return schedule_; }
    int  startTime(int gid)         const { return schedule_.startOf(gid); }
    int  finishTime(int gid)        const { return schedule_.endOf(gid); }

    // ---- the objective values -----------------------------------------
    int       makespan()    const { return schedule_.makespan(); }
    long long fitness()     const { return fitness_; }
    double    totalPayoff() const { return totalPayoff_; }   // sum of every U_i

    // ---- the per-player view ------------------------------------------
    int                     numJobs()             const { return (int)jobStrategies_.size(); }
    const Strategy&         jobStrategy(int job)  const { return jobStrategies_[job]; }
    const vector<Strategy>& jobStrategies()       const { return jobStrategies_; }

private:
    Solution(const StrategyProfile& profile, const Schedule& sched);

    StrategyProfile  profile_;          // OSV + MAV (alternative indices)
    Schedule         schedule_;         // start/finish/makespan
    vector<int>      machine_;          // MAV decoded to actual machine ids
    vector<Strategy> jobStrategies_;    // one Strategy per job-player
    long long        fitness_     = 0;
    double           totalPayoff_ = 0.0;
};

} // namespace fjs
