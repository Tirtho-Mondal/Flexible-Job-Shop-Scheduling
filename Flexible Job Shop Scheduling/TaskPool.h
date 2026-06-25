#pragma once
// ============================================================================
//  TaskPool.h
//  ---------------------------------------------------------------------------
//  The task-pool constructor. It builds a whole strategy profile by letting the
//  READY jobs compete one machine slot at a time (a Giffler-Thompson earliest-
//  completion rule): at each step only the next operation of each job is "ready",
//  and the ready operation that can COMPLETE earliest (over its eligible machines)
//  is dispatched. This jointly builds the OSV (dispatch order) and MAV (machine
//  choice) and gives the game a strong, congestion-aware starting profile.
// ============================================================================

#include "Instance.h"
#include "StrategyProfile.h"

using namespace std;

namespace fjs {

class TaskPool {
public:
    // Build a complete strategy profile for the given instance.
    static StrategyProfile build(const Instance& instance);
};

} // namespace fjs
