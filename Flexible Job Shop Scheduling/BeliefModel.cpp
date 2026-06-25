// ============================================================================
//  BeliefModel.cpp - elite pool + belief (frequency) maps for fictitious play.
// ============================================================================
#include "BeliefModel.h"
#include <algorithm>
#include <climits>

using namespace std;

namespace fjs {

BeliefModel::BeliefModel(const Instance& inst, int capacity)
    : inst_(inst), capacity_(max(1, capacity)) {}

int BeliefModel::bestMakespan() const {
    int b = INT_MAX;
    for (int m : makespans_) b = min(b, m);
    return b;
}

void BeliefModel::consider(const StrategyProfile& state, int makespan) {
    // Reject exact duplicates (same routing) to keep the pool diverse.
    for (const StrategyProfile& e : pool_)
        if (e.routing() == state.routing()) return;

    if ((int)pool_.size() < capacity_) {
        pool_.push_back(state);
        makespans_.push_back(makespan);
    } else {
        // Replace the worst elite if this one is better.
        int worst = 0;
        for (int i = 1; i < (int)makespans_.size(); ++i)
            if (makespans_[i] > makespans_[worst]) worst = i;
        if (makespan < makespans_[worst]) {
            pool_[worst]      = state;
            makespans_[worst] = makespan;
        } else {
            return;   // not good enough to change the beliefs
        }
    }
    rebuild();
}

void BeliefModel::rebuild() {
    const int n = inst_.totalOperations();
    const double alpha = 0.5;   // Laplace smoothing -> keeps exploration alive
    freq_.assign(n, {});
    for (int gid = 0; gid < n; ++gid) {
        const int altCount = inst_.operationByGlobalId(gid).alternativeCount();
        vector<double> count(altCount, 0.0);
        for (const StrategyProfile& e : pool_) count[e.alternativeOf(gid)] += 1.0;
        vector<double> p(altCount, 0.0);
        const double denom = pool_.size() + alpha * altCount;
        for (int a = 0; a < altCount; ++a) p[a] = (count[a] + alpha) / denom;
        freq_[gid] = move(p);
    }
}

vector<int> BeliefModel::sampleRouting(mt19937& rng) const {
    vector<int> routing(inst_.totalOperations(), 0);
    for (int gid = 0; gid < inst_.totalOperations(); ++gid)
        routing[gid] = sampleAlternative(gid, rng);
    return routing;
}

int BeliefModel::sampleAlternative(int globalId, mt19937& rng) const {
    if (freq_.empty() || freq_[globalId].empty())
        return 0;
    discrete_distribution<int> dist(freq_[globalId].begin(), freq_[globalId].end());
    return dist(rng);
}

int BeliefModel::argmaxAlternative(int globalId) const {
    if (freq_.empty() || freq_[globalId].empty()) return 0;
    const vector<double>& p = freq_[globalId];
    return (int)(max_element(p.begin(), p.end()) - p.begin());
}

} // namespace fjs
