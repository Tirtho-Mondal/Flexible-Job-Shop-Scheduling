#pragma once
// ============================================================================
//  Schedule.h
//  ---------------------------------------------------------------------------
//  The concrete, timed result of decoding a StrategyProfile against an Instance.
//  For every operation it stores the machine it ran on plus its start and end
//  times; for every job it stores the completion time C_i; and it aggregates
//  the makespan (the global objective we report against the best-known values).
//
//  This is the object the PayoffFunction reads to score each player.
//
//  Header-only CLASS with encapsulated vectors and const accessors.
// ============================================================================

#include <vector>
#include <algorithm>

using namespace std;

namespace fjs {

class Schedule {
public:
    Schedule(int operationCount, int jobCount)
        : machine_(operationCount, -1),
          start_(operationCount, 0),
          end_(operationCount, 0),
          completion_(jobCount, 0) {}

    // Filled in by the ScheduleBuilder while it lays operations onto machines.
    void recordOperation(int gid, int machine, int start, int end) {
        machine_[gid] = machine;
        start_[gid]   = start;
        end_[gid]     = end;
    }
    void setJobCompletion(int job, int c) { completion_[job] = c; }
    void setMakespan(int c)               { makespan_ = c; }

    // ---- Objective values ----------------------------------------------
    int makespan()             const { return makespan_; }
    int jobCompletion(int job) const { return completion_[job]; }

    // ---- Per-operation timing ------------------------------------------
    int machineOf(int gid) const { return machine_[gid]; }
    int startOf(int gid)   const { return start_[gid]; }
    int endOf(int gid)     const { return end_[gid]; }

    // Sum of every player's completion time - a social-welfare measure and a
    // handy secondary objective for breaking ties between equal-makespan
    // equilibria.
    long long totalCompletion() const {
        long long s = 0;
        for (int c : completion_) s += c;
        return s;
    }

private:
    vector<int> machine_;     // gid -> machine it ran on
    vector<int> start_;       // gid -> start time
    vector<int> end_;         // gid -> finish time
    vector<int> completion_;  // job -> completion time C_i
    int              makespan_ = 0;
};

} // namespace fjs
