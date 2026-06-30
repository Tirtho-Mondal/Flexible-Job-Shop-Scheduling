#pragma once
// ============================================================================
//  PayoffFunction.h   <<<  THE HEART OF THE GAME-THEORETIC MODEL  >>>
//  ---------------------------------------------------------------------------
//  There is exactly ONE payoff function. Each JOB is a self-interested player;
//  machines are shared resources the jobs compete for. The player's OWN-interest
//  cost (lower = better) is
//
//        own_i = a*C_i + b*W_i + g*Conf_i + t*Toll_i
//
//    C_i   = completion time of job i (its last operation's finish),
//    W_i   = waiting time          = C_i - (sum of its processing times),
//    Conf_i= machine-conflict load = sum over its operations of the total
//            processing booked on the machine each one chose,
//    Toll_i= Pigouvian congestion toll (delay externality on machine rivals).
//
//  The payoff is a STABLE, makespan-aligned LEXICOGRAPHIC form:
//        d > 0 :  U_i = 1 / ( 1 + d*Cmax + own_i/(1+own_i) )   (default, STABLE)
//        d = 0 :  U_i = 1 / ( 1 + own_i )                      (pure selfish, PoA)
//  When d>0 the integer makespan is the PRIMARY key and own_i/(1+own_i) in [0,1) only
//  breaks ties, so a strictly lower Cmax ALWAYS gives a strictly higher U_i for every
//  player (payoff and makespan can never disagree). When d=0 the makespan is absent,
//  so selfish equilibria may be inefficient (the price of anarchy).
//
//  forPlayer() is that single function: it returns the payoff U_i together with
//  the parts it is built from (including own_i in ownCost), so there is ONE place
//  that defines the payoff.
//
//  globalPotential() is NOT a payoff - it is the makespan-dominated GLOBAL POTENTIAL
//  Phi that best-response moves descend and the solver keeps the minimum of, so the
//  reported makespan / best-known comparison is never affected by the richer payoff.
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
    double toll       = 0;   // Toll_i (Pigouvian congestion toll - NOVELTY)
    double ownCost    = 0;   // a*C_i + b*W_i + g*Conf_i + t*Toll_i (own-interest only,
                             //   NO makespan) - a per-job discriminating signal for the
                             //   payoff-guided crossover, independent of the stable U_i.
    double cost       = 0;   // the cost inside U_i (makespan-dominated when delta>0)
    double utility    = 0;   // U_i = 1 / (1 + cost)   <-- the payoff value
};

class PayoffFunction {
public:
    // The payoff weights are public data (a, b, g, d, t in U_i's cost term).
    double alpha, beta, gamma, delta, tau;

    PayoffFunction(double alpha = 1.0, double beta = 0.3, double gamma = 0.05,
                   double delta = 0.5, double tau = 0.0)
        : alpha(alpha), beta(beta), gamma(gamma), delta(delta), tau(tau) {}

    // THE payoff function: player `job`'s payoff U_i (and its parts) under `s`.
    Payoff forPlayer(const Schedule& s, const Instance& inst, int job) const;

    // GLOBAL POTENTIAL function Phi (not a payoff): makespan dominates, total
    // completion breaks ties. Best-response moves are accepted when they lower Phi,
    // and the solver keeps the schedule of minimum Phi - the potential-game selection
    // key. (Formerly named fitness().)
    long long globalPotential(const Schedule& s) const;

    // The reported social objective.
    int socialObjective(const Schedule& s) const { return s.makespan(); }

    std::string description() const;
};

} // namespace fjs
