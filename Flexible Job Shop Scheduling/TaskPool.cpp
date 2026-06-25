// ============================================================================
//  TaskPool.cpp - earliest-completion constructor.
// ============================================================================
#include "TaskPool.h"
#include <vector>
#include <algorithm>
#include <climits>

using namespace std;

namespace fjs {

StrategyProfile TaskPool::build(const Instance& instance) {
    StrategyProfile profile(instance.totalOperations());
    const int jobs = instance.numJobs();
    vector<int> nextOp(jobs, 0), jobReady(jobs, 0);
    vector<int> machineFree(instance.numMachines(), 0);
    vector<int>& sequence = profile.sequence;
    sequence.clear();

    const int total = instance.totalOperations();
    for (int placed = 0; placed < total; ++placed) {
        // Among all ready operations (one per unfinished job) and their eligible
        // machines, pick the (operation, machine) with the earliest completion.
        int bestJob = -1, bestAlt = -1, bestComplete = INT_MAX;
        for (int j = 0; j < jobs; ++j) {
            if (nextOp[j] >= instance.job(j).operationCount()) continue;
            const Operation& op = instance.job(j).operation(nextOp[j]);
            for (int a = 0; a < op.alternativeCount(); ++a) {
                const int machine  = op.machineOfAlternative(a);
                const int start    = max(machineFree[machine], jobReady[j]);
                const int complete = start + op.timeOfAlternative(a);
                if (complete < bestComplete) { bestComplete = complete; bestJob = j; bestAlt = a; }
            }
        }

        const Operation& op = instance.job(bestJob).operation(nextOp[bestJob]);
        const int machine = op.machineOfAlternative(bestAlt);
        const int start   = max(machineFree[machine], jobReady[bestJob]);
        machineFree[machine] = start + op.timeOfAlternative(bestAlt);
        jobReady[bestJob]    = machineFree[machine];
        profile.reroute(op.globalId, bestAlt);
        sequence.push_back(op.globalId);
        ++nextOp[bestJob];
    }
    return profile;
}

} // namespace fjs
