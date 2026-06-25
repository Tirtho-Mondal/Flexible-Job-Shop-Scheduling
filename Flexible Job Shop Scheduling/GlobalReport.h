#pragma once
// ============================================================================
//  GlobalReport.h
//  ---------------------------------------------------------------------------
//  Owns the two cumulative output files:
//    * output/allresult.txt - a running, human-readable table that gets one
//      extra row appended (and flushed) the moment each instance finishes, so
//      it always reflects progress.  Each row shows our makespan next to the
//      literature best-known/optimal value and the gap.
//    * output/README.md      - a markdown summary written once at the end, with
//      per-benchmark aggregate statistics and the full results table.
// ============================================================================

#include "Instance.h"
#include "GameSolver.h"
#include <string>
#include <vector>
#include <fstream>

using namespace std;

namespace fjs {

// One results row (a CLASS, not a struct, with public fields for brevity).
class ResultRow {
public:
    string name, group;
    int  jobs = 0, machines = 0, operations = 0;
    int  initialMakespan = 0, ourMakespan = 0;
    int  bestKnown = -1;       // -1 == unavailable
    bool equilibrium = false;
};

class GlobalReport {
public:
    explicit GlobalReport(const string& allResultPath);

    // Append one finished instance to allresult.txt and remember it for README.
    void append(const Instance& inst, const SolveResult& result, int bestKnown);

    // Write the final markdown summary.
    void writeReadme(const string& readmePath) const;

private:
    ofstream           out;
    vector<ResultRow>  rows;
};

} // namespace fjs
