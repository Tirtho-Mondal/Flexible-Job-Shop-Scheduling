// ============================================================================
//  GlobalReport.cpp - allresult.txt (incremental) + README.md (final).
// ============================================================================
#include "GlobalReport.h"
#include <iomanip>
#include <sstream>
#include <map>
#include <cmath>

using namespace std;

namespace fjs {

GlobalReport::GlobalReport(const string& allResultPath)
    : out_(allResultPath) {
    out_ << "================================================================================\n";
    out_ << " GAME-THEORETIC FLEXIBLE JOB SHOP SCHEDULING - CUMULATIVE RESULTS\n";
    out_ << " (jobs are selfish players minimising their own completion time;\n";
    out_ << "  the makespan emerges from the Nash equilibrium of the routing/sequencing game)\n";
    out_ << "================================================================================\n\n";
    out_ << left
         << setw(14) << "Instance"
         << setw(13) << "Group"
         << setw(6)  << "Jobs"
         << setw(6)  << "Mch"
         << setw(6)  << "Ops"
         << right
         << setw(10) << "InitCmax"
         << setw(9)  << "OurCmax"
         << setw(11) << "BestKnown"
         << setw(9)  << "Gap%"
         << "  Status\n";
    out_ << string(96, '-') << "\n";
    out_.flush();
}

void GlobalReport::append(const Instance& inst, const SolveResult& result, int bestKnown) {
    ResultRow row;
    row.name = inst.name();  row.group = inst.group();
    row.jobs = inst.numJobs(); row.machines = inst.numMachines();
    row.operations = inst.totalOperations();
    row.initialMakespan = result.initialMakespan;
    row.ourMakespan = result.bestMakespan;
    row.bestKnown = bestKnown;
    row.equilibrium = result.equilibriumReached;
    rows_.push_back(row);

    ostringstream gap, status;
    if (bestKnown >= 0) {
        double g = 100.0 * (row.ourMakespan - bestKnown) / bestKnown;
        gap << fixed << setprecision(2) << g;
        if (row.ourMakespan < bestKnown)       status << "BEAT best-known!";
        else if (row.ourMakespan == bestKnown) status << "matched optimal/BKS";
        else                                   status << "above best-known";
    } else {
        gap << "N/A";
        status << "no published BKS";
    }

    out_ << left
         << setw(14) << row.name
         << setw(13) << row.group
         << setw(6)  << row.jobs
         << setw(6)  << row.machines
         << setw(6)  << row.operations
         << right
         << setw(10) << row.initialMakespan
         << setw(9)  << row.ourMakespan
         << setw(11) << (bestKnown >= 0 ? to_string(bestKnown) : string("N/A"))
         << setw(9)  << gap.str()
         << "  " << status.str() << "\n";
    out_.flush();
}

void GlobalReport::writeReadme(const string& readmePath) const {
    ofstream md(readmePath);
    if (!md) return;

    md << "# Game-Theoretic Flexible Job Shop Scheduling\n\n";
    md << "Each **job is a player** in a non-cooperative game. A player's strategy is the\n";
    md << "machine it picks for each of its operations plus where those operations sit in\n";
    md << "the shared dispatch order. Every player is **selfish**: it minimises only its own\n";
    md << "completion time `C_i` (payoff `u_i = -C_i`). The shop **makespan** "
       << "`Cmax = max_i C_i`\n";
    md << "is what we report against the literature best-known values.\n\n";
    md << "Every instance starts from a **random** strategy profile; the jobs then play\n";
    md << "**best-response dynamics** (with random restarts) until no job can finish earlier\n";
    md << "- a **Nash equilibrium**. The makespan-critical job always has an incentive to\n";
    md << "deviate, so selfish play drives the makespan down.\n\n";

    md << "## How to run\n\n";
    md << "The program takes no console input. It auto-detects the `data/` folder, solves\n";
    md << "every `.fjs` instance and writes everything under `output/`:\n\n";
    md << "- `output/allresult.txt` - cumulative table (our makespan vs best-known),\n";
    md << "- `output/<instance>_log.txt` - per-instance trace + final schedule,\n";
    md << "- `output/README.md` - this summary,\n";
    md << "- `output/code_explanation.md` - how the code and the game model fit together.\n\n";

    // ---- per-group aggregate statistics --------------------------------
    md << "## Summary by benchmark group\n\n";
    md << "| Group | Instances | With BKS | Matched | Beaten | Avg gap % (where known) |\n";
    md << "|---|---:|---:|---:|---:|---:|\n";

    map<string, vector<const ResultRow*>> byGroup;
    for (const ResultRow& r : rows_) byGroup[r.group].push_back(&r);

    for (auto& kv : byGroup) {
        int withBks = 0, matched = 0, beaten = 0; double sumGap = 0;
        for (const ResultRow* r : kv.second) {
            if (r->bestKnown >= 0) {
                ++withBks;
                sumGap += 100.0 * (r->ourMakespan - r->bestKnown) / r->bestKnown;
                if (r->ourMakespan == r->bestKnown) ++matched;
                if (r->ourMakespan <  r->bestKnown) ++beaten;
            }
        }
        md << "| " << kv.first << " | " << kv.second.size() << " | " << withBks
           << " | " << matched << " | " << beaten << " | ";
        if (withBks) { md << fixed << setprecision(2) << (sumGap / withBks); }
        else md << "-";
        md << " |\n";
    }
    md << "\n";

    // ---- full results table --------------------------------------------
    md << "## Full results\n\n";
    md << "| Instance | Group | Jobs | Mch | Ops | Init Cmax | Our Cmax | Best-known | Gap % | Eq |\n";
    md << "|---|---|---:|---:|---:|---:|---:|---:|---:|:--:|\n";
    md << fixed << setprecision(2);
    for (const ResultRow& r : rows_) {
        md << "| " << r.name << " | " << r.group << " | " << r.jobs << " | "
           << r.machines << " | " << r.operations << " | " << r.initialMakespan
           << " | " << r.ourMakespan << " | ";
        if (r.bestKnown >= 0) {
            md << r.bestKnown << " | " << (100.0 * (r.ourMakespan - r.bestKnown) / r.bestKnown);
        } else {
            md << "N/A | -";
        }
        md << " | " << (r.equilibrium ? "Y" : "~") << " |\n";
    }
    md << "\n";
}

} // namespace fjs
