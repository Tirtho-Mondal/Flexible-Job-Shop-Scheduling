// ============================================================================
//  Operation.cpp - implementation of the Operation domain class.
// ============================================================================
#include "Operation.h"

using namespace std;

namespace fjs {

Operation::Operation(int globalId, int jobIndex, int positionInJob)
    : globalId_(globalId), jobIndex_(jobIndex), positionInJob_(positionInJob) {}

void Operation::addAlternative(int machine, int processingTime) {
    machines_.push_back(machine);
    times_.push_back(processingTime);
}

string Operation::label() const {
    return "O(" + to_string(jobIndex_ + 1) + "," +
                  to_string(positionInJob_ + 1) + ")";
}

} // namespace fjs
