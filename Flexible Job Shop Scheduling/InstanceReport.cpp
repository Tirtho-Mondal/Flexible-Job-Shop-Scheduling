// ============================================================================
//  InstanceReport.cpp - the detailed per-instance trace + final schedule.
// ============================================================================
#include "InstanceReport.h"
#include "ScheduleBuilder.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <algorithm>

using namespace std;

namespace fjs {

static double flexibilityOf(const Instance& inst) {
    long alt = 0, ops = 0;
    for (int j = 0; j < inst.numJobs(); ++j)
        for (const Operation& op : inst.job(j).operations()) { alt += op.alternativeCount(); ++ops; }
    return ops ? (double)alt / ops : 0.0;
}

// Operations on a critical path of `sched` (those that set the makespan).
static vector<char> criticalSet(const Instance& inst, const Schedule& sched) {
    const int n = inst.totalOperations(), m = inst.numMachines();
    const int makespan = sched.makespan();
    vector<vector<int>> perM(m);
    for (int gid = 0; gid < n; ++gid) perM[sched.machineOf(gid)].push_back(gid);
    vector<int> machSucc(n, -1);
    for (int k = 0; k < m; ++k) {
        auto& v = perM[k];
        sort(v.begin(), v.end(), [&](int a, int b){ return sched.startOf(a) < sched.startOf(b); });
        for (size_t i = 0; i + 1 < v.size(); ++i) machSucc[v[i]] = v[i + 1];
    }
    vector<int> jobSucc(n, -1);
    for (int j = 0; j < inst.numJobs(); ++j) {
        const Job& job = inst.job(j);
        for (int k = 0; k + 1 < job.operationCount(); ++k)
            jobSucc[job.operation(k).globalId()] = job.operation(k + 1).globalId();
    }
    vector<int> order(n);
    for (int i = 0; i < n; ++i) order[i] = i;
    sort(order.begin(), order.end(), [&](int a, int b){ return sched.endOf(a) > sched.endOf(b); });
    vector<int> tail(n, 0);
    for (int gid : order) {
        int dur = sched.endOf(gid) - sched.startOf(gid), after = 0;
        if (jobSucc[gid]  >= 0) after = max(after, tail[jobSucc[gid]]);
        if (machSucc[gid] >= 0) after = max(after, tail[machSucc[gid]]);
        tail[gid] = dur + after;
    }
    vector<char> isCrit(n, 0);
    for (int gid = 0; gid < n; ++gid)
        if (sched.startOf(gid) + tail[gid] == makespan) isCrit[gid] = 1;
    return isCrit;
}

// Print one individual as the two aligned vectors OSV (job ids, dispatch order)
// and MAV (chosen eligible-machine index, aligned to OSV).
static void writeEncoding(ofstream& f, const Instance& inst,
                          const GameState& st, const string& label) {
    f << label << "\n";
    f << "  OSV :";
    for (int gid : st.sequence()) f << " " << (inst.operationByGlobalId(gid).jobIndex() + 1);
    f << "\n  MAV :";
    for (int gid : st.sequence()) f << " " << (st.alternativeOf(gid) + 1);
    f << "\n";
}

void InstanceReport::write(const string& path, const Instance& inst,
                           const SolveResult& result, const PayoffFunction& payoff,
                           int bestKnown) {
    ofstream f(path);
    if (!f) return;
    f << fixed << setprecision(0);

    // ---- header ---------------------------------------------------------
    f << "============================================================\n";
    f << "  GAME-THEORETIC FJSSP  -  DETAILED RUN LOG\n";
    f << "  Instance : " << inst.name() << "   (group: " << inst.group() << ")\n";
    f << "============================================================\n\n";
    f << "Jobs (players)      : " << inst.numJobs() << "\n";
    f << "Machines (resources): " << inst.numMachines() << "\n";
    f << "Operations          : " << inst.totalOperations() << "\n";
    f << "Avg. flexibility    : " << setprecision(2) << flexibilityOf(inst)
      << setprecision(0) << " machines/operation\n";
    if (bestKnown >= 0) f << "Best-known makespan : " << bestKnown << "\n";
    else                f << "Best-known makespan : N/A\n";
    f << "\n";

    f << "MODEL\n-----\n" << payoff.description() << "\n\n";
    f << "Search: critical-path best-response with SINGLE-JOB and JOB-vs-JOB moves +\n"
      << "fictitious-play belief learning + Global/Local greedy seeding +\n"
      << "iterated local search + multiple runs.\n"
      << "Run 0 starts from a fully RANDOM profile; later runs are seeded by the\n"
      << "players' learned beliefs and greedy heuristics. Each step makes the best\n"
      << "deviation - a single job re-routing/re-sequencing, or two rival jobs that\n"
      << "share a machine swapping order or rerouting together - until neither any\n"
      << "job nor any rival pair can improve (a pairwise Nash-stable schedule). The\n"
      << "single best run is reported below. A single payoff drives everything.\n\n";

    // ---- initial random profile ----------------------------------------
    f << "INITIAL RANDOM PROFILE (restart 0)\n----------------------------------\n";
    f << "Makespan of the first random assignment: " << result.initialMakespan << "\n";
    f << "Initial machine choice per operation:\n";
    for (int j = 0; j < inst.numJobs(); ++j) {
        f << "  " << inst.job(j).label() << ":";
        for (const Operation& op : inst.job(j).operations()) {
            int alt = result.initialState.alternativeOf(op.globalId());
            f << " " << op.label() << "@M" << (op.machineOfAlternative(alt) + 1);
        }
        f << "\n";
    }
    f << "\n";

    // ---- iteration table ------------------------------------------------
    f << "CRITICAL-PATH BEST-RESPONSE ITERATIONS (one row per accepted move)\n";
    f << "Each row: the makespan-critical player re-routes or re-sequences a\n";
    f << "critical operation, lowering the makespan.\n";
    f << "-----------------------------------------------------------------\n";
    f << left
      << setw(7)  << "Iter"
      << setw(5)  << "Run"
      << setw(6)  << "Job"
      << setw(46) << "Action (critical-path deviation)"
      << right
      << setw(9)  << "Cmax_old"
      << setw(9)  << "Cmax_new"
      << setw(11) << "SumC" << "\n";
    f << string(101, '-') << "\n";
    for (const MoveRecord& m : result.trace) {
        string act = m.action;
        if (act.size() > 45) act = act.substr(0, 42) + "...";
        f << left
          << setw(7)  << m.iteration
          << setw(5)  << m.run
          << setw(6)  << inst.job(m.job).label()
          << setw(46) << act
          << right
          << setw(9)  << (long long)m.oldCost
          << setw(9)  << (long long)m.newCost
          << setw(11) << m.sumCompletion << "\n";
    }
    if (result.acceptedMoves > (int)result.trace.size())
        f << "... (" << (result.acceptedMoves - (int)result.trace.size())
          << " further moves omitted; trace capped for readability)\n";
    f << "\n";

    // ---- search summary -------------------------------------------------
    f << "SEARCH SUMMARY\n--------------\n";
    f << "Runs (best kept)    : " << result.runsRun << "\n";
    if (!result.runBests.empty()) {
        int rb = result.runBests.front(), rw = result.runBests.front();
        double sum = 0;
        for (int v : result.runBests) { rb = min(rb, v); rw = max(rw, v); sum += v; }
        f << "Per-run best Cmax   : best=" << rb << "  worst=" << rw
          << "  mean=" << setprecision(1) << (sum / result.runBests.size())
          << setprecision(0) << "  (over " << result.runBests.size() << " runs)\n";
        f << "First runs' best    :";
        for (int i = 0; i < (int)result.runBests.size() && i < 40; ++i) f << " " << result.runBests[i];
        if ((int)result.runBests.size() > 40) f << " ...";
        f << "\n";
    }
    f << "Accepted moves      : " << result.acceptedMoves << "\n";
    f << "Schedule evaluations: " << result.evaluations << "\n";
    f << "Nash equilibrium    : " << (result.equilibriumReached ? "reached (critical-path local optimum)" : "budget limited") << "\n\n";

    // ---- final schedule -------------------------------------------------
    Schedule best = ScheduleBuilder::build(inst, result.bestState);

    f << "FINAL EQUILIBRIUM SCHEDULE\n--------------------------\n";
    f << "Makespan (our result): " << best.makespan() << "\n";
    if (bestKnown >= 0) {
        double gap = 100.0 * (best.makespan() - bestKnown) / bestKnown;
        f << "Best-known makespan  : " << bestKnown << "\n";
        f << "Gap to best-known    : " << setprecision(2) << gap << "%"
          << setprecision(0) << "\n";
    }
    f << "Sum of completions   : " << best.totalCompletion() << "\n";
    f << "Improvement vs init  : " << result.initialMakespan << " -> " << best.makespan()
      << "  (" << setprecision(2)
      << (result.initialMakespan ? 100.0 * (result.initialMakespan - best.makespan()) / result.initialMakespan : 0.0)
      << "% lower)" << setprecision(0) << "\n\n";

    // per-machine sequences
    f << "Machine sequences (op[start-end]):\n";
    vector<vector<int>> byMachine(inst.numMachines());
    for (int gid = 0; gid < inst.totalOperations(); ++gid)
        byMachine[best.machineOf(gid)].push_back(gid);
    for (int m = 0; m < inst.numMachines(); ++m) {
        sort(byMachine[m].begin(), byMachine[m].end(),
                  [&](int a, int b){ return best.startOf(a) < best.startOf(b); });
        f << "  M" << setw(3) << left << (m + 1) << right << ":";
        for (int gid : byMachine[m]) {
            const Operation& op = inst.operationByGlobalId(gid);
            f << " " << op.label() << "[" << best.startOf(gid) << "-" << best.endOf(gid) << "]";
        }
        f << "\n";
    }
    f << "\n";

    // per-job completion
    f << "Job completion times:\n";
    for (int j = 0; j < inst.numJobs(); ++j)
        f << "  " << inst.job(j).label() << " : C = " << best.jobCompletion(j)
          << (best.jobCompletion(j) == best.makespan() ? "   <- makespan-critical job\n" : "\n");
    f << "\n";

    // per-job payoff breakdown (JC-NCGS hybrid payoff)
    f << "Per-job payoff  U_i = 1/(a*C_i + b*W_i + g*Conf_i + d*Cmax):\n";
    f << left << setw(6) << "Job" << right << setw(8) << "C_i"
      << setw(8) << "W_i" << setw(10) << "Conf_i" << setw(10) << "U_i" << "\n";
    f << fixed << setprecision(4);
    for (int j = 0; j < inst.numJobs(); ++j)
        f << left << setw(6) << inst.job(j).label() << right
          << setw(8) << setprecision(0) << best.jobCompletion(j)
          << setw(8) << payoff.jobWaiting(best, inst, j)
          << setw(10) << payoff.jobConflict(best, inst, j)
          << setw(10) << setprecision(4) << payoff.playerPayoff(best, inst, j) << "\n";
    f << setprecision(0) << "\n";

    f << "Full operation timetable:\n";
    f << left << setw(10) << "Op" << setw(6) << "Job"
      << setw(8) << "Machine" << right << setw(8) << "Start"
      << setw(8) << "End" << "\n";
    for (int j = 0; j < inst.numJobs(); ++j)
        for (const Operation& op : inst.job(j).operations()) {
            int gid = op.globalId();
            f << left << setw(10) << op.label()
              << setw(6) << inst.job(j).label()
              << setw(8) << ("M" + to_string(best.machineOf(gid) + 1))
              << right << setw(8) << best.startOf(gid)
              << setw(8) << best.endOf(gid) << "\n";
        }
    f << "\n";

    // ---- individual encoding: the two-vector representation X = (OSV, MAV) ----
    f << "INDIVIDUAL SOLUTION ENCODING  X = (OSV, MAV)\n";
    f << "--------------------------------------------\n";
    f << "One individual = one GameState. OSV = operation sequence vector (job ids in\n";
    f << "dispatch order); MAV = machine assignment vector (chosen eligible-machine #,\n";
    f << "aligned to OSV; 1 = the operation's first eligible machine). Rule: the k-th\n";
    f << "occurrence of job j in OSV is operation O(j,k).\n\n";
    writeEncoding(f, inst, result.bestState,    "Final best individual:");
    f << "\n";
    writeEncoding(f, inst, result.initialState, "Initial random individual (run 0):");
    f << "\n";
    f << "MAV decoded (chosen machine per operation), by job:\n";
    for (int j = 0; j < inst.numJobs(); ++j) {
        f << "  " << inst.job(j).label() << ":";
        for (const Operation& op : inst.job(j).operations()) {
            int alt = result.bestState.alternativeOf(op.globalId());
            f << "  " << op.label() << " alt" << (alt + 1)
              << "->M" << (op.machineOfAlternative(alt) + 1);
        }
        f << "\n";
    }
    f << "\n";

    // ---- job-vs-job interaction graph of the final schedule -------------
    vector<char> isCrit = criticalSet(inst, best);
    f << "JOB-vs-JOB INTERACTIONS (final schedule)\n";
    f << "----------------------------------------\n";
    f << "Rivals = two operations consecutive on a machine that belong to different\n";
    f << "jobs. (*) marks a pair on the critical path - the rivalry that sets Cmax.\n";
    vector<int> degree(inst.numJobs(), 0);
    int edgeCount = 0;
    for (int m = 0; m < inst.numMachines(); ++m) {
        const auto& v = byMachine[m];                 // already start-sorted above
        for (size_t q = 0; q + 1 < v.size(); ++q) {
            const int u = v[q], w = v[q + 1];
            const int ju = inst.operationByGlobalId(u).jobIndex();
            const int jw = inst.operationByGlobalId(w).jobIndex();
            if (ju == jw) continue;
            const bool crit = isCrit[u] && isCrit[w];
            f << "  M" << (m + 1) << ": " << inst.job(ju).label() << " -> " << inst.job(jw).label()
              << "  (" << inst.operationByGlobalId(u).label() << " before "
              << inst.operationByGlobalId(w).label() << ")" << (crit ? "  *" : "") << "\n";
            ++degree[ju]; ++degree[jw]; ++edgeCount;
        }
    }
    if (edgeCount == 0) f << "  (none)\n";
    f << "Interaction degree per job:";
    for (int j = 0; j < inst.numJobs(); ++j)
        f << "  " << inst.job(j).label() << ":" << degree[j];
    f << "\n";
}

} // namespace fjs
