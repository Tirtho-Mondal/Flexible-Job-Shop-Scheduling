// ============================================================================
//  RandomKick.cpp - iterated-local-search perturbation.
// ============================================================================
#include "RandomKick.h"
#include <vector>

using namespace std;

namespace fjs {

RandomKick::RandomKick(const Instance& inst, mt19937& engine)
    : instance(inst), rng(engine) {}

void RandomKick::apply(StrategyProfile& profile, int strength, const FictitiousPlay* belief) {
    const int n = instance.totalOperations();
    uniform_int_distribution<int> coin(0, 1);

    for (int s = 0; s < strength; ++s) {
        uniform_int_distribution<int> pickGid(0, n - 1);
        const int gid = pickGid(rng);
        const Operation& op = instance.operationByGlobalId(gid);

        if (op.alternativeCount() > 1 && coin(rng) == 0) {
            // Re-route: draw from beliefs when available (aim the kick at the
            // machine assignments good solutions agree on), else uniformly.
            if (belief && belief->ready())
                profile.reroute(gid, belief->sampleAlternative(gid, rng));
            else {
                uniform_int_distribution<int> pickAlt(0, op.alternativeCount() - 1);
                profile.reroute(gid, pickAlt(rng));
            }
        } else {
            // Re-sequence: move the operation to a random legal slot in its window.
            vector<int> posOf(n, -1);
            for (int i = 0; i < (int)profile.sequence.size(); ++i) posOf[profile.sequence[i]] = i;
            const int job = op.jobIndex, p = op.positionInJob, pos = posOf[gid];
            const int predPos = (p == 0) ? -1
                              : posOf[instance.job(job).operation(p - 1).globalId];
            const int succPos = (p + 1 >= instance.job(job).operationCount())
                              ? (int)profile.sequence.size()
                              : posOf[instance.job(job).operation(p + 1).globalId];
            if (succPos - predPos <= 2) continue;            // no room to move
            uniform_int_distribution<int> pickPos(predPos + 1, succPos - 1);
            const int target = pickPos(rng);
            if (target == pos) continue;
            vector<int> candidate = profile.resequenced(pos, target);
            profile.sequence.swap(candidate);
        }
    }
}

} // namespace fjs
