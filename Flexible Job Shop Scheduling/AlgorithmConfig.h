#pragma once
// ============================================================================
//  AlgorithmConfig.h
//  ---------------------------------------------------------------------------
//  Every tunable parameter of the solver in ONE place, so the whole program can
//  be controlled from a single AlgorithmSetting.txt file (parsed in main.cpp).
//  The defaults below reproduce the built-in behaviour exactly, so an absent or
//  empty AlgorithmSetting.txt changes nothing.
// ============================================================================

namespace fjs {

class AlgorithmConfig {
public:
    // ---- payoff weights -------------------------------------------------
    // U_i = 1 / ( 1 + alpha*C_i + beta*W_i + gamma*Conf_i + delta*Cmax + tau*Toll_i )
    double alpha = 1.0;     // completion-time weight
    double beta  = 0.3;     // waiting-time weight
    double gamma = 0.05;    // machine-conflict weight
    double delta = 0.5;     // shared-makespan coupling (coordination device)
    double tau   = 0.0;     // NOVELTY: Pigouvian congestion-toll weight (delay
                            //          externality a job imposes on machine rivals);
                            //          0 = off (default), >0 = tolled coordination game

    // ---- game mode ------------------------------------------------------
    // 0 = COORDINATED potential-game engine (DEFAULT, best makespan): critical-path
    //     + two-player moves accepted by the global makespan potential; reaches the
    //     optimum on several instances (use this to get the best results).
    // 1 = pure SELFISH non-cooperative game: unilateral then pairwise (Pareto) best
    //     response on each job's OWN payoff U_i. Converges to a certified Nash
    //     equilibrium but has a PRICE OF ANARCHY (worse makespan) - use this to
    //     MEASURE the PoA / the toll's effect, not to get the best makespan.
    int selfish         = 0;

    // Inertia for the simultaneous (independent) selfish game: probability that a
    // job which wants to deviate actually moves in a given round. <1 damps the
    // oscillation of fully-synchronous play so the dynamics converge to a Nash
    // equilibrium. 1.0 = fully synchronous; ~0.5 is a good default.
    double inertia      = 0.5;

    // ---- search control -------------------------------------------------
    int runs            = 50;   // independent multi-start runs per instance
    int beliefPool      = 30;   // elite-pool size for fictitious-play learning
    int ilsPatienceBase = 60;   // ILS stops after (base + ops/div) non-improving kicks
    int ilsPatienceDiv  = 4;
    int kickMin         = 4;    // ILS kick strength = max(kickMin, ops/kickDiv)
    int kickDiv         = 8;
    int traceRows       = 2500; // accepted moves captured in the per-instance report
};

} // namespace fjs
