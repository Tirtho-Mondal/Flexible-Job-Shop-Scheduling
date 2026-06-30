// ============================================================================
//  InstanceReport.cpp - the detailed per-instance trace + final schedule.
// ============================================================================
#include "InstanceReport.h"
#include "ScheduleBuilder.h"
#include "Player.h"
#include "Solution.h"
#include "StableSolution.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <algorithm>

using namespace std;

namespace fjs {

// Which game layer an accepted move belongs to: L1(SCL) = routing game (Strategic
// Coordination Layer), L2(ODL) = sequencing game (Operational Dispatching Layer).
// Uses the explicit tag when present, else infers it from the move type.
static string layerOf(const MoveRecord& m) {
    if (!m.layer.empty()) return m.layer;
    const string& t = m.moveType;
    const bool routing = t.find("reroute") != string::npos || t.find("mutual") != string::npos;
    const bool seq     = t.find("seq")     != string::npos || t.find("swap")   != string::npos
                       || t.find("resequence") != string::npos;
    if (routing && seq) return "L1+L2";
    if (routing)        return "L1(SCL)";
    if (seq)            return "L2(ODL)";
    return "-";
}

static double flexibilityOf(const Instance& inst) {
    long alt = 0, ops = 0;
    for (int j = 0; j < inst.numJobs(); ++j)
        for (const Operation& op : inst.job(j).operations()) { alt += op.alternativeCount(); ++ops; }
    return ops ? (double)alt / ops : 0.0;
}

// Operations on a critical path of `sched` (those that set the makespan).
static vector<char> criticalSet(const Instance& inst, const Schedule& sched) {
    const int n = inst.totalOperations(), m = inst.numMachines();
    const int makespan = sched.makespan();
    vector<vector<int>> perM(m);
    for (int gid = 0; gid < n; ++gid) perM[sched.machineOf(gid)].push_back(gid);
    vector<int> machSucc(n, -1);
    for (int k = 0; k < m; ++k) {
        auto& v = perM[k];
        sort(v.begin(), v.end(), [&](int a, int b){ return sched.startOf(a) < sched.startOf(b); });
        for (size_t i = 0; i + 1 < v.size(); ++i) machSucc[v[i]] = v[i + 1];
    }
    vector<int> jobSucc(n, -1);
    for (int j = 0; j < inst.numJobs(); ++j) {
        const Job& job = inst.job(j);
        for (int k = 0; k + 1 < job.operationCount(); ++k)
            jobSucc[job.operation(k).globalId] = job.operation(k + 1).globalId;
    }
    vector<int> order(n);
    for (int i = 0; i < n; ++i) order[i] = i;
    sort(order.begin(), order.end(), [&](int a, int b){ return sched.endOf(a) > sched.endOf(b); });
    vector<int> tail(n, 0);
    for (int gid : order) {
        int dur = sched.endOf(gid) - sched.startOf(gid), after = 0;
        if (jobSucc[gid]  >= 0) after = max(after, tail[jobSucc[gid]]);
        if (machSucc[gid] >= 0) after = max(after, tail[machSucc[gid]]);
        tail[gid] = dur + after;
    }
    vector<char> isCrit(n, 0);
    for (int gid = 0; gid < n; ++gid)
        if (sched.startOf(gid) + tail[gid] == makespan) isCrit[gid] = 1;
    return isCrit;
}

// Print one individual as the two aligned vectors OSV (job ids, dispatch order)
// and MAV (chosen eligible-machine index, aligned to OSV).
static void writeEncoding(ofstream& f, const Instance& inst,
                          const StrategyProfile& st, const string& label) {
    f << label << "\n";
    f << "  OSV :";
    for (int gid : st.sequence) f << " " << (inst.operationByGlobalId(gid).jobIndex + 1);
    f << "\n  MAV :";
    for (int gid : st.sequence) f << " " << (st.alternativeOf(gid) + 1);
    f << "\n";
}

static string fmtU(double x) {            // utility -> fixed 4-decimal string
    ostringstream os; os << fixed << setprecision(4) << x; return os.str();
}

// One contested machine -> the normal-form (bimatrix) game between the two rival
// jobs. Each player's strategy set is the eligible machines it can route its
// contested operation to; the cell payoff is each job's utility U_i (and its
// completion C_i) when EVERY other player's strategy is held fixed at the
// equilibrium. The played cell is marked (*), and every pure-strategy Nash
// equilibrium of the 2-player game is marked [NE]. Returns true if a (non-trivial)
// table was printed.
static bool writePairGame(ofstream& f, const Instance& inst, const PayoffFunction& payoff,
                          const StrategyProfile& base, int oaGid, int obGid,
                          int machine, bool critical) {
    const Operation& oa = inst.operationByGlobalId(oaGid);
    const Operation& ob = inst.operationByGlobalId(obGid);
    const int na = oa.alternativeCount(), nb = ob.alternativeCount();
    if (na <= 1 && nb <= 1) return false;     // neither rival can re-route: trivial game
    if (na > 5 || nb > 5)   return false;     // keep the printed table readable
    const int jobA = oa.jobIndex, jobB = ob.jobIndex;
    const string LA = inst.job(jobA).label(), LB = inst.job(jobB).label();

    // Build the payoff bimatrix by re-routing ONLY these two operations and
    // re-decoding, leaving the rest of the equilibrium profile untouched.
    vector<vector<double>> Ua(na, vector<double>(nb)), Ub(na, vector<double>(nb));
    vector<vector<int>>    Ca(na, vector<int>(nb)),    Cb(na, vector<int>(nb)), Cm(na, vector<int>(nb));
    for (int a = 0; a < na; ++a)
        for (int b = 0; b < nb; ++b) {
            StrategyProfile p = base;
            p.reroute(oaGid, a); p.reroute(obGid, b);
            Schedule s = ScheduleBuilder::build(inst, p);
            Ua[a][b] = payoff.forPlayer(s, inst, jobA).utility;
            Ub[a][b] = payoff.forPlayer(s, inst, jobB).utility;
            Ca[a][b] = s.jobCompletion(jobA);
            Cb[a][b] = s.jobCompletion(jobB);
            Cm[a][b] = s.makespan();
        }
    const int a0 = base.alternativeOf(oaGid), b0 = base.alternativeOf(obGid);

    // A cell is a pure Nash eq iff neither player can raise its OWN utility by a
    // unilateral switch (row for A, column for B).
    auto isNash = [&](int a, int b) -> bool {
        for (int x = 0; x < na; ++x) if (Ua[x][b] > Ua[a][b] + 1e-12) return false;
        for (int y = 0; y < nb; ++y) if (Ub[a][y] > Ub[a][b] + 1e-12) return false;
        return true;
    };

    auto pad = [](string s, int w) { if ((int)s.size() < w) s += string(w - s.size(), ' '); return s; };

    f << "\n====================================================================\n";
    f << "Machine M" << (machine + 1) << "  -  TWO-PLAYER GAME\n";
    f << "  Player 1 = " << LA << "  (operation " << oa.label() << ")\n";
    f << "  Player 2 = " << LB << "  (operation " << ob.label() << ")\n";
    if (critical) f << "  critical-path rivalry: this clash is what fixes the makespan Cmax\n";
    f << "====================================================================\n";

    // ---- (1) the payoff bimatrix in the Job1 x Job2 table layout ----
    const int WL = 13, WC = 30;
    auto bar = [&]() {
        f << "  +" << string(WL, '-') << "+";
        for (int b = 0; b < nb; ++b) f << string(WC, '-') << "+";
        f << "\n";
    };
    f << "\n(1) PAYOFF BIMATRIX   (each cell = payoff of Player 1 , payoff of Player 2 ; C<Cmax>)\n";
    bar();
    f << "  |" << pad(" Job1 \\ Job2", WL) << "|";
    for (int b = 0; b < nb; ++b)
        f << pad(" Job2: M" + to_string(ob.machineOfAlternative(b) + 1), WC) << "|";
    f << "\n";
    bar();
    for (int a = 0; a < na; ++a) {
        f << "  |" << pad(" Job1: M" + to_string(oa.machineOfAlternative(a) + 1), WL) << "|";
        for (int b = 0; b < nb; ++b) {
            string cell = fmtU(Ua[a][b]) + ", " + fmtU(Ub[a][b]) + "  C" + to_string(Cm[a][b]);
            if (a == a0 && b == b0) cell += " *";
            if (isNash(a, b))       cell += " NASH";
            f << pad(" " + cell, WC) << "|";
        }
        f << "\n";
    }
    bar();
    f << "  legend:  *  = cell the solver actually plays    NASH = pure-strategy Nash equilibrium\n";

    // ---- (2) how every payoff in the table is calculated ----
    f << "\n(2) PAYOFF CALCULATION   (how each cell above is obtained)\n";
    f << "  U_i = 1/(1 + d*Cmax + own_i/(1+own_i)),  own_i = a*C_i + b*W_i + g*Conf_i + t*Toll_i   with  a="
      << fmtU(payoff.alpha) << "  b=" << fmtU(payoff.beta) << "  g=" << fmtU(payoff.gamma)
      << "  d=" << fmtU(payoff.delta) << "  (d>0 => lower Cmax always gives higher U_i)\n";
    f << "  " << pad("strategy pair", 24) << pad("C_" + LA, 8) << pad("C_" + LB, 8)
      << pad("Cmax", 8) << pad("U_" + LA, 10) << pad("U_" + LB, 10) << "note\n";
    for (int a = 0; a < na; ++a)
        for (int b = 0; b < nb; ++b) {
            string sp = LA + ":M" + to_string(oa.machineOfAlternative(a) + 1) + " & "
                      + LB + ":M" + to_string(ob.machineOfAlternative(b) + 1);
            string note;
            if (a == a0 && b == b0) note += "played ";
            if (isNash(a, b))       note += "Nash";
            f << "  " << pad(sp, 24)
              << pad(to_string(Ca[a][b]), 8) << pad(to_string(Cb[a][b]), 8)
              << pad(to_string(Cm[a][b]), 8)
              << pad(fmtU(Ua[a][b]), 10) << pad(fmtU(Ub[a][b]), 10) << note << "\n";
        }

    // ---- (3) Nash equilibrium by iterated best response (the selection) ----
    f << "\n(3) NASH CALCULATION   (iterated best response - how the equilibrium is selected)\n";
    f << "  Step 1 - Player 1 (" << LA << ")'s best reply to each move of Player 2:\n";
    for (int b = 0; b < nb; ++b) {
        int br = 0; for (int a = 1; a < na; ++a) if (Ua[a][b] > Ua[br][b]) br = a;
        f << "      if " << LB << " plays M" << (ob.machineOfAlternative(b) + 1)
          << "  ->  " << LA << "'s best = M" << (oa.machineOfAlternative(br) + 1)
          << "   (U_" << LA << "=" << fmtU(Ua[br][b]) << ")\n";
    }
    f << "  Step 2 - Player 2 (" << LB << ")'s best reply to each move of Player 1:\n";
    for (int a = 0; a < na; ++a) {
        int br = 0; for (int b = 1; b < nb; ++b) if (Ub[a][b] > Ub[a][br]) br = b;
        f << "      if " << LA << " plays M" << (oa.machineOfAlternative(a) + 1)
          << "  ->  " << LB << "'s best = M" << (ob.machineOfAlternative(br) + 1)
          << "   (U_" << LB << "=" << fmtU(Ub[a][br]) << ")\n";
    }
    f << "  Step 3 - Nash equilibrium = a cell that is a MUTUAL best reply:\n";
    int neCount = 0;
    for (int a = 0; a < na; ++a)
        for (int b = 0; b < nb; ++b)
            if (isNash(a, b)) {
                ++neCount;
                f << "      (" << LA << ":M" << (oa.machineOfAlternative(a) + 1)
                  << " , " << LB << ":M" << (ob.machineOfAlternative(b) + 1) << ")"
                  << (a == a0 && b == b0 ? "   <- the solver plays this cell" : "") << "\n";
            }
    if (neCount == 0) f << "      (no pure-strategy Nash equilibrium in this game)\n";
    f << "  Selected: the solver plays (" << LA << ":M" << (oa.machineOfAlternative(a0) + 1)
      << " , " << LB << ":M" << (ob.machineOfAlternative(b0) + 1) << "), chosen for the lowest\n"
      << "  shared makespan (Cmax=" << Cm[a0][b0] << "). It "
      << (isNash(a0, b0) ? "IS also a Nash equilibrium - neither job can raise its own payoff alone."
                         : "is the makespan optimum; one player could raise its own U_i alone, but\n  that would not lower the makespan, so the solver keeps this cell.") << "\n";
    return true;
}

// Re-draw the routing game the solver faced at ONE iteration, from the captured
// pre-move profile. A solo re-route (rivalGid < 0) prints a one-row best-response
// table over the mover's machines; a mutual re-route prints the full 2-player
// bimatrix. The BEFORE cell is [B] and the chosen AFTER cell is [A].
static void writeMoveBimatrix(ofstream& f, const Instance& inst, const PayoffFunction& payoff,
                              const StrategyProfile& base, int oaGid, int obGid,
                              int aBefore, int bBefore, int aAfter, int bAfter) {
    auto pad = [](string s, int w) { if ((int)s.size() < w) s += string(w - s.size(), ' '); return s; };
    const Operation& oa = inst.operationByGlobalId(oaGid);
    const int jobA = oa.jobIndex; const string LA = inst.job(jobA).label();

    if (obGid < 0) {                          // single-player best-response table
        const int na = oa.alternativeCount();
        f << "  routing best-response of " << LA << " (operation " << oa.label()
          << ") - each machine it could pick:\n";
        f << "    " << pad("strategy", 14) << pad("U_" + LA, 10) << pad("Cmax", 8) << "note\n";
        for (int a = 0; a < na; ++a) {
            StrategyProfile p = base; p.reroute(oaGid, a);
            Schedule s = ScheduleBuilder::build(inst, p);
            const double U = payoff.forPlayer(s, inst, jobA).utility;
            string note;
            if (a == aBefore) note += "BEFORE ";
            if (a == aAfter)  note += "AFTER (chosen)";
            f << "    " << pad(LA + ":M" + to_string(oa.machineOfAlternative(a) + 1), 14)
              << pad(fmtU(U), 10) << pad(to_string(s.makespan()), 8) << note << "\n";
        }
        return;
    }

    const Operation& ob = inst.operationByGlobalId(obGid);
    const int jobB = ob.jobIndex; const string LB = inst.job(jobB).label();
    const int na = oa.alternativeCount(), nb = ob.alternativeCount();
    if (na > 6 || nb > 6) { f << "  (routing game too large to print)\n"; return; }

    vector<vector<double>> Ua(na, vector<double>(nb)), Ub(na, vector<double>(nb));
    vector<vector<int>>    Cm(na, vector<int>(nb));
    for (int a = 0; a < na; ++a)
        for (int b = 0; b < nb; ++b) {
            StrategyProfile p = base; p.reroute(oaGid, a); p.reroute(obGid, b);
            Schedule s = ScheduleBuilder::build(inst, p);
            Ua[a][b] = payoff.forPlayer(s, inst, jobA).utility;
            Ub[a][b] = payoff.forPlayer(s, inst, jobB).utility;
            Cm[a][b] = s.makespan();
        }
    auto isNash = [&](int a, int b) {
        for (int x = 0; x < na; ++x) if (Ua[x][b] > Ua[a][b] + 1e-12) return false;
        for (int y = 0; y < nb; ++y) if (Ub[a][y] > Ub[a][b] + 1e-12) return false;
        return true;
    };
    const int WL = 13, WC = 30;
    auto bar = [&]() {
        f << "  +" << string(WL, '-') << "+";
        for (int b = 0; b < nb; ++b) f << string(WC, '-') << "+";
        f << "\n";
    };
    f << "  routing bimatrix " << LA << " (Job1) x " << LB << " (Job2)   cell = U_"
      << LA << ", U_" << LB << " ; C<Cmax>\n";
    bar();
    f << "  |" << pad(" Job1 \\ Job2", WL) << "|";
    for (int b = 0; b < nb; ++b)
        f << pad(" Job2: M" + to_string(ob.machineOfAlternative(b) + 1), WC) << "|";
    f << "\n";
    bar();
    for (int a = 0; a < na; ++a) {
        f << "  |" << pad(" Job1: M" + to_string(oa.machineOfAlternative(a) + 1), WL) << "|";
        for (int b = 0; b < nb; ++b) {
            string cell = fmtU(Ua[a][b]) + ", " + fmtU(Ub[a][b]) + "  C" + to_string(Cm[a][b]);
            if (a == aBefore && b == bBefore) cell += " [B]";
            if (a == aAfter  && b == bAfter)  cell += " [A]";
            if (isNash(a, b))                 cell += " NE";
            f << pad(" " + cell, WC) << "|";
        }
        f << "\n";
    }
    bar();
    f << "  legend: [B] = before this move,  [A] = after (the cell the solver chose),"
      << "  NE = own-payoff Nash equilibrium,  C<n> = makespan of that cell\n";
    f << "  note: the solver picks the LOWEST-Cmax cell [A], which need not be the\n"
      << "  own-payoff NE - when [A] != NE that gap is the price of anarchy.\n";
}

void InstanceReport::write(const string& path, const Instance& inst,
                           const SolveResult& result, const PayoffFunction& payoff,
                           int bestKnown, bool selfish) {
    ofstream f(path);
    if (!f) return;
    f << fixed << setprecision(0);

    // ---- header ---------------------------------------------------------
    f << "============================================================\n";
    f << "  GAME-THEORETIC FJSSP  -  DETAILED RUN LOG\n";
    f << "  Instance : " << inst.name << "   (group: " << inst.group << ")\n";
    f << "============================================================\n\n";
    f << "Jobs (players)      : " << inst.numJobs() << "\n";
    f << "Machines (resources): " << inst.numMachines() << "\n";
    f << "Operations          : " << inst.totalOperations() << "\n";
    f << "Avg. flexibility    : " << setprecision(2) << flexibilityOf(inst)
      << setprecision(0) << " machines/operation\n";
    if (bestKnown >= 0) f << "Best-known makespan : " << bestKnown << "\n";
    else                f << "Best-known makespan : N/A\n";
    f << "\n";

    f << "GAME-THEORY MODEL (a microeconomics view of FJSP)\n";
    f << "-------------------------------------------------\n";
    f << "  Player      : a job - the decision-maker\n";
    f << "  Strategy    : its machine choice per operation + its dispatch positions\n";
    f << "  Payoff      : U_i = 1/(1 + d*Cmax + own_i/(1+own_i)),  own_i = a*C_i + b*W_i + g*Conf_i + t*Toll_i\n";
    f << "                (d>0: STABLE makespan-aligned - lower Cmax => higher U_i for every player;\n";
    f << "                 d=0: pure selfish own_i only, with a price of anarchy)\n";
    f << "  Game        : all jobs competing for the shared machines\n";
    f << "  Solution    : the decoded schedule once every job fixes its strategy\n";
    f << "  Decision    : each player plays its BEST RESPONSE; the NASH EQUILIBRIUM\n";
    f << "                (no job can raise its own payoff alone) is the decision rule\n";
    f << "Two jobs interact whenever their operations meet on the same machine: the\n";
    f << "earlier one delays the other, so each job re-routes / re-sequences to raise\n";
    f << "its OWN payoff, and the schedule settles at an equilibrium.\n\n";

    f << "MODEL\n-----\n" << payoff.description() << "\n\n";
    if (selfish) {
        f << "Search (PURE SELFISH NON-COOPERATIVE GAME - own-payoff best response):\n"
          << "Every job is an INDEPENDENT, self-interested player. Acting on its own and one\n"
          << "at a time (asynchronous best response), a job makes the single unilateral move\n"
          << "(reroute or reposition one of its operations) that most raises its OWN payoff\n"
          << "U_i (see MODEL above) - accepted ONLY IF U_i strictly improves. With delta = 0\n"
          << "the makespan is absent from U_i, so equilibria may be inefficient (the PRICE OF\n"
          << "ANARCHY); the congestion toll t*Toll_i pulls them toward efficiency. No\n"
          << "cooperation, no joint moves. A sweep in which no job can improve is a pure-\n"
          << "strategy NASH EQUILIBRIUM under U_i; the best one (by Cmax) over many random\n"
          << "restarts is reported.\n\n";
    } else {
        f << "Search (COORDINATED MAKESPAN ENGINE):\n"
          << "Run 0 starts from a fully RANDOM profile; later runs are seeded by the\n"
          << "players' learned beliefs (fictitious play) or at random - NO greedy/dispatch-\n"
          << "rule construction. Each step the two rival jobs that share a critical machine\n"
          << "play their 2-player game - swapping order or jointly re-routing to the joint\n"
          << "best response that most lowers Cmax - with a single-job move as fallback. Moves\n"
          << "are accepted by the makespan potential. When none helps, a CROSSOVER/random\n"
          << "KICK perturbs the profile and the search repeats.\n\n";
    }

    // ---- initial random profile ----------------------------------------
    f << "INITIAL RANDOM PROFILE (restart 0)\n----------------------------------\n";
    f << "Makespan of the first random assignment: " << result.initialMakespan << "\n";
    f << "Initial machine choice per operation:\n";
    for (int j = 0; j < inst.numJobs(); ++j) {
        f << "  " << inst.job(j).label() << ":";
        for (const Operation& op : inst.job(j).operations()) {
            int alt = result.initialState.alternativeOf(op.globalId);
            f << " " << op.label() << "@M" << (op.machineOfAlternative(alt) + 1);
        }
        f << "\n";
    }
    f << "\n";

    // ---- iteration table ------------------------------------------------
    f << "CRITICAL-PATH BEST-RESPONSE ITERATIONS (one row per accepted move)\n";
    f << "Each row: the makespan-critical player re-routes or re-sequences a\n";
    f << "critical operation, lowering the makespan.\n";
    f << "Layer: L1(SCL) = Strategic Coordination Layer (routing game) | "
         "L2(ODL) = Operational Dispatching Layer (sequencing game).\n";
    f << "-----------------------------------------------------------------\n";
    f << left
      << setw(7)  << "Iter"
      << setw(5)  << "Run"
      << setw(9)  << "Layer"
      << setw(6)  << "Job"
      << setw(46) << "Action (critical-path deviation)"
      << right
      << setw(9)  << "Cmax_old"
      << setw(9)  << "Cmax_new"
      << setw(11) << "SumC" << "\n";
    f << string(110, '-') << "\n";
    for (const MoveRecord& m : result.trace) {
        string act = m.action;
        if (act.size() > 45) act = act.substr(0, 42) + "...";
        f << left
          << setw(7)  << m.iteration
          << setw(5)  << m.run
          << setw(9)  << layerOf(m)
          << setw(6)  << inst.job(m.job).label()
          << setw(46) << act
          << right
          << setw(9)  << (long long)m.oldCost
          << setw(9)  << (long long)m.newCost
          << setw(11) << m.sumCompletion << "\n";
    }
    if (result.acceptedMoves > (int)result.trace.size())
        f << "... (" << (result.acceptedMoves - (int)result.trace.size())
          << " further moves omitted; trace capped for readability)\n";
    f << "\n";

    // ---- per-iteration detail: calculation + interaction + bimatrix per move ----
    f << "PER-ITERATION DETAIL (calculation, interaction & bimatrix per accepted move)\n";
    f << "---------------------------------------------------------------------------\n";
    f << "After the initial random assignment, each accepted move below is expanded into:\n";
    f << "the player(s) involved (interaction), the makespan/completion change it produced\n";
    f << "(calculation), and - for re-routing moves - the routing game the solver evaluated\n";
    f << "to choose it (the BEFORE cell vs the chosen AFTER cell).\n";
    {
        int shownIters = 0;
        for (const MoveRecord& m : result.trace) {
            if (!m.hasDetail) continue;
            ++shownIters;
            f << "\n--------------------------------------------------------------------\n";
            f << "ITERATION " << m.iteration << "  (run " << m.run << ")    move type: "
              << m.moveType << "\n";
            if (m.rival >= 0) {
                f << "  interaction : " << inst.job(m.job).label() << "  vs  "
                  << inst.job(m.rival).label();
                if (m.contestMachine >= 0) f << "   on machine M" << (m.contestMachine + 1);
                f << "\n";
            } else {
                f << "  player      : " << inst.job(m.job).label() << " acts alone\n";
            }
            f << "  change      : " << m.action << "\n";
            f << "  calculation : Cmax " << (long long)m.oldCost << " -> " << (long long)m.newCost
              << "   SumC(after)=" << m.sumCompletion << "\n";
            f << "      " << inst.job(m.job).label() << ": completion C "
              << m.moverCBefore << " -> " << m.moverCAfter << "\n";
            if (m.rival >= 0)
                f << "      " << inst.job(m.rival).label() << ": completion C "
                  << m.rivalCBefore << " -> " << m.rivalCAfter << "\n";
            // Classify the move so the routing bimatrix is drawn for ANY re-routing
            // move - coordinated ("reroute"/"mutual"), bilevel, OR selfish
            // ("selfish-reroute"/"pairwise-mutual"). Sequencing-only moves get a note.
            const bool isCombo   = (m.moveType == "reroute+swap");
            const bool isMutual  = (m.moveType.find("mutual")  != string::npos);
            const bool isReroute = (!isCombo && !isMutual && m.moveType.find("reroute") != string::npos);
            if (isReroute && m.moverOp >= 0)
                writeMoveBimatrix(f, inst, payoff, m.stateBefore, m.moverOp, -1,
                                  m.moverAltBefore, -1, m.moverAltAfter, -1);
            else if (isMutual && m.moverOp >= 0 && m.rivalOp >= 0)
                writeMoveBimatrix(f, inst, payoff, m.stateBefore, m.moverOp, m.rivalOp,
                                  m.moverAltBefore, m.rivalAltBefore, m.moverAltAfter, m.rivalAltAfter);
            else if (isCombo && m.moverOp >= 0) {
                f << "  (combined move: the mover RE-ROUTES while the two players SWAP their\n"
                     "   dispatch order. The mover's routing game is shown below; the swap is the\n"
                     "   sequencing half of the same two-player interaction.)\n";
                writeMoveBimatrix(f, inst, payoff, m.stateBefore, m.moverOp, -1,
                                  m.moverAltBefore, -1, m.moverAltAfter, -1);
            }
            else
                f << "  (sequencing move: the operations' dispatch order changed, routing is\n"
                     "   unchanged - so there is no routing bimatrix; the completion change above\n"
                     "   is the effect of the re-ordering.)\n";
            if (selfish || m.moveType.rfind("selfish", 0) == 0)
                f << "  decision    : kept because the moving job's OWN payoff U_i strictly\n"
                     "                increased (unilateral best response toward the Nash\n"
                     "                equilibrium); the makespan was NOT the acceptance rule\n"
                     "                (Cmax " << (long long)m.oldCost << " -> " << (long long)m.newCost << ").\n";
            else
                f << "  decision    : kept because it lowers the makespan-dominated cost (Cmax "
                  << (long long)m.oldCost << " -> " << (long long)m.newCost << ").\n";
        }
        if (shownIters == 0)
            f << "\n  (no accepted moves were captured for detailing.)\n";
        else if (result.acceptedMoves > shownIters)
            f << "\n(" << shownIters << " iterations detailed in full; "
              << (result.acceptedMoves - shownIters)
              << " further moves exceeded the trace capacity and appear only in the summary table.)\n";
        else
            f << "\n(all " << shownIters << " accepted moves detailed in full.)\n";
    }
    f << "\n";

    // ---- the dynamic two-player interactions: which two jobs clashed & the benefit ----
    f << "TWO-PLAYER INTERACTION MOVES (which two jobs clashed, the change, the benefit)\n";
    f << "----------------------------------------------------------------------------\n";
    f << "Filtered from the trace above: only the moves where TWO rival jobs that share a\n";
    f << "machine act together. 'swap' = the two jobs exchange order on the contested\n";
    f << "machine; 'mutual' = both reroute off it. Such a move is accepted only when it\n";
    f << "lowers the shared makespan Cmax, so it shows exactly how resolving the clash\n";
    f << "benefits the players. C = a job's completion time (lower = better payoff).\n";
    {
        int shown = 0;
        auto verdict = [](int before, int after) -> string {
            if (after < before) return "  (finishes earlier - gains)";
            if (after > before) return "  (gives ground)";
            return "  (unchanged)";
        };
        for (const MoveRecord& m : result.trace) {
            if (m.rival < 0) continue;                 // keep only two-player interactions
            ++shown;
            f << "\n  [iter " << m.iteration << "]  " << inst.job(m.job).label()
              << "  vs  " << inst.job(m.rival).label();
            if (m.contestMachine >= 0) f << "   on M" << (m.contestMachine + 1);
            f << "   (" << m.moveType << ")\n";
            f << "      change  : " << m.action << "\n";
            f << "      " << inst.job(m.job).label() << "      : C "
              << m.moverCBefore << " -> " << m.moverCAfter << verdict(m.moverCBefore, m.moverCAfter) << "\n";
            f << "      " << inst.job(m.rival).label() << "      : C "
              << m.rivalCBefore << " -> " << m.rivalCAfter << verdict(m.rivalCBefore, m.rivalCAfter) << "\n";
            if (m.newCost < m.oldCost)
                f << "      shared  : Cmax " << (long long)m.oldCost << " -> " << (long long)m.newCost
                  << "   (clash resolved; the shared makespan drops)\n";
            else
                f << "      shared  : Cmax " << (long long)m.oldCost
                  << " (unchanged) - kept because the rivals' total completion falls (tie-break)\n";
        }
        if (shown == 0)
            f << "\n  (none in the kept trace - this run reached equilibrium through single-job\n"
                 "   deviations alone; no two-job joint move was needed.)\n";
        else
            f << "\nRead: each block is one interaction - the two rival players, the change they\n"
                 "made, how each one's completion C moved, and the shared makespan they lowered\n"
                 "together. One player often 'gives ground' so the makespan-critical one gains;\n"
                 "the move is kept because their joint cost (Cmax) drops.\n";
    }
    f << "\n";

    // ---- search summary -------------------------------------------------
    f << "SEARCH SUMMARY\n--------------\n";
    f << "Runs (best kept)    : " << result.runsRun << "\n";
    if (!result.runBests.empty()) {
        int rb = result.runBests.front(), rw = result.runBests.front();
        double sum = 0;
        for (int v : result.runBests) { rb = min(rb, v); rw = max(rw, v); sum += v; }
        f << "Per-run best Cmax   : best=" << rb << "  worst=" << rw
          << "  mean=" << setprecision(1) << (sum / result.runBests.size())
          << setprecision(0) << "  (over " << result.runBests.size() << " runs)\n";
        f << "First runs' best    :";
        for (int i = 0; i < (int)result.runBests.size() && i < 40; ++i) f << " " << result.runBests[i];
        if ((int)result.runBests.size() > 40) f << " ...";
        f << "\n";
    }
    f << "Accepted moves      : " << result.acceptedMoves << "\n";
    f << "Schedule evaluations: " << result.evaluations << "\n";
    f << "Nash equilibrium    : " << (result.equilibriumReached ? "reached (critical-path local optimum)" : "budget limited") << "\n\n";

    // ---- final schedule -------------------------------------------------
    Schedule best = ScheduleBuilder::build(inst, result.bestState);

    f << "FINAL EQUILIBRIUM SCHEDULE\n--------------------------\n";
    f << "Makespan (our result): " << best.makespan() << "\n";
    if (bestKnown >= 0) {
        double gap = 100.0 * (best.makespan() - bestKnown) / bestKnown;
        f << "Best-known makespan  : " << bestKnown << "\n";
        f << "Gap to best-known    : " << setprecision(2) << gap << "%"
          << setprecision(0) << "\n";
    }
    f << "Sum of completions   : " << best.totalCompletion() << "\n";
    f << "Improvement vs init  : " << result.initialMakespan << " -> " << best.makespan()
      << "  (" << setprecision(2)
      << (result.initialMakespan ? 100.0 * (result.initialMakespan - best.makespan()) / result.initialMakespan : 0.0)
      << "% lower)" << setprecision(0) << "\n\n";

    // per-machine sequences
    f << "Machine sequences (op[start-end]):\n";
    vector<vector<int>> byMachine(inst.numMachines());
    for (int gid = 0; gid < inst.totalOperations(); ++gid)
        byMachine[best.machineOf(gid)].push_back(gid);
    for (int m = 0; m < inst.numMachines(); ++m) {
        sort(byMachine[m].begin(), byMachine[m].end(),
                  [&](int a, int b){ return best.startOf(a) < best.startOf(b); });
        f << "  M" << setw(3) << left << (m + 1) << right << ":";
        for (int gid : byMachine[m]) {
            const Operation& op = inst.operationByGlobalId(gid);
            f << " " << op.label() << "[" << best.startOf(gid) << "-" << best.endOf(gid) << "]";
        }
        f << "\n";
    }
    f << "\n";

    // per-job completion
    f << "Job completion times:\n";
    for (int j = 0; j < inst.numJobs(); ++j)
        f << "  " << inst.job(j).label() << " : C = " << best.jobCompletion(j)
          << (best.jobCompletion(j) == best.makespan() ? "   <- makespan-critical job\n" : "\n");
    f << "\n";

    // per-job payoff breakdown (stable makespan-aligned payoff)
    f << "Per-job payoff  U_i = 1/(1 + d*Cmax + own_i/(1+own_i)),  own_i = a*C_i + b*W_i + g*Conf_i + t*Toll_i:\n";
    f << left << setw(6) << "Job" << right << setw(8) << "C_i"
      << setw(8) << "W_i" << setw(10) << "Conf_i" << setw(10) << "U_i" << "\n";
    f << fixed << setprecision(4);
    for (int j = 0; j < inst.numJobs(); ++j) {
        const Payoff p = payoff.forPlayer(best, inst, j);
        f << left << setw(6) << inst.job(j).label() << right
          << setw(8) << setprecision(0) << p.completion
          << setw(8) << p.waiting
          << setw(10) << p.conflict
          << setw(10) << setprecision(4) << p.utility << "\n";
    }
    f << setprecision(0) << "\n";

    // ---- the three game-theoretic objects: Strategy / Solution / StableSolution ----
    Solution solution = Solution::decode(inst, result.bestState, payoff);
    StableSolution stable(inst, result.bestState, payoff);
    f << "GAME-THEORETIC SOLUTION (Strategy / Solution / Stable Solution)\n";
    f << "--------------------------------------------------------------\n";
    f << "Solution  : makespan=" << solution.makespan()
      << "  fitness=" << solution.fitness
      << "  total payoff (sum U_i)=" << setprecision(4) << solution.totalPayoff
      << setprecision(0) << "\n";
    f << "Stable?   : " << (stable.isStable
            ? string("YES - no job can lower the makespan by changing its own strategy alone (Nash-stable)")
            : ("NO - " + stable.profitableDeviation)) << "\n\n";
    f << "Players (jobs) and the strategy each one plays\n";
    f << "(operation->machine@seq-position | interaction | best-response status):\n";
    for (int j = 0; j < inst.numJobs(); ++j) {
        Player player(inst, j);                       // the job AS a player/agent
        const Strategy& s = solution.jobStrategy(j);
        f << "  " << player.label() << ": ";
        for (const OperationChoice& c : s.choices)
            f << c.operationLabel << "->M" << (c.machine + 1) << "@" << c.sequencePosition << "  ";
        f << "| C=" << s.completion << " W=" << s.waiting << " Conf=" << s.conflict
          << " U=" << setprecision(4) << s.payoff << setprecision(0);
        string how;
        if (player.canImprove(result.bestState, payoff, &how))
            f << "  [could improve alone: " << how << "]";
        else
            f << "  [best response - cannot improve alone]";
        f << "\n";
    }
    f << "\n";

    f << "Full operation timetable:\n";
    f << left << setw(10) << "Op" << setw(6) << "Job"
      << setw(8) << "Machine" << right << setw(8) << "Start"
      << setw(8) << "End" << "\n";
    for (int j = 0; j < inst.numJobs(); ++j)
        for (const Operation& op : inst.job(j).operations()) {
            int gid = op.globalId;
            f << left << setw(10) << op.label()
              << setw(6) << inst.job(j).label()
              << setw(8) << ("M" + to_string(best.machineOf(gid) + 1))
              << right << setw(8) << best.startOf(gid)
              << setw(8) << best.endOf(gid) << "\n";
        }
    f << "\n";

    // ---- individual encoding: the two-vector representation X = (OSV, MAV) ----
    f << "INDIVIDUAL SOLUTION ENCODING  X = (OSV, MAV)\n";
    f << "--------------------------------------------\n";
    f << "One individual = one StrategyProfile. OSV = operation sequence vector (job ids in\n";
    f << "dispatch order); MAV = machine assignment vector (chosen eligible-machine #,\n";
    f << "aligned to OSV; 1 = the operation's first eligible machine). Rule: the k-th\n";
    f << "occurrence of job j in OSV is operation O(j,k).\n\n";
    writeEncoding(f, inst, result.bestState,    "Final best individual:");
    f << "\n";
    writeEncoding(f, inst, result.initialState, "Initial random individual (run 0):");
    f << "\n";
    f << "MAV decoded (chosen machine per operation), by job:\n";
    for (int j = 0; j < inst.numJobs(); ++j) {
        f << "  " << inst.job(j).label() << ":";
        for (const Operation& op : inst.job(j).operations()) {
            int alt = result.bestState.alternativeOf(op.globalId);
            f << "  " << op.label() << " alt" << (alt + 1)
              << "->M" << (op.machineOfAlternative(alt) + 1);
        }
        f << "\n";
    }
    f << "\n";

    // ---- two-player interactions: who delays whom + the benefit each strategy buys ----
    // TWO players INTERACT whenever their operations meet on the same machine: the one
    // scheduled first occupies the resource and forces the other to wait (a negative
    // externality). We quantify that interaction as the DELAY one job imposes on the
    // other, then show how each player's own strategy converts into its own payoff.
    vector<char> isCrit = criticalSet(inst, best);

    // Time at which an operation's part is ready = the finish of its job-predecessor.
    auto readyTime = [&](int gid) -> int {
        const Operation& op = inst.operationByGlobalId(gid);
        if (op.positionInJob == 0) return 0;
        const int predGid = inst.job(op.jobIndex).operation(op.positionInJob - 1).globalId;
        return best.endOf(predGid);
    };

    f << "TWO-PLAYER INTERACTIONS (who delays whom & how each strategy pays off)\n";
    f << "--------------------------------------------------------------------\n";
    f << "Two jobs INTERACT when their operations land on the same machine: the earlier\n";
    f << "one holds the resource and DELAYS the later one. 'Delay' = how long the later\n";
    f << "job was already ready but had to wait while the machine finished the rival's\n";
    f << "operation. (*) marks a pair on the critical path - that exact rivalry is what\n";
    f << "fixes the makespan Cmax, so resolving it is how the players reach the optimum.\n\n";

    vector<vector<long>> delayMx(inst.numJobs(), vector<long>(inst.numJobs(), 0));
    vector<int> degree(inst.numJobs(), 0);
    int edgeCount = 0;
    f << "Per-machine adjacencies (earlier player delays the later player):\n";
    for (int m = 0; m < inst.numMachines(); ++m) {
        const auto& v = byMachine[m];                 // already start-sorted above
        for (size_t q = 0; q + 1 < v.size(); ++q) {
            const int u = v[q], w = v[q + 1];
            const int ju = inst.operationByGlobalId(u).jobIndex;
            const int jw = inst.operationByGlobalId(w).jobIndex;
            if (ju == jw) continue;                   // same job: route order, not rivalry
            const int endU   = best.endOf(u);
            const int readyW = readyTime(w);
            const long delay = max(0, endU - readyW); // time u held the machine past w's readiness
            delayMx[ju][jw] += delay;
            const bool crit = isCrit[u] && isCrit[w];
            f << "  M" << (m + 1) << ": " << inst.job(ju).label() << " delays "
              << inst.job(jw).label() << " by " << delay << "  ("
              << inst.operationByGlobalId(u).label() << "[" << best.startOf(u) << "-" << endU
              << "] before " << inst.operationByGlobalId(w).label() << "["
              << best.startOf(w) << "-" << best.endOf(w) << "]; "
              << inst.job(jw).label() << " ready at " << readyW << ")"
              << (crit ? "  *" : "") << "\n";
            ++degree[ju]; ++degree[jw]; ++edgeCount;
        }
    }
    if (edgeCount == 0) f << "  (none - no two jobs share a machine slot)\n";
    f << "\n";

    // Pairwise totals: who imposes the most waiting on whom.
    f << "Pairwise delay totals (Ja -> Jb : total time Ja forced Jb to wait):\n";
    bool anyPair = false;
    for (int a = 0; a < inst.numJobs(); ++a)
        for (int b = 0; b < inst.numJobs(); ++b)
            if (delayMx[a][b] > 0) {
                f << "  " << inst.job(a).label() << " -> " << inst.job(b).label()
                  << " : " << delayMx[a][b] << "\n";
                anyPair = true;
            }
    if (!anyPair) f << "  (none)\n";
    f << "\n";

    // Per-player: the strategy it applied and the benefit (payoff) that strategy buys,
    // tying its machine choices to its own completion, waiting and resulting payoff.
    f << "Per-player strategy -> own benefit (each job maximises its OWN payoff U_i):\n";
    for (int j = 0; j < inst.numJobs(); ++j) {
        Player player(inst, j);                       // the job AS a self-interested agent
        const Strategy& s = solution.jobStrategy(j);
        long out = 0, in = 0;
        for (int k = 0; k < inst.numJobs(); ++k) { out += delayMx[j][k]; in += delayMx[k][j]; }
        f << "  " << player.label() << ": plays ";
        for (const OperationChoice& c : s.choices)
            f << c.operationLabel << "->M" << (c.machine + 1) << " ";
        f << "\n     benefit  : finishes C=" << s.completion << ", own waiting W=" << s.waiting
          << ", congestion Conf=" << s.conflict << ", payoff U="
          << setprecision(4) << s.payoff << setprecision(0) << "\n";
        f << "     interact : delays rivals by " << out << " in total, is delayed by rivals by " << in;
        if (best.jobCompletion(j) == best.makespan()) f << "   <- this job's finish SETS the makespan";
        f << "\n";
        string how;
        if (player.canImprove(result.bestState, payoff, &how))
            f << "     response : NOT best yet - it could raise its payoff by " << how << "\n";
        else
            f << "     response : best response - no unilateral re-route helps it, so it stays put (Nash-stable)\n";
    }
    f << "Interaction degree per job (number of rival adjacencies):";
    for (int j = 0; j < inst.numJobs(); ++j)
        f << "  " << inst.job(j).label() << ":" << degree[j];
    f << "\n\n";

    // ---- per-machine two-player Nash games (the bimatrix payoff tables) -----
    f << "PER-MACHINE TWO-PLAYER NASH GAMES (payoff tables, by machine section)\n";
    f << "--------------------------------------------------------------------\n";
    f << "For every machine where two jobs are rivals at the equilibrium, here is the\n";
    f << "full normal-form game between those two players: rows/columns are the machines\n";
    f << "each rival could route its contested operation to, and each cell is the pair of\n";
    f << "payoffs (U_A, U_B) that results when all other players keep their strategies.\n";
    f << "The played cell is (*); a pure-strategy Nash equilibrium is [NE]. This is where\n";
    f << "you can read whether the two players are individually in equilibrium per machine.\n";

    // Collect rival adjacencies (critical-path ones first), print up to a cap.
    vector<int> gM, gA, gB; vector<char> gC;
    for (int m = 0; m < inst.numMachines(); ++m) {
        const auto& v = byMachine[m];
        for (size_t q = 0; q + 1 < v.size(); ++q) {
            const int u = v[q], w = v[q + 1];
            if (inst.operationByGlobalId(u).jobIndex == inst.operationByGlobalId(w).jobIndex) continue;
            gM.push_back(m); gA.push_back(u); gB.push_back(w);
            gC.push_back((char)(isCrit[u] && isCrit[w] ? 1 : 0));
        }
    }
    const int maxGames = 6;
    int printed = 0;
    for (int pass = 0; pass < 2 && printed < maxGames; ++pass)        // pass 0: critical first
        for (size_t i = 0; i < gM.size() && printed < maxGames; ++i) {
            if ((pass == 0) != (gC[i] != 0)) continue;
            if (writePairGame(f, inst, payoff, result.bestState, gA[i], gB[i], gM[i], gC[i] != 0))
                ++printed;
        }
    if (printed == 0)
        f << "\n  (no non-trivial two-player game: every rival pair had a single eligible\n"
             "   machine, so there was no routing choice to play.)\n";
    else
        f << "\n(" << printed << " two-player game" << (printed == 1 ? "" : "s")
          << " shown, critical-path rivalries first"
          << (printed >= maxGames ? "; further machines omitted for readability" : "") << ".)\n";
}

} // namespace fjs
