// ============================================================================
//  StrategicCoordinationLayer.cpp  (Layer 1) - global routing search:
//  multi-start + fictitious-play beliefs + crossover, with the Operational
//  Dispatching Layer (the Nash game) as the local optimiser for each plan.
// ============================================================================
#define _CRT_SECURE_NO_WARNINGS   // getenv below
#include "StrategicCoordinationLayer.h"
#include "ScheduleBuilder.h"
#include "NashChecker.h"
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

// Fictitious-play seed: routing drawn from the players' learned beliefs.
StrategyProfile StrategicCoordinationLayer::beliefProfile(const FictitiousPlay& belief) {
    StrategyProfile state(inst.totalOperations());
    state.routing = belief.sampleRouting(rng);
    fillRandomSequence(state);
    return state;
}

void StrategicCoordinationLayer::considerIncumbent(SolveResult& result, long long& bestFit,
                                   const StrategyProfile& state, const Schedule& sched) {
    long long f = payoff.globalPotential(sched);
    if (f < bestFit) {
        bestFit = f;
        result.bestState           = state;
        result.bestMakespan        = sched.makespan();
        result.bestTotalCompletion = sched.totalCompletion();
    }
}

// GLOBAL ROUTING GAME (bilevel upper level). Each job best-responds on its OWN
// payoff by re-routing one of its operations; every candidate routing is evaluated
// by re-equilibrating the LOCAL sequencing game (op.sequencingGame), so jobs choose
// routing ANTICIPATING the sequencing outcome. A sweep in which no job can improve
// its routing is a routing Nash equilibrium = a subgame-perfect equilibrium of the
// two-stage game.
void StrategicCoordinationLayer::playRoutingGame(StrategyProfile& state, int run,
        SolveResult& result, long long& bestFit, int& iteration) {
    const double EPS = 1e-9;
    const int MAX_SWEEPS = 80;

    for (int sweep = 0; sweep < MAX_SWEEPS; ++sweep) {
        // LOCAL layer: re-equilibrate the SEQUENCING GAME once for the current routing.
        op.sequencingGame(state, run, result, iteration);
        considerIncumbent(result, bestFit, state, op.evaluate(state));

        // GLOBAL layer: each job best-responds on its OWN payoff by re-routing one of
        // its CRITICAL operations (only those can change Cmax), evaluated against the
        // current sequencing equilibrium with a SINGLE decode per candidate. The two
        // games ALTERNATE (sequencing re-equilibrates next sweep), converging to a
        // subgame-perfect routing Nash equilibrium - fast, no nested inner game.
        Schedule snap = op.evaluate(state);
        vector<int> crit = op.criticalOperations(snap);
        sort(crit.begin(), crit.end(), [&](int a, int b){
            return snap.jobCompletion(inst.operationByGlobalId(a).jobIndex)
                 > snap.jobCompletion(inst.operationByGlobalId(b).jobIndex); });

        bool improvedAny = false;
        for (int gid : crit) {
            const Operation& opc = inst.operationByGlobalId(gid);
            if (opc.alternativeCount() <= 1) continue;        // no routing choice
            const int curAlt = state.alternativeOf(gid);
            const long long curFit = payoff.globalPotential(op.evaluate(state));   // global potential Phi

            int bestAlt = -1; long long bestFit = curFit;
            for (int alt = 0; alt < opc.alternativeCount(); ++alt) {
                if (alt == curAlt) continue;
                state.reroute(gid, alt);
                const long long f = payoff.globalPotential(op.evaluate(state));
                state.reroute(gid, curAlt);                    // revert (one decode per candidate)
                if (f < bestFit) { bestFit = f; bestAlt = alt; }
            }
            if (bestAlt >= 0) { state.reroute(gid, bestAlt); improvedAny = true; ++result.acceptedMoves; }
        }

        // (2) MUTUAL reroute: two adjacent CRITICAL rival ops on a machine jointly
        // re-pick machines (the joint best response of the routing game) - accepted
        // when it lowers Phi. This is the routing-game two-player move.
        {
            Schedule cur2 = op.evaluate(state);
            vector<int> crit2 = op.criticalOperations(cur2);
            vector<char> isCrit(inst.totalOperations(), 0);
            for (int g : crit2) isCrit[g] = 1;
            vector<vector<int>> perM(inst.numMachines());
            for (int gid = 0; gid < inst.totalOperations(); ++gid) perM[cur2.machineOf(gid)].push_back(gid);
            for (auto& v : perM)
                sort(v.begin(), v.end(), [&](int a, int b){ return cur2.startOf(a) < cur2.startOf(b); });
            int budget = 24;
            for (int m = 0; m < inst.numMachines() && budget > 0; ++m) {
                auto& v = perM[m];
                for (size_t q = 0; q + 1 < v.size() && budget > 0; ++q) {
                    const int u = v[q], w = v[q + 1];
                    if (!isCrit[u] || !isCrit[w]) continue;
                    const Operation& ou = inst.operationByGlobalId(u);
                    const Operation& ow = inst.operationByGlobalId(w);
                    if (ou.jobIndex == ow.jobIndex) continue;
                    if (ou.alternativeCount() * ow.alternativeCount() > 36 ||
                        (ou.alternativeCount() <= 1 && ow.alternativeCount() <= 1)) continue;
                    --budget;
                    const int cu = state.alternativeOf(u), cw = state.alternativeOf(w);
                    const long long curF = payoff.globalPotential(op.evaluate(state));
                    int bAu = -1, bAw = -1; long long bestF = curF;
                    for (int au = 0; au < ou.alternativeCount(); ++au)
                        for (int aw = 0; aw < ow.alternativeCount(); ++aw) {
                            if (au == cu && aw == cw) continue;
                            state.reroute(u, au); state.reroute(w, aw);
                            const long long f = payoff.globalPotential(op.evaluate(state));
                            state.reroute(u, cu); state.reroute(w, cw);
                            if (f < bestF) { bestF = f; bAu = au; bAw = aw; }
                        }
                    if (bAu >= 0) { state.reroute(u, bAu); state.reroute(w, bAw);
                                    improvedAny = true; ++result.acceptedMoves; }
                }
            }
        }

        if (!improvedAny) { result.equilibriumReached = true; return; }   // routing Nash equilibrium
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
        const long long f = payoff.globalPotential(sc);
        if (f < ff) { ff = f; fs = st; }
    };
    const int kickStrength = max(cfg.kickMin, inst.totalOperations() / max(1, cfg.kickDiv));

    // Fictitious-play memory, the ILS kick, and the crossover operators.
    FictitiousPlay belief(inst, cfg.memorySize);
    RandomKick     kick(inst, rng);
    Crossover      xover(inst, payoff);

    int totalRun = cfg.runs;
    if (const char* e = getenv("FJS_RUNS")) { int v = atoi(e); if (v >= 1 && v <= 100000) totalRun = v; }
    const int ILS_PATIENCE = (cfg.selfish || cfg.bilevel)
        ? max(6, inst.totalOperations() / 20)
        : cfg.ilsPatienceBase + inst.totalOperations() / max(1, cfg.ilsPatienceDiv);

    for (run = 0; run < totalRun; ++run) {
        // STRATEGIC LAYER: propose a routing plan. NO GREEDY CONSTRUCTION - run 0 is
        // always fully RANDOM (the required random initialisation); later runs are
        // seeded either from the players' LEARNED BELIEFS (fictitious play) or again
        // at random. The Nash game (Layer 2), the beliefs and the crossover are what
        // drive the search - there is no greedy/dispatching-rule constructor.
        StrategyProfile state;
        if (run > 0 && belief.ready() && (run % 2 == 0)) state = beliefProfile(belief);
        else                                            state = randomProfile();

        Schedule s0 = op.evaluate(state);
        if (run == 0) { result.initialMakespan = s0.makespan(); result.initialState = state; }
        if (!cfg.selfish && !cfg.bilevel) considerIncumbent(result, bestFit, state, s0);

        // Play the game to a Nash equilibrium. BILEVEL = global routing game over the
        // local sequencing game; SELFISH = unilateral+pairwise own-payoff game;
        // otherwise the coordinated makespan engine.
        bool conv;
        if (cfg.bilevel)      { playRoutingGame(state, run, result, bestFit, iteration); conv = true; }
        else if (cfg.selfish)   conv = op.descendSelfish(state, run, result, iteration);
        else                  { op.descend(state, run, result, bestFit, iteration); conv = true; }

        StrategyProfile runBest = state;
        Schedule sd = op.evaluate(state);
        if (conv) considerIncumbent(result, bestFit, state, sd);     // NE only
        if (cfg.selfish) updateFallback(fallbackFit, fallbackState, state, sd);
        long long runBestFit = payoff.globalPotential(sd);

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
            if (cfg.bilevel)      { playRoutingGame(work, run, result, bestFit, iteration); c2 = true; }
            else if (cfg.selfish)   c2 = op.descendSelfish(work, run, result, iteration);
            else                  { op.descend(work, run, result, bestFit, iteration); c2 = true; }
            Schedule sw = op.evaluate(work);
            if (c2) considerIncumbent(result, bestFit, work, sw);    // NE only
            if (cfg.selfish) updateFallback(fallbackFit, fallbackState, work, sw);
            long long f = payoff.globalPotential(sw);
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
    // selfish = own-U_i deviations; bilevel/coordinated = makespan-reducing deviations
    // (both games descend the global potential Phi, so the makespan certifier applies).
    result.profitableDeviations = checker.countProfitableDeviations(result.bestState, cfg.selfish != 0);
    result.nashStable           = (result.profitableDeviations == 0);
    return result;
}

} // namespace fjs
