// ============================================================================
//  Crossover.cpp - POX, OUX (payoff-guided) and OOX recombination operators.
// ============================================================================
#include "Crossover.h"
#include "ScheduleBuilder.h"
#include <vector>
#include <algorithm>
#include <numeric>

using namespace std;

namespace fjs {

StrategyProfile Crossover::recombine(int type, const StrategyProfile& a,
                                     const StrategyProfile& b, mt19937& rng) const {
    switch (type) {
        case 0:  return pox(a, b, rng);
        case 2:  return oox(a, b, rng);
        case 3:  return pwx(a, b, rng);
        case 4:  return rmx(a, b, rng);
        case 5:  return wgx(a, b, rng);
        case 6:  return cpx(a, b, rng);
        default: return oux(a, b, rng);     // 1 = OUX (default)
    }
}

// ---------------------------------------------------------------------------
//  POX: UNIFORM crossover on the routing (MAV) - each operation inherits its
//  machine from parent A or B with equal probability - plus a RANDOM-partition
//  POX on the dispatch order (OSV): a random subset of jobs keeps parent A's
//  positions, the rest are filled in parent B's relative order.
// ---------------------------------------------------------------------------
StrategyProfile Crossover::pox(const StrategyProfile& a, const StrategyProfile& b, mt19937& rng) const {
    const int n = inst.totalOperations();
    StrategyProfile child(n);

    uniform_int_distribution<int> coin(0, 1);
    for (int gid = 0; gid < n; ++gid)
        child.routing[gid] = coin(rng) ? a.routing[gid] : b.routing[gid];

    vector<char> fromA(inst.numJobs(), 0);
    for (int j = 0; j < inst.numJobs(); ++j) fromA[j] = (char)coin(rng);

    vector<int>& seq = child.sequence;
    seq.assign(n, -1);
    for (int i = 0; i < (int)a.sequence.size(); ++i) {
        const int gid = a.sequence[i];
        if (fromA[inst.operationByGlobalId(gid).jobIndex]) seq[i] = gid;
    }
    int ib = 0;
    for (int i = 0; i < n; ++i) {
        if (seq[i] != -1) continue;
        while (ib < (int)b.sequence.size() &&
               fromA[inst.operationByGlobalId(b.sequence[ib]).jobIndex]) ++ib;
        seq[i] = b.sequence[ib++];
    }
    return child;
}

// ---------------------------------------------------------------------------
//  OUX (payoff-guided): decode both parents and, for each job-player, compare its
//  OWN-interest cost own_j between them. The job inherits its WHOLE strategy (machine
//  choices + sequence positions) from the parent in which it is "happier" (lower
//  own_j). The job partition is decided by payoff rather than a fixed 0.5 mixing
//  ratio. (own_j, not the full U_i, because the stable makespan-aligned U_i is nearly
//  identical across jobs and would collapse the recombination.)
// ---------------------------------------------------------------------------
StrategyProfile Crossover::oux(const StrategyProfile& a, const StrategyProfile& b, mt19937&) const {
    const int n = inst.totalOperations();
    StrategyProfile child(n);

    const Schedule sa = ScheduleBuilder::build(inst, a);
    const Schedule sb = ScheduleBuilder::build(inst, b);
    // Per-job parent selection by the OWN-interest cost (not the makespan-aligned U_i,
    // which is nearly identical across jobs under the stable payoff and would collapse
    // the recombination): a job inherits from the parent where IT is individually
    // happier - i.e. where its own cost is lower.
    vector<char> fromA(inst.numJobs(), 0);
    for (int j = 0; j < inst.numJobs(); ++j)
        fromA[j] = (payoff.forPlayer(sa, inst, j).ownCost <=
                    payoff.forPlayer(sb, inst, j).ownCost) ? 1 : 0;

    for (int gid = 0; gid < n; ++gid) {
        const int j = inst.operationByGlobalId(gid).jobIndex;
        child.routing[gid] = fromA[j] ? a.routing[gid] : b.routing[gid];
    }

    vector<int>& seq = child.sequence;
    seq.assign(n, -1);
    for (int i = 0; i < (int)a.sequence.size(); ++i) {
        const int gid = a.sequence[i];
        if (fromA[inst.operationByGlobalId(gid).jobIndex]) seq[i] = gid;
    }
    int ib = 0;
    for (int i = 0; i < n; ++i) {
        if (seq[i] != -1) continue;
        while (ib < (int)b.sequence.size() &&
               fromA[inst.operationByGlobalId(b.sequence[ib]).jobIndex]) ++ib;
        seq[i] = b.sequence[ib++];
    }
    return child;
}

// ---------------------------------------------------------------------------
//  PWX (Payoff-Weighted Crossover) - a stochastic, ROULETTE version of OUX. Each
//  job is assigned to a parent at RANDOM, with probability proportional to how good
//  that parent is FOR THAT JOB (lower own-interest cost = higher probability):
//        P(job j inherits from A) = own_j(B) / ( own_j(A) + own_j(B) ),
//  so the cheaper (happier) parent is more likely - but not certain. This keeps the
//  payoff guidance of OUX while injecting diversity (OUX is the limit where the
//  happier parent is always chosen). The job then inherits its WHOLE strategy
//  (machines + sequence positions) from the drawn parent; the interleave is the same
//  feasibility-preserving construction as OUX/POX.
// ---------------------------------------------------------------------------
StrategyProfile Crossover::pwx(const StrategyProfile& a, const StrategyProfile& b, mt19937& rng) const {
    const int n = inst.totalOperations();
    StrategyProfile child(n);

    const Schedule sa = ScheduleBuilder::build(inst, a);
    const Schedule sb = ScheduleBuilder::build(inst, b);
    vector<char> fromA(inst.numJobs(), 0);
    uniform_real_distribution<double> coin(0.0, 1.0);
    for (int j = 0; j < inst.numJobs(); ++j) {
        const double oa = payoff.forPlayer(sa, inst, j).ownCost;
        const double ob = payoff.forPlayer(sb, inst, j).ownCost;
        const double denom = oa + ob;
        // P(from A) grows as A's own cost falls relative to B's; ties -> fair coin.
        const double pA = (denom > 1e-12) ? (ob / denom) : 0.5;
        fromA[j] = (coin(rng) < pA) ? 1 : 0;
    }

    for (int gid = 0; gid < n; ++gid) {
        const int j = inst.operationByGlobalId(gid).jobIndex;
        child.routing[gid] = fromA[j] ? a.routing[gid] : b.routing[gid];
    }

    vector<int>& seq = child.sequence;
    seq.assign(n, -1);
    for (int i = 0; i < (int)a.sequence.size(); ++i) {
        const int gid = a.sequence[i];
        if (fromA[inst.operationByGlobalId(gid).jobIndex]) seq[i] = gid;
    }
    int ib = 0;
    for (int i = 0; i < n; ++i) {
        if (seq[i] != -1) continue;
        while (ib < (int)b.sequence.size() &&
               fromA[inst.operationByGlobalId(b.sequence[ib]).jobIndex]) ++ib;
        seq[i] = b.sequence[ib++];
    }
    return child;
}

// ---------------------------------------------------------------------------
//  RMX (Regret-Matching Crossover) - a payoff-guided operator grounded in REGRET
//  MATCHING (Hart & Mas-Colell, 2000), the canonical no-regret learning dynamic in
//  game theory. For each job-player j and parent P, its REGRET is how much its own
//  cost it could still shave by UNILATERALLY best-responding (re-routing one of its
//  operations) in P:
//        regret_j(P) = own_j(P) - min_{j's single reroutes} own_j .
//  A LOW regret means j is already (near) its best response in P - its strategy slice
//  is equilibrium-settled and STABLE. The child therefore inherits each job's whole
//  strategy from the parent where that job has the LOWER regret, recombining the most
//  Nash-stable building blocks. (Ties broken by a fair coin.)
// ---------------------------------------------------------------------------
StrategyProfile Crossover::rmx(const StrategyProfile& a, const StrategyProfile& b, mt19937& rng) const {
    const int n = inst.totalOperations();
    StrategyProfile child(n);

    // Per-job regret in a profile: one base decode, then probe each operation's
    // reroutes and keep the best OWN cost each job could reach by deviating alone.
    auto regrets = [&](const StrategyProfile& p) -> vector<double> {
        const int J = inst.numJobs();
        vector<double> base(J), best(J);
        StrategyProfile q = p;
        const Schedule s0 = ScheduleBuilder::build(inst, q);
        for (int j = 0; j < J; ++j) { base[j] = payoff.forPlayer(s0, inst, j).ownCost; best[j] = base[j]; }
        for (int gid = 0; gid < n; ++gid) {
            const Operation& op = inst.operationByGlobalId(gid);
            if (op.alternativeCount() <= 1) continue;
            const int j = op.jobIndex;
            const int cur = q.alternativeOf(gid);
            for (int alt = 0; alt < op.alternativeCount(); ++alt) {
                if (alt == cur) continue;
                q.reroute(gid, alt);
                const double c = payoff.forPlayer(ScheduleBuilder::build(inst, q), inst, j).ownCost;
                q.reroute(gid, cur);
                if (c < best[j]) best[j] = c;
            }
        }
        vector<double> r(J);
        for (int j = 0; j < J; ++j) r[j] = base[j] - best[j];   // >= 0
        return r;
    };

    const vector<double> ra = regrets(a), rb = regrets(b);
    vector<char> fromA(inst.numJobs(), 0);
    uniform_int_distribution<int> coin(0, 1);
    for (int j = 0; j < inst.numJobs(); ++j) {
        if      (ra[j] < rb[j]) fromA[j] = 1;     // job j is more settled in A
        else if (rb[j] < ra[j]) fromA[j] = 0;     // ... more settled in B
        else                    fromA[j] = (char)coin(rng);
    }

    for (int gid = 0; gid < n; ++gid) {
        const int j = inst.operationByGlobalId(gid).jobIndex;
        child.routing[gid] = fromA[j] ? a.routing[gid] : b.routing[gid];
    }

    vector<int>& seq = child.sequence;
    seq.assign(n, -1);
    for (int i = 0; i < (int)a.sequence.size(); ++i) {
        const int gid = a.sequence[i];
        if (fromA[inst.operationByGlobalId(gid).jobIndex]) seq[i] = gid;
    }
    int ib = 0;
    for (int i = 0; i < n; ++i) {
        if (seq[i] != -1) continue;
        while (ib < (int)b.sequence.size() &&
               fromA[inst.operationByGlobalId(b.sequence[ib]).jobIndex]) ++ib;
        seq[i] = b.sequence[ib++];
    }
    return child;
}

// ---------------------------------------------------------------------------
//  WGX (Welfare-Guided Crossover) - NOVEL. Unlike payoff-guided crossovers that use
//  only a player's PRIVATE payoff, WGX selects each job by its INTERNALISED SOCIAL
//  COST - its private cost PLUS the Pigouvian delay externality it imposes on the
//  rivals queued behind it on its machines:
//        psi_j(P) = own_j(P) + Toll_j(P),
//        Toll_j   = sum over j's ops of  p(op) * (# later rival ops on the same machine).
//  Each job inherits its whole strategy from the parent where psi_j is LOWER - the
//  slice that is good for the job AND least harmful to the system. Recombining these
//  socially-efficient slices steers the offspring toward low-congestion, low-makespan,
//  low-price-of-anarchy schedules: a crossover that embodies the tolling (mechanism-
//  design) mechanism of the model. (Ties broken by a fair coin.)
// ---------------------------------------------------------------------------
StrategyProfile Crossover::wgx(const StrategyProfile& a, const StrategyProfile& b, mt19937& rng) const {
    const int n = inst.totalOperations();
    StrategyProfile child(n);

    // Internalised social cost psi_j = own_j + Toll_j for every job (one decode/parent;
    // the toll is computed per machine from the dispatch order on it).
    auto socialCosts = [&](const StrategyProfile& p) -> vector<double> {
        const int J = inst.numJobs();
        const Schedule s = ScheduleBuilder::build(inst, p);
        vector<double> psi(J);
        for (int j = 0; j < J; ++j) psi[j] = payoff.forPlayer(s, inst, j).ownCost;
        vector<vector<int>> perM(inst.numMachines());
        for (int gid = 0; gid < n; ++gid) perM[s.machineOf(gid)].push_back(gid);
        for (auto& v : perM)
            sort(v.begin(), v.end(), [&](int x, int y){ return s.startOf(x) < s.startOf(y); });
        for (auto& v : perM)
            for (size_t i = 0; i < v.size(); ++i) {
                const int gid = v[i];
                const int j   = inst.operationByGlobalId(gid).jobIndex;
                const int dur = s.endOf(gid) - s.startOf(gid);
                int laterRivals = 0;
                for (size_t k = i + 1; k < v.size(); ++k)
                    if (inst.operationByGlobalId(v[k]).jobIndex != j) ++laterRivals;
                psi[j] += (double)dur * laterRivals;        // delay externality on rivals
            }
        return psi;
    };

    const vector<double> pa = socialCosts(a), pb = socialCosts(b);
    vector<char> fromA(inst.numJobs(), 0);
    uniform_int_distribution<int> coin(0, 1);
    for (int j = 0; j < inst.numJobs(); ++j) {
        if      (pa[j] < pb[j]) fromA[j] = 1;      // job j is socially cheaper in A
        else if (pb[j] < pa[j]) fromA[j] = 0;      // ... cheaper in B
        else                    fromA[j] = (char)coin(rng);
    }

    for (int gid = 0; gid < n; ++gid) {
        const int j = inst.operationByGlobalId(gid).jobIndex;
        child.routing[gid] = fromA[j] ? a.routing[gid] : b.routing[gid];
    }

    vector<int>& seq = child.sequence;
    seq.assign(n, -1);
    for (int i = 0; i < (int)a.sequence.size(); ++i) {
        const int gid = a.sequence[i];
        if (fromA[inst.operationByGlobalId(gid).jobIndex]) seq[i] = gid;
    }
    int ib = 0;
    for (int i = 0; i < n; ++i) {
        if (seq[i] != -1) continue;
        while (ib < (int)b.sequence.size() &&
               fromA[inst.operationByGlobalId(b.sequence[ib]).jobIndex]) ++ib;
        seq[i] = b.sequence[ib++];
    }
    return child;
}

// ---------------------------------------------------------------------------
//  CPX (Coalitional Payoff Crossover) - NOVEL, payoff-guided. Existing payoff-guided
//  crossovers (OUX, PWX) decide each job INDEPENDENTLY, which can break the JOINT
//  machine-sharing arrangement that two interacting jobs relied on. CPX respects the
//  game's interaction structure: jobs that contend on a shared machine (in either
//  parent) are grouped into a COALITION (connected components of the interaction
//  graph), and each WHOLE coalition is inherited from the parent where the coalition's
//  TOTAL payoff is higher (lowest total own cost). This preserves the joint
//  equilibrium configurations that yield low makespan. A small per-job exploration
//  flip keeps diversity (and guarantees recombination even if one coalition dominates).
// ---------------------------------------------------------------------------
StrategyProfile Crossover::cpx(const StrategyProfile& a, const StrategyProfile& b, mt19937& rng) const {
    const int n = inst.totalOperations();
    const int J = inst.numJobs();
    StrategyProfile child(n);

    const Schedule sa = ScheduleBuilder::build(inst, a);
    const Schedule sb = ScheduleBuilder::build(inst, b);
    vector<double> ca(J), cb(J);
    for (int j = 0; j < J; ++j) {
        ca[j] = payoff.forPlayer(sa, inst, j).ownCost;
        cb[j] = payoff.forPlayer(sb, inst, j).ownCost;
    }

    // Interaction graph over jobs (union-find): two jobs are linked if they share a
    // machine in EITHER parent (their operations contend for that resource).
    vector<int> uf(J);
    iota(uf.begin(), uf.end(), 0);
    auto find = [&](int x){ while (uf[x] != x) { uf[x] = uf[uf[x]]; x = uf[x]; } return x; };
    auto link = [&](int x, int y){ uf[find(x)] = find(y); };
    for (const Schedule* s : { &sa, &sb }) {
        vector<int> firstJobOnM(inst.numMachines(), -1);
        for (int gid = 0; gid < n; ++gid) {
            const int m = s->machineOf(gid);
            const int j = inst.operationByGlobalId(gid).jobIndex;
            if (firstJobOnM[m] < 0) firstJobOnM[m] = j;
            else                    link(firstJobOnM[m], j);
        }
    }

    // Each coalition (component root) inherits from the parent with lower TOTAL own cost.
    vector<double> sumA(J, 0.0), sumB(J, 0.0);
    for (int j = 0; j < J; ++j) { const int r = find(j); sumA[r] += ca[j]; sumB[r] += cb[j]; }
    vector<char> rootFromA(J, 0);
    uniform_int_distribution<int> coin(0, 1);
    for (int j = 0; j < J; ++j) {
        const int r = find(j);
        if      (sumA[r] < sumB[r]) rootFromA[r] = 1;
        else if (sumB[r] < sumA[r]) rootFromA[r] = 0;
        else                        rootFromA[r] = (char)coin(rng);
    }

    // Assign each job by its coalition, with a small exploration flip for diversity.
    uniform_real_distribution<double> u01(0.0, 1.0);
    vector<char> fromA(J, 0);
    for (int j = 0; j < J; ++j) {
        char c = rootFromA[find(j)];
        if (u01(rng) < 0.15) c = (char)(1 - c);
        fromA[j] = c;
    }

    for (int gid = 0; gid < n; ++gid) {
        const int j = inst.operationByGlobalId(gid).jobIndex;
        child.routing[gid] = fromA[j] ? a.routing[gid] : b.routing[gid];
    }

    vector<int>& seq = child.sequence;
    seq.assign(n, -1);
    for (int i = 0; i < (int)a.sequence.size(); ++i) {
        const int gid = a.sequence[i];
        if (fromA[inst.operationByGlobalId(gid).jobIndex]) seq[i] = gid;
    }
    int ib = 0;
    for (int i = 0; i < n; ++i) {
        if (seq[i] != -1) continue;
        while (ib < (int)b.sequence.size() &&
               fromA[inst.operationByGlobalId(b.sequence[ib]).jobIndex]) ++ib;
        seq[i] = b.sequence[ib++];
    }
    return child;
}

// ---------------------------------------------------------------------------
//  OOX (order-based one-point): pick a random crossover point. Copy parent A's
//  prefix (operations AND their machine assignments) into the child, then fill the
//  remainder from parent B in order, skipping operations already placed (carrying
//  B's machine for them). A prefix of a feasible sequence is feasible, and the
//  leftover B-operations keep their route order, so the child stays feasible.
// ---------------------------------------------------------------------------
StrategyProfile Crossover::oox(const StrategyProfile& a, const StrategyProfile& b, mt19937& rng) const {
    const int n = inst.totalOperations();
    StrategyProfile child(n);

    int cut = (n > 2) ? (uniform_int_distribution<int>(1, n - 1))(rng) : 1;

    vector<char> placed(n, 0);
    vector<int>& seq = child.sequence;
    seq.clear();
    seq.reserve(n);

    for (int t = 0; t < cut && t < (int)a.sequence.size(); ++t) {   // prefix from A
        const int gid = a.sequence[t];
        seq.push_back(gid);
        placed[gid] = 1;
        child.routing[gid] = a.routing[gid];
    }
    for (int gid : b.sequence) {                                    // remainder from B, in order
        if (placed[gid]) continue;
        seq.push_back(gid);
        placed[gid] = 1;
        child.routing[gid] = b.routing[gid];
    }
    return child;
}

} // namespace fjs
