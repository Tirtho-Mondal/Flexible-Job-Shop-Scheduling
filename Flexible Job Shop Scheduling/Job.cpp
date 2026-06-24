// ============================================================================
//  Job.cpp - implementation of the Job (player) domain class.
// ============================================================================
#include "Job.h"

using namespace std;

namespace fjs {

Job::Job(int index)
    : index_(index) {}

Operation& Job::addOperation(int globalId) {
    ops_.emplace_back(globalId, index_, (int)ops_.size());
    return ops_.back();
}

int Job::routeWorkload() const {
    int sum = 0;
    for (const Operation& op : ops_)
        sum += op.assignedProcessingTime();
    return sum;
}

} // namespace fjs
