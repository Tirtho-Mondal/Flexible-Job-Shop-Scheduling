#pragma once
// ============================================================================
//  PayoffFunction.h   <<<  THE HEART OF THE GAME-THEORETIC MODEL  >>>
//  ---------------------------------------------------------------------------
//  There is exactly ONE payoff function. Each JOB is a self-interested player;
//  machines are shared resources the jobs compete for. A player's payoff is
//  higher when it finishes earlier, waits less, causes less machine conflict,
//  and the overall makespan is smaller:
//
//        U_i = 1 / ( 1 + a*C_i + b*W_i + g*Conf_i + d*Cmax )
//
//    C_i   = completion time of job i (its last operation's finish),
//    W_i   = waiting time          = C_i - (sum of its processing times),
//    Conf_i= machine-conflict load = sum over its operations of the total
//            processing booked on the machine each one chose,
//    Cmax  = makespan (the shared term linking each player to global quality).
//
//  forPlayer() is that single function: it returns the payoff U_i together with
//  the parts it is built from, so there is ONE place that defines the payoff.
//
//  fitness() is NOT a payoff - it is the makespan-dominated SELECTION key used to
//  decide which schedule to keep, so the reported makespan / best-known
//  comparison is never affected by the richer payoff.
// ============================================================================

#include "Schedule.h"
#include "Instance.h"
#include <string>

namespace fjs {

// The value of the single payoff function for one player: the payoff U_i and the
// terms it is composed of (kept together so the payoff is computed in one place).
class Payoff {
public:
    double completion = 0;   // C_i
    double waiting    = 0;   // W_i
    double conflict   = 0;   // Conf_i
    double makespan   = 0;   // Cmax (the shared global term)
    double cost       = 0;   // a*C_i + b*W_i + g*Conf_i + d*Cmax
    double utility    = 0;   // U_i = 1 / (1 + cost)   <-- the payoff value
};

class PayoffFunction {
public:
    PayoffFunction(double alpha = 1.0, double beta = 0.3,
                   double gamma = 0.05, double delta = 0.5)
        : alpha_(alpha), beta_(beta), gamma_(gamma), delta_(delta) {}

    // THE payoff function: player `job`'s payoff U_i (and its parts) under `s`.
    Payoff forPlayer(const Schedule& s, const Instance& inst, int job) const;

    // SELECTION key (not a payoff): makespan dominates, total completion breaks
    // ties. This is what the solver keeps the best of.
    long long fitness(const Schedule& s) const;

    // The reported social objective.
    int socialObjective(const Schedule& s) const { return s.makespan(); }

    double alpha() const { return alpha_; }
    double beta()  const { return beta_; }
    double gamma() const { return gamma_; }
    double delta() const { return delta_; }

    std::string description() const;

private:
    double alpha_, beta_, gamma_, delta_;
};

} // namespace fjs
