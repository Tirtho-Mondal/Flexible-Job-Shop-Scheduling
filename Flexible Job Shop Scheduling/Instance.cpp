// ============================================================================
//  Instance.cpp - implementation of the Instance aggregate.
// ============================================================================
#include "Instance.h"

using namespace std;

namespace fjs {

void Instance::setMachineCount(int m) {
    machines.clear();
    machines.reserve(m);
    for (int i = 0; i < m; ++i)
        machines.emplace_back(i);
}

Job& Instance::addJob() {
    jobs.emplace_back((int)jobs.size());
    return jobs.back();
}

void Instance::finalise() {
    // Default every operation to its first eligible machine and build the flat
    // globalId -> (job, position) lookup.  After this point no jobs/operations
    // are added, so references stay stable for the rest of the run.
    opJob.assign(nextGlobalId, -1);
    opPos.assign(nextGlobalId, -1);
    for (Job& j : jobs) {
        for (Operation& op : j.operations()) {
            op.chooseAlternative(0);
            opJob[op.globalId] = op.jobIndex;
            opPos[op.globalId] = op.positionInJob;
        }
    }
}

Operation& Instance::operationByGlobalId(int gid) {
    return jobs[opJob[gid]].operation(opPos[gid]);
}

const Operation& Instance::operationByGlobalId(int gid) const {
    return jobs[opJob[gid]].operation(opPos[gid]);
}

int Instance::totalWork() const {
    int sum = 0;
    for (const Job& j : jobs)
        for (const Operation& op : j.operations())
            sum += op.assignedProcessingTime();
    return sum;
}

} // namespace fjs
