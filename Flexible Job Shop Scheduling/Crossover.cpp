// ============================================================================
//  Crossover.cpp - POX, OUX (payoff-guided) and OOX recombination operators.
// ============================================================================
#include "Crossover.h"
#include "ScheduleBuilder.h"
#include <vector>

using namespace std;

namespace fjs {

StrategyProfile Crossover::recombine(int type, const StrategyProfile& a,
                                     const StrategyProfile& b, mt19937& rng) const {
    switch (type) {
        case 0:  return pox(a, b, rng);
        case 2:  return oox(a, b, rng);
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
//  payoff U_j between them. The job inherits its WHOLE strategy (machine choices +
//  sequence positions) from the parent in which it is "happier" (higher U_j). The
//  job partition is decided by payoff rather than a fixed 0.5 mixing ratio.
// ---------------------------------------------------------------------------
StrategyProfile Crossover::oux(const StrategyProfile& a, const StrategyProfile& b, mt19937&) const {
    const int n = inst.totalOperations();
    StrategyProfile child(n);

    const Schedule sa = ScheduleBuilder::build(inst, a);
    const Schedule sb = ScheduleBuilder::build(inst, b);
    vector<char> fromA(inst.numJobs(), 0);
    for (int j = 0; j < inst.numJobs(); ++j)
        fromA[j] = (payoff.forPlayer(sa, inst, j).utility >=
                    payoff.forPlayer(sb, inst, j).utility) ? 1 : 0;

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
