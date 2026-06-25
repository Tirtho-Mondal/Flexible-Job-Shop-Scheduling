// ============================================================================
//  Operation.cpp - implementation of the Operation domain class.
// ============================================================================
#include "Operation.h"

using namespace std;

namespace fjs {

Operation::Operation(int globalId, int jobIndex, int positionInJob)
    : globalId(globalId), jobIndex(jobIndex), positionInJob(positionInJob) {}

void Operation::addAlternative(int machine, int processingTime) {
    machines.push_back(machine);
    times.push_back(processingTime);
}

string Operation::label() const {
    return "O(" + to_string(jobIndex + 1) + "," +
                  to_string(positionInJob + 1) + ")";
}

} // namespace fjs
