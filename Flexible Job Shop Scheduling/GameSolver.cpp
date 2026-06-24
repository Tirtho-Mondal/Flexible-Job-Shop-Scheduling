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
    : inst_(inst), payoff_(payoff), rng_(seed) {
    const long ops = inst_.totalOperations();

    // Each run must be long enough for one critical-path descent to actually
    // CONVERGE to a local optimum (otherwise large instances are cut off mid
    // descent and never reach a good schedule). So the run length scales with
    // the instance size; on top of that we do a fixed number of belief-learning
    // runs, which the FJS_EFFORT env var multiplies for higher quality.
    perRunBudget_ = 500L * ops;
    if (perRunBudget_ < 25000L)  perRunBudget_ = 25000L;
    if (perRunBudget_ > 300000L) perRunBudget_ = 300000L;

    // Number of independent belief-learning runs per instance. Default 1000;
    // override with the FJS_RUNS env var (e.g. 100 for a quick pass). FJS_EFFORT
    // is an extra multiplier kept for convenience.
    long runs = 100;
    if (const char* e = getenv("FJS_RUNS")) {
        runs = atol(e);
        if (runs < 1)      runs = 1;
        if (runs > 100000) runs = 100000;
    }
    long effort = 1;
    if (const char* e = getenv("FJS_EFFORT")) {
        effort = atol(e);
        if (effort < 1)  effort = 1;
        if (effort > 50) effort = 50;
    }
    evalBudget_ = perRunBudget_ * runs * effort;
}

Schedule GameSolver::evaluate(const GameState& state) {
    ++evals_;
    return ScheduleBuilder::build(inst_, state);
}

void GameSolver::fillRandomSequence(GameState& state) {
    vector<int> placed(inst_.numJobs(), 0);
    vector<int> active;
    for (int j = 0; j < inst_.numJobs(); ++j)
        if (inst_.job(j).operationCount() > 0) active.push_back(j);
    vector<int>& seq = state.sequence();
    seq.clear();
    while (!active.empty()) {
        uniform_int_distribution<int> pickJob(0, (int)active.size() - 1);
        int idx = pickJob(rng_), j = active[idx], k = placed[j];
        seq.push_back(inst_.job(j).operation(k).globalId());
        if (++placed[j] >= inst_.job(j).operationCount()) {
            active[idx] = active.back(); active.pop_back();
        }
    }
}

GameState GameSolver::randomProfile() {
    GameState state(inst_.totalOperations());
    for (int gid = 0; gid < inst_.totalOperations(); ++gid) {
        const Operation& op = inst_.operationByGlobalId(gid);
        uniform_int_distribution<int> pick(0, op.alternativeCount() - 1);
        state.routing()[gid] = pick(rng_);
    }
    fillRandomSequence(state);
    return state;
}

// Reijnen et al. "Global" machine selection: greedily balance the machine
// workloads (a strong load-balanced starting point for the makespan).
GameState GameSolver::greedyGlobalProfile() {
    GameState state(inst_.totalOperations());
    vector<long long> load(inst_.numMachines(), 0);
    for (int gid = 0; gid < inst_.totalOperations(); ++gid) {
        const Operation& op = inst_.operationByGlobalId(gid);
        int bestAlt = 0; long long bestFinish = LLONG_MAX;
        for (int a = 0; a < op.alternativeCount(); ++a) {
            long long f = load[op.machineOfAlternative(a)] + op.timeOfAlternative(a);
            if (f < bestFinish) { bestFinish = f; bestAlt = a; }
        }
        state.routing()[gid] = bestAlt;
        load[op.machineOfAlternative(bestAlt)] += op.timeOfAlternative(bestAlt);
    }
    fillRandomSequence(state);
    return state;
}

// Reijnen et al. "Local" machine selection: shortest processing time per op.
GameState GameSolver::greedyLocalProfile() {
    GameState state(inst_.totalOperations());
    for (int gid = 0; gid < inst_.totalOperations(); ++gid) {
        const Operation& op = inst_.operationByGlobalId(gid);
        int bestAlt = 0, bestTime = INT_MAX;
        for (int a = 0; a < op.alternativeCount(); ++a)
            if (op.timeOfAlternative(a) < bestTime) { bestTime = op.timeOfAlternative(a); bestAlt = a; }
        state.routing()[gid] = bestAlt;
    }
    fillRandomSequence(state);
    return state;
}

// Task-pool constructor (the prompt's "task pool"): repeatedly, only the
// precedence-feasible next operation of each job is "ready" (in the pool); the
// ready operations compete and the one that can COMPLETE earliest (over its
// eligible machines) is dispatched next. This jointly builds OSV (the dispatch
// order) and MAV (the chosen machine) and yields a strong active schedule.
GameState GameSolver::taskPoolProfile() {
    GameState state(inst_.totalOperations());
    const int J = inst_.numJobs();
    vector<int> nextOp(J, 0), jobReady(J, 0);
    vector<int> machineFree(inst_.numMachines(), 0);
    vector<int>& seq = state.sequence();
    seq.clear();

    const int total = inst_.totalOperations();
    for (int placed = 0; placed < total; ++placed) {
        // Among all ready operations (one per unfinished job) and their eligible
        // machines, pick the (operation, machine) with the earliest completion.
        int bestJob = -1, bestAlt = -1, bestComplete = INT_MAX;
        for (int j = 0; j < J; ++j) {
            if (nextOp[j] >= inst_.job(j).operationCount()) continue;
            const Operation& op = inst_.job(j).operation(nextOp[j]);
            for (int a = 0; a < op.alternativeCount(); ++a) {
                const int m = op.machineOfAlternative(a);
                const int start = max(machineFree[m], jobReady[j]);
                const int comp  = start + op.timeOfAlternative(a);
                if (comp < bestComplete) { bestComplete = comp; bestJob = j; bestAlt = a; }
            }
        }
        const Operation& op = inst_.job(bestJob).operation(nextOp[bestJob]);
        const int m = op.machineOfAlternative(bestAlt);
        const int start = max(machineFree[m], jobReady[bestJob]);
        machineFree[m] = start + op.timeOfAlternative(bestAlt);
        jobReady[bestJob] = machineFree[m];
        state.routing()[op.globalId()] = bestAlt;
        seq.push_back(op.globalId());
        ++nextOp[bestJob];
    }
    return state;
}

// Fictitious-play seed: routing drawn from the players' learned beliefs.
GameState GameSolver::beliefProfile(const BeliefModel& belief) {
    GameState state(inst_.totalOperations());
    state.routing() = belief.sampleRouting(rng_);
    fillRandomSequence(state);
    return state;
}

vector<int> GameSolver::criticalOperations(const Schedule& sched) const {
    const int n = inst_.totalOperations();
    const int m = inst_.numMachines();
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
    for (int j = 0; j < inst_.numJobs(); ++j) {
        const Job& job = inst_.job(j);
        for (int k = 0; k + 1 < job.operationCount(); ++k)
            jobSucc[job.operation(k).globalId()] = job.operation(k + 1).globalId();
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

// Copy of `seq` with the element at `pos` moved to index `target`.
static vector<int> moveTo(const vector<int>& seq, int pos, int target) {
    vector<int> r = seq;
    int g = r[pos];
    r.erase(r.begin() + pos);
    int t = (target > pos) ? target - 1 : target;
    r.insert(r.begin() + t, g);
    return r;
}

// True iff `seq` lists each job's operations in their route order (feasible).
static bool precedenceOK(const Instance& inst, const vector<int>& seq) {
    vector<int> need(inst.numJobs(), 0);
    for (int gid : seq) {
        const Operation& op = inst.operationByGlobalId(gid);
        if (op.positionInJob() != need[op.jobIndex()]) return false;
        ++need[op.jobIndex()];
    }
    return true;
}

void GameSolver::considerIncumbent(SolveResult& result, long long& bestFit,
                                   const GameState& state, const Schedule& sched) {
    long long f = payoff_.fitness(sched);
    if (f < bestFit) {
        bestFit = f;
        result.bestState           = state;
        result.bestMakespan        = sched.makespan();
        result.bestTotalCompletion = sched.totalCompletion();
    }
}

void GameSolver::descend(GameState& state, int run, long budgetEnd,
                         SolveResult& result, long long& bestFit, int& iteration) {
    while (evals_ < budgetEnd) {
        Schedule cur = evaluate(state);
        const long long curFit = payoff_.fitness(cur);
        const int curMk = cur.makespan();

        // Positions of every operation in the current dispatch order.
        vector<int> posOf(inst_.totalOperations(), -1);
        for (int i = 0; i < (int)state.sequence().size(); ++i)
            posOf[state.sequence()[i]] = i;

        vector<int> crit = criticalOperations(cur);

        // Best deviation found this round.
        long long bestFitMove = curFit;
        int kind = 0;          // 0 none, 1 routing, 2 sequence, 3 mutual reroute (job pair)
        int moveGid = -1, moveAlt = -1, moveMk = curMk;
        int moveGid2 = -1, moveAlt2 = -1;     // second operation of a mutual reroute
        long long moveSum = cur.totalCompletion();
        vector<int> moveSeq;
        string action;

        for (int gid : crit) {
            const Operation& op = inst_.operationByGlobalId(gid);
            const int job = op.jobIndex();

            // (1) re-route the critical operation to another eligible machine.
            const int curAlt = state.alternativeOf(gid);
            for (int alt = 0; alt < op.alternativeCount(); ++alt) {
                if (alt == curAlt) continue;
                state.routing()[gid] = alt;
                Schedule s = evaluate(state);
                long long f = payoff_.fitness(s);
                state.routing()[gid] = curAlt;
                if (f < bestFitMove) {
                    bestFitMove = f; kind = 1; moveGid = gid; moveAlt = alt;
                    moveMk = s.makespan(); moveSum = s.totalCompletion();
                    action = "reroute " + op.label() + ": M" +
                             to_string(op.machineOfAlternative(curAlt) + 1) +
                             " -> M" + to_string(op.machineOfAlternative(alt) + 1);
                }
                if (evals_ >= budgetEnd) break;
            }
            if (evals_ >= budgetEnd) break;

            // (2) re-sequence the critical operation within its legal window.
            const int pos = posOf[gid];
            const int p   = op.positionInJob();
            const int predPos = (p == 0) ? -1
                              : posOf[inst_.job(job).operation(p - 1).globalId()];
            const int succPos = (p + 1 >= inst_.job(job).operationCount())
                              ? (int)state.sequence().size()
                              : posOf[inst_.job(job).operation(p + 1).globalId()];
            int targets[4] = { predPos + 1, succPos - 1, pos - 1, pos + 1 };
            int lastT = -999;
            for (int t : targets) {
                if (t <= predPos || t >= succPos || t == pos || t < 0) continue;
                if (t == lastT) continue;
                lastT = t;
                vector<int> cand = moveTo(state.sequence(), pos, t);
                state.sequence().swap(cand);
                Schedule s = evaluate(state);
                long long f = payoff_.fitness(s);
                state.sequence().swap(cand);
                if (f < bestFitMove) {
                    bestFitMove = f; kind = 2; moveGid = gid; moveSeq = move(cand);
                    moveMk = s.makespan(); moveSum = s.totalCompletion();
                    action = "move " + op.label() + " to dispatch slot " +
                             to_string(t + 1) + " (was " + to_string(pos + 1) + ")";
                }
                if (evals_ >= budgetEnd) break;
            }
            if (evals_ >= budgetEnd) break;
        }

        // ---- (3) JOB-vs-JOB pairwise moves on a shared machine -------------
        // Two operations consecutive in time on the same machine that belong to
        // DIFFERENT jobs are direct rivals for that slot. On the critical path,
        // resolving that rivalry is exactly what lowers the makespan.
        {
            vector<char> isCrit(inst_.totalOperations(), 0);
            for (int g : crit) isCrit[g] = 1;
            vector<vector<int>> perMachine(inst_.numMachines());
            for (int gid = 0; gid < inst_.totalOperations(); ++gid)
                perMachine[cur.machineOf(gid)].push_back(gid);
            int mutualPairs = 0;                 // cap on the costly mutual reroutes
            for (int mm = 0; mm < inst_.numMachines() && evals_ < budgetEnd; ++mm) {
                auto& v = perMachine[mm];
                sort(v.begin(), v.end(), [&](int a, int b){ return cur.startOf(a) < cur.startOf(b); });
                for (size_t q = 0; q + 1 < v.size() && evals_ < budgetEnd; ++q) {
                    const int u = v[q], w = v[q + 1];
                    if (!isCrit[u] || !isCrit[w]) continue;
                    const Operation& opU = inst_.operationByGlobalId(u);
                    const Operation& opW = inst_.operationByGlobalId(w);
                    if (opU.jobIndex() == opW.jobIndex()) continue;     // same job: not rivals

                    // Action 1 - swap the rivals' order (move w just ahead of u).
                    vector<int> cand = moveTo(state.sequence(), posOf[w], posOf[u]);
                    if (precedenceOK(inst_, cand)) {
                        state.sequence().swap(cand);
                        Schedule s = evaluate(state);
                        long long f = payoff_.fitness(s);
                        state.sequence().swap(cand);
                        if (f < bestFitMove) {
                            bestFitMove = f; kind = 2; moveGid = w; moveSeq = move(cand);
                            moveMk = s.makespan(); moveSum = s.totalCompletion();
                            action = "swap " + opW.label() + " ahead of " + opU.label() +
                                     " on M" + to_string(mm + 1);
                        }
                    }
                    if (evals_ >= budgetEnd) break;

                    // Action 5 - mutual rerouting: both rivals leave the contested
                    // machine together (bounded; this is the costly move).
                    if (mutualPairs < 4 && opU.alternativeCount() > 1 && opW.alternativeCount() > 1
                        && opU.alternativeCount() * opW.alternativeCount() <= 12) {
                        ++mutualPairs;
                        const int curU = state.alternativeOf(u), curW = state.alternativeOf(w);
                        for (int au = 0; au < opU.alternativeCount() && evals_ < budgetEnd; ++au)
                            for (int aw = 0; aw < opW.alternativeCount() && evals_ < budgetEnd; ++aw) {
                                if (au == curU && aw == curW) continue;
                                state.routing()[u] = au; state.routing()[w] = aw;
                                Schedule s = evaluate(state);
                                long long f = payoff_.fitness(s);
                                state.routing()[u] = curU; state.routing()[w] = curW;
                                if (f < bestFitMove) {
                                    bestFitMove = f; kind = 3;
                                    moveGid = u; moveAlt = au; moveGid2 = w; moveAlt2 = aw;
                                    moveMk = s.makespan(); moveSum = s.totalCompletion();
                                    action = "mutual reroute " + opU.label() + "->M" +
                                        to_string(opU.machineOfAlternative(au) + 1) + ", " +
                                        opW.label() + "->M" +
                                        to_string(opW.machineOfAlternative(aw) + 1);
                                }
                            }
                    }
                }
            }
        }

        if (kind == 0) { result.equilibriumReached = true; return; } // local optimum

        // Apply the best deviation (single reroute, re-sequence/swap, or mutual reroute).
        if (kind == 1)      state.routing()[moveGid] = moveAlt;
        else if (kind == 2) state.sequence() = moveSeq;
        else /* kind 3 */ { state.routing()[moveGid] = moveAlt; state.routing()[moveGid2] = moveAlt2; }

        ++result.acceptedMoves;
        if ((int)result.trace.size() < maxTraceRows_) {
            MoveRecord rec;
            rec.iteration = ++iteration; rec.run = run;
            rec.job = inst_.operationByGlobalId(moveGid).jobIndex();
            rec.action = action; rec.oldCost = curMk; rec.newCost = moveMk;
            rec.makespan = moveMk; rec.sumCompletion = moveSum;
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

void GameSolver::perturb(GameState& state, int strength, const BeliefModel* belief) {
    const int n = inst_.totalOperations();
    uniform_int_distribution<int> coin(0, 1);
    for (int s = 0; s < strength; ++s) {
        uniform_int_distribution<int> pickGid(0, n - 1);
        int gid = pickGid(rng_);
        const Operation& op = inst_.operationByGlobalId(gid);
        if (op.alternativeCount() > 1 && coin(rng_) == 0) {
            // Re-route: draw from beliefs when available (aim the kick at the
            // machine assignments good solutions agree on), else uniformly.
            if (belief && belief->ready())
                state.routing()[gid] = belief->sampleAlternative(gid, rng_);
            else {
                uniform_int_distribution<int> pa(0, op.alternativeCount() - 1);
                state.routing()[gid] = pa(rng_);
            }
        } else {
            // reposition within its legal window
            vector<int> posOf(n, -1);
            for (int i = 0; i < (int)state.sequence().size(); ++i) posOf[state.sequence()[i]] = i;
            const int job = op.jobIndex(), p = op.positionInJob(), pos = posOf[gid];
            const int predPos = (p == 0) ? -1
                              : posOf[inst_.job(job).operation(p - 1).globalId()];
            const int succPos = (p + 1 >= inst_.job(job).operationCount())
                              ? (int)state.sequence().size()
                              : posOf[inst_.job(job).operation(p + 1).globalId()];
            if (succPos - predPos <= 2) continue;            // no room to move
            uniform_int_distribution<int> pt(predPos + 1, succPos - 1);
            int t = pt(rng_);
            if (t == pos) continue;
            vector<int> cand = moveTo(state.sequence(), pos, t);
            state.sequence().swap(cand);
        }
    }
}

SolveResult GameSolver::solve() {
    SolveResult result;
    result.name          = inst_.name();
    result.numJobs       = inst_.numJobs();
    result.numMachines   = inst_.numMachines();
    result.numOperations = inst_.totalOperations();

    long long bestFit = LLONG_MAX;
    int iteration = 0;
    int run = 0;
    const int kickStrength = max(2, inst_.totalOperations() / 20);

    // Fictitious-play memory of the best equilibria found so far.
    BeliefModel belief(inst_, 30);

    while (evals_ < evalBudget_) {
        const long budgetEnd = min(evalBudget_, evals_ + perRunBudget_);

        // Choose how this run is seeded. Run 0 is ALWAYS fully random (the
        // required random initialisation); later runs are seeded by the learned
        // beliefs and the Global/Local greedy heuristics, then refined by play.
        GameState state;
        if (run == 0) {
            state = randomProfile();
        } else if (run == 1) {
            state = taskPoolProfile();       // strong task-pool (earliest-completion) seed
        } else {
            int mode = run % 6;
            if (mode == 1 && belief.ready()) state = beliefProfile(belief);
            else if (mode == 2)              state = greedyGlobalProfile();
            else if (mode == 3)              state = greedyLocalProfile();
            else if (mode == 4)              state = taskPoolProfile();
            else                             state = randomProfile();
        }

        Schedule s0 = evaluate(state);
        if (run == 0) { result.initialMakespan = s0.makespan(); result.initialState = state; }
        considerIncumbent(result, bestFit, state, s0);

        GameState runBest = state;
        long long runBestFit = payoff_.fitness(s0);

        descend(state, run, budgetEnd, result, bestFit, iteration);
        { Schedule sd = evaluate(state); long long f = payoff_.fitness(sd);
          if (f < runBestFit) { runBestFit = f; runBest = state; } }

        // Iterated local search: kick the run's best (aimed by beliefs), descend.
        while (evals_ < budgetEnd) {
            GameState work = runBest;
            perturb(work, kickStrength, &belief);
            descend(work, run, budgetEnd, result, bestFit, iteration);
            Schedule sw = evaluate(work);
            long long f = payoff_.fitness(sw);
            if (f < runBestFit) { runBestFit = f; runBest = work; }
        }

        const int runBestMk = (int)(runBestFit / 1000000LL);
        result.runBests.push_back(runBestMk);
        belief.consider(runBest, runBestMk);   // learn from this run's best
        ++run;
    }

    result.runsRun     = run;
    result.evaluations = evals_;
    return result;
}

} // namespace fjs
