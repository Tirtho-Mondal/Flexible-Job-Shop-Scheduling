// ============================================================================
//  StableSolution.cpp - verify the Nash-stability of a decoded solution.
// ============================================================================
#include "StableSolution.h"
#include "Instance.h"
#include "StrategyProfile.h"
#include "PayoffFunction.h"
#include "ScheduleBuilder.h"
#include <sstream>

using namespace std;

namespace fjs {

StableSolution::StableSolution(const Instance& inst, const StrategyProfile& profile,
                               const PayoffFunction& payoff)
    : solution(Solution::decode(inst, profile, payoff)) {

    const long long baseFit = solution.fitness;

    // Try every unilateral single-operation re-route. A job acts "alone": only
    // one of its operations moves to another eligible machine, nothing else
    // changes. If that strictly lowers the fitness, the profile is not a Nash
    // equilibrium - record the first profitable deviation and stop.
    for (int j = 0; j < inst.numJobs() && isStable; ++j) {
        const Job& job = inst.job(j);
        for (const Operation& op : job.operations()) {
            const int gid = op.globalId;
            const int cur = profile.alternativeOf(gid);
            for (int a = 0; a < op.alternativeCount(); ++a) {
                if (a == cur) continue;
                StrategyProfile deviation = profile;     // copy, change one choice
                deviation.setAlternativeOf(gid, a);
                Schedule s = ScheduleBuilder::build(inst, deviation);
                if (payoff.globalPotential(s) < baseFit) {
                    ostringstream os;
                    os << job.label() << " could re-route " << op.label()
                       << " to M" << (op.machineOfAlternative(a) + 1)
                       << "  (Cmax " << solution.makespan() << " -> " << s.makespan() << ")";
                    profitableDeviation = os.str();
                    isStable = false;
                    break;
                }
            }
            if (!isStable) break;
        }
    }
}

} // namespace fjs
