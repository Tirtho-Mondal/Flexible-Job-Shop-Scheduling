// ============================================================================
//  Machine.cpp - implementation of the Machine domain class.
// ============================================================================
#include "Machine.h"

using namespace std;

namespace fjs {

Machine::Machine(int index)
    : index_(index) {}

string Machine::label() const {
    return "M" + to_string(index_ + 1);
}

} // namespace fjs
