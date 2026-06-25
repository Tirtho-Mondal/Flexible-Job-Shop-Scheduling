// ============================================================================
//  BeliefModel.cpp - elite pool + belief (frequency) maps for fictitious play.
// ============================================================================
#include "BeliefModel.h"
#include <algorithm>
#include <climits>

using namespace std;

namespace fjs {

BeliefModel::BeliefModel(const Instance& inst, int capacity)
    : inst(inst), capacity(max(1, capacity)) {}

int BeliefModel::bestMakespan() const {
    int b = INT_MAX;
    for (int m : makespans) b = min(b, m);
    return b;
}

void BeliefModel::consider(const StrategyProfile& state, int makespan) {
    // Reject exact duplicates (same routing) to keep the pool diverse.
    for (const StrategyProfile& e : pool)
        if (e.routing == state.routing) return;

    if ((int)pool.size() < capacity) {
        pool.push_back(state);
        makespans.push_back(makespan);
    } else {
        // Replace the worst elite if this one is better.
        int worst = 0;
        for (int i = 1; i < (int)makespans.size(); ++i)
            if (makespans[i] > makespans[worst]) worst = i;
        if (makespan < makespans[worst]) {
            pool[worst]      = state;
            makespans[worst] = makespan;
        } else {
            return;   // not good enough to change the beliefs
        }
    }
    rebuild();
}

void BeliefModel::rebuild() {
    const int n = inst.totalOperations();
    const double alpha = 0.5;   // Laplace smoothing -> keeps exploration alive
    freq.assign(n, {});
    for (int gid = 0; gid < n; ++gid) {
        const int altCount = inst.operationByGlobalId(gid).alternativeCount();
        vector<double> count(altCount, 0.0);
        for (const StrategyProfile& e : pool) count[e.alternativeOf(gid)] += 1.0;
        vector<double> p(altCount, 0.0);
        const double denom = pool.size() + alpha * altCount;
        for (int a = 0; a < altCount; ++a) p[a] = (count[a] + alpha) / denom;
        freq[gid] = move(p);
    }
}

vector<int> BeliefModel::sampleRouting(mt19937& rng) const {
    vector<int> routing(inst.totalOperations(), 0);
    for (int gid = 0; gid < inst.totalOperations(); ++gid)
        routing[gid] = sampleAlternative(gid, rng);
    return routing;
}

int BeliefModel::sampleAlternative(int globalId, mt19937& rng) const {
    if (freq.empty() || freq[globalId].empty())
        return 0;
    discrete_distribution<int> dist(freq[globalId].begin(), freq[globalId].end());
    return dist(rng);
}

int BeliefModel::argmaxAlternative(int globalId) const {
    if (freq.empty() || freq[globalId].empty()) return 0;
    const vector<double>& p = freq[globalId];
    return (int)(max_element(p.begin(), p.end()) - p.begin());
}

} // namespace fjs
