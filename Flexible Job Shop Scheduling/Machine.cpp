// ============================================================================
//  Machine.cpp - implementation of the Machine domain class.
// ============================================================================
#include "Machine.h"

using namespace std;

namespace fjs {

Machine::Machine(int index)
    : index(index) {}

string Machine::label() const {
    return "M" + to_string(index + 1);
}

} // namespace fjs
