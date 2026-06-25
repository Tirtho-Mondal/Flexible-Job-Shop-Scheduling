// ============================================================================
//  Player.cpp - the job-as-player agent of the scheduling game.
// ============================================================================
#include "Player.h"
#include "Instance.h"
#include "StrategyProfile.h"
#include "Schedule.h"
#include "PayoffFunction.h"
#include "ScheduleBuilder.h"
#include "Strategy.h"
#include <sstream>

using namespace std;

namespace fjs {

Player::Player(const Instance& inst, int jobIndex)
    : inst_(&inst), index_(jobIndex), label_(inst.job(jobIndex).label()) {}

int Player::operationCount() const {
    return inst_->job(index_).operationCount();
}

Strategy Player::strategy(const StrategyProfile& profile, const Schedule& sched,
                          const PayoffFunction& payoff) const {
    return Strategy::fromProfile(*inst_, profile, sched, payoff, index_);
}

int Player::completion(const Schedule& sched) const {
    return sched.jobCompletion(index_);
}

double Player::waiting(const Schedule& sched, const PayoffFunction& payoff) const {
    return payoff.forPlayer(sched, *inst_, index_).waiting;
}

double Player::conflict(const Schedule& sched, const PayoffFunction& payoff) const {
    return payoff.forPlayer(sched, *inst_, index_).conflict;
}

double Player::utility(const Schedule& sched, const PayoffFunction& payoff) const {
    return payoff.forPlayer(sched, *inst_, index_).utility;
}

bool Player::canImprove(const StrategyProfile& profile, const PayoffFunction& payoff,
                        string* how) const {
    // Fitness of the current joint outcome.
    const long long baseFit = payoff.fitness(ScheduleBuilder::build(*inst_, profile));

    // Try every unilateral single-operation re-route of THIS player's operations.
    const Job& job = inst_->job(index_);
    for (const Operation& op : job.operations()) {
        const int gid = op.globalId();
        const int cur = profile.alternativeOf(gid);
        for (int a = 0; a < op.alternativeCount(); ++a) {
            if (a == cur) continue;
            StrategyProfile deviation = profile;          // copy, change one choice
            deviation.setAlternativeOf(gid, a);
            Schedule s = ScheduleBuilder::build(*inst_, deviation);
            if (payoff.fitness(s) < baseFit) {
                if (how) {
                    ostringstream os;
                    os << "re-route " << op.label()
                       << " to M" << (op.machineOfAlternative(a) + 1);
                    *how = os.str();
                }
                return true;
            }
        }
    }
    return false;
}

} // namespace fjs
