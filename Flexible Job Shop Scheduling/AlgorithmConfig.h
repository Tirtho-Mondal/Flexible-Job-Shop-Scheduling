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
    //     equilibrium. NOTE: a genuine price of anarchy appears only with delta = 0
    //     (the makespan absent from U_i); with delta > 0 the stable payoff is
    //     makespan-aligned, so set delta 0 for the PoA / toll study.
    int selfish         = 0;

    // BILEVEL GAME (game theory on BOTH layers): the GLOBAL layer is a routing game
    // (jobs re-route their critical operations - solo or mutual - whenever it lowers
    // the global potential Phi, anticipating the sequencing equilibrium) ALTERNATING
    // with the LOCAL sequencing game. Converges to a subgame-perfect equilibrium.
    // 1 = on (overrides selfish/potential engine).
    int bilevel         = 0;

    // (UNUSED) Inertia knob from the old simultaneous (synchronous) selfish game,
    // which was replaced by asynchronous best response (descendSelfish) and is no
    // longer read by any engine. Kept only so old settings files still parse.
    double inertia      = 0.5;

    // Memetic mode: when 1, the iterated-local-search perturbation is a CROSSOVER
    // (recombination of two elite equilibria) instead of a random kick, then the
    // game is replayed on the offspring - a genetic/memetic algorithm whose local
    // search is the Nash game. 0 = plain random-kick ILS.
    int crossover       = 1;

    // Which crossover operator (when crossover = 1):
    //   0 = POX : uniform crossover on routing + random-partition POX on sequence.
    //   1 = OUX : payoff-guided - each job inherits its whole strategy from the parent
    //             where it is individually happier (lower own-interest cost own_j).
    //   2 = OOX : order-based one-point - prefix (ops + machines) from parent 1, the
    //             remainder from parent 2 in order.
    int crossoverType   = 1;

    // ---- search control -------------------------------------------------
    int runs            = 50;   // independent multi-start runs per instance
    int memorySize      = 30;   // fictitious-play MEMORY: how many past equilibria the
                            //     players recall to form their beliefs (bounded-memory
                            //     fictitious play). Was "beliefPool".
    int ilsPatienceBase = 60;   // ILS stops after (base + ops/div) non-improving kicks
    int ilsPatienceDiv  = 4;
    int kickMin         = 4;    // ILS kick strength = max(kickMin, ops/kickDiv)
    int kickDiv         = 8;
    int traceRows       = 2500; // accepted moves captured in the per-instance report
};

} // namespace fjs
