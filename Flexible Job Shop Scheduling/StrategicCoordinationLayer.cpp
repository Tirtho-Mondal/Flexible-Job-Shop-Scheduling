// ============================================================================
//  StrategicCoordinationLayer.cpp  (Layer 1) - global routing search:
//  multi-start + fictitious-play beliefs + crossover, with the Operational
//  Dispatching Layer (the Nash game) as the local optimiser for each plan.
// ============================================================================
#define _CRT_SECURE_NO_WARNINGS   // getenv below
#include "StrategicCoordinationLayer.h"
#include "ScheduleBuilder.h"
#include "NashChecker.h"
#include "TaskPool.h"
#include "RandomKick.h"
#include "Crossover.h"
#include <algorithm>
#include <climits>
#include <numeric>
#include <cstdlib>

using namespace std;

namespace fjs {

StrategicCoordinationLayer::StrategicCoordinationLayer(const Instance& inst,
        const PayoffFunction& payoff, unsigned seed, const AlgorithmConfig& cfg)
    : inst(inst), payoff(payoff), rng(seed), cfg(cfg), op(inst, payoff, cfg) {}

// ---- routing-plan proposals ------------------------------------------------

void StrategicCoordinationLayer::fillRandomSequence(StrategyProfile& state) {
    vector<int> placed(inst.numJobs(), 0);
    vector<int> active;
    for (int j = 0; j < inst.numJobs(); ++j)
        if (inst.job(j).operationCount() > 0) active.push_back(j);
    vector<int>& seq = state.sequence;
    seq.clear();
    while (!active.empty()) {
        uniform_int_distribution<int> pickJob(0, (int)active.size() - 1);
        int idx = pickJob(rng), j = active[idx], k = placed[j];
        seq.push_back(inst.job(j).operation(k).globalId);
        if (++placed[j] >= inst.job(j).operationCount()) {
            active[idx] = active.back(); active.pop_back();
        }
    }
}

StrategyProfile StrategicCoordinationLayer::randomProfile() {
    StrategyProfile state(inst.totalOperations());
    for (int gid = 0; gid < inst.totalOperations(); ++gid) {
        const Operation& op2 = inst.operationByGlobalId(gid);
        uniform_int_distribution<int> pick(0, op2.alternativeCount() - 1);
        state.routing[gid] = pick(rng);
    }
    fillRandomSequence(state);
    return state;
}

// Reijnen et al. "Global" machine selection: greedily balance the machine workloads.
StrategyProfile StrategicCoordinationLayer::greedyGlobalProfile() {
    StrategyProfile state(inst.totalOperations());
    vector<long long> load(inst.numMachines(), 0);
    for (int gid = 0; gid < inst.totalOperations(); ++gid) {
        const Operation& op2 = inst.operationByGlobalId(gid);
        int bestAlt = 0; long long bestFinish = LLONG_MAX;
        for (int a = 0; a < op2.alternativeCount(); ++a) {
            long long f = load[op2.machineOfAlternative(a)] + op2.timeOfAlternative(a);
            if (f < bestFinish) { bestFinish = f; bestAlt = a; }
        }
        state.routing[gid] = bestAlt;
        load[op2.machineOfAlternative(bestAlt)] += op2.timeOfAlternative(bestAlt);
    }
    fillRandomSequence(state);
    return state;
}

// Reijnen et al. "Local" machine selection: shortest processing time per operation.
StrategyProfile StrategicCoordinationLayer::greedyLocalProfile() {
    StrategyProfile state(inst.totalOperations());
    for (int gid = 0; gid < inst.totalOperations(); ++gid) {
        const Operation& op2 = inst.operationByGlobalId(gid);
        int bestAlt = 0, bestTime = INT_MAX;
        for (int a = 0; a < op2.alternativeCount(); ++a)
            if (op2.timeOfAlternative(a) < bestTime) { bestTime = op2.timeOfAlternative(a); bestAlt = a; }
        state.routing[gid] = bestAlt;
    }
    fillRandomSequence(state);
    return state;
}

// Fictitious-play seed: routing drawn from the players' learned beliefs.
StrategyProfile StrategicCoordinationLayer::beliefProfile(const FictitiousPlay& belief) {
    StrategyProfile state(inst.totalOperations());
    state.routing = belief.sampleRouting(rng);
    fillRandomSequence(state);
    return state;
}

void StrategicCoordinationLayer::considerIncumbent(SolveResult& result, long long& bestFit,
                                   const StrategyProfile& state, const Schedule& sched) {
    long long f = payoff.fitness(sched);
    if (f < bestFit) {
        bestFit = f;
        result.bestState           = state;
        result.bestMakespan        = sched.makespan();
        result.bestTotalCompletion = sched.totalCompletion();
    }
}

// ---- the global search loop ------------------------------------------------

SolveResult StrategicCoordinationLayer::solve() {
    SolveResult result;
    result.name          = inst.name;
    result.numJobs       = inst.numJobs();
    result.numMachines   = inst.numMachines();
    result.numOperations = inst.totalOperations();

    long long bestFit = LLONG_MAX;
    int iteration = 0;
    int run = 0;

    // Fallback incumbent for selfish mode (only if no run reaches a certified Nash
    // equilibrium): the best feasible profile seen, by makespan.
    long long fallbackFit = LLONG_MAX;
    StrategyProfile fallbackState;
    auto updateFallback = [&](long long& ff, StrategyProfile& fs,
                              const StrategyProfile& st, const Schedule& sc) {
        const long long f = payoff.fitness(sc);
        if (f < ff) { ff = f; fs = st; }
    };
    const int kickStrength = max(cfg.kickMin, inst.totalOperations() / max(1, cfg.kickDiv));

    // Fictitious-play memory, the ILS kick, and the crossover operators.
    FictitiousPlay belief(inst, cfg.beliefPool);
    RandomKick     kick(inst, rng);
    Crossover      xover(inst, payoff);

    int totalRun = cfg.runs;
    if (const char* e = getenv("FJS_RUNS")) { int v = atoi(e); if (v >= 1 && v <= 100000) totalRun = v; }
    const int ILS_PATIENCE = cfg.selfish
        ? max(6, inst.totalOperations() / 20)
        : cfg.ilsPatienceBase + inst.totalOperations() / max(1, cfg.ilsPatienceDiv);

    for (run = 0; run < totalRun; ++run) {
        // STRATEGIC LAYER: propose a routing plan. Run 0 is always fully random;
        // later runs use beliefs / greedy / task-pool / crossover.
        StrategyProfile state;
        if (cfg.selfish) {
            if (run > 0 && belief.ready() && (run % 2 == 0)) state = beliefProfile(belief);
            else                                            state = randomProfile();
        } else if (run == 0) {
            state = randomProfile();
        } else if (run == 1) {
            state = TaskPool::build(inst);
        } else {
            int mode = run % 6;
            if (mode == 1 && belief.ready()) state = beliefProfile(belief);
            else if (mode == 2)              state = greedyGlobalProfile();
            else if (mode == 3)              state = greedyLocalProfile();
            else if (mode == 4)              state = TaskPool::build(inst);
            else                             state = randomProfile();
        }

        Schedule s0 = op.evaluate(state);
        if (run == 0) { result.initialMakespan = s0.makespan(); result.initialState = state; }
        if (!cfg.selfish) considerIncumbent(result, bestFit, state, s0);

        // OPERATIONAL LAYER: play the conflict game to a Nash equilibrium.
        bool conv;
        if (cfg.selfish) conv = op.descendSelfish(state, run, result, iteration);
        else           { op.descend(state, run, result, bestFit, iteration); conv = true; }

        StrategyProfile runBest = state;
        Schedule sd = op.evaluate(state);
        if (conv) considerIncumbent(result, bestFit, state, sd);     // NE only
        if (cfg.selfish) updateFallback(fallbackFit, fallbackState, state, sd);
        long long runBestFit = payoff.fitness(sd);

        // Iterated local search: perturb the run's best (crossover or random kick),
        // replay the operational game, keep the best Nash endpoint by Cmax.
        int stagnantKicks = 0;
        while (stagnantKicks < ILS_PATIENCE) {
            StrategyProfile work;
            if (cfg.crossover && belief.ready()) {
                uniform_int_distribution<int> pick(0, belief.eliteCount() - 1);
                work = xover.recombine(cfg.crossoverType, runBest, belief.elite(pick(rng)), rng);
                kick.apply(work, max(1, kickStrength / 3), &belief);
            } else {
                work = runBest;
                kick.apply(work, kickStrength, &belief);
            }
            bool c2;
            if (cfg.selfish) c2 = op.descendSelfish(work, run, result, iteration);
            else           { op.descend(work, run, result, bestFit, iteration); c2 = true; }
            Schedule sw = op.evaluate(work);
            if (c2) considerIncumbent(result, bestFit, work, sw);    // NE only
            if (cfg.selfish) updateFallback(fallbackFit, fallbackState, work, sw);
            long long f = payoff.fitness(sw);
            if (f < runBestFit) { runBestFit = f; runBest = work; stagnantKicks = 0; }
            else                ++stagnantKicks;
        }

        const int runBestMk = (int)(runBestFit / 1000000LL);
        result.runBests.push_back(runBestMk);
        belief.consider(runBest, runBestMk);   // feedback: learn from this run's best
    }

    // Safety net for selfish mode: if no certified Nash endpoint was found, report
    // the best feasible profile seen (NashChecker flags it honestly).
    if (cfg.selfish && bestFit == LLONG_MAX && fallbackFit != LLONG_MAX) {
        Schedule fs = op.evaluate(fallbackState);
        result.bestState           = fallbackState;
        result.bestMakespan        = fs.makespan();
        result.bestTotalCompletion = fs.totalCompletion();
    }

    result.runsRun     = run;
    result.evaluations = op.evaluations();

    // Certify the reported schedule as an equilibrium (selfish: own-U_i deviations;
    // coordinated: makespan-reducing deviations). 0 = pure Nash-stable.
    NashChecker checker(inst, payoff);
    result.profitableDeviations = checker.countProfitableDeviations(result.bestState, cfg.selfish != 0);
    result.nashStable           = (result.profitableDeviations == 0);
    return result;
}

} // namespace fjs
