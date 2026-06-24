#pragma once
// ============================================================================
//  PayoffFunction.h   <<<  THE HEART OF THE GAME-THEORETIC MODEL  >>>
//  ---------------------------------------------------------------------------
//  JC-NCGS payoff: each JOB is a self-interested player; machines are shared
//  resources the jobs compete for. A job's payoff is HIGHER when it finishes
//  earlier, waits less, causes less machine conflict, and the overall makespan
//  is smaller. Following the methodology, the payoff is
//
//        U_i(s) = 1 / ( alpha*C_i + beta*W_i + gamma*Conf_i + delta*Cmax )
//
//    C_i   = completion time of job i (its last operation's finish),
//    W_i   = waiting time of job i   = C_i - (sum of its processing times),
//    Conf_i= machine-conflict load   = sum over its operations of the total
//            processing assigned to the machine each one chose (busier machine
//            = more contention with rival jobs),
//    Cmax  = makespan (the global, shared term that ties each job's payoff to
//            overall schedule quality - the "hybrid" individual+global payoff).
//
//  The individual COST minimised is the denominator
//        cost_i = alpha*C_i + beta*W_i + gamma*Conf_i + delta*Cmax,
//  and U_i = 1/(1+cost_i) is the bounded payoff (higher = better).
//
//  The SOCIAL objective we report (and compare to best-known) stays the
//  makespan; `fitness` is the makespan-dominated key used to keep the best
//  schedule, so adding the richer payoff never degrades the reported makespan.
// ============================================================================

#include "Schedule.h"
#include "Instance.h"
#include <string>

namespace fjs {

class PayoffFunction {
public:
    PayoffFunction(double alpha = 1.0, double beta = 0.3,
                   double gamma = 0.05, double delta = 0.5)
        : alpha_(alpha), beta_(beta), gamma_(gamma), delta_(delta) {}

    // ---- payoff components for one job ---------------------------------
    double jobCompletion(const Schedule& s, int job) const { return s.jobCompletion(job); }
    double jobWaiting(const Schedule& s, const Instance& inst, int job) const;
    double jobConflict(const Schedule& s, const Instance& inst, int job) const;

    // Hybrid individual cost and bounded payoff U_i = 1/(1+cost_i).
    double playerCost(const Schedule& s, const Instance& inst, int job) const;
    double playerPayoff(const Schedule& s, const Instance& inst, int job) const;

    // ---- social / search ----------------------------------------------
    int       socialObjective(const Schedule& s) const { return s.makespan(); }
    long long fitness(const Schedule& s) const;        // makespan-dominated key

    double alpha() const { return alpha_; }
    double beta()  const { return beta_; }
    double gamma() const { return gamma_; }
    double delta() const { return delta_; }

    std::string description() const;

private:
    double alpha_, beta_, gamma_, delta_;
};

} // namespace fjs
