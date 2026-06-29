#pragma once
// ============================================================================
//  NashChecker.h   <<<  pure-strategy Nash verification + price of anarchy  >>>
//  ---------------------------------------------------------------------------
//  Header-only verifier for the job-as-player FJSP game. Given a strategy
//  profile it counts how many PROFITABLE UNILATERAL DEVIATIONS exist: a single
//  job re-routing one of its operations to a different eligible machine, with
//  every other job's strategy held fixed, such that the deviating job's own
//  payoff U_i strictly increases. A profile is a pure-strategy Nash-stable
//  schedule iff that count is zero.
//
//  This is what turns the solver into a game-theoretic study: it certifies the
//  reported schedules as equilibria and supplies the makespan needed to measure
//  the empirical PRICE OF ANARCHY (selfish-equilibrium Cmax vs best-known).
// ============================================================================

#include "Instance.h"
#include "StrategyProfile.h"
#include "PayoffFunction.h"
#include "ScheduleBuilder.h"

namespace fjs {

class NashChecker {
public:
    NashChecker(const Instance& inst, const PayoffFunction& payoff)
        : inst(inst), payoff(payoff) {}

    // Number of profitable unilateral machine-reassignment deviations.
    //   selfish = true  : a job re-routing one operation so that its OWN payoff U_i
    //                     strictly rises (the equilibrium concept of the selfish
    //                     game; zero <=> pure-strategy Nash-stable under U_i).
    //   selfish = false : a job re-routing one operation so that the shared MAKESPAN
    //                     strictly falls (zero <=> a makespan local optimum, the
    //                     reference used by the coordinated/potential engine).
    int countProfitableDeviations(const StrategyProfile& state, bool selfish) const {
        const double eps = 1e-9;
        const Schedule base = ScheduleBuilder::build(inst, state);
        const int baseMk = base.makespan();
        int count = 0;
        for (int j = 0; j < inst.numJobs(); ++j) {
            const double u0 = selfish ? payoff.forPlayer(base, inst, j).utility : 0.0;
            for (const Operation& op : inst.job(j).operations()) {
                const int gid = op.globalId;
                const int cur = state.alternativeOf(gid);
                for (int a = 0; a < op.alternativeCount(); ++a) {
                    if (a == cur) continue;
                    StrategyProfile cand = state;
                    cand.reroute(gid, a);
                    const Schedule s = ScheduleBuilder::build(inst, cand);
                    if (selfish) { if (payoff.forPlayer(s, inst, j).utility > u0 + eps) ++count; }
                    else         { if (s.makespan() < baseMk)                            ++count; }
                }
            }
        }
        return count;
    }

    bool isNashStable(const StrategyProfile& state, bool selfish) const {
        return countProfitableDeviations(state, selfish) == 0;
    }

private:
    const Instance&       inst;
    const PayoffFunction& payoff;
};

} // namespace fjs
