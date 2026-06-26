// ============================================================================
//  GameSolver.cpp - critical-path best-response game + iterated local search +
//                   multiple runs (keep the best).
// ============================================================================
#define _CRT_SECURE_NO_WARNINGS   // allow getenv under MSVC /sdl
#include "GameSolver.h"
#include "ScheduleBuilder.h"
#include <algorithm>
#include <climits>
#include <numeric>
#include <cstdlib>

using namespace std;

namespace fjs {

GameSolver::GameSolver(const Instance& inst, const PayoffFunction& payoff, unsigned seed)
    : inst(inst), payoff(payoff), rng(seed) {
    // No evaluation budget and no "effort" knob. The search runs until it
    // CONVERGES: every descent runs all the way to a true local optimum (no
    // cut-off), and the solver keeps restarting until neither the per-run
    // iterated local search nor the global incumbent improves for a sustained
    // streak (the stagnation criterion in solve()).
}

Schedule GameSolver::evaluate(const StrategyProfile& state) {
    ++evals;
    return ScheduleBuilder::build(inst, state);
}

void GameSolver::fillRandomSequence(StrategyProfile& state) {
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

StrategyProfile GameSolver::randomProfile() {
    StrategyProfile state(inst.totalOperations());
    for (int gid = 0; gid < inst.totalOperations(); ++gid) {
        const Operation& op = inst.operationByGlobalId(gid);
        uniform_int_distribution<int> pick(0, op.alternativeCount() - 1);
        state.routing[gid] = pick(rng);
    }
    fillRandomSequence(state);
    return state;
}

// Reijnen et al. "Global" machine selection: greedily balance the machine
// workloads (a strong load-balanced starting point for the makespan).
StrategyProfile GameSolver::greedyGlobalProfile() {
    StrategyProfile state(inst.totalOperations());
    vector<long long> load(inst.numMachines(), 0);
    for (int gid = 0; gid < inst.totalOperations(); ++gid) {
        const Operation& op = inst.operationByGlobalId(gid);
        int bestAlt = 0; long long bestFinish = LLONG_MAX;
        for (int a = 0; a < op.alternativeCount(); ++a) {
            long long f = load[op.machineOfAlternative(a)] + op.timeOfAlternative(a);
            if (f < bestFinish) { bestFinish = f; bestAlt = a; }
        }
        state.routing[gid] = bestAlt;
        load[op.machineOfAlternative(bestAlt)] += op.timeOfAlternative(bestAlt);
    }
    fillRandomSequence(state);
    return state;
}

// Reijnen et al. "Local" machine selection: shortest processing time per op.
StrategyProfile GameSolver::greedyLocalProfile() {
    StrategyProfile state(inst.totalOperations());
    for (int gid = 0; gid < inst.totalOperations(); ++gid) {
        const Operation& op = inst.operationByGlobalId(gid);
        int bestAlt = 0, bestTime = INT_MAX;
        for (int a = 0; a < op.alternativeCount(); ++a)
            if (op.timeOfAlternative(a) < bestTime) { bestTime = op.timeOfAlternative(a); bestAlt = a; }
        state.routing[gid] = bestAlt;
    }
    fillRandomSequence(state);
    return state;
}

// Fictitious-play seed: routing drawn from the players' learned beliefs.
StrategyProfile GameSolver::beliefProfile(const BeliefModel& belief) {
    StrategyProfile state(inst.totalOperations());
    state.routing = belief.sampleRouting(rng);
    fillRandomSequence(state);
    return state;
}

vector<int> GameSolver::criticalOperations(const Schedule& sched) const {
    const int n = inst.totalOperations();
    const int m = inst.numMachines();
    const int makespan = sched.makespan();

    // Machine successor = the next operation on the same machine in time order.
    vector<vector<int>> perMachine(m);
    for (int gid = 0; gid < n; ++gid) perMachine[sched.machineOf(gid)].push_back(gid);
    vector<int> machSucc(n, -1);
    for (int k = 0; k < m; ++k) {
        auto& v = perMachine[k];
        sort(v.begin(), v.end(), [&](int a, int b){ return sched.startOf(a) < sched.startOf(b); });
        for (size_t i = 0; i + 1 < v.size(); ++i) machSucc[v[i]] = v[i + 1];
    }
    // Job successor = the next operation of the same job.
    vector<int> jobSucc(n, -1);
    for (int j = 0; j < inst.numJobs(); ++j) {
        const Job& job = inst.job(j);
        for (int k = 0; k + 1 < job.operationCount(); ++k)
            jobSucc[job.operation(k).globalId] = job.operation(k + 1).globalId;
    }
    // Tail (longest weighted path from an op to the sink), computed by scanning
    // operations in order of decreasing finish time so successors come first.
    vector<int> order(n);
    iota(order.begin(), order.end(), 0);
    sort(order.begin(), order.end(), [&](int a, int b){ return sched.endOf(a) > sched.endOf(b); });
    vector<int> tail(n, 0);
    for (int gid : order) {
        int dur = sched.endOf(gid) - sched.startOf(gid);
        int after = 0;
        if (jobSucc[gid]  >= 0) after = max(after, tail[jobSucc[gid]]);
        if (machSucc[gid] >= 0) after = max(after, tail[machSucc[gid]]);
        tail[gid] = dur + after;
    }
    // An operation is critical iff head(start) + tail == makespan.
    vector<int> crit;
    for (int gid = 0; gid < n; ++gid)
        if (sched.startOf(gid) + tail[gid] == makespan) crit.push_back(gid);
    return crit;
}

// True iff `seq` lists each job's operations in their route order (feasible).
static bool precedenceOK(const Instance& inst, const vector<int>& seq) {
    vector<int> need(inst.numJobs(), 0);
    for (int gid : seq) {
        const Operation& op = inst.operationByGlobalId(gid);
        if (op.positionInJob != need[op.jobIndex]) return false;
        ++need[op.jobIndex];
    }
    return true;
}

void GameSolver::considerIncumbent(SolveResult& result, long long& bestFit,
                                   const StrategyProfile& state, const Schedule& sched) {
    long long f = payoff.fitness(sched);
    if (f < bestFit) {
        bestFit = f;
        result.bestState           = state;
        result.bestMakespan        = sched.makespan();
        result.bestTotalCompletion = sched.totalCompletion();
    }
}

void GameSolver::descend(StrategyProfile& state, int run,
                         SolveResult& result, long long& bestFit, int& iteration) {
    // Keep applying the single best improving deviation until none remains - a
    // true Nash equilibrium / critical-path local optimum. There is no budget:
    // the loop ends only when the schedule has converged (kind == 0).
    while (true) {
        Schedule cur = evaluate(state);
        const long long curFit = payoff.fitness(cur);
        const int curMk = cur.makespan();

        // Positions of every operation in the current dispatch order.
        vector<int> posOf(inst.totalOperations(), -1);
        for (int i = 0; i < (int)state.sequence.size(); ++i)
            posOf[state.sequence[i]] = i;

        vector<int> crit = criticalOperations(cur);

        // ============================================================
        //  ONE PLAY OF THE GAME  -  TWO-PLAYER INTERACTION FIRST
        //  -----------------------------------------------------------
        //  Methodology: the makespan is driven down PRIMARILY by two
        //  rival players (jobs whose operations meet on a critical
        //  machine) interacting - swapping their order or jointly
        //  re-routing - until that 2-player game reaches a Nash point
        //  that lowers Cmax. A single job acting ALONE is only a
        //  FALLBACK, used when no rival pair can improve. When neither
        //  helps, the profile is a Nash equilibrium and the caller
        //  applies a random kick and the game is played again.
        // ============================================================

        // ---- bucket A: the TWO-PLAYER interaction move (primary) ----
        long long pairFit = curFit;
        int  pairKind = 0;                 // 2 = order swap, 3 = joint (mutual) reroute
        int  pairGid = -1, pairAlt = -1, pairGid2 = -1, pairAlt2 = -1, pairMk = curMk;
        long long pairSum = cur.totalCompletion();
        vector<int> pairSeq;
        string pairAction, pairType;
        int  pairPartner = -1, pairMachine = -1;
        {
            vector<char> isCrit(inst.totalOperations(), 0);
            for (int g : crit) isCrit[g] = 1;
            vector<vector<int>> perMachine(inst.numMachines());
            for (int gid = 0; gid < inst.totalOperations(); ++gid)
                perMachine[cur.machineOf(gid)].push_back(gid);
            int mutualPairs = 0;
            for (int mm = 0; mm < inst.numMachines(); ++mm) {
                auto& v = perMachine[mm];
                sort(v.begin(), v.end(), [&](int a, int b){ return cur.startOf(a) < cur.startOf(b); });
                for (size_t q = 0; q + 1 < v.size(); ++q) {
                    const int u = v[q], w = v[q + 1];
                    if (!isCrit[u] || !isCrit[w]) continue;
                    const Operation& opU = inst.operationByGlobalId(u);
                    const Operation& opW = inst.operationByGlobalId(w);
                    if (opU.jobIndex == opW.jobIndex) continue;     // same job: not rivals

                    // interaction 1 - the two rivals swap their order on the machine.
                    vector<int> cand = state.resequenced(posOf[w], posOf[u]);
                    if (precedenceOK(inst, cand)) {
                        state.sequence.swap(cand);
                        Schedule s = evaluate(state);
                        long long f = payoff.fitness(s);
                        state.sequence.swap(cand);
                        if (f < pairFit) {
                            pairFit = f; pairKind = 2; pairGid = w; pairSeq = move(cand);
                            pairMk = s.makespan(); pairSum = s.totalCompletion();
                            pairPartner = u; pairMachine = mm; pairType = "swap";
                            pairAction = "swap " + opW.label() + " ahead of " + opU.label() +
                                         " on M" + to_string(mm + 1);
                        }
                    }

                    // interaction 2 - the two rivals play their routing game and move
                    // to the joint best response (both re-pick machines together).
                    if (mutualPairs < 16 && (opU.alternativeCount() > 1 || opW.alternativeCount() > 1)
                        && opU.alternativeCount() * opW.alternativeCount() <= 36) {
                        ++mutualPairs;
                        const int curU = state.alternativeOf(u), curW = state.alternativeOf(w);
                        for (int au = 0; au < opU.alternativeCount(); ++au)
                            for (int aw = 0; aw < opW.alternativeCount(); ++aw) {
                                if (au == curU && aw == curW) continue;
                                state.reroute(u, au); state.reroute(w, aw);
                                Schedule s = evaluate(state);
                                long long f = payoff.fitness(s);
                                state.reroute(u, curU); state.reroute(w, curW);   // revert
                                if (f < pairFit) {
                                    pairFit = f; pairKind = 3;
                                    pairGid = u; pairAlt = au; pairGid2 = w; pairAlt2 = aw;
                                    pairMk = s.makespan(); pairSum = s.totalCompletion();
                                    pairPartner = w; pairMachine = mm; pairType = "mutual";
                                    pairAction = "mutual reroute " + opU.label() + "->M" +
                                        to_string(opU.machineOfAlternative(au) + 1) + ", " +
                                        opW.label() + "->M" +
                                        to_string(opW.machineOfAlternative(aw) + 1);
                                }
                            }
                    }
                }
            }
        }

        // ---- bucket B: a SINGLE-PLAYER move (fallback only) ----
        // Computed only when no two-player interaction can lower the cost, so the
        // game always tries player-vs-player first and falls back to a lone
        // adjustment just to finish off what the interaction left.
        long long soloFit = curFit;
        int  soloKind = 0;                 // 1 = reroute, 2 = resequence
        int  soloGid = -1, soloAlt = -1, soloMk = curMk;
        long long soloSum = cur.totalCompletion();
        vector<int> soloSeq;
        string soloAction, soloType;
        if (pairFit >= curFit) {
            for (int gid : crit) {
                const Operation& op = inst.operationByGlobalId(gid);
                const int job = op.jobIndex;

                const int curAlt = state.alternativeOf(gid);
                for (int alt = 0; alt < op.alternativeCount(); ++alt) {
                    if (alt == curAlt) continue;
                    state.reroute(gid, alt);
                    Schedule s = evaluate(state);
                    long long f = payoff.fitness(s);
                    state.reroute(gid, curAlt);
                    if (f < soloFit) {
                        soloFit = f; soloKind = 1; soloGid = gid; soloAlt = alt;
                        soloMk = s.makespan(); soloSum = s.totalCompletion(); soloType = "reroute";
                        soloAction = "reroute " + op.label() + ": M" +
                                 to_string(op.machineOfAlternative(curAlt) + 1) +
                                 " -> M" + to_string(op.machineOfAlternative(alt) + 1);
                    }
                }

                const int pos = posOf[gid];
                const int p   = op.positionInJob;
                const int predPos = (p == 0) ? -1
                                  : posOf[inst.job(job).operation(p - 1).globalId];
                const int succPos = (p + 1 >= inst.job(job).operationCount())
                                  ? (int)state.sequence.size()
                                  : posOf[inst.job(job).operation(p + 1).globalId];
                int targets[4] = { predPos + 1, succPos - 1, pos - 1, pos + 1 };
                int lastT = -999;
                for (int t : targets) {
                    if (t <= predPos || t >= succPos || t == pos || t < 0) continue;
                    if (t == lastT) continue;
                    lastT = t;
                    vector<int> cand = state.resequenced(pos, t);
                    state.sequence.swap(cand);
                    Schedule s = evaluate(state);
                    long long f = payoff.fitness(s);
                    state.sequence.swap(cand);
                    if (f < soloFit) {
                        soloFit = f; soloKind = 2; soloGid = gid; soloSeq = move(cand);
                        soloMk = s.makespan(); soloSum = s.totalCompletion(); soloType = "resequence";
                        soloAction = "move " + op.label() + " to dispatch slot " +
                                 to_string(t + 1) + " (was " + to_string(pos + 1) + ")";
                    }
                }
            }
        }

        // ---- choose the move: INTERACTION first, lone move only as fallback ----
        int kind = 0;
        int moveGid = -1, moveAlt = -1, moveGid2 = -1, moveAlt2 = -1, moveMk = curMk;
        long long moveSum = cur.totalCompletion();
        vector<int> moveSeq;
        string action, moveType;
        int movePartnerGid = -1, moveMachine = -1;

        if (pairFit < curFit) {                        // PRIMARY: two players interact
            kind = pairKind; moveGid = pairGid; moveAlt = pairAlt;
            moveGid2 = pairGid2; moveAlt2 = pairAlt2;
            moveMk = pairMk; moveSum = pairSum; moveSeq = move(pairSeq);
            action = pairAction; moveType = pairType;
            movePartnerGid = pairPartner; moveMachine = pairMachine;
        } else if (soloFit < curFit) {                 // FALLBACK: one player adjusts alone
            kind = soloKind; moveGid = soloGid; moveAlt = soloAlt;
            moveMk = soloMk; moveSum = soloSum; moveSeq = move(soloSeq);
            action = soloAction; moveType = soloType;
        } else {
            result.equilibriumReached = true; return;  // Nash point - the caller kicks
        }

        // Snapshot the profile we are about to change, for the report's per-iteration
        // bimatrix/calculation block (only for the first few moves, to bound size).
        const bool wantDetail = ((int)result.trace.size() < detailRows);
        StrategyProfile beforeProfile;
        int mAltBefore = -1, rAltBefore = -1;
        if (wantDetail) {
            beforeProfile = state;
            mAltBefore = state.alternativeOf(moveGid);
            if (movePartnerGid >= 0) rAltBefore = state.alternativeOf(movePartnerGid);
        }

        // Apply the best deviation (single reroute, re-sequence/swap, or mutual reroute).
        if (kind == 1)      state.reroute(moveGid, moveAlt);
        else if (kind == 2) state.sequence = moveSeq;
        else /* kind 3 */ { state.reroute(moveGid, moveAlt); state.reroute(moveGid2, moveAlt2); }

        ++result.acceptedMoves;
        if ((int)result.trace.size() < maxTraceRows) {
            const int moverJob = inst.operationByGlobalId(moveGid).jobIndex;
            const int rivalJob = (movePartnerGid >= 0)
                               ? inst.operationByGlobalId(movePartnerGid).jobIndex : -1;
            // The schedule the chosen move produced (so we can read each player's
            // completion AFTER the move and show how their benefit changed).
            const Schedule after = evaluate(state);

            MoveRecord rec;
            rec.iteration = ++iteration; rec.run = run;
            rec.job = moverJob;
            rec.action = action; rec.oldCost = curMk; rec.newCost = moveMk;
            rec.makespan = moveMk; rec.sumCompletion = moveSum;
            rec.rival = rivalJob; rec.contestMachine = moveMachine; rec.moveType = moveType;
            rec.moverCBefore = cur.jobCompletion(moverJob);
            rec.moverCAfter  = after.jobCompletion(moverJob);
            if (rivalJob >= 0) {
                rec.rivalCBefore = cur.jobCompletion(rivalJob);
                rec.rivalCAfter  = after.jobCompletion(rivalJob);
            }
            if (wantDetail) {
                rec.hasDetail = true;
                rec.moverOp = moveGid;
                rec.rivalOp = movePartnerGid;
                rec.moverAltBefore = mAltBefore;
                rec.moverAltAfter  = state.alternativeOf(moveGid);
                rec.rivalAltBefore = rAltBefore;
                rec.rivalAltAfter  = (movePartnerGid >= 0) ? state.alternativeOf(movePartnerGid) : -1;
                rec.stateBefore    = beforeProfile;
            }
            result.trace.push_back(rec);
        } else { ++iteration; }

        long long f = (long long)moveMk * 1000000LL + moveSum;
        if (f < bestFit) {
            bestFit = f;
            result.bestState = state;
            result.bestMakespan = moveMk;
            result.bestTotalCompletion = moveSum;
        }
    }
}

SolveResult GameSolver::solve() {
    SolveResult result;
    result.name          = inst.name;
    result.numJobs       = inst.numJobs();
    result.numMachines   = inst.numMachines();
    result.numOperations = inst.totalOperations();

    long long bestFit = LLONG_MAX;
    int iteration = 0;
    int run = 0;
    // A stronger kick than a plain ILS: because the descent now plays TWO-PLAYER
    // moves first, its local optima are "interaction-stable", so escaping them
    // needs a slightly larger perturbation to re-open new rival pairings.
    const int kickStrength = max(4, inst.totalOperations() / 8);

    // Fictitious-play memory of the best equilibria found so far, plus the ILS kick.
    BeliefModel belief(inst, 30);
    RandomKick  kick(inst, rng);

    // totalRun controls the whole search: every instance performs this many
    // independent runs. There is no evaluation budget - each run descends to a
    // true local optimum and its iterated local search runs to convergence
    // (ILS_PATIENCE consecutive non-improving kicks). Default 20 (the two-player-
    // first descent settles on interaction-stable optima, so a few more restarts
    // recover the optimum); override with the FJS_RUNS env var if needed.
    int totalRun = 50;
    if (const char* e = getenv("FJS_RUNS")) { int v = atoi(e); if (v >= 1 && v <= 100000) totalRun = v; }
    const int ILS_PATIENCE = 60 + inst.totalOperations() / 4;

    for (run = 0; run < totalRun; ++run) {
        // Choose how this run is seeded. Run 0 is ALWAYS fully random (the
        // required random initialisation); later runs are seeded by the learned
        // beliefs and the Global/Local greedy heuristics, then refined by play.
        StrategyProfile state;
        if (run == 0) {
            state = randomProfile();
        } else if (run == 1) {
            state = TaskPool::build(inst);   // strong task-pool (earliest-completion) seed
        } else {
            int mode = run % 6;
            if (mode == 1 && belief.ready()) state = beliefProfile(belief);
            else if (mode == 2)              state = greedyGlobalProfile();
            else if (mode == 3)              state = greedyLocalProfile();
            else if (mode == 4)              state = TaskPool::build(inst);
            else                             state = randomProfile();
        }

        Schedule s0 = evaluate(state);
        if (run == 0) { result.initialMakespan = s0.makespan(); result.initialState = state; }
        considerIncumbent(result, bestFit, state, s0);

        StrategyProfile runBest = state;
        long long runBestFit = payoff.fitness(s0);

        descend(state, run, result, bestFit, iteration);
        { Schedule sd = evaluate(state); long long f = payoff.fitness(sd);
          if (f < runBestFit) { runBestFit = f; runBest = state; } }

        // Iterated local search until convergence: kick the run's best (aimed by
        // beliefs) and descend, until no improvement for ILS_PATIENCE kicks.
        int stagnantKicks = 0;
        while (stagnantKicks < ILS_PATIENCE) {
            StrategyProfile work = runBest;
            kick.apply(work, kickStrength, &belief);
            descend(work, run, result, bestFit, iteration);
            Schedule sw = evaluate(work);
            long long f = payoff.fitness(sw);
            if (f < runBestFit) { runBestFit = f; runBest = work; stagnantKicks = 0; }
            else                ++stagnantKicks;
        }

        const int runBestMk = (int)(runBestFit / 1000000LL);
        result.runBests.push_back(runBestMk);
        belief.consider(runBest, runBestMk);   // learn from this run's best
    }

    result.runsRun     = run;
    result.evaluations = evals;
    return result;
}

} // namespace fjs
