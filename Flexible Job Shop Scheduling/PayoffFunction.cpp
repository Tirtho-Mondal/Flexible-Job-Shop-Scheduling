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

    // Own-interest cost: the part a job cares about for itself (lower = better).
    const double own = alpha * Ci + beta * Wi + gamma * Conf + tau * Toll;
    p.ownCost = own;

    if (delta > 0.0) {
        // STABLE, makespan-aligned payoff (LEXICOGRAPHIC). The integer makespan is the
        // PRIMARY key; the own-interest cost is squashed into [0,1) via own/(1+own) and
        // only breaks ties between EQUAL-makespan cells. Because that tie-breaker is
        // strictly < 1, a strictly lower Cmax ALWAYS lowers the cost - so it ALWAYS
        // gives a strictly HIGHER U_i for EVERY player. Hence the minimum-Cmax cell is
        // always the best response / Nash equilibrium: payoff and makespan can never
        // disagree (no more "[A] != NE" inconsistency). delta scales the makespan key
        // (kept >= 1 so the integer ordering is never overturned).
        const double w = (delta >= 1.0) ? delta : 1.0;
        p.cost = w * (double)p.makespan + own / (1.0 + own);
    } else {
        // Pure SELFISH game (delta = 0): own cost only, makespan is NOT in the payoff,
        // so equilibria may be inefficient - the PRICE OF ANARCHY (use for the PoA /
        // congestion-toll study). Set delta > 0 for the stable makespan-aligned payoff.
        p.cost = own;
    }
    p.utility = 1.0 / (1.0 + p.cost);
    return p;
}

long long PayoffFunction::globalPotential(const Schedule& s) const {
    // The global potential Phi: makespan dominates (so the reported objective and
    // best-known comparison are unaffected); total completion time breaks ties toward
    // schedules where jobs finish earlier on average. NOT a payoff - it is the
    // potential the best-response dynamics descends.
    const long long kMakespanWeight = 1000000LL;
    return (long long)s.makespan() * kMakespanWeight + s.totalCompletion();
}

string PayoffFunction::description() const {
    return
        "ONE payoff function. Each job is a self-interested player; machines are\n"
        "shared resources the jobs compete for. Let the job's OWN-interest cost be\n"
        "    own_i = a*C_i + b*W_i + g*Conf_i + t*Toll_i        (lower = better)\n"
        "with C_i = completion, W_i = waiting (= C_i - processing), Conf_i =\n"
        "machine-conflict load (busier chosen machines cost more), and Toll_i the\n"
        "Pigouvian CONGESTION TOLL (the delay externality job i imposes on rivals\n"
        "queued behind it - the coordination device that lowers the price of anarchy).\n"
        "\n"
        "The payoff is a STABLE, makespan-aligned LEXICOGRAPHIC form:\n"
        "    d > 0 :  U_i = 1 / ( 1 + d*Cmax + own_i/(1+own_i) )   (default, STABLE)\n"
        "    d = 0 :  U_i = 1 / ( 1 + own_i )                      (pure selfish)\n"
        "When d>0 the integer makespan is the PRIMARY key and own_i/(1+own_i) in [0,1)\n"
        "only breaks ties between equal-makespan cells. Therefore a strictly lower Cmax\n"
        "ALWAYS gives a strictly higher U_i for EVERY player: payoff and makespan can\n"
        "never disagree, so the minimum-makespan cell is always the Nash equilibrium.\n"
        "When d=0 the makespan is absent from the payoff, so selfish equilibria may be\n"
        "inefficient (the PRICE OF ANARCHY). A schedule is a Nash equilibrium when no\n"
        "job can raise U_i by changing its own machine/sequence alone. The reported\n"
        "social objective is the makespan Cmax.";
}

} // namespace fjs
