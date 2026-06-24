// ============================================================================
//  PayoffFunction.cpp - JC-NCGS hybrid payoff (completion + waiting + conflict
//                       + makespan), with a makespan-dominated search key.
// ============================================================================
#include "PayoffFunction.h"
#include <vector>

using namespace std;

namespace fjs {

// Waiting time of a job: its completion minus the time it actually spent being
// processed (i.e. all the idle/blocked time it endured).
double PayoffFunction::jobWaiting(const Schedule& s, const Instance& inst, int job) const {
    long proc = 0;
    for (const Operation& op : inst.job(job).operations()) {
        int gid = op.globalId();
        proc += s.endOf(gid) - s.startOf(gid);
    }
    double w = (double)s.jobCompletion(job) - (double)proc;
    return w < 0 ? 0 : w;
}

// Machine-conflict load seen by a job: for each of its operations, the total
// processing assigned to the machine it chose (a busier machine means more
// contention with rival jobs). Lower is better.
double PayoffFunction::jobConflict(const Schedule& s, const Instance& inst, int job) const {
    // total processing load per machine
    vector<long> load(inst.numMachines(), 0);
    for (int gid = 0; gid < inst.totalOperations(); ++gid)
        load[s.machineOf(gid)] += s.endOf(gid) - s.startOf(gid);
    double conf = 0;
    for (const Operation& op : inst.job(job).operations())
        conf += (double)load[s.machineOf(op.globalId())];
    return conf;
}

double PayoffFunction::playerCost(const Schedule& s, const Instance& inst, int job) const {
    const double Ci   = s.jobCompletion(job);
    const double Wi   = jobWaiting(s, inst, job);
    const double Conf = jobConflict(s, inst, job);
    const double Cmax = s.makespan();
    return alpha_ * Ci + beta_ * Wi + gamma_ * Conf + delta_ * Cmax;
}

double PayoffFunction::playerPayoff(const Schedule& s, const Instance& inst, int job) const {
    return 1.0 / (1.0 + playerCost(s, inst, job));
}

long long PayoffFunction::fitness(const Schedule& s) const {
    // Makespan dominates (so the reported objective and best-known comparison
    // are unaffected); total completion time breaks ties toward schedules where
    // jobs finish earlier on average.
    const long long kMakespanWeight = 1000000LL;
    return (long long)s.makespan() * kMakespanWeight + s.totalCompletion();
}

string PayoffFunction::description() const {
    return
        "JC-NCGS: each job is a self-interested player; machines are shared\n"
        "resources jobs compete for. Job i's hybrid payoff is\n"
        "    U_i = 1 / ( a*C_i + b*W_i + g*Conf_i + d*Cmax )\n"
        "with C_i = completion, W_i = waiting (= C_i - processing), Conf_i =\n"
        "machine-conflict load (busier chosen machines cost more), and Cmax the\n"
        "shared makespan that links each job's payoff to global quality. A\n"
        "schedule is a Nash equilibrium when no job can raise U_i by changing its\n"
        "own machine assignment or sequence position alone. The reported social\n"
        "objective is the makespan Cmax.";
}

} // namespace fjs
