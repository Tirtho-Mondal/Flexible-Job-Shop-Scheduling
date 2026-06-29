// ============================================================================
//  CodeExplanation.cpp - generates the human guide to the code + model.
// ============================================================================
#include "CodeExplanation.h"
#include <fstream>

using namespace std;

namespace fjs {

void CodeExplanation::write(const string& path, const PayoffFunction& payoff) {
    ofstream f(path);
    if (!f) return;

    f <<
"# Code Explanation - Game-Theoretic Flexible Job Shop Scheduling\n\n"
"This program solves the Flexible Job Shop Scheduling Problem (FJSSP) by modelling\n"
"it as a **non-cooperative game** and computing **Nash equilibria** through\n"
"best-response dynamics. It reads every `.fjs` instance under `data/`\n"
"(Brandimarte, Hurink edata/rdata/sdata/vdata and the RCFJSSP set) and writes its\n"
"results under `output/`. No console input is required.\n\n"

"## 1. The game model\n\n"
"**Players.** Every job is a player - one selfish decision maker.\n\n"
"**StrategyProfile.** A job decides, for each of its operations, (a) which eligible\n"
"machine to run on (routing) and (b) how early that operation appears in the\n"
"shared global dispatch order (sequencing). The combined choice of all players is\n"
"a *strategy profile*, held in `StrategyProfile`.\n\n"
"**Referee / outcome.** `ScheduleBuilder` is an as-early-as-possible list\n"
"scheduler. Given a profile it produces the timed `Schedule` (start/finish of\n"
"every operation, each job's completion time `C_i`, and the makespan).\n\n"

"## 2. The payoff function (the core)\n\n"
"`PayoffFunction` defines what players want:\n\n"
"```\n" + payoff.description() + "\n```\n\n"
"The individual cost is `cost_i = C_i` and the payoff maximised is `u_i = -C_i`.\n"
"The social objective we report is the makespan `Cmax = max_i C_i`. These are\n"
"linked: the makespan IS the cost of the unhappiest player, so that player always\n"
"wants to deviate to something faster - and any such deviation lowers the\n"
"makespan too. A profile where nobody can improve unilaterally is a pure-strategy\n"
"**Nash equilibrium**, and at that point the makespan is locally optimal against\n"
"single-job deviations.\n\n"

"## 3. How equilibria are found (`GameSolver`)\n\n"
"A SINGLE payoff (Section 2) governs everything; the search just helps the\n"
"players reach good equilibria of that one game. The core idea is **two-player\n"
"interaction**: rival jobs play against each other first, and only the inability\n"
"of ANY pair to improve triggers a random kick. For each run:\n"
"1. **seed** a strategy profile. Run 0 is ALWAYS fully random (random machine per\n"
"   operation + random precedence-feasible dispatch order). Later runs are seeded\n"
"   by (a) the players' learned BELIEFS, (b) a Global load-balancing machine\n"
"   selection, (c) a Local shortest-processing-time selection, or (d) random;\n"
"2. **two-player interaction descent (primary)**: on a critical path, the two\n"
"   rival jobs that share a critical machine PLAY THEIR 2-PLAYER GAME - they swap\n"
"   their order on the machine, or jointly re-route to the joint best response -\n"
"   taking the interaction that most lowers Cmax. A single job acting ALONE\n"
"   (re-route / re-sequence) is only a FALLBACK, used when no rival pair can\n"
"   improve. This repeats until neither any rival pair nor any lone job can\n"
"   improve - a **Nash-equilibrium** schedule;\n"
"3. **random kick + replay the game**: when Cmax can no longer be improved, a\n"
"   RANDOM KICK perturbs the profile and the two-player game is played again,\n"
"   keeping the better (iterated local search);\n"
"4. **multiple runs**: repeat from many seeds and keep the **best run** (lowest\n"
"   makespan).\n\n"
"### Belief learning (fictitious play) - how we approach optimal\n"
"Borrowing the long-term-memory idea of Kasapidis et al. (2025) and the\n"
"Global/Local seeding of Reijnen et al. (2026), `FictitiousPlay` keeps a pool of the\n"
"best equilibria and measures, for each operation, how often each machine is used\n"
"across them: `belief(op,machine)`. New runs and kicks are drawn from these\n"
"beliefs, so the players converge on the machine assignments that good solutions\n"
"agree on - the mechanism that drives the search toward optimal. This is pure\n"
"fictitious play and changes nothing about the payoff.\n\n"
"Work is capped by an evaluation budget that scales with the instance size so the\n"
"whole 364-instance benchmark finishes in a modest time.\n\n"

"## 4. File-by-file (OOP design)\n\n"
"Every concept is its own class; encapsulation (private data + const accessors),\n"
"abstraction and a clear single responsibility are used throughout. No `struct`\n"
"is used anywhere - only `class`.\n\n"
"| File | Responsibility |\n"
"|---|---|\n"
"| `Machine` | an immutable shared resource (a machine) |\n"
"| `Operation` | one operation: its eligible machines/times + the chosen one |\n"
"| `Job` | a **player**: an ordered route of operations |\n"
"| `Instance` | the whole problem (jobs + machines + flat operation index) |\n"
"| `StrategyProfile` | a complete strategy profile (routing + dispatch order) |\n"
"| `Schedule` | the timed outcome of decoding a profile |\n"
"| `ScheduleBuilder` | the referee: profile -> schedule |\n"
"| `PayoffFunction` | the SINGLE payoff (selfish completion / makespan) - the core model |\n"
"| `FictitiousPlay` | fictitious-play memory: elite pool + machine-choice beliefs |\n"
"| `GameSolver` | critical-path best-response + belief/greedy seeding + multi-run |\n"
"| `FjsInstanceReader` | parses `.fjs` (classic FJSSP and RCFJSSP layouts) |\n"
"| `BestKnownRegistry` | literature best-known makespans for the gap column |\n"
"| `InstanceReport` | the detailed per-instance log + final schedule |\n"
"| `GlobalReport` | `allresult.txt` (incremental) and `README.md` |\n"
"| `CodeExplanation` | this file |\n"
"| `main` | wiring: scan data, solve each instance, write reports |\n\n"

"## 5. Reading a per-instance log\n\n"
"`output/<group>_<instance>_log.txt` shows the initial random profile, then one\n"
"table row per accepted best-response move (which job moved, what it changed, its\n"
"completion before/after, and the resulting makespan), and finally the\n"
"equilibrium schedule with per-machine sequences and the full timetable.\n";
}

} // namespace fjs
