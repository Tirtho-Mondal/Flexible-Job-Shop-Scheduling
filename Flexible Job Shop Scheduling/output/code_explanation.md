# Code Explanation - Game-Theoretic Flexible Job Shop Scheduling

This program solves the Flexible Job Shop Scheduling Problem (FJSSP) by modelling
it as a **non-cooperative game** and computing **Nash equilibria** through
best-response dynamics. It reads every `.fjs` instance under `data/`
(Brandimarte, Hurink edata/rdata/sdata/vdata and the RCFJSSP set) and writes its
results under `output/`. No console input is required.

## 1. The game model

**Players.** Every job is a player - one selfish decision maker.

**StrategyProfile.** A job decides, for each of its operations, (a) which eligible
machine to run on (routing) and (b) how early that operation appears in the
shared global dispatch order (sequencing). The combined choice of all players is
a *strategy profile*, held in `StrategyProfile`.

**Referee / outcome.** `ScheduleBuilder` is an as-early-as-possible list
scheduler. Given a profile it produces the timed `Schedule` (start/finish of
every operation, each job's completion time `C_i`, and the makespan).

## 2. The payoff function (the core)

`PayoffFunction` defines what players want:

```
ONE payoff function. Each job is a self-interested player; machines are
shared resources the jobs compete for. Job i's payoff is
    U_i = 1 / ( 1 + a*C_i + b*W_i + g*Conf_i + d*Cmax )
with C_i = completion, W_i = waiting (= C_i - processing), Conf_i =
machine-conflict load (busier chosen machines cost more), and Cmax the
shared makespan that links each job's payoff to global quality. A schedule
is a Nash equilibrium when no job can raise U_i by changing its own
machine assignment or sequence position alone. The reported social
objective is the makespan Cmax.
```

The individual cost is `cost_i = C_i` and the payoff maximised is `u_i = -C_i`.
The social objective we report is the makespan `Cmax = max_i C_i`. These are
linked: the makespan IS the cost of the unhappiest player, so that player always
wants to deviate to something faster - and any such deviation lowers the
makespan too. A profile where nobody can improve unilaterally is a pure-strategy
**Nash equilibrium**, and at that point the makespan is locally optimal against
single-job deviations.

## 3. How equilibria are found (`GameSolver`)

A SINGLE payoff (Section 2) governs everything; the search just helps the
players reach good equilibria of that one game. For each run:
1. **seed** a strategy profile. Run 0 is ALWAYS fully random (random machine per
   operation + random precedence-feasible dispatch order). Later runs are seeded
   by (a) the players' learned BELIEFS, (b) a Global load-balancing machine
   selection, (c) a Local shortest-processing-time selection, or (d) random;
2. **critical-path descent (single-job + job-pair moves)**: find a critical path
   (only those operations can change the makespan) and make the single best
   deviation that most lowers the makespan - either a UNILATERAL move (re-route a
   critical operation, or re-sequence it) or a JOB-vs-JOB move on a shared machine
   (swap two rival operations, or reroute both off the contested machine); repeat
   until neither any job nor any rival pair can improve (a **pairwise
   Nash-stable** schedule);
3. **iterated local search**: kick the run's best profile (the re-routing part
   aimed by the beliefs) and descend again, keeping the better;
4. **multiple runs**: repeat from many seeds and simply keep the **best run**
   (the one with the lowest makespan).

### Belief learning (fictitious play) - how we approach optimal
Borrowing the long-term-memory idea of Kasapidis et al. (2025) and the
Global/Local seeding of Reijnen et al. (2026), `BeliefModel` keeps a pool of the
best equilibria and measures, for each operation, how often each machine is used
across them: `belief(op,machine)`. New runs and kicks are drawn from these
beliefs, so the players converge on the machine assignments that good solutions
agree on - the mechanism that drives the search toward optimal. This is pure
fictitious play and changes nothing about the payoff.

Work is capped by an evaluation budget that scales with the instance size so the
whole 364-instance benchmark finishes in a modest time.

## 4. File-by-file (OOP design)

Every concept is its own class; encapsulation (private data + const accessors),
abstraction and a clear single responsibility are used throughout. No `struct`
is used anywhere - only `class`.

| File | Responsibility |
|---|---|
| `Machine` | an immutable shared resource (a machine) |
| `Operation` | one operation: its eligible machines/times + the chosen one |
| `Job` | a **player**: an ordered route of operations |
| `Instance` | the whole problem (jobs + machines + flat operation index) |
| `StrategyProfile` | a complete strategy profile (routing + dispatch order) |
| `Schedule` | the timed outcome of decoding a profile |
| `ScheduleBuilder` | the referee: profile -> schedule |
| `PayoffFunction` | the SINGLE payoff (selfish completion / makespan) - the core model |
| `BeliefModel` | fictitious-play memory: elite pool + machine-choice beliefs |
| `GameSolver` | critical-path best-response + belief/greedy seeding + multi-run |
| `FjsInstanceReader` | parses `.fjs` (classic FJSSP and RCFJSSP layouts) |
| `BestKnownRegistry` | literature best-known makespans for the gap column |
| `InstanceReport` | the detailed per-instance log + final schedule |
| `GlobalReport` | `allresult.txt` (incremental) and `README.md` |
| `CodeExplanation` | this file |
| `main` | wiring: scan data, solve each instance, write reports |

## 5. Reading a per-instance log

`output/<group>_<instance>_log.txt` shows the initial random profile, then one
table row per accepted best-response move (which job moved, what it changed, its
completion before/after, and the resulting makespan), and finally the
equilibrium schedule with per-machine sequences and the full timetable.
