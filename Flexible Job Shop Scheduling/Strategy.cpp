// ============================================================================
//  Strategy.cpp - one job-player's strategy, extracted from a decoded profile.
// ============================================================================
#include "Strategy.h"
#include "Instance.h"
#include "StrategyProfile.h"
#include "Schedule.h"
#include "PayoffFunction.h"

using namespace std;

namespace fjs {

Strategy::Strategy(int jobIndex, const string& label)
    : jobIndex(jobIndex), name(label) {}

void Strategy::setInteraction(double c, double w, double cf, double u) {
    completion = c;
    waiting    = w;
    conflict   = cf;
    payoff     = u;
}

Strategy Strategy::fromProfile(const Instance& inst, const StrategyProfile& profile,
                               const Schedule& sched, const PayoffFunction& payoff,
                               int job) {
    const Job& j = inst.job(job);
    Strategy s(job, j.label());

    // Where does each operation sit in the dispatch order (OSV)?
    const vector<int>& seq = profile.sequence;
    vector<int> position(inst.totalOperations(), -1);
    for (int i = 0; i < (int)seq.size(); ++i) position[seq[i]] = i;

    for (const Operation& op : j.operations()) {
        const int gid = op.globalId;
        const int alt = profile.alternativeOf(gid);
        OperationChoice c;
        c.globalId         = gid;
        c.operationLabel   = op.label();
        c.machine          = op.machineOfAlternative(alt);
        c.processingTime   = op.timeOfAlternative(alt);
        c.sequencePosition = position[gid];
        c.start            = sched.startOf(gid);
        c.finish           = sched.endOf(gid);
        s.addChoice(c);
    }

    // The "interaction" outcome of this strategy against the others, read from
    // the single payoff function.
    const Payoff p = payoff.forPlayer(sched, inst, job);
    s.setInteraction(p.completion, p.waiting, p.conflict, p.utility);
    return s;
}

} // namespace fjs
