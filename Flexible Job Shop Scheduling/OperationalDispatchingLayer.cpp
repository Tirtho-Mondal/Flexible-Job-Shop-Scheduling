// ============================================================================
//  OperationalDispatchingLayer.cpp  (Layer 2) - the local conflict game:
//  critical-path + two-player best response (descend) and the pure selfish
//  unilateral+pairwise game (descendSelfish), both played to a Nash equilibrium.
// ============================================================================
#include "OperationalDispatchingLayer.h"
#include "ScheduleBuilder.h"
#include <algorithm>
#include <climits>
#include <numeric>
#include <string>

using namespace std;

namespace fjs {

OperationalDispatchingLayer::OperationalDispatchingLayer(const Instance& inst,
        const PayoffFunction& payoff, const AlgorithmConfig& cfg)
    : inst(inst), payoff(payoff), cfg(cfg) {
    maxTraceRows = max(0, cfg.traceRows);
    detailRows   = maxTraceRows;
}

Schedule OperationalDispatchingLayer::evaluate(const StrategyProfile& state) {
    ++evals;
    return ScheduleBuilder::build(inst, state);
}

vector<int> OperationalDispatchingLayer::criticalOperations(const Schedule& sched) const {
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

void OperationalDispatchingLayer::descend(StrategyProfile& state, int run,
                         SolveResult& result, long long& bestFit, int& iteration) {
    // Keep applying the single best improving deviation until none remains - a
    // true Nash equilibrium / critical-path local optimum. There is no budget:
    // the loop ends only when the schedule has converged (kind == 0).
    while (true) {
        Schedule cur = evaluate(state);
        const long long curFit = payoff.globalPotential(cur);
        const int curMk = cur.makespan();

        // Positions of every operation in the current dispatch order.
        vector<int> posOf(inst.totalOperations(), -1);
        for (int i = 0; i < (int)state.sequence.size(); ++i)
            posOf[state.sequence[i]] = i;

        vector<int> crit = criticalOperations(cur);

        // ============================================================
        //  ONE PLAY OF THE GAME  -  EVERY OPERATION IS A TWO-PLAYER MOVE
        //  -----------------------------------------------------------
        //  Methodology: EVERY makespan-critical operation is a "mover";
        //  the job owning it FIRST plays a two-player game against each
        //  rival job it meets on its machine (its machine neighbours).
        //  The two players may
        //    (2) SWAP their order on the machine,
        //    (3) JOINTLY RE-ROUTE (both re-pick machines), or
        //    (4) COMBINE the two - the mover re-routes WHILE the pair
        //        swaps order (a reroute + resequence between two players).
        //  A single job acting ALONE (bucket B) is only a FALLBACK, used
        //  when no rival interaction can improve. When neither helps the
        //  profile is a Nash equilibrium and the caller kicks it.
        // ============================================================

        // ---- bucket A: the TWO-PLAYER interaction move (primary) ----
        long long pairFit = curFit;
        int  pairKind = 0;                 // 2 = swap, 3 = mutual reroute, 4 = reroute+resequence
        int  pairGid = -1, pairAlt = -1, pairGid2 = -1, pairAlt2 = -1, pairMk = curMk;
        long long pairSum = cur.totalCompletion();
        vector<int> pairSeq;
        string pairAction, pairType;
        int  pairPartner = -1, pairMachine = -1;
        {
            // Operations on each machine in time order, so we can read off the
            // machine NEIGHBOURS - the rival jobs an operation actually meets.
            vector<vector<int>> perMachine(inst.numMachines());
            for (int gid = 0; gid < inst.totalOperations(); ++gid)
                perMachine[cur.machineOf(gid)].push_back(gid);
            for (auto& v : perMachine)
                sort(v.begin(), v.end(), [&](int a, int b){ return cur.startOf(a) < cur.startOf(b); });
            vector<int> slotOf(inst.totalOperations(), -1);
            for (int mm = 0; mm < inst.numMachines(); ++mm)
                for (int i = 0; i < (int)perMachine[mm].size(); ++i)
                    slotOf[perMachine[mm][i]] = i;

            int mutualPairs = 0;
            // EVERY critical operation is the mover; it plays against each of its
            // machine neighbours (the rivals it directly contends with for slots).
            for (int u : crit) {
                const int mm = cur.machineOf(u);
                auto& v = perMachine[mm];
                const int su = slotOf[u];
                const Operation& opU = inst.operationByGlobalId(u);

                for (int dir = -1; dir <= 1; dir += 2) {      // previous and next neighbour
                    const int sv = su + dir;
                    if (sv < 0 || sv >= (int)v.size()) continue;
                    const int w = v[sv];
                    const Operation& opW = inst.operationByGlobalId(w);
                    if (opU.jobIndex == opW.jobIndex) continue;   // same job: not rivals

                    // The two operations in dispatch order (earlier first); a "swap"
                    // pulls the later one ahead of the earlier one.
                    const int earlier = (cur.startOf(u) <= cur.startOf(w)) ? u : w;
                    const int later   = (earlier == u) ? w : u;
                    vector<int> swapSeq = state.resequenced(posOf[later], posOf[earlier]);
                    const bool swapOK = precedenceOK(inst, swapSeq);
                    const string swapText = "swap " + inst.operationByGlobalId(later).label() +
                        " ahead of " + inst.operationByGlobalId(earlier).label() +
                        " on M" + to_string(mm + 1);

                    // interaction 1 - the two rivals SWAP their order on the machine.
                    if (swapOK) {
                        state.sequence.swap(swapSeq);
                        Schedule s = evaluate(state);
                        long long f = payoff.globalPotential(s);
                        state.sequence.swap(swapSeq);
                        if (f < pairFit) {
                            pairFit = f; pairKind = 2; pairGid = u; pairSeq = swapSeq;
                            pairMk = s.makespan(); pairSum = s.totalCompletion();
                            pairPartner = w; pairMachine = mm; pairType = "swap";
                            pairAction = swapText;
                        }
                    }

                    // interaction 2 - the two rivals JOINTLY RE-ROUTE (both re-pick
                    // machines together - the joint best response of the routing game).
                    if (mutualPairs < 24 && (opU.alternativeCount() > 1 || opW.alternativeCount() > 1)
                        && opU.alternativeCount() * opW.alternativeCount() <= 36) {
                        ++mutualPairs;
                        const int curU = state.alternativeOf(u), curW = state.alternativeOf(w);
                        for (int au = 0; au < opU.alternativeCount(); ++au)
                            for (int aw = 0; aw < opW.alternativeCount(); ++aw) {
                                if (au == curU && aw == curW) continue;
                                state.reroute(u, au); state.reroute(w, aw);
                                Schedule s = evaluate(state);
                                long long f = payoff.globalPotential(s);
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

                    // interaction 3 - COMBINED: the mover RE-ROUTES while the two
                    // players SWAP order (a reroute + resequence between two players).
                    if (swapOK && opU.alternativeCount() > 1) {
                        const int curU = state.alternativeOf(u);
                        for (int au = 0; au < opU.alternativeCount(); ++au) {
                            if (au == curU) continue;
                            state.reroute(u, au);
                            state.sequence.swap(swapSeq);
                            Schedule s = evaluate(state);
                            long long f = payoff.globalPotential(s);
                            state.sequence.swap(swapSeq);
                            state.reroute(u, curU);
                            if (f < pairFit) {
                                pairFit = f; pairKind = 4;
                                pairGid = u; pairAlt = au; pairSeq = swapSeq;
                                pairMk = s.makespan(); pairSum = s.totalCompletion();
                                pairPartner = w; pairMachine = mm; pairType = "reroute+swap";
                                pairAction = "reroute " + opU.label() + "->M" +
                                    to_string(opU.machineOfAlternative(au) + 1) +
                                    " & " + swapText;
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
                    long long f = payoff.globalPotential(s);
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
                    long long f = payoff.globalPotential(s);
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

        // Apply the best deviation (single reroute, re-sequence/swap, mutual reroute,
        // or the combined reroute+resequence between the two players).
        if (kind == 1)      state.reroute(moveGid, moveAlt);
        else if (kind == 2) state.sequence = moveSeq;
        else if (kind == 3) { state.reroute(moveGid, moveAlt); state.reroute(moveGid2, moveAlt2); }
        else /* kind 4 */   { state.reroute(moveGid, moveAlt); state.sequence = moveSeq; }

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

// ----------------------------------------------------------------------------
//  NON-COOPERATIVE NASH GAME  -  UNILATERAL then PAIRWISE (job-vs-job) BEST RESPONSE
//  ---------------------------------------------------------------------------
//  THE engine that carries the novelty. Two phases, alternated to convergence:
//
//   PHASE 1 (unilateral): each job, acting ALONE, makes the single move - reroute
//     one operation or shift one within its window - that most raises its OWN payoff
//        U_i = 1 / (1 + a*C_i + b*W_i + g*Conf_i + d*Cmax + t*Toll_i),
//     accepted ONLY IF U_i strictly improves (makespan is NOT the rule, so equilibria
//     may be inefficient = the PRICE OF ANARCHY; the toll t*Toll_i pulls them down).
//
//   PHASE 2 (pairwise, job-vs-job): when no single job can improve, two RIVAL jobs on
//     a shared machine deviate TOGETHER (swap order / joint reroute), accepted ONLY IF
//     BOTH gain (a Pareto-improving coalition) AND Cmax does not rise. This escapes the
//     coordination traps unilateral play cannot, refining the result to a PAIRWISE-
//     STABLE (strong) Nash equilibrium with a lower makespan.
//
//  When NEITHER a single job NOR a rival pair can improve, the profile is returned as a
//  certified equilibrium (true). The outer solve() then applies a RANDOM KICK and
//  replays the game (iterated local search), keeping the best equilibrium by Cmax -
//  exactly: Nash game -> if stuck, kick -> Nash game again -> repeat until stable.
// ----------------------------------------------------------------------------
bool OperationalDispatchingLayer::descendSelfish(StrategyProfile& state, int run,
                                SolveResult& result, int& iteration) {
    const double EPS = 1e-9;
    const int MAX_SWEEPS = 500;
    for (int sweep = 0; sweep < MAX_SWEEPS; ++sweep) {
        bool improvedAny = false;

        // bottleneck-first: the latest-finishing players take their turn first
        Schedule snap = evaluate(state);
        vector<int> order(inst.numJobs());
        iota(order.begin(), order.end(), 0);
        sort(order.begin(), order.end(),
             [&](int a, int b){ return snap.jobCompletion(a) > snap.jobCompletion(b); });

        for (int j : order) {
            Schedule cur = evaluate(state);
            const double uCur  = payoff.forPlayer(cur, inst, j).utility;
            const int    curMk = cur.makespan();

            vector<int> posOf(inst.totalOperations(), -1);
            for (int i = 0; i < (int)state.sequence.size(); ++i) posOf[state.sequence[i]] = i;

            double bestU = uCur;
            int  bKind = 0, bGid = -1, bAlt = -1, bMk = curMk;
            long long bSum = cur.totalCompletion();
            vector<int> bSeq;
            string bAction;

            // a self-interested job optimises its OWN payoff over ALL of its operations
            for (const Operation& op : inst.job(j).operations()) {
                const int gid = op.globalId;

                // (1) unilateral REROUTE (machine-assignment strategy)
                const int curAlt = state.alternativeOf(gid);
                for (int alt = 0; alt < op.alternativeCount(); ++alt) {
                    if (alt == curAlt) continue;
                    state.reroute(gid, alt);
                    const Schedule s = evaluate(state);
                    const double u = payoff.forPlayer(s, inst, j).utility;
                    state.reroute(gid, curAlt);
                    if (u > bestU + EPS) {
                        bestU = u; bKind = 1; bGid = gid; bAlt = alt;
                        bMk = s.makespan(); bSum = s.totalCompletion();
                        bAction = "reroute " + op.label() + ": M" +
                                  to_string(op.machineOfAlternative(curAlt) + 1) + " -> M" +
                                  to_string(op.machineOfAlternative(alt) + 1);
                    }
                }

                // (2) unilateral RESEQUENCE within the precedence window (priority)
                const int pos = posOf[gid];
                const int p   = op.positionInJob;
                const int predPos = (p == 0) ? -1
                                  : posOf[inst.job(j).operation(p - 1).globalId];
                const int succPos = (p + 1 >= inst.job(j).operationCount())
                                  ? (int)state.sequence.size()
                                  : posOf[inst.job(j).operation(p + 1).globalId];
                const int slots[4] = { predPos + 1, succPos - 1, pos - 1, pos + 1 };
                int lastT = -999;
                for (int t : slots) {
                    if (t <= predPos || t >= succPos || t == pos || t < 0 || t == lastT) continue;
                    lastT = t;
                    vector<int> cand = state.resequenced(pos, t);
                    state.sequence.swap(cand);
                    const Schedule s = evaluate(state);
                    const double u = payoff.forPlayer(s, inst, j).utility;
                    state.sequence.swap(cand);
                    if (u > bestU + EPS) {
                        bestU = u; bKind = 2; bGid = gid; bSeq = cand;
                        bMk = s.makespan(); bSum = s.totalCompletion();
                        bAction = "move " + op.label() + " to dispatch slot " +
                                  to_string(t + 1) + " (was " + to_string(pos + 1) + ")";
                    }
                }
            }

            if (bKind == 0) continue;          // job j already plays its best response

            const int cBefore = cur.jobCompletion(j);
            if (bKind == 1) state.reroute(bGid, bAlt);
            else            state.sequence = bSeq;
            improvedAny = true;
            ++result.acceptedMoves;

            if ((int)result.trace.size() < maxTraceRows) {
                const Schedule after = evaluate(state);
                MoveRecord rec;
                rec.iteration = ++iteration; rec.run = run; rec.job = j;
                rec.action = bAction; rec.oldCost = curMk; rec.newCost = bMk;
                rec.makespan = bMk; rec.sumCompletion = bSum;
                rec.rival = -1;
                rec.moveType = (bKind == 1) ? "selfish-reroute" : "selfish-resequence";
                rec.moverCBefore = cBefore; rec.moverCAfter = after.jobCompletion(j);
                result.trace.push_back(rec);
            } else { ++iteration; }
        }

        if (improvedAny) continue;   // keep doing unilateral best responses first

        // ============ PAIRWISE (JOB vs JOB) PARETO BEST RESPONSE ============
        // No single job can improve alone. Now let two RIVAL jobs that meet on a
        // machine deviate TOGETHER - swap their order, or jointly re-route - and
        // accept the move ONLY IF BOTH jobs' payoffs strictly rise (a Pareto-
        // improving coalition) AND it does not raise Cmax. This escapes the
        // coordination traps that block unilateral play and refines the result to a
        // PAIRWISE-STABLE (strong) Nash equilibrium with a lower makespan. Among all
        // mutually-beneficial pair moves we take the one that most lowers Cmax.
        bool pairImproved = false;
        {
            Schedule cur = evaluate(state);
            const int curMk = cur.makespan();
            const long long curSum = cur.totalCompletion();
            vector<int> posOf(inst.totalOperations(), -1);
            for (int i = 0; i < (int)state.sequence.size(); ++i) posOf[state.sequence[i]] = i;
            vector<double> uNow(inst.numJobs());
            for (int jx = 0; jx < inst.numJobs(); ++jx) uNow[jx] = payoff.forPlayer(cur, inst, jx).utility;

            vector<int> crit = criticalOperations(cur);
            vector<char> isCrit(inst.totalOperations(), 0);
            for (int g : crit) isCrit[g] = 1;
            vector<vector<int>> perMachine(inst.numMachines());
            for (int gid = 0; gid < inst.totalOperations(); ++gid) perMachine[cur.machineOf(gid)].push_back(gid);
            for (auto& v : perMachine)
                sort(v.begin(), v.end(), [&](int a, int b){ return cur.startOf(a) < cur.startOf(b); });

            int bMk = curMk; long long bSum = curSum;
            int pKind = 0, pGid = -1, pAlt = -1, pGid2 = -1, pAlt2 = -1, pI = -1, pJ = -1, pMach = -1;
            vector<int> pSeq; string pAction;
            int mutualBudget = 24;

            for (int m = 0; m < inst.numMachines(); ++m) {
                auto& v = perMachine[m];
                for (size_t q = 0; q + 1 < v.size(); ++q) {
                    const int u = v[q], w = v[q + 1];
                    if (!isCrit[u] && !isCrit[w]) continue;          // at least one rival is critical
                    const Operation& ou = inst.operationByGlobalId(u);
                    const Operation& ow = inst.operationByGlobalId(w);
                    const int ji = ou.jobIndex, jj = ow.jobIndex;
                    if (ji == jj) continue;                          // same job: not rivals

                    // (A) the two rivals SWAP their order on the machine
                    {
                        vector<int> cand = state.resequenced(posOf[w], posOf[u]);
                        if (precedenceOK(inst, cand)) {
                            state.sequence.swap(cand);
                            const Schedule s = evaluate(state);
                            const double ui = payoff.forPlayer(s, inst, ji).utility;
                            const double uj = payoff.forPlayer(s, inst, jj).utility;
                            const int mk = s.makespan(); const long long sm = s.totalCompletion();
                            state.sequence.swap(cand);
                            if (ui > uNow[ji] + EPS && uj > uNow[jj] + EPS &&
                                (mk < bMk || (mk == bMk && sm < bSum))) {
                                bMk = mk; bSum = sm; pKind = 2; pSeq = cand; pI = ji; pJ = jj; pMach = m;
                                pAction = "pairwise swap " + ow.label() + " ahead of " + ou.label() +
                                          " on M" + to_string(m + 1);
                            }
                        }
                    }

                    // (B) the two rivals JOINTLY RE-ROUTE (both re-pick machines)
                    if (mutualBudget > 0 && (ou.alternativeCount() > 1 || ow.alternativeCount() > 1)
                        && ou.alternativeCount() * ow.alternativeCount() <= 36) {
                        --mutualBudget;
                        const int cu = state.alternativeOf(u), cw = state.alternativeOf(w);
                        for (int au = 0; au < ou.alternativeCount(); ++au)
                            for (int aw = 0; aw < ow.alternativeCount(); ++aw) {
                                if (au == cu && aw == cw) continue;
                                state.reroute(u, au); state.reroute(w, aw);
                                const Schedule s = evaluate(state);
                                const double ui = payoff.forPlayer(s, inst, ji).utility;
                                const double uj = payoff.forPlayer(s, inst, jj).utility;
                                const int mk = s.makespan(); const long long sm = s.totalCompletion();
                                state.reroute(u, cu); state.reroute(w, cw);
                                if (ui > uNow[ji] + EPS && uj > uNow[jj] + EPS &&
                                    (mk < bMk || (mk == bMk && sm < bSum))) {
                                    bMk = mk; bSum = sm; pKind = 3;
                                    pGid = u; pAlt = au; pGid2 = w; pAlt2 = aw; pI = ji; pJ = jj; pMach = m;
                                    pAction = "pairwise mutual reroute " + ou.label() + "->M" +
                                              to_string(ou.machineOfAlternative(au) + 1) + ", " +
                                              ow.label() + "->M" + to_string(ow.machineOfAlternative(aw) + 1);
                                }
                            }
                    }
                }
            }

            if (pKind != 0) {
                const int cBeforeI = cur.jobCompletion(pI), cBeforeJ = cur.jobCompletion(pJ);
                if (pKind == 2) state.sequence = pSeq;
                else { state.reroute(pGid, pAlt); state.reroute(pGid2, pAlt2); }
                pairImproved = true;
                ++result.acceptedMoves;
                if ((int)result.trace.size() < maxTraceRows) {
                    const Schedule after = evaluate(state);
                    MoveRecord rec;
                    rec.iteration = ++iteration; rec.run = run; rec.job = pI; rec.rival = pJ;
                    rec.contestMachine = pMach;
                    rec.action = pAction; rec.oldCost = curMk; rec.newCost = after.makespan();
                    rec.makespan = after.makespan(); rec.sumCompletion = after.totalCompletion();
                    rec.moveType = (pKind == 2) ? "pairwise-swap" : "pairwise-mutual";
                    rec.moverCBefore = cBeforeI; rec.moverCAfter = after.jobCompletion(pI);
                    rec.rivalCBefore = cBeforeJ; rec.rivalCAfter = after.jobCompletion(pJ);
                    result.trace.push_back(rec);
                } else { ++iteration; }
            }
        }
        if (pairImproved) continue;   // a mutually-beneficial pair move was applied: replay the game

        result.equilibriumReached = true; return true;   // PAIRWISE-STABLE Nash equilibrium
    }
    return false;   // hit the round cap (rare)
}


} // namespace fjs
