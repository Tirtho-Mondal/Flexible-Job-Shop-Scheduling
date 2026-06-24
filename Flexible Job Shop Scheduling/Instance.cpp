// ============================================================================
//  Instance.cpp - implementation of the Instance aggregate.
// ============================================================================
#include "Instance.h"

using namespace std;

namespace fjs {

void Instance::setMachineCount(int m) {
    machines_.clear();
    machines_.reserve(m);
    for (int i = 0; i < m; ++i)
        machines_.emplace_back(i);
}

Job& Instance::addJob() {
    jobs_.emplace_back((int)jobs_.size());
    return jobs_.back();
}

void Instance::finalise() {
    // Default every operation to its first eligible machine and build the flat
    // globalId -> (job, position) lookup.  After this point no jobs/operations
    // are added, so references stay stable for the rest of the run.
    opJob_.assign(nextGlobalId_, -1);
    opPos_.assign(nextGlobalId_, -1);
    for (Job& j : jobs_) {
        for (Operation& op : j.operations()) {
            op.chooseAlternative(0);
            opJob_[op.globalId()] = op.jobIndex();
            opPos_[op.globalId()] = op.positionInJob();
        }
    }
}

Operation& Instance::operationByGlobalId(int gid) {
    return jobs_[opJob_[gid]].operation(opPos_[gid]);
}

const Operation& Instance::operationByGlobalId(int gid) const {
    return jobs_[opJob_[gid]].operation(opPos_[gid]);
}

int Instance::totalWork() const {
    int sum = 0;
    for (const Job& j : jobs_)
        for (const Operation& op : j.operations())
            sum += op.assignedProcessingTime();
    return sum;
}

} // namespace fjs
