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
        : machine(operationCount, -1),
          start(operationCount, 0),
          end(operationCount, 0),
          completion(jobCount, 0) {}

    // Filled in by the ScheduleBuilder while it lays operations onto machines.
    void recordOperation(int gid, int mc, int st, int en) {
        machine[gid] = mc;
        start[gid]   = st;
        end[gid]     = en;
    }
    void setJobCompletion(int job, int c) { completion[job] = c; }
    void setMakespan(int c)               { cmax = c; }

    // ---- Objective values ----------------------------------------------
    int makespan()             const { return cmax; }
    int jobCompletion(int job) const { return completion[job]; }

    // ---- Per-operation timing ------------------------------------------
    int machineOf(int gid) const { return machine[gid]; }
    int startOf(int gid)   const { return start[gid]; }
    int endOf(int gid)     const { return end[gid]; }

    // Sum of every player's completion time - a social-welfare measure and a
    // handy secondary objective for breaking ties between equal-makespan
    // equilibria.
    long long totalCompletion() const {
        long long s = 0;
        for (int c : completion) s += c;
        return s;
    }

private:
    vector<int> machine;      // gid -> machine it ran on
    vector<int> start;        // gid -> start time
    vector<int> end;          // gid -> finish time
    vector<int> completion;   // job -> completion time C_i
    int              cmax = 0;     // the makespan (returned by makespan())
};

} // namespace fjs
