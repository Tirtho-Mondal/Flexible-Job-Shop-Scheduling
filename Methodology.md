# A Non-Cooperative Game-Theoretic Bilevel Optimization Approach for the Flexible Job-Shop Scheduling Problem

**Methodology (thesis-defence reference)**

This document derives, line by line, the complete methodology implemented in the solver.
Every modelling decision, equation, data structure and algorithmic step is stated and
mapped to the source file that realises it. It is organised so that an examiner can trace
the path from *problem definition* → *game model* → *bilevel game* → *solution algorithm*
→ *certification* → *experiments*.

**List of figures.**
- Figure 1 — Instance data model and a strategy profile $s=(\mathbf r,\boldsymbol\pi)$ (§1).
- Figure 2 — Active-schedule decoder with gap insertion (§2).
- Figure 3 — Why the stable lexicographic payoff never disagrees with the makespan (§3).
- Figure 4 — Critical path (head + tail) (§4).
- Figure 5 — The three game modes at a glance (§5).
- Figure 6 — **Bilevel two-layer game architecture** (the contribution) (§6).
- Figure 7 — Alternating (Gauss–Seidel) best-response flow (§6.4).
- **§6.5 — Worked play-through** of the two-layer game: sequencing NE → routing NE → SPNE.
- **§6.7 — Case study**: a two-job routing game (Cases 01–04) — Case 01 (both on M1) → Cmax 9 → 8.
- Figure 8 — End-to-end equilibrium-driven memetic ILS (§10).
- Figure 9 — The crossover operators (POX / OUX / OOX; + PWX / RMX / WGX / CPX in `crossover.md`) (Appendix D).
- Appendix A — Worked numerical example: a 2-job bilevel routing bimatrix.
- Appendix B — Complete parameter reference · Appendix C — I/O formats · Appendix D — Fine-grained mechanics.
- Appendix E — Theoretical foundations & related work · Appendix F — Defence cheat-sheet.
- **Figure 0 — Methodology at a glance: all stages (block diagram).**

---

## Methodology at a Glance — All Stages (block diagram)

```
Figure 0. The complete methodology pipeline (all stages).

  ┌────────────────────────────────────────────────────────────────────────────┐
  │  STAGE 0 · INPUT                                                            │
  │  .fjs instance  ──►  Instance :  jobs = PLAYERS , machines , operations     │
  │                      (each operation: eligible machines + processing times) │
  └────────────────────────────────────┬───────────────────────────────────────┘
                                        ▼
  ┌───────────────── STAGE 1 · MULTI-START   (for run = 0 .. runs-1) ───────────┐
  │  1a  SEED  s = (MAV , OSV)      run0 = RANDOM  |  later = ELITE-POOL          │
  │           │                                                                 │
  │           ▼                                                                 │
  │  1b  DECODE  s ─► Schedule   (active-schedule, gap insertion)               │
  │           │                   → completions C_j , makespan Cmax , potential Φ│
  │           ▼                                                                 │
  │  ╔════════════ STAGE 2 · PLAY THE BILEVEL GAME ═══════════════════════════╗ │
  │  ║  repeat (ALTERNATE) until neither layer lowers Φ :                      ║ │
  │  ║   ┌───────────────────────────────────────────────────────────────┐   ║ │
  │  ║   │ LAYER 2 · SEQUENCING GAME  (routing FIXED)                     │   ║ │
  │  ║   │   players QUEUED on a machine fight for slot priority          │   ║ │
  │  ║   │   moves: RESEQUENCE + N5 SWAP   →  sequencing NE  E2(r)         │   ║ │
  │  ║   └───────────────────────────────────────────────────────────────┘   ║ │
  │  ║                     ▲  anticipation │  (evaluate reroute at E2)         ║ │
  │  ║   ┌─────────────────┴─────────────────────────────────────────────┐   ║ │
  │  ║   │ LAYER 1 · ROUTING GAME  (over E2)                              │   ║ │
  │  ║   │   players fight for the MACHINES (congestion)                  │   ║ │
  │  ║   │   moves: SOLO REROUTE + MUTUAL REROUTE   →  routing NE          │   ║ │
  │  ║   └───────────────────────────────────────────────────────────────┘   ║ │
  │  ║   ⇒  SUBGAME-PERFECT NASH EQUILIBRIUM   (r*, π*=E2(r*))                 ║ │
  │  ╚═════════════════════════════════════════════════════════════════════════╝ │
  │           │                                                                 │
  │           ▼                                                                 │
  │  STAGE 3 · ILS LOOP  (until patience) :                                     │
  │     PERTURB  crossover(runBest, elite)  OR  random kick                     │
  │     REPLAY   Stage 2 (the bilevel game) → new equilibrium                   │
  │     KEEP     the better; update incumbent by Φ                              │
  │           │                                                                 │
  │           ▼                                                                 │
  │  STAGE 4 · ELITE-POOL LEARNING   elitePool.consider(run-best)               │
  └────────────────────────────────────┬───────────────────────────────────────┘
                                        ▼
  ┌────────────────────────────────────────────────────────────────────────────┐
  │  STAGE 5 · CERTIFY & REPORT                                                 │
  │  NashChecker (0 profitable deviations?)  ──►  Cmax , gap vs BKS ,           │
  │  per-instance log (L1/L2 moves + routing bimatrices) , Gantt chart          │
  └────────────────────────────────────────────────────────────────────────────┘
```

Stage 2 (the two-layer game) is the core contribution and is detailed with a full worked
play-through and player-interaction analysis in **§6** (see also the companion `bilevel.md`).

---

## 1. Problem definition (Flexible Job-Shop Scheduling Problem)

### 1.1 Data
An instance consists of

- a set of **jobs** (the **players**) $\mathcal{J}=\{1,2,\dots,n\}$;
- a set of **machines** (the shared **resources**) $\mathcal{M}=\{1,2,\dots,m\}$;
- each job $j$ has an ordered route of operations $O_{j,1},O_{j,2},\dots,O_{j,n_j}$ that
  must run in that order (**precedence**);
- each operation $O$ has a set of **eligible machines** $E(O)\subseteq\mathcal{M}$; running
  $O$ on machine $\mu\in E(O)$ takes processing time $p(O,\mu)>0$.

The total number of operations is $N=\sum_{j\in\mathcal{J}} n_j$. Every operation is given a
flat **global id** $g\in\{0,\dots,N-1\}$ so that the whole instance can be addressed by one
index (`Operation.globalId`, `Instance.operationByGlobalId`). The eligibility table is
immutable problem data; the *chosen* machine is the mutable decision
(`Operation.h`: `machines[]`, `times[]`, `chosen`).

**Flexibility.** "Flexible" means $|E(O)|\ge 1$ may exceed one, so the problem couples two
decisions: **routing** (which machine) and **sequencing** (in which order). This coupling is
the structural reason a *two-layer* game is natural (Section 6).

### 1.2 Decision variables
A complete decision is a **strategy profile** $s=(\mathbf{r},\boldsymbol{\pi})$
(`StrategyProfile.h`):

- **Routing vector (MAV)** $\mathbf{r}\in\prod_{g} \{0,\dots,|E(O_g)|-1\}$: for each operation
  $g$, the index $r_g$ of the eligible alternative chosen. We write $\mu(g)$ and $p(g)$ for the
  machine and processing time of the chosen alternative.
- **Operation sequence (OSV)** $\boldsymbol{\pi}$: a permutation of all $N$ operations that is
  **precedence-feasible** — the operations of any job appear in route order. Formally,
$$
\forall j,\;\forall k<k':\quad \mathrm{pos}_{\boldsymbol\pi}(O_{j,k}) < \mathrm{pos}_{\boldsymbol\pi}(O_{j,k'}).
$$

Every move the solver makes preserves precedence-feasibility, so every profile decodes to a
valid schedule.

```
Figure 1. Instance data model and a strategy profile s = (r, π).

  INSTANCE
  ├── Machines :  M1  M2  ...  Mm                       (shared resources)
  └── Jobs (PLAYERS)
        Job j ──►  O(j,1) ─► O(j,2) ─► ... ─► O(j,nj)   route order (precedence ►)
                     │
                     └── eligible machines & times:  { M1:p1 , M3:p3 , M4:p4 }
                                                       └── choose ONE  =  r_g  (routing)

  STRATEGY PROFILE   s = ( r , π )
     r  (MAV) : per operation, the chosen machine            ← ROUTING   decision
     π  (OSV) : ONE precedence-feasible order of ALL N ops   ← SEQUENCING decision

        π = [ O(2,1)  O(1,1)  O(2,2)  O(3,1)  O(1,2) ... ]
              └─ within each job, operations stay in route order ─┘
```

### 1.3 Objective
The reported social objective is the **makespan**
$$
C_{\max}(s)=\max_{j\in\mathcal{J}} C_j(s),
$$
where $C_j(s)$ is the completion time of job $j$ (the finish time of its last operation).
FJSP with $C_{\max}$ is **NP-hard** (it contains the classical job shop as a special case),
which justifies a heuristic/equilibrium approach.

---

## 2. The decoder: from a strategy profile to a timed schedule

The map $s\mapsto \text{Schedule}$ is the **referee** of the game
(`ScheduleBuilder::build`). It is an **active-schedule decoder with gap insertion** (also
called an *as-early-as-possible* list scheduler).

Maintain, for each machine $k$, the ordered list of already-placed busy intervals
$B_k=\{[a_1,b_1],[a_2,b_2],\dots\}$, and for each job $j$ the finish time of its latest
placed operation $\rho_j$ (initially $0$). Process the operations in dispatch order
$\boldsymbol\pi$. For the current operation $g$ with machine $k=\mu(g)$ and time $p=p(g)$:

1. **Release time** (precedence): the operation cannot start before its job-predecessor
   finished,
$$
\text{release}(g)=\rho_{job(g)}.
$$
2. **Earliest feasible start** (gap insertion): find the earliest time
   $t\ge\text{release}(g)$ such that $[t,t+p]$ fits in an idle gap of machine $k$, i.e.
$$
s(g)=\min\Big\{\,t\ge \text{release}(g)\;:\;[t,t+p]\cap [a_i,b_i]=\varnothing\ \forall [a_i,b_i]\in B_k\,\Big\}.
$$
   The code scans the sorted intervals, tracking the running end `prevEnd`; the first gap
   $[\max(\text{release},\text{prevEnd}),a_i]$ wide enough ($\ge p$) is used; otherwise the
   operation is appended after the last interval (`ScheduleBuilder.cpp` lines 37–45).
3. **Finish** $e(g)=s(g)+p$. Record $\mu(g),s(g),e(g)$, update $\rho_{job(g)}=e(g)$, and insert
   $[s(g),e(g)]$ into $B_k$ keeping it sorted.

Finally
$$
C_j=\rho_j,\qquad C_{\max}=\max_j C_j .
$$

This decoder always produces an **active schedule** (no operation can be shifted earlier
without delaying another), guaranteeing the search ranges over a dominant set of schedules.
The decode is the unit of computational cost; the solver counts decodes
(`OperationalDispatchingLayer::evaluations`).

```
Figure 2. Active-schedule decoder (gap insertion) on machine k.

  time ─►   0    2    4    6    8   10   12
  M_k :    |== A ==|          |==== B ====|
                    └─ idle gap [4,6], width 2 ─┘

  new operation g  (p = 2,  release = 3):
     find earliest t ≥ 3 with [t , t+2] fitting an idle gap  →  t = 4

  M_k :    |== A ==|= g =|==== B ====|       g slots INTO the gap (not appended)
                   4     6
  If no gap is wide enough, g is appended after the last interval:  s(g)=max(release,prevEnd).
```

---

## 3. The game-theoretic model

### 3.1 Players, strategies, interaction
- **Players** = jobs. Each job is an independent, self-interested decision maker.
- **Strategy of player $j$** = its own routing choices for its operations + its operations'
  positions in the dispatch order.
- **Interaction**: two jobs *interact* whenever two of their operations are assigned to the
  same machine — the earlier one delays the later one. This shared-resource contention is the
  source of all conflict in the game.

### 3.2 The per-player payoff (the heart of the model)
Implemented once in `PayoffFunction::forPlayer`. First define the **own-interest cost** of
job $i$ (lower is better):
$$
\mathrm{own}_i(s)=\alpha\,C_i+\beta\,W_i+\gamma\,\mathrm{Conf}_i+\tau\,\mathrm{Toll}_i ,
$$
with the four terms:

- **Completion** $C_i$ — the job's finish time.
- **Waiting** $W_i=\max\{0,\;C_i-\sum_{k} p(O_{i,k})\}$ — idle time the job spends waiting in
  queues (completion minus its own total processing).
- **Machine conflict** $\mathrm{Conf}_i=\sum_{O\in i}\,\mathrm{load}\big(\mu(O)\big)$, where
  $\mathrm{load}(k)=\sum_{g:\ \mu(g)=k} p(g)$ is the total processing booked on machine $k$.
  A busier chosen machine is penalised (it means more contention with rivals).
- **Congestion toll** $\mathrm{Toll}_i$ — the novelty term, defined in Section 9.

**Stable, makespan-aligned payoff (lexicographic).** The payoff returned to a player is
$$
\boxed{\;U_i(s)=
\begin{cases}
\dfrac{1}{\,1+w\,C_{\max}+\dfrac{\mathrm{own}_i}{1+\mathrm{own}_i}\,}, & \delta>0\quad(\text{stable}),\\[2.2ex]
\dfrac{1}{\,1+\mathrm{own}_i\,}, & \delta=0\quad(\text{pure selfish}),
\end{cases}\;}
\qquad w=\max(1,\delta).
$$

The design property (verified empirically on 4068 bimatrix cells, 0 violations; provable):
because the makespan is an **integer** primary key and the tie-breaker
$\mathrm{own}_i/(1+\mathrm{own}_i)\in[0,1)$ is strictly bounded below $1$,
$$
C_{\max}(x)<C_{\max}(y)\;\Longrightarrow\;U_i(x)>U_i(y)\quad\text{for every player }i.
$$
*Proof.* For two profiles with $C_{\max}(y)-C_{\max}(x)\ge 1$,
$\text{cost}(y)-\text{cost}(x)=w\,(C_{\max}(y)-C_{\max}(x))+(f_y-f_x)\ge w-1\ge 0$ since
$w\ge 1$ and $f_y-f_x\in(-1,1)$; hence $U_i(x)>U_i(y)$. $\square$

Consequence: when $\delta>0$ the **minimum-makespan strategy is always the best response and
the Nash equilibrium** — payoff and makespan can never disagree (no price of anarchy). When
$\delta=0$ the makespan is absent from $U_i$, so selfish equilibria may be inefficient (the
**price of anarchy**, Section 8). The two regimes are the basis of the three game modes.

The `Payoff` object also exposes $\mathrm{own}_i$ separately (`Payoff.ownCost`) because the
recombination operator needs a per-job discriminating signal that is *not* makespan-dominated
(Section 10.3).

```
Figure 3. Why the stable payoff never disagrees with the makespan (lexicographic order).

  cost_i =  w·Cmax            +   own_i/(1+own_i)
            └ INTEGER primary ┘   └ tie-breaker in [0,1) ┘
            dominates                cannot overturn an
            (step ≥ w ≥ 1)           integer Cmax gap

  Cmax:   52        53        54           higher Cmax  ─►  lower payoff
  U_i :  0.0189 >  0.0185 >  0.0181        ALWAYS, for EVERY player
                                            ⇒ min-Cmax cell = best response = Nash eq.
```

### 3.3 The global potential
Define the **global potential** (`PayoffFunction::globalPotential`)
$$
\Phi(s)=C_{\max}(s)\cdot 10^{6}+\sum_{j\in\mathcal J} C_j(s).
$$
This is **not** a payoff; it is a single scalar the coordinated/bilevel best-response
dynamics descend. The $10^6$ weight makes the makespan strictly dominate, with total
completion $\sum_j C_j$ a tie-breaker that prefers schedules where jobs finish earlier on
average. Minimising $\Phi$ is equivalent to lexicographically minimising
$(C_{\max},\sum_j C_j)$.

---

## 4. Critical-path neighbourhood (which operations can change the makespan)

Only operations on a **critical path** can change $C_{\max}$; restricting moves to them is
both faster and game-theoretically meaningful (the bottleneck players)
(`OperationalDispatchingLayer::criticalOperations`).

Build two successor relations on the decoded schedule:

- **Job successor** $\text{jobSucc}(g)$ — the next operation of the same job;
- **Machine successor** $\text{machSucc}(g)$ — the next operation in time order on the same
  machine.

The **tail** (longest remaining path from $g$ to the schedule end) is computed by dynamic
programming over operations in decreasing finish-time order:
$$
\text{tail}(g)=\big(e(g)-s(g)\big)+\max\big\{\text{tail}(\text{jobSucc}(g)),\;\text{tail}(\text{machSucc}(g))\big\}.
$$
An operation is **critical** iff its head (start time) plus its tail equals the makespan:
$$
g\ \text{is critical}\quad\Longleftrightarrow\quad s(g)+\text{tail}(g)=C_{\max}.
$$
Let $\mathcal{C}(s)$ denote the set of critical operations.

```
Figure 4. Critical path — only critical operations can change Cmax.

   |◄─────────────── makespan  Cmax ───────────────►|
   start(g)      tail(g) = longest remaining path
      │              │
   ─► O(a) ─► O(b) ─► O(g) ─► O(c) ─► ... ─► O(z) ─►| end
        ▲       ▲       ▲                            ▲
        └───────┴───────┴── job-successor (►) OR machine-successor (►)

   tail(g) = dur(g) + max( tail(jobSucc g) , tail(machSucc g) )
   critical ⇔ start(g) + tail(g) = Cmax     ⇒  moves act ONLY on  C(s)  (the bottleneck players)
```

---

## 5. The three game modes (acceptance rules)

The solver implements three distinct games, selected by `acceptance` in
`AlgorithmSetting.txt`. They differ only in **what move is accepted** and **what structure
the game has**.

```
Figure 5. The three game modes.

                 │ acceptance rule        │ structure        │ equilibrium      │ makespan
  ───────────────┼────────────────────────┼──────────────────┼──────────────────┼──────────
  potential      │ accept iff Φ (makespan)│ flat /           │ potential-game   │ BEST
  (descend)      │ DECREASES              │ single-level     │ NE (makespan opt)│ (PoA≈1)
  ───────────────┼────────────────────────┼──────────────────┼──────────────────┼──────────
  selfish        │ accept iff own U_i     │ flat /           │ pure Nash eq.    │ worst
  (descendSelfish)│ INCREASES             │ single-level     │ (own payoff)     │ (PoA>1)
  ───────────────┼────────────────────────┼──────────────────┼──────────────────┼──────────
  bilevel        │ both layers descend Φ, │ HIERARCHICAL     │ SUBGAME-PERFECT  │ good
  (playRouting   │ routing anticipates    │ two-level        │ Nash eq.         │ (efficient)
   Game)         │ the sequencing eq.     │                  │                  │
```

### 5.1 `potential` — coordinated potential game (`descend`)
All players share one objective, the potential $\Phi$. A move is accepted **iff it lowers
$\Phi$**. This is an **identical-interest potential game**: every player's incentive is
aligned with $-\Phi$, so best-response dynamics converge to a pure-strategy equilibrium that
is a **makespan local optimum**. Move set (Section 7): two-player swap, mutual reroute,
combined reroute+swap (primary), plus single-job reroute/resequence (fallback). This mode
gives the **best makespan** and serves as the efficiency baseline.

### 5.2 `selfish` — pure non-cooperative game (`descendSelfish`)
Each job maximises **its own** $U_i$. Two phases, alternated to convergence:

- **Phase 1 (unilateral best response).** In bottleneck-first order, each job makes the single
  move — reroute one operation or shift one within its precedence window — that most raises
  its own $U_i$, accepted **iff $U_i$ strictly improves**:
$$
\text{accept } s\to s' \iff U_j(s')>U_j(s)+\varepsilon .
$$
  A sweep with no improving job is a **pure-strategy Nash equilibrium** under $U$.
- **Phase 2 (pairwise Pareto).** When no single job can improve, two rival jobs sharing a
  machine deviate together (swap order or jointly reroute), accepted **iff both gain and
  $C_{\max}$ does not rise** — a Pareto-improving coalition refining to a stronger
  (pairwise-stable) equilibrium. This phase carries a mild coordination constraint (the
  no-rise-in-$C_{\max}$ clause) and is disclosed as such.

With $\delta=0$ this is the genuine selfish game whose equilibria exhibit the **price of
anarchy**; with $\delta>0$ the stable payoff makes it makespan-aligned.

### 5.3 `bilevel` — two-layer game (the contribution)
Detailed in Section 6.

---

## 6. The bilevel optimization model (the core methodology)

The flexible job shop has **two coupled decisions** — *routing* (assignment, global and
combinatorial) and *sequencing* (per-machine ordering, local). We cast this as a **bilevel
optimization** problem and solve **both levels as non-cooperative games**, so the players choose
machines while **anticipating** how the sequencing conflict will resolve.

### 6.1 The bilevel optimization formulation

FJSP is formulated as a **two-level (bilevel) program**: an **upper-level (leader)** routing
problem whose objective is evaluated *through* a **lower-level (follower)** sequencing problem.

**Upper level (leader) — routing.** Choose the machine assignment $\mathbf r$ to minimise the
makespan induced by the lower level:
$$
\min_{\mathbf r\in\mathcal R}\; F(\mathbf r)=C_{\max}\big(\mathbf r,\ \boldsymbol\pi^{*}(\mathbf r)\big),
$$
where $\mathcal R=\prod_g\{0,\dots,|E(O_g)|-1\}$ is the set of feasible routings.

**Lower level (follower) — sequencing, parametrised by $\mathbf r$.** For the routing fixed by the
leader, choose the dispatch order that resolves the machine contention:
$$
\boldsymbol\pi^{*}(\mathbf r)\in\arg\min_{\boldsymbol\pi\in\Pi(\mathbf r)}\; C_{\max}(\mathbf r,\boldsymbol\pi),
$$
where $\Pi(\mathbf r)$ is the set of precedence-feasible sequences. The leader **anticipates** this
follower response — that nesting is what makes the program **bilevel** (as opposed to a single
flat optimisation over $(\mathbf r,\boldsymbol\pi)$).

**Game-theoretic realisation (this work).** Rather than a central optimiser, **each level is
solved as a non-cooperative game** whose Nash equilibrium plays the role of the level's optimum:

- the **lower level** is the **Sequencing Game** $\mathcal G_2(\mathbf r)$ — jobs best-respond on
  their dispatch positions to a Nash equilibrium $E_2(\mathbf r)$ (which, under the potential
  $\Phi$, is a makespan local optimum), so $\boldsymbol\pi^{*}(\mathbf r)=E_2(\mathbf r)$;
- the **upper level** is the **Routing Game** $\mathcal G_1$ — jobs best-respond on machine choice,
  each candidate evaluated against $E_2(\mathbf r)$, to a routing Nash equilibrium.

Substituting the equilibrium for the follower's optimum turns the bilevel program into a
**bilevel program with an equilibrium constraint** (an MPEC-type structure), whose solution is the
**subgame-perfect Nash equilibrium**
$$
\boxed{\;\mathbf r^{*}=\arg\min_{\mathbf r\in\mathcal R} C_{\max}\big(E_2(\mathbf r)\big),\qquad
\boldsymbol\pi^{*}=E_2(\mathbf r^{*}).\;}
$$
This is exactly the **bilevel optimization** the title names: the **makespan is optimised over the
routing decisions, subject to the sequencing equilibrium at the lower level.** (The solver
approximates it by *alternating* the two games — §6.4 — rather than fully re-solving the lower
level per routing candidate.)

### 6.2 Definition of the two games
$$
\mathcal{G}=\big(\mathcal{G}_1,\ \mathcal{G}_2\big).
$$

- **Lower stage — the Sequencing Game $\mathcal{G}_2(\mathbf r)$** (`sequencingGame`). With the
  routing $\mathbf{r}$ **fixed**, the players contend for *timing* on their assigned machines.
  The strategy of a player is the dispatch position of its operations. A move is accepted iff
  it lowers $\Phi$, using a steepest descent over the two strongest sequencing moves on the
  critical path:
  - **resequence** a critical operation within its precedence window, and
  - the **critical-block swap (N5)** — two adjacent critical rival operations exchange order on
    a machine.

  When no move lowers $\Phi$, the result is a **sequencing equilibrium**
$$
E_2(\mathbf r)=\arg\!\min_{\boldsymbol\pi\in \text{Seq-moves}}\Phi(\mathbf r,\boldsymbol\pi),
$$
  i.e. a makespan-stable schedule for the routing $\mathbf r$.

- **Upper stage — the Routing Game $\mathcal{G}_1$** (`playRoutingGame`). The players reconsider
  **which machines** to use. A move re-routes a **critical** operation (solo) or two rival
  jobs jointly re-route (**mutual**), and crucially **each candidate routing is evaluated against
  the sequencing equilibrium it would induce**. A move is accepted iff it lowers $\Phi$ at that
  induced equilibrium. When no job can improve its routing, the result is a **routing
  equilibrium**.

### 6.3 The solution concept: subgame-perfect equilibrium
The two stages are linked by **anticipation**: routing is chosen knowing the sequencing game
will re-settle. The solution concept is therefore a **Subgame-Perfect Nash Equilibrium (SPNE)**
of the two-stage game,
$$
\boxed{\;\mathbf r^{*}=\arg\!\min_{\mathbf r}\;C_{\max}\big(E_2(\mathbf r)\big),\qquad
\boldsymbol\pi^{*}=E_2(\mathbf r^{*}).\;}
$$
$\mathbf r^{*}$ is a Nash equilibrium of $\mathcal{G}_1$ in which each player's outcome is read
at the sequencing equilibrium $E_2(\mathbf r^{*})$, and $E_2(\mathbf r^{*})$ is itself a Nash
equilibrium of $\mathcal{G}_2$.

**This hierarchy — routing chosen by anticipating the sequencing equilibrium — is what makes
the model *bilevel*, as opposed to a flat single-level game in which a reroute and a resequence
are merely two moves in one neighbourhood.** It is the distinguishing feature versus the
`potential` and `selfish` modes.

```
Figure 6. Bilevel two-layer game architecture (the contribution).

  ┌────────────────────────────────────────────────────────────────────────┐
  │  LAYER 1 — STRATEGIC COORDINATION LAYER        ROUTING GAME  G1         │
  │  players choose MACHINES (MAV)                                          │
  │  move = reroute a CRITICAL op (solo)  or  two rivals jointly (mutual)   │
  │                                                                        │
  │      propose routing  r'  ───────────────┐                             │
  │            ▲                              ▼   evaluate r' AFTER ...     │
  │            │     ┌────────────────────────────────────────────────┐    │
  │            │     │ LAYER 2 — OPERATIONAL DISPATCHING LAYER         │    │
  │            │     │                       SEQUENCING GAME  G2(r')   │    │
  │            │     │ routing FIXED;  players choose ORDER (OSV)      │    │
  │            │     │ move = resequence  +  critical-block SWAP (N5)  │    │
  │            │     │ descend Φ   ──►   sequencing equilibrium E2(r') │    │
  │            └─────┴────────────────────────────────────────────────┘    │
  │      accept r'  ⇔  it lowers Φ at the induced equilibrium E2(r')        │
  └────────────────────────────────────────────────────────────────────────┘
        STOP when NEITHER a reroute (L1) NOR a resequence/swap (L2) lowers Φ
                                      │
                                      ▼
        SUBGAME-PERFECT NASH EQUILIBRIUM     r* = argmin_r  Cmax( E2(r) )
```

### 6.4 The solving algorithm: alternating best response
The exact bilevel game would re-solve $\mathcal G_2$ for every routing candidate (a nested
inner game). For tractability the solver uses an **alternating (Gauss–Seidel) best-response**
scheme that re-equilibrates the sequencing game once per routing sweep:

```
playRoutingGame(state):
  repeat (sweeps):
    (L2) state ← sequencingGame(state)              # routing fixed → sequencing equilibrium E2
    (L1) crit ← criticalOperations(decode(state))
         improved ← false
         for each critical op g (bottleneck-first):
             best ← argmin over alternatives a of Φ(reroute(g,a))     # single decode/candidate
             if Φ improves: apply best; improved ← true
         for each adjacent critical rival pair (u,w) on a machine:    # MUTUAL move
             best ← argmin over (a_u,a_w) of Φ(reroute u,w)
             if Φ improves: apply best; improved ← true
    until not improved                               # routing Nash equilibrium → SPNE
```

Each routing candidate is evaluated with a single decode against the current sequencing
equilibrium (evaluate, then revert in place), so the two games **alternate** rather than
nest — the same subgame-perfect endpoint, but ~10× faster. This is the precise sense in which
the implementation is an *alternating best-response approximation of the bilevel game*.

```
Figure 7. Alternating (Gauss–Seidel) best-response solving of the bilevel game.

        ┌───────────────────────────────┐
        │ start:  profile s = (r, π)     │
        └───────────────┬───────────────┘
                        ▼
        ┌───────────────────────────────┐
   ┌───►│ L2  sequencingGame(s)          │   resequence + N5 swap, descend Φ
   │    │     → sequencing equilibrium   │
   │    └───────────────┬───────────────┘
   │                    ▼
   │    ┌───────────────────────────────┐
   │    │ L1  for each critical op:      │   reroute (solo) / mutual,
   │    │     reroute if Φ falls (vs E2) │   single decode per candidate
   │    └───────────────┬───────────────┘
   │                    ▼
   │           any routing move improved?
   │              │ yes            │ no
   └──────────────┘                ▼
                          SPNE reached → return  (r*, π*)
```

### 6.5 Worked example — playing the two-layer game to a subgame-perfect equilibrium

We play the full bilevel game on a small instance (3 jobs, 5 operations), reaching a **Nash
equilibrium at each layer** and the **subgame-perfect equilibrium** overall. Solutions are shown
as **OSV** (job-id dispatch order) + **MAV** (chosen eligible-machine index) — see the appendix
`solution.md`. Eligible machines (machine : time):

| operation | alt 1 | alt 2 | | operation | alt 1 | alt 2 |
|---|---|---|---|---|---|---|
| O(1,1) | M1:3 | M2:5 | | O(2,2) | M3:4 | M1:7 |
| O(1,2) | M2:4 | M3:2 | | O(3,1) | M1:5 | M2:3 |
| O(2,1) | M1:2 | M3:6 | | | | |

**Initial profile** (a poor start — every op on its first machine, a bad order):
```
OSV = [ 2 , 3 , 1 , 1 , 2 ]   = O(2,1) O(3,1) O(1,1) O(1,2) O(2,2)
MAV = [ 1 , 1 , 1 , 1 , 1 ]   (all first-choice machines)
```
Decode → **machine queues** (start–end):
```
  M1 : O(2,1)[0,2]J2 → O(3,1)[2,7]J3 → O(1,1)[7,10]J1     ◄ J2, J3, J1 all contend on M1
  M2 : O(1,2)[10,14]J1
  M3 : O(2,2)[2,6]J2
  C = (J1:14, J2:6, J3:7),   Cmax = 14
```
► **Player interaction:** on **M1**, J1's O(1,1) is stuck **last** (behind J2 and J3), finishing
at 10, so J1's second op O(1,2) cannot start until 10 → **J1 = 14**. J1 is the victim of the M1 queue.

---

#### ROUND 1 · LAYER 2 — the Sequencing Game $\mathcal{G}_2(\mathbf r)$ (routing FIXED)

The sequencing game is a **steepest descent of the potential** $\Phi=C_{\max}\cdot 10^{6}+\sum_j C_j$:
it applies the single resequence/swap of a **critical** operation that lowers $\Phi$ the most
(code: `if (f < bestFit)`), repeating to a $\Phi$-local-minimum. The critical operations are the
M1 chain O(2,1), O(3,1), O(1,1) and O(1,2).

The best move pulls **J1's O(1,1) ahead of J3's O(3,1)** on M1 (a resequence / N5 swap — a
**J1-vs-J3** interaction). It does **not** jump past J2's O(2,1): that would delay J2 and *raise*
$\sum C_j$, giving a higher $\Phi$.
```
sequencing move:  O(1,1) moves ahead of O(3,1) on M1   (J1 overtakes J3)
OSV = [ 2 , 1 , 3 , 1 , 2 ]   MAV = [ 1 , 1 , 1 , 1 , 1 ]
  M1 : O(2,1)[0,2]J2 → O(1,1)[2,5]J1 → O(3,1)[5,10]J3
  M2 : O(1,2)[5,9]J1
  M3 : O(2,2)[2,6]J2
  C = (J1:9, J2:6, J3:10),  Cmax = 14 → 10,  ΣC = 25   ✓ accepted (Φ fell)
```
► **Player interaction:** J1 **overtakes J3** on M1, dropping from 14 to 9; J3 stays last on the
saturated M1 (load 3+2+5 = 10), finishing at 10. M1 cannot finish before 10, and any further
re-ordering only raises $\sum C_j$ → **no move lowers $\Phi$.**

> **Sequencing Nash equilibrium reached:** $E_2(\mathbf r)$ with $C_{\max}=10$, $C=(9,6,10)$.

---

#### ROUND 1 · LAYER 1 — the Routing Game $\mathcal{G}_1$ (evaluated against $E_2$)

At $E_2$ the makespan-critical operation is **O(3,1)** (J3, last on the saturated M1, ends at
$10=C_{\max}$). **J3's routing best response** is to leave the congested M1 for its other machine
M2 (alt 1 → alt 2); the candidate is scored by a **single decode against the current sequence**:
```
routing move (J3):  reroute O(3,1)  M1(alt1:5) → M2(alt2:3)
```
► **Player interaction:** J3 **leaves M1** (load 10 → 5), freeing M1 for J1 and J2, while J3 now
shares **M2** with J1's O(1,2) — a **new J3-vs-J1 clash**. The move lowers $\Phi$
($C_{\max}\,10\to 9$, next), so it is accepted.

---

#### ROUND 2 · LAYER 2 — re-equilibrate the Sequencing Game for the new routing
```
MAV = [ 1 , 1 , 2 , 1 , 1 ]   (O(3,1) now on M2)      OSV = [ 2 , 1 , 3 , 1 , 2 ]
  M1 : O(2,1)[0,2]J2 → O(1,1)[2,5]J1                  (load 5 — decongested)
  M2 : O(3,1)[0,3]J3 → O(1,2)[5,9]J1                  (J3 and J1 share M2)
  M3 : O(2,2)[2,6]J2
  C = (J1:9, J2:6, J3:3),  Cmax = 9,  ΣC = 18
```
No resequence lowers $\Phi$.
> **New sequencing Nash equilibrium:** $E_2(\mathbf r')$ with $C_{\max}=9$, $C=(9,6,3)$.

---

#### ROUND 2 · LAYER 1 — the Routing Game again

At $C_{\max}=9$ the critical path is $O(2,1)[0,2]\to O(1,1)[2,5]\to O(1,2)[5,9]$ (J2 → J1 → J1);
the critical operations are O(2,1), O(1,1), O(1,2). Check each owner's routing best response
(single decode against the sequence):

| candidate reroute | effect | improves $\Phi$? |
|---|---|---|
| J1: O(1,1) M1:3 → M2:5 | longer + M2 busy (J3, J1) | ✗ |
| J1: O(1,2) M2:4 → M3:2 | pushes J2's O(2,2) → Cmax 11 | ✗ |
| J2: O(2,1) M1:2 → M3:6 | longer + M3 occupied | ✗ |

**No job can lower $\Phi$ by rerouting.**
> **Routing Nash equilibrium reached.** Neither a routing move (L1) nor a sequencing move (L2)
> improves ⇒ **subgame-perfect Nash equilibrium.**

---

#### Result

$$
\mathbf r^{*}:\ \text{MAV}=[1,1,2,1,1],\qquad \boldsymbol\pi^{*}:\ \text{OSV}=[2,1,3,1,2],\qquad
C=(9,6,3),\ C_{\max}=9.
$$

The play descended $C_{\max}:\;14 \xrightarrow{\text{L2 seq. game}} 10 \xrightarrow{\text{L1 routing game}} 9$,
alternating the two games until **each layer is a Nash equilibrium and the pair is a subgame-perfect
equilibrium** — *routing optimized over the sequencing equilibrium.* (Another valid $C_{\max}=9$
schedule is OSV=[1,2,3,1,2], $C=(7,9,3)$, used illustratively in `solution.md` / `crossover.md`;
the game's $\Phi$ tie-break picks [2,1,3,1,2] with the lower $\sum C_j=18$.)

```
Alternation with the player interactions annotated:

  s0 (Cmax 14)
     │  LAYER 2 :  J1 overtakes J3 on M1   → O(1,1) placed ahead of O(3,1)
     ▼
  E2 (Cmax 10)  ── sequencing Nash equilibrium   C=(9,6,10)
     │  LAYER 1 :  J3 leaves congested M1 → M2   (frees M1; new J3–J1 clash on M2)
     ▼
  E2'(Cmax 9)   ── new sequencing Nash equilibrium   C=(9,6,3)
     │  LAYER 1 :  no job can profitably reroute
     ▼
  SPNE (Cmax 9) ── routing NE + sequencing NE  =  subgame-perfect equilibrium
```

### 6.6 Player interaction — the two games side by side

The two layers are the **same players** competing for the **same machines**, but over **different
variables** and with **different kinds of interaction**:

| | **Layer 2 — Sequencing Game** | **Layer 1 — Routing Game** |
|---|---|---|
| Contested resource | a machine's **time slots** | the **machines** themselves |
| Player strategy | dispatch positions (OSV) | machine choice (MAV) |
| How players interact | rivals **queued on the same machine** — the earlier delays the later | rivals **loading the same machine** — one leaving frees it for the rest |
| One-player move | **resequence** an op in its precedence window | **reroute** a critical op |
| Two-player move | **N5 swap** — adjacent rivals exchange order | **mutual reroute** — rivals jointly re-pick machines |
| Acceptance | move that most lowers $\Phi$ (routing fixed) | reroute that lowers $\Phi$ **evaluated at $E_2$** (anticipation) |
| Equilibrium | **sequencing NE** $E_2(\mathbf r)$ | **routing NE** ⇒ with $E_2$ = **SPNE** |
| In the example | J1 **overtakes J3** on M1 (14→10) | J3 **leaves congested M1** → M2 (10→9) |

**In words.** *Layer 2 is the **timing** game — jobs queued on a machine fight for slot priority
(resequence / swap) until nobody can lower $\Phi$ = a sequencing Nash equilibrium. Layer 1 is the
**congestion** game — jobs fight for the machines themselves (a job leaving a loaded machine helps
its rivals) and reroute until nobody can lower $\Phi$, every reroute judged against the sequencing
equilibrium = a routing Nash equilibrium. Alternating the two until neither improves is the
subgame-perfect equilibrium.* The companion `bilevel.md` gives the same play-through with a
machine-queue diagram at every step.

### 6.7 Illustrative case study — a two-job routing game (the congestion game)

**Instance (Table 1).** Two jobs, four operations; only O₁₂ and O₂₂ are flexible:

| Job | Operation | Machine options |
|---|---|---|
| **J1** | O₁₁ | M3 : 2 |
|  | O₁₂ | **M1 : 4  or  M2 : 6** |
| **J2** | O₂₁ | M4 : 3 |
|  | O₂₂ | **M1 : 3  or  M2 : 5** |

Dispatch order (fixed): OSV = $[O_{11}, O_{21}, O_{12}, O_{22}]$. Since only O₁₂ and O₂₂ have a
choice, the **routing game** is a $2\times2$ game: **J1** (via O₁₂) and **J2** (via O₂₂) each pick
**M1 or M2**.

**Case 01 — both pick M1 (the congestion).**
```
  M3 : |O11|                          O11 M3[0,2]
  M4 : |==O21==|                      O21 M4[0,3]
  M1 :      |==== O12 ====|== O22 ==|  O12 M1[2,6] , O22 M1[6,9]   (O22 waits for O12)
  M2 :
         0    2    4    6    8   9
  C = (J1:6, J2:9),   Cmax = 9
```
O₁₂ and O₂₂ **contend for M1**: O₂₂ is ready at 3 but must wait until O₁₂ frees M1 at 6, so it
finishes at 9. This is the congestion the routing game removes.

**Detailed calculation.** The decoder starts each operation at
$$
s(O)=\max\big(\text{release}(O),\ \text{machine-free-time}\big),\qquad e(O)=s(O)+p(O),
$$
where $\text{release}(O)$ is its job-predecessor's finish. The fixed operations are
$O_{11}\!\to\!\text{M3}\,[0,2]$ and $O_{21}\!\to\!\text{M4}\,[0,3]$, so
$\text{release}(O_{12})=e(O_{11})=2$ and $\text{release}(O_{22})=e(O_{21})=3$. Then per case:

$$
\begin{aligned}
\textbf{Case 01 (M1,M1):}\ & s(O_{12})=\max(2,0)=2,\ e=6\Rightarrow C_1=6;\
                             s(O_{22})=\max(3,6)=6,\ e=9\Rightarrow C_2=9;\ C_{\max}=9.\\
\textbf{Case 02 (M1,M2):}\ & O_{12}\,\text{M1}[2,6]\Rightarrow C_1=6;\
                             s(O_{22})=\max(3,0)=3,\ e=8\Rightarrow C_2=8;\ C_{\max}=8.\\
\textbf{Case 03 (M2,M1):}\ & s(O_{12})=\max(2,0)=2,\ e=8\Rightarrow C_1=8;\
                             s(O_{22})=\max(3,0)=3,\ e=6\Rightarrow C_2=6;\ C_{\max}=8.\\
\textbf{Case 04 (M2,M2):}\ & O_{12}\,\text{M2}[2,8]\Rightarrow C_1=8;\
                             s(O_{22})=\max(3,8)=8,\ e=13\Rightarrow C_2=13;\ C_{\max}=13.
\end{aligned}
$$

**Cmax bimatrix** (each cell $=(C_1,C_2\,;\,C_{\max})$):
```
                     O22 : M1              O22 : M2
   O12 : M1     (6, 9 ; 9)  Case01      (6, 8 ; 8)  Case02
   O12 : M2     (8, 6 ; 8)  Case03      (8, 13 ; 13) Case04
```

**Payoff calculation.** With $\mathrm{own}_i=C_i$ (here $\alpha=1,\beta=\gamma=\tau=0$) and
$$
U_i=\frac{1}{\,1+w\,C_{\max}+\dfrac{\mathrm{own}_i}{1+\mathrm{own}_i}\,},\qquad w=\max(1,\delta)=1,
$$
each cell evaluates to (substituting $C_{\max}$ and $C_i$):
$$
\begin{aligned}
\textbf{Case 01:}\ & U_1=\tfrac{1}{1+9+\frac{6}{7}}=\tfrac{1}{10.857}=\mathbf{0.0921},\quad
                     U_2=\tfrac{1}{1+9+\frac{9}{10}}=\tfrac{1}{10.900}=\mathbf{0.0917};\\
\textbf{Case 02:}\ & U_1=\tfrac{1}{1+8+\frac{6}{7}}=\tfrac{1}{9.857}=\mathbf{0.1015},\quad
                     U_2=\tfrac{1}{1+8+\frac{8}{9}}=\tfrac{1}{9.889}=\mathbf{0.1011};\\
\textbf{Case 03:}\ & U_1=\tfrac{1}{1+8+\frac{8}{9}}=\mathbf{0.1011},\quad
                     U_2=\tfrac{1}{1+8+\frac{6}{7}}=\mathbf{0.1015};\\
\textbf{Case 04:}\ & U_1=\tfrac{1}{1+13+\frac{8}{9}}=\tfrac{1}{14.889}=\mathbf{0.0672},\quad
                     U_2=\tfrac{1}{1+13+\frac{13}{14}}=\tfrac{1}{14.929}=\mathbf{0.0670}.
\end{aligned}
$$

**Payoff bimatrix** (each cell $(U_1,U_2)$):
```
                     O22 : M1                O22 : M2
   O12 : M1     (0.0921 , 0.0917)       (0.1015 , 0.1011) ◄NE
   O12 : M2     (0.1011 , 0.1015) ◄NE   (0.0672 , 0.0670)
```

**Nash-equilibrium analysis** (a cell is a pure NE iff each player is already best-responding):

- **Case 01 (M1,M1) is NOT a NE.** Given O₂₂ = M1, J1's best reply is
  $\arg\max\{U_1(\text{M1,M1})=0.0921,\ U_1(\text{M2,M1})=0.1011\}=\text{M2}$
  — since $0.1011>0.0921$, **J1 deviates to M2** (it wants off the crowded M1).
- **Case 02 (M1,M2) IS a NE.** Both players best-respond:
$$
\text{J1}\mid O_{22}=\text{M2}:\ U_1(\text{M1})=0.1015>U_1(\text{M2})=0.0672\ \Rightarrow\ \text{keep M1};\quad
\text{J2}\mid O_{12}=\text{M1}:\ U_2(\text{M2})=0.1011>U_2(\text{M1})=0.0917\ \Rightarrow\ \text{keep M2}.
$$
- **Case 03 (M2,M1) IS a NE** by the symmetric argument. **Case 04** is not
  ($U_1(\text{M1})=0.1015>U_1(\text{M2})=0.0672$, so J1 deviates).
- So there are **two pure Nash equilibria — Case 02 and Case 03 — each with $C_{\max}=8$** (one job
  on M1, the other on M2). The stable payoff makes both NE also the **minimum-makespan** cells (no
  price of anarchy).

**What the routing game does.** From Case 01 ($C_{\max}=9$) the critical operation is **O₂₂**
(ends at 9); its owner **J2 reroutes it to M2**, reaching the equilibrium **Case 02**:
```
  M3 : |O11|                          O11 M3[0,2]
  M4 : |==O21==|                      O21 M4[0,3]
  M1 :      |==== O12 ====|            O12 M1[2,6]
  M2 :        |===== O22 =====|        O22 M2[3,8]
         0    2    4    6    8
  C = (J1:6, J2:8),   Cmax = 9 → 8     ◄ routing Nash equilibrium
```
> **Result.** The routing game moves **Case 01 → Case 02** (or the symmetric Case 03), lowering the
> makespan **9 → 8**. The players **avoid the crowded machine** and share the two machines — a clean
> instance of the **congestion game**, and the Nash equilibrium coincides with the minimum makespan
> (no price of anarchy under the stable payoff).

---

## 7. The move set (the neighbourhood of the games)

All moves preserve precedence-feasibility (`precedenceOK`). On a decoded schedule:

- **Reroute (routing, MAV move)** — change one operation's machine:
  $r_g \leftarrow a$ for some $a\in\{0,\dots,|E(O_g)|-1\}$.
- **Resequence (sequencing, OSV move)** — move one operation to another slot **within its
  precedence window** $[\text{predPos}+1,\ \text{succPos}-1]$ (`StrategyProfile::resequenced`).
- **Swap / critical-block N5 (sequencing, two-player)** — two adjacent *critical rival*
  operations on a machine exchange their dispatch order; the strongest makespan move.
- **Mutual reroute (routing, two-player)** — two rival operations on a machine jointly re-pick
  machines; the joint best response of the routing game.
- **Combined reroute+swap (coordinated only)** — the mover re-routes while the pair swaps
  order (a routing + sequencing move in one).

The `potential` engine searches reroute + resequence + swap + mutual + combined; the bilevel
sequencing game searches resequence + swap; the bilevel routing game searches reroute (solo) +
mutual.

---

## 8. Nash certification and the price of anarchy

### 8.1 Certifying an equilibrium
`NashChecker::countProfitableDeviations` counts profitable **unilateral routing deviations**:
for each job $j$, each operation, each alternative machine, re-route and test improvement:

- **selfish certifier** (`selfish=true`): a deviation is profitable iff the job's own payoff
  strictly rises, $U_j(s')>U_j(s)+\varepsilon$;
- **makespan certifier** (`selfish=false`): a deviation is profitable iff $C_{\max}(s')<C_{\max}(s)$.

A profile is **Nash-stable** iff the count is zero. *Scope note:* the certifier tests routing
deviations (machine reassignment); it does not test re-sequencing deviations, so it certifies
routing-stability rather than full sequencing stability — disclosed as a limitation.

### 8.2 Empirical price of anarchy
The **price of anarchy** is the efficiency loss of selfish equilibria relative to the optimum:
$$
\mathrm{PoA}=\frac{C_{\max}(\text{worst selfish Nash equilibrium})}{C_{\max}(\text{optimal})}\ \ge\ 1 .
$$
In practice it is measured as $C_{\max}(\text{selfish NE})/C_{\max}(\text{best-known})$. With
$\delta=0,\tau=0$ the selfish equilibria are inefficient ($\mathrm{PoA}>1$); the congestion
toll (Section 9) reduces it.

---

## 9. The congestion toll (novelty: a Pigouvian coordination device)

In a selfish game each job ignores the **delay externality** it imposes on rivals queued
behind it. Internalising that externality with a **Pigouvian toll** steers selfish best
responses toward the socially efficient (low-makespan) outcome and provably reduces the price
of anarchy. The toll on job $i$ is
$$
\mathrm{Toll}_i(s)=\sum_{O\in i}\ p(O)\cdot \big|\{\,g'\ :\ \mu(g')=\mu(O),\ s(g')>s(O),\ job(g')\ne i\,\}\big|,
$$
i.e. for each of job $i$'s operations, its processing time charged once for every *later*
operation of a *rival* job on the same machine (each such rival is pushed back by at least
that processing time). It is added to $\mathrm{own}_i$ with weight $\tau$ and computed only
when $\tau\ne 0$ (`PayoffFunction.cpp` lines 41–57). Comparing $\tau=0$ vs $\tau>0$ in
`selfish` mode demonstrates the toll lowering the empirical PoA.

---

## 10. The metaheuristic wrapper (global search over equilibria)

The games above are *local* operators; finding a **good** equilibrium uses a multi-start,
elite-seeded, memetic iterated local search (`StrategicCoordinationLayer::solve`).

```
Figure 8. End-to-end equilibrium-driven memetic ILS (global search over equilibria).

  ╔══════════════════ for run = 0 .. runs-1 ══════════════════╗
  ║  SEED   run 0 = RANDOM                                     ║
  ║         later = ELITE POOL (elite-pool learning) OR random ║
  ║    │                                                       ║
  ║    ▼                                                       ║
  ║  PLAY GAME to equilibrium   ┌ potential : descend         ║
  ║                             ├ selfish   : descendSelfish   ║
  ║                             └ bilevel   : playRoutingGame  ║
  ║    │                                                       ║
  ║    ▼   update incumbent by Φ                               ║
  ║  ┌──────────── ILS loop (until patience) ───────────────┐ ║
  ║  │  PERTURB :  crossover(runBest, elite)  OR  random kick │ ║
  ║  │  REPLAY  :  play game to equilibrium                  │ ║
  ║  │  KEEP    :  better endpoint; update incumbent         │ ║
  ║  └───────────────────────────┬───────────────────────────┘ ║
  ║                              ▼                             ║
  ║  elitePool.consider(run-best) ◄── elite-pool learning      ║
  ╚═══════════════════════════════════════════════════════════╝
                                │
                                ▼
   CERTIFY incumbent (NashChecker) → report  Cmax , gap vs BKS , Nash-stable
```

### 10.1 Multi-start with elite-pool seeding
For each of `runs` independent restarts:

- **Run 0** is always fully **random** (random machine per operation + random
  precedence-feasible order) — there is **no greedy/dispatch-rule construction**
  (`randomProfile`, `fillRandomSequence`).
- Later runs are seeded either from the players' **learned elite frequencies** (elite-pool
  learning) or again at random (`eliteProfile`).
- Each seed is played to an equilibrium by the chosen game engine; only **converged**
  endpoints update the incumbent (`considerIncumbent`), which keeps the profile of minimum
  $\Phi$.

### 10.2 Iterated local search (ILS)
After an equilibrium, the run is perturbed and the game replayed, keeping the best endpoint —
the *Nash game → perturb → Nash game → repeat* loop. The perturbation is either:

- a **random kick** of strength $\max(\text{kick\_min},\ N/\text{kick\_div})$ that reroutes
  and reshuffles a few operations within their windows (`RandomKick::apply`), or
- a **crossover** (memetic mode, `crossover=1`).

ILS stops after `ils_patience` $+\ N/\text{ils\_patience\_div}$ non-improving kicks.

### 10.3 Crossover operators (memetic recombination, `Crossover`)
Two elite parents (Parent 1 = the run's best equilibrium; Parent 2 = a random elite from the
pool) recombine into one **precedence-feasible** child. **Seven operators** are selectable via
`crossover_type`; the last four are the payoff-guided contributions of this work.

- **POX** — uniform crossover on routing + random-partition POX on the dispatch order.
- **OUX** — payoff-guided: each job inherits its whole strategy from the parent where its
  **own-interest cost $\mathrm{own}_j$ is lower** (using $\mathrm{own}_j$, not the makespan-aligned
  $U_i$, which is nearly identical across jobs and would collapse the recombination).
- **OOX** — order-based one-point: prefix (operations + machines) from parent 1, remainder from
  parent 2 in order.
- **PWX** — payoff-*weighted* (roulette OUX): each job inherits from a parent with probability
  $P(\text{A})=\mathrm{own}_j(B)/(\mathrm{own}_j(A)+\mathrm{own}_j(B))$ — diversity + payoff guidance.
- **RMX** — regret-matching (Hart & Mas-Colell): each job inherits from the parent where it has
  the **lower regret** (closer to its best response).
- **WGX** — welfare-guided: each job inherits from the parent where its **social cost**
  $\mathrm{own}_j+\mathrm{Toll}_j$ (private cost + Pigouvian externality) is lower.
- **CPX** — coalitional payoff: jobs sharing a machine form a coalition inherited together from
  the parent with the higher coalition payoff (preserving joint configurations).

A full numerical example of **every** operator on two parents (in OSV/MAV form, with payoff
calculations) is given in the companion `crossover.md`; the runs-50 operator ablation is in
`comparison.md`.

### 10.4 Elite-pool learning (`ElitePlay`)
The solver keeps a bounded **memory** of the best `memory` equilibria (rejecting duplicate
routings) and estimates, for each operation $g$, the empirical frequency with which each machine
alternative is used across them, with Laplace smoothing $\lambda=0.5$:
$$
\text{freq}(g,a)=\frac{\#\{\text{elites using }a\text{ for }g\}+\lambda}{|\text{pool}|+\lambda\,|E(O_g)|}.
$$
New seeds and kicks sample machines from these **elite frequencies**, so the players converge on
the machine assignments that good equilibria agree on — *elite-pool learning* (a bounded-memory
adaptation of the classical learning-from-frequency-of-play idea).

---

## 11. Convergence, optimality and complexity

- **Convergence (potential/bilevel):** every accepted move strictly decreases the integer-valued
  potential $\Phi$, which is bounded below, so each game terminates at a local minimum of $\Phi$
  (a pure-strategy equilibrium) in finitely many moves.
- **Convergence (selfish):** asynchronous (one-job-at-a-time) best response on $U_i$ converges
  to a pure Nash equilibrium; the synchronous variant can cycle and is therefore not used.
- **Optimality:** equilibria are **local** optima. The `potential`/`bilevel` modes are
  efficient (no PoA); the `selfish` mode is provably inefficient in general (PoA $>1$). No mode
  guarantees the global optimum (NP-hardness); the multi-start + elite pool + crossover drive the
  search toward it empirically.
- **Cost:** dominated by schedule decodes; each decode is $O(N\log N)$ (gap insertion with
  sorted interval lists). The critical-path restriction keeps the number of candidate moves per
  sweep small.

---

## 12. Reproducibility and experimental protocol

- Each instance uses a **deterministic seed** derived from its name, so the same configuration
  reproduces the same result.
- Parameters are read at runtime from `AlgorithmSetting.txt`; the instances to run are listed in
  `DatasetSetting.txt`; outputs (per-instance logs, `allresult.txt`, Gantt charts) are written
  under `output/`.
- **Benchmarks:** Brandimarte (mk01–mk10) and Hurink (edata/rdata/sdata/vdata); gaps reported
  against best-known makespans.
- **Recommended configurations.** Best makespan: `potential`, `runs 50`. Two-layer game:
  `bilevel`, `runs 20`. For the crossover, **PWX / OOX** led the runs-50 ablation (`comparison.md`);
  OUX is a fair baseline. Price of anarchy: `selfish`, `delta 0`, `tau 0`; toll effect:
  `selfish`, `delta 0`, `tau 0.02`.

---

## 13. End-to-end algorithm (summary pseudocode)

```
INPUT: instance (jobs, machines, operations with eligible machines/times); AlgorithmConfig
for each independent run r = 0 .. runs-1:
    seed s ← (r==0 ? random : (elitePool ready ? eliteProfile : random))
    if mode == bilevel:  playRoutingGame(s)         # routing game over sequencing game → SPNE
    elif mode == selfish: descendSelfish(s)          # unilateral + pairwise Pareto → Nash eq.
    else:                 descend(s)                 # coordinated potential game → makespan opt.
    update incumbent by Φ(s)
    repeat until ILS patience exhausted:
        s' ← crossover(runBest, elitePool.sample) or randomKick(s)
        replay the chosen game on s'
        update incumbent by Φ(s'); keep run-best
    elitePool.consider(run-best)                         # elite-pool learning
certify incumbent with NashChecker; report C_max, gap vs best-known, Nash stability
```

---

## Appendix A. Worked numerical example (the bilevel game on two jobs)

Two jobs, each one operation, contesting two machines. Eligibility (machine : time):

| Operation | M1 | M2 |
|---|---|---|
| Job 1 ($O_1$) | 6 | 5 |
| Job 2 ($O_2$) | 4 | 7 |

**Lower stage — the sequencing game $E_2$.** Only when both jobs pick the *same* machine do they
contend; the sequencing game then orders them (the faster-completing order wins). E.g. for
$(M1,M1)$ it sequences $O_2$ (time 4) before $O_1$ (time 6): $C_2=4,\ C_1=10$.

**Upper stage — the routing game $\mathcal G_1$** (each cell shows $(C_1,C_2,\;C_{\max})$ at its
sequencing equilibrium):

```
                    Job2 : M1            Job2 : M2
   Job1 : M1     ( 10 , 4 , 10 )       ( 6 , 7 , 7 )
   Job1 : M2     (  5 , 4 ,  5 ) ◄NE   ( 5 , 12 , 12 )
```

**Payoff bimatrix** under the stable payoff $U_i=1/(1+w\,C_{\max}+\mathrm{own}_i/(1+\mathrm{own}_i))$
(here $w{=}1,\ \mathrm{own}_i{=}C_i$); each cell is $(U_1,U_2;\,C_{\max})$:

```
                    Job2 : M1                   Job2 : M2
   Job1 : M1   (0.0840 , 0.0847 ; C10)      (0.1129 , 0.1127 ; C7)
   Job1 : M2   (0.1463 , 0.1471 ; C5) ◄NE   (0.0723 , 0.0718 ; C12)
```
*(e.g. cell (M2,M1): $U_1=1/(1+5+5/6)=0.1463$, $U_2=1/(1+5+4/5)=0.1471$.)*

**Reading the example.**
1. The cell $(M2,M1)$ has the **lowest makespan** $C_{\max}=5$.
2. Under the stable payoff, that same cell gives the **highest $U_1$ and highest $U_2$**
   simultaneously — so neither job wants to deviate: it is the **pure-strategy Nash equilibrium**
   of the routing game.
3. Because routing is chosen **anticipating** the sequencing equilibrium $E_2$ in every cell, the
   selected profile $\mathbf r^*=(M2,M1)$ with $\boldsymbol\pi^*=E_2(\mathbf r^*)$ is the
   **subgame-perfect equilibrium**, and $C_{\max}(E_2(\mathbf r^*))=5$ is the makespan reported.

This is the per-iteration object the solver prints for every accepted routing move
(`InstanceReport`'s routing bimatrix with the `[A]` chosen cell and the `NE` marker), so the
whole run can be audited move by move. With the stable payoff `[A]` and `NE` always coincide;
in `selfish` mode (`δ=0`) they can differ — and that gap **is** the price of anarchy.

---

## 14. Summary of the contribution

1. A **bilevel non-cooperative game** for FJSP: a global **routing game** played over a local
   **sequencing game**, solved to a **subgame-perfect equilibrium** by alternating best response
   — game theory applied on **both** decision layers.
2. A **stable, makespan-aligned lexicographic payoff** for which a lower makespan always yields a
   higher payoff for every player (proved and verified), removing the payoff/makespan
   inconsistency.
3. A **price-of-anarchy analysis** with a **Pigouvian congestion toll** that internalises the
   delay externality and drives selfish equilibria toward efficiency.
4. An **equilibrium-driven memetic algorithm** (multi-start + ILS + payoff-guided crossover +
   bounded-memory elite-pool learning) with **no greedy construction**, certified by an explicit
   **Nash checker**.

The three modes — `potential` (efficient baseline), `selfish` (price of anarchy), and `bilevel`
(the two-layer contribution) — together form a coherent game-theoretic study of the flexible
job-shop scheduling problem.

---

## Appendix B. Complete parameter reference (`AlgorithmConfig`)

Every tunable is read at runtime from `AlgorithmSetting.txt` (key value per line); defaults
reproduce the built-in behaviour.

| Key | Symbol | Default | Role / formula |
|---|---|---|---|
| `alpha` | $\alpha$ | 1.0 | completion-time weight in $\mathrm{own}_i$ |
| `beta` | $\beta$ | 0.3 | waiting-time weight in $\mathrm{own}_i$ |
| `gamma` | $\gamma$ | 0.05 | machine-conflict weight in $\mathrm{own}_i$ |
| `delta` | $\delta$ | 0.5 | makespan coupling; $w=\max(1,\delta)$; **$\delta=0$ ⇒ pure selfish** |
| `tau` | $\tau$ | 0.0 | congestion-toll weight (Section 9); $0=$ off |
| `acceptance` | — | `potential` | game mode: `potential` / `selfish` / `bilevel` |
| `crossover` | — | 1 | $1=$ memetic recombination, $0=$ random-kick ILS |
| `crossover_type` | — | `oux` | `pox` (0) / `oux` (1) / `oox` (2) |
| `runs` | $R$ | 50 | independent multi-start restarts per instance |
| `memory` | $K$ | 30 | elite-pool capacity (best equilibria kept) |
| `ils_patience` | $P_0$ | 60 | base ILS patience |
| `ils_patience_div` | $D_P$ | 4 | patience size divisor |
| `kick_min` | $\kappa_0$ | 4 | minimum kick strength |
| `kick_div` | $D_\kappa$ | 8 | kick size divisor |
| `trace_rows` | — | 2500 | accepted moves logged per instance report |

**Derived quantities** ($N=$ number of operations):

- **ILS patience** $\displaystyle P=\begin{cases}\max(6,\ \lfloor N/20\rfloor), & \text{selfish or bilevel},\\[1ex] P_0+\lfloor N/D_P\rfloor, & \text{potential}.\end{cases}$
- **Kick strength** $\kappa=\max(\kappa_0,\ \lfloor N/D_\kappa\rfloor)$; after a crossover the kick is reduced to $\max(1,\lfloor\kappa/3\rfloor)$.
- **Acceptance tolerance** $\varepsilon=10^{-9}$ for the selfish utility comparison; potential/$\Phi$ comparisons are exact integer comparisons.
- (`inertia` exists in the config for backward compatibility but is **unused** — the synchronous selfish game it controlled was replaced by asynchronous best response.)

---

## Appendix C. Input and output formats

### C.1 Instance file (`.fjs` / `.txt`) — `FjsInstanceReader::read`
- **Header line:** `numJobs numMachines [...]`. If the header holds **≥ 7** numbers the file is
  the resource-constrained (RCFJSSP) layout and the extra resource sections (buffers, utilities,
  tools, WIP, arbitrary resources) are **skipped** — only the routing/timing data is used.
- **Per job:** `numOps`, then for each operation `numAlt` followed by `numAlt` pairs
  `(machine, time)`. Machine ids are **1-based** in the file and converted to **0-based**
  internally (`machine - 1`).
- After parsing, `Instance::finalise()` builds the flat operation index
  ($g \mapsto$ job, position).

### C.2 Outputs (written under `output/`)
- **`<family>_<instance>_log.txt`** — the detailed per-instance report: header, initial random
  profile, the **CRITICAL-PATH BEST-RESPONSE ITERATIONS** table (with the **Layer** column
  `L1(SCL)` / `L2(ODL)`), the **PER-ITERATION DETAIL** with the routing **bimatrix** (each cell
  $(U_1,U_2;\,C_{\max})$, markers `[B]` before / `[A]` chosen / `NE`), per-machine two-player
  games, the final schedule, and the Nash-stability certificate. It is **streamed in real time**
  during solving, then replaced by the full report.
- **`allresult.txt`** — incremental table of $C_{\max}$ vs best-known and the gap per instance.
- **`ganchart/<instance>.svg`** — Gantt chart of the best schedule (one colour per job).
- **`README.md`, `code_explanation.md`** — auto-generated summaries.

Each instance uses a deterministic seed `hash(family/name) XOR 0x9E3779B9`, so a configuration is
exactly reproducible.

---

## Appendix D. Fine-grained mechanics (nothing skipped)

### D.1 Precedence window of a sequencing move
For operation $g$ at position $\mathrm{pos}$, owned by job $j$ at route position $p$:
$$
\text{predPos}=\begin{cases}-1,&p=0\\ \mathrm{pos}(O_{j,p-1}),&p>0\end{cases},\qquad
\text{succPos}=\begin{cases}N,&p=n_j-1\\ \mathrm{pos}(O_{j,p+1}),&\text{else}\end{cases}.
$$
The candidate target slots are $\{\text{predPos}+1,\ \text{succPos}-1,\ \mathrm{pos}-1,\ \mathrm{pos}+1\}$
intersected with the open window $(\text{predPos},\text{succPos})$ — i.e. the operation may move
anywhere that keeps it after its predecessor and before its successor (precedence preserved).

### D.2 Two-player neighbours (who plays whom)
On each machine the operations are sorted by start time. For a critical operation $u$ at machine
slot $s_u$, its rivals are the **immediate neighbours** at slots $s_u\pm 1$ that belong to a
**different job**. The "earlier" of the pair is the one with the smaller start; a swap pulls the
later one just ahead of the earlier. The mutual-reroute pass is **budgeted** to at most 24
adjacent critical rival pairs per sweep, and only pairs with $|E(O_u)|\cdot|E(O_w)|\le 36$ are
enumerated (to bound the joint search).

### D.3 Crossover operators (step by step) — `Crossover`
All three return a precedence-feasible child.

- **POX.** (1) Routing: for each operation, inherit its machine from parent A or B by a fair
  coin. (2) Sequence: choose a random subset $S$ of jobs; keep $S$-jobs' operations at parent
  A's absolute positions; fill the remaining slots with the non-$S$ operations taken from parent
  B in B's order. Feasible because A-kept jobs retain A's order and B-filled jobs retain B's order.
- **OUX (payoff-guided).** Decode both parents. For each job $j$ set
  $\text{fromA}[j]=\mathbb{1}[\,\mathrm{own}_j(s_A)\le \mathrm{own}_j(s_B)\,]$ (inherit from the
  parent where the job is individually happier). Then the child takes each job's **whole strategy**
  (machines + positions) from its chosen parent, using the same feasible interleave as POX.
- **OOX (order-based one-point).** Pick a random cut $c\in[1,N-1]$. Copy parent A's first $c$
  operations (with A's machines) as the child prefix; append the remaining operations in parent
  B's order (with B's machines), skipping those already placed. A prefix of a feasible order is
  feasible and the appended remainder keeps route order, so the child is feasible.

```
Figure 9. The three crossover operators (parents A, B → one feasible child).

  POX  routing: per-op coin A/B        sequence: random job-set keeps A's slots,
       r:  A B A B A ...  (coin)                  rest filled from B in order
       π:  [A-jobs at A-positions | B-jobs from B] 

  OUX  per JOB, pick the parent where own_j is lower (job is happier):
       fromA[j] = 1 if own_j(A) ≤ own_j(B) else 0
       child takes that job's WHOLE strategy (machines + positions) from the winner

  OOX  cut c (random):
       child = [ A.seq[0..c-1] with A's machines ] ++ [ remaining ops from B in order, B's machines ]
              └────── prefix from A ──────┘   └──────── suffix from B ────────┘
```

### D.4 Seeding cadence and ILS perturbation — `solve`
- **Seed:** run 0 is random; for run $r>0$, if the elite pool is ready and $r$ is even, seed from
  elite frequencies (`eliteProfile`), else random.
- **Perturbation:** if `crossover=1` and the elite pool is ready, recombine the run-best with a random
  elite, then apply a small kick ($\max(1,\lfloor\kappa/3\rfloor)$); otherwise copy the run-best
  and apply a full kick ($\kappa$).
- **Acceptance of a restart endpoint:** only **converged** equilibria update the global incumbent
  via `considerIncumbent` (which keeps the minimum-$\Phi$ profile). In `selfish` mode, if no run
  produces a certified Nash endpoint, a **fallback** reports the best feasible profile seen
  (flagged honestly by the Nash checker).
- **Elite-pool feedback:** each run's best is passed to `ElitePlay::consider`, which rejects
  duplicate routings and replaces the worst elite when the new one is better, then rebuilds the
  frequency map.

### D.5 Complexity (per component)
| Step | Cost |
|---|---|
| one schedule decode | $O(N\log N)$ (sorted-interval gap insertion) |
| critical-path computation | $O(N\log N)$ (sort by finish time + linear DP) |
| one sequencing-game sweep | $O(|\mathcal C|\cdot \text{decodes})$ (resequence + N5 swap on critical ops) |
| one routing-game sweep | $O(|\mathcal C|\cdot \overline{|E|}\cdot \text{decode}) + O(24\cdot\text{decode})$ (solo + mutual) |
| one full solve | $O(R\cdot(\text{game} + P\cdot\text{game}))$ over $R$ runs with ILS patience $P$ |

The makespan-critical restriction and the single-decode candidate evaluation keep each sweep
cheap; the whole Brandimarte suite (mk01–mk10) runs in under a minute in `bilevel` mode.

---

## Appendix E. Theoretical foundations and related work

The methodology rests on four established lines of game theory, each realised by a concrete
component of the solver.

| Concept | Foundational reference | Realised by |
|---|---|---|
| Non-cooperative game, **Nash equilibrium** | Nash (1950, 1951) | `descendSelfish`, `NashChecker` |
| **Potential games** (pure-NE existence, best-response convergence) | Monderer & Shapley (1996) | `PayoffFunction::globalPotential`, `descend` |
| **Congestion games** (players competing for shared resources) | Rosenthal (1973) | jobs contending for machines; the toll term |
| **Price of anarchy** (efficiency loss of selfish equilibria) | Koutsoupias & Papadimitriou (1999) | `selfish` mode + PoA measurement |
| **Pigouvian taxation / mechanism design** (internalising externalities) | Pigou (1920); Roughgarden (2005) | the congestion toll $\mathrm{Toll}_i$ |
| **Subgame-perfect / bilevel (Stackelberg) games** | Selten (1965); von Stackelberg | `playRoutingGame` over `sequencingGame` |

**Problem and benchmarks.** The flexible job-shop scheduling problem and its standard test sets:
Brandimarte (1993, the `mk` instances), Hurink–Jurisch–Thole (1994, the `edata/rdata/sdata/vdata`
families), Kacem et al. (2002). Best-known makespans are taken from the literature for the gap
column.

**Positioning of the contribution.** Game-theoretic models of scheduling exist, but to our
knowledge the **bilevel formulation — a routing game played over a sequencing game, solved to a
subgame-perfect equilibrium — together with the stable makespan-aligned payoff and the empirical
price-of-anarchy / congestion-toll analysis** is novel for the FJSP. The solver does not claim to
beat the state of the art on every instance; its contribution is the **model and its equilibrium
analysis**, with competitive makespans (several Brandimarte optima) as supporting evidence.

---

## Appendix F. Defence cheat-sheet (anticipated examiner questions)

1. **Is this cooperative or non-cooperative game theory?**
   Non-cooperative — the solution concept is Nash / subgame-perfect equilibrium; there are no
   coalitions, side-payments, Shapley values or the core. (`potential`/`bilevel` are
   *identical-interest* non-cooperative games; `selfish` is a competitive one.)

2. **How is the bilevel game different from ordinary local search over routing + sequencing?**
   By **anticipation/hierarchy**: routing best-responds against the *induced sequencing
   equilibrium* $E_2(\mathbf r)$, yielding a **subgame-perfect** equilibrium. In a flat game a
   reroute and a resequence are just two moves in one neighbourhood — no leader–follower structure.

3. **Does it guarantee the global optimum?**
   No. FJSP is NP-hard; equilibria are *local* optima. `potential`/`bilevel` are efficient
   (no price of anarchy), `selfish` is provably inefficient. Multi-start + elite pool + crossover
   drive the search toward the optimum empirically (3 Brandimarte optima).

4. **What exactly is the price of anarchy here, and how do you measure it?**
   $\mathrm{PoA}=C_{\max}(\text{selfish NE})/C_{\max}(\text{optimal})$; measured by running
   `selfish` with $\delta=0$ and comparing its certified-NE makespan to best-known. The toll
   $\tau>0$ reduces it.

5. **Why the stable lexicographic payoff — and is it still a game?**
   Because an additive payoff let a lower-makespan profile score a *lower* $U_i$, so the bimatrix
   NE disagreed with the chosen cell. The lexicographic form makes a lower $C_{\max}$ always raise
   every $U_i$ (proved). It remains a non-cooperative **potential game** (each player's payoff is
   its marginal contribution to $-\Phi$).

6. **What is the novelty?** (a) the bilevel routing-over-sequencing game and its SPNE; (b) the
   stable makespan-aligned payoff; (c) the empirical PoA study with a Pigouvian congestion toll;
   (d) an equilibrium-driven memetic algorithm with **no greedy construction**.

7. **How do you certify a Nash equilibrium?** `NashChecker` counts profitable unilateral routing
   deviations; zero ⇒ Nash-stable. (Limitation: routing deviations only, disclosed.)

8. **Why no greedy/dispatch-rule construction?** To keep the search *purely* equilibrium-driven —
   seeds are random or elite-sampled; the Nash game, elite pool and crossover do all the work. This
   isolates the game-theoretic mechanism as the source of quality.

9. **What is the complexity / why is it fast?** One decode is $O(N\log N)$; moves are restricted to
   the critical path and each routing candidate is a single decode against the current sequencing
   equilibrium (alternating, not nested), so the full Brandimarte suite runs in under a minute.

10. **Why not state-of-the-art on every instance (e.g. mk06, mk10)?** Those instances are hard for
    *every* heuristic (NP-hardness); even the coordinated engine has large gaps there. The claim is
    a **novel game-theoretic model with competitive makespans and a principled equilibrium
    analysis**, not a new makespan record.
