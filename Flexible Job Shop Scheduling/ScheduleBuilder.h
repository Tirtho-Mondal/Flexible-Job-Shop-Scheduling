#pragma once
// ============================================================================
//  ScheduleBuilder.h
//  ---------------------------------------------------------------------------
//  Turns a strategy profile (GameState) into a concrete timed Schedule.
//
//  It is a deterministic list-scheduler ("as-early-as-possible" decoder): it
//  walks the global dispatch sequence and places each operation at the earliest
//  moment that respects (a) its job predecessor's finish and (b) the freedom of
//  its chosen machine.  Because the sequence is always precedence-feasible the
//  resulting schedule is always valid.
//
//  This decoder is the shared "referee" of the game: it converts everyone's
//  simultaneous choices into the outcome (start/finish times) each player is
//  then scored on.
// ============================================================================

#include "Instance.h"
#include "GameState.h"
#include "Schedule.h"

using namespace std;

namespace fjs {

class ScheduleBuilder {
public:
    // Decode `state` against `inst` into a fully timed Schedule.
    static Schedule build(const Instance& inst, const GameState& state);
};

} // namespace fjs
