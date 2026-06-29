// ============================================================================
//  PayoffFunction.cpp - the single hybrid payoff U_i, plus the selection key.
// ============================================================================
#include "PayoffFunction.h"
#include <vector>

using namespace std;

namespace fjs {

// The one and only payoff function. Everything about player `job` is computed
// here: its completion, waiting and machine-conflict, then the hybrid cost and
// the resulting payoff U_i = 1/(1+cost).
Payoff PayoffFunction::forPlayer(const Schedule& s, const Instance& inst, int job) const {
    // C_i and W_i: completion, and waiting = completion - time actually processed.
    long proc = 0;
    for (const Operation& op : inst.job(job).operations()) {
        const int gid = op.globalId;
        proc += s.endOf(gid) - s.startOf(gid);
    }
    const double Ci = s.jobCompletion(job);
    double Wi = Ci - (double)proc;
    if (Wi < 0) Wi = 0;

    // Conf_i: for each of the job's operations, the total processing booked on
    // the machine it chose (a busier machine = more contention with rivals).
    vector<long> load(inst.numMachines(), 0);
    for (int gid = 0; gid < inst.totalOperations(); ++gid)
        load[s.machineOf(gid)] += s.endOf(gid) - s.startOf(gid);
    double Conf = 0;
    for (const Operation& op : inst.job(job).operations())
        Conf += (double)load[s.machineOf(op.globalId)];

    // Toll_i (NOVELTY - Pigouvian congestion toll): the delay externality job i
    // imposes on OTHER jobs that share its machines. For each operation of job i
    // we charge its processing time once for every later operation of a rival job
    // queued behind it on the same machine (each such rival is pushed back by at
    // least that processing time). Internalising this externality steers the
    // selfish best response toward the socially efficient (low-makespan) outcome
    // and provably reduces the price of anarchy. Computed only when tau != 0.
    double Toll = 0;
    if (tau != 0.0) {
        for (const Operation& op : inst.job(job).operations()) {
            const int gid = op.globalId;
            const int mr  = s.machineOf(gid);
            const int st  = s.startOf(gid);
            const int p   = s.endOf(gid) - st;
            int laterRivals = 0;
            for (int g2 = 0; g2 < inst.totalOperations(); ++g2) {
                if (s.machineOf(g2) != mr) continue;            // same machine only
                if (s.startOf(g2) <= st) continue;              // strictly behind it
                if (inst.operationByGlobalId(g2).jobIndex == job) continue;  // rival jobs only
                ++laterRivals;
            }
            Toll += (double)p * laterRivals;
        }
    }

    Payoff p;
    p.completion = Ci;
    p.waiting    = Wi;
    p.conflict   = Conf;
    p.makespan   = s.makespan();
    p.toll       = Toll;
    p.cost       = alpha * Ci + beta * Wi + gamma * Conf + delta * p.makespan + tau * Toll;
    p.utility    = 1.0 / (1.0 + p.cost);
    return p;
}

long long PayoffFunction::fitness(const Schedule& s) const {
    // Makespan dominates (so the reported objective and best-known comparison are
    // unaffected); total completion time breaks ties toward schedules where jobs
    // finish earlier on average. This is the selection key, NOT a payoff.
    const long long kMakespanWeight = 1000000LL;
    return (long long)s.makespan() * kMakespanWeight + s.totalCompletion();
}

string PayoffFunction::description() const {
    return
        "ONE payoff function. Each job is a self-interested player; machines are\n"
        "shared resources the jobs compete for. Job i's payoff is\n"
        "    U_i = 1 / ( 1 + a*C_i + b*W_i + g*Conf_i + d*Cmax + t*Toll_i )\n"
        "with C_i = completion, W_i = waiting (= C_i - processing), Conf_i =\n"
        "machine-conflict load (busier chosen machines cost more), Cmax the shared\n"
        "makespan, and Toll_i the Pigouvian CONGESTION TOLL: the delay externality\n"
        "job i imposes on rival jobs queued behind it on its machines (the novel\n"
        "coordination device that reduces the price of anarchy). A schedule is a\n"
        "Nash equilibrium when no job can raise U_i by changing its own machine\n"
        "assignment or sequence position alone. The reported social objective is\n"
        "the makespan Cmax.";
}

} // namespace fjs
