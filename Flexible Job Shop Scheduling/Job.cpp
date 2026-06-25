// ============================================================================
//  Job.cpp - implementation of the Job (player) domain class.
// ============================================================================
#include "Job.h"

using namespace std;

namespace fjs {

Job::Job(int index)
    : index(index) {}

Operation& Job::addOperation(int globalId) {
    ops.emplace_back(globalId, index, (int)ops.size());
    return ops.back();
}

int Job::routeWorkload() const {
    int sum = 0;
    for (const Operation& op : ops)
        sum += op.assignedProcessingTime();
    return sum;
}

} // namespace fjs
