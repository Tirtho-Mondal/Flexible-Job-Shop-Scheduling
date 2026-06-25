// ============================================================================
//  Solution.cpp - decode a strategy profile into a complete timed solution.
// ============================================================================
#include "Solution.h"
#include "Instance.h"
#include "ScheduleBuilder.h"
#include "PayoffFunction.h"

using namespace std;

namespace fjs {

Solution::Solution(const StrategyProfile& profile, const Schedule& sched)
    : profile(profile), schedule(sched) {}

Solution Solution::decode(const Instance& inst, const StrategyProfile& profile,
                          const PayoffFunction& payoff) {
    // 1. the referee turns the joint strategy into a timed schedule
    Schedule sched = ScheduleBuilder::build(inst, profile);
    Solution sol(profile, sched);

    // 2. decode the MAV to actual machine ids
    const int n = inst.totalOperations();
    sol.machine.assign(n, -1);
    for (int gid = 0; gid < n; ++gid) {
        const Operation& op = inst.operationByGlobalId(gid);
        sol.machine[gid] = op.machineOfAlternative(profile.alternativeOf(gid));
    }

    // 3. each player's own strategy + the total payoff (sum of U_i)
    double total = 0.0;
    for (int j = 0; j < inst.numJobs(); ++j) {
        Strategy s = Strategy::fromProfile(inst, profile, sched, payoff, j);
        total += s.payoff;
        sol.jobStrategies.push_back(s);
    }
    sol.totalPayoff = total;

    // 4. the makespan-dominated fitness used to keep the best solution
    sol.fitness = payoff.fitness(sched);
    return sol;
}

} // namespace fjs
