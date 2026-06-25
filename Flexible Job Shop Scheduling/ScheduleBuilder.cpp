// ============================================================================
//  ScheduleBuilder.cpp - the as-early-as-possible decoder.
// ============================================================================
#include "ScheduleBuilder.h"
#include <vector>
#include <algorithm>

using namespace std;

namespace fjs {

Schedule ScheduleBuilder::build(const Instance& inst, const StrategyProfile& state) {
    const int numOps = inst.totalOperations();
    const int numJobs = inst.numJobs();
    const int numMachines = inst.numMachines();

    Schedule sched(numOps, numJobs);

    // For an ACTIVE schedule we remember the busy intervals already placed on
    // each machine (kept sorted by start time) so a later operation can slot
    // into an earlier idle gap instead of always queueing at the very end.
    vector<vector<pair<int,int>>> busy(numMachines);
    vector<int> jobLastEnd(numJobs, 0); // finish of a job's latest op so far

    for (int gid : state.sequence) {
        const Operation& op = inst.operationByGlobalId(gid);
        const int alt     = state.alternativeOf(gid);
        const int machine = op.machineOfAlternative(alt);
        const int ptime   = op.timeOfAlternative(alt);
        const int job     = op.jobIndex;

        const int release = jobLastEnd[job];      // earliest the part is ready
        auto& iv = busy[machine];

        // Find the earliest start >= release where a slot of length ptime fits
        // between the machine's existing busy intervals (gap insertion).
        int start = release;
        int prevEnd = 0;
        bool placed = false;
        for (size_t i = 0; i < iv.size(); ++i) {
            const int gapStart = max(release, prevEnd);
            if (gapStart + ptime <= iv[i].first) { start = gapStart; placed = true; break; }
            prevEnd = max(prevEnd, iv[i].second);
        }
        if (!placed) start = max(release, prevEnd); // append after the last op

        const int end = start + ptime;
        sched.recordOperation(gid, machine, start, end);
        jobLastEnd[job] = end;

        // Insert [start,end] keeping the interval list sorted by start time.
        size_t pos = 0;
        while (pos < iv.size() && iv[pos].first < start) ++pos;
        iv.insert(iv.begin() + pos, {start, end});
    }

    int makespan = 0;
    for (int j = 0; j < numJobs; ++j) {
        sched.setJobCompletion(j, jobLastEnd[j]);
        makespan = max(makespan, jobLastEnd[j]);
    }
    sched.setMakespan(makespan);
    return sched;
}

} // namespace fjs
