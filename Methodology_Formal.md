# Methodology: A Job-as-Player Non-Cooperative Game for the Flexible Job Shop Scheduling Problem

**Method name — JC-NCGS** (*Job-Conflict Non-Cooperative Game Scheduling*).

This chapter specifies, in a self-contained and reproducible way, the model and
algorithm implemented in this project. The Flexible Job Shop Scheduling Problem
(FJSP) is formulated as a **non-cooperative game among jobs** and solved by
**two-player interaction-driven best-response dynamics** embedded in a multi-start,
belief-learning search. All notation is collected in Section 12.

---

## 1. Overview

Each **job is an autonomous, self-interested player**; the **machines** are scarce,
unit-capacity resources the players compete for. A player's **strategy** is how it
routes its operations to machines and where those operations sit in the global
dispatch order. Players are scored by a **hybrid payoff** $U_i$ that blends each
job's private experience (completion, waiting, congestion) with the shared
makespan. The solver drives the players toward a **Nash-stable** schedule through
interactions between rival jobs on the bottleneck machine, and the makespan
$C_{\max}$ emerges as the equilibrium outcome.

---

## 2. Problem Formulation

### 2.1 Sets and data

$$
\mathcal{J}=\{1,\dots,\ell\}\ \text{(jobs)},\qquad
\mathcal{M}=\{1,\dots,m\}\ \text{(machines)},\qquad
\Omega=\{1,\dots,n\}\ \text{(all operations)}.
$$

Job $j$ has an ordered route of operations $O_{j,1}\prec O_{j,2}\prec\cdots\prec
O_{j,n_j}$. Each operation $i\in\Omega$ has a set of **eligible machines**
$E(i)\subseteq\mathcal{M}$ and a processing time

$$
p(i,r)\in\mathbb{Z}_{>0}\quad\text{for each } r\in E(i).
$$

A problem is a *flexible* job shop because $|E(i)|$ may exceed $1$: a solution must
choose **both** the machine and the timing of every operation.

### 2.2 Decision variables

For every operation $i\in\Omega$:

$$
\mu_i \in E(i)\ \text{(assigned machine)},\qquad
S_i \ge 0\ \text{(start time)},\qquad
C_i = S_i + p(i,\mu_i)\ \text{(completion)}.
$$

Job completion and the social objective (makespan):

$$
C_j \;=\; C_{O_{j,n_j}},\qquad
C_{\max} \;=\; \max_{j\in\mathcal{J}} C_j .
$$

### 2.3 Constraints

$$
\textbf{(Precedence)}\quad S_{O_{j,k}} \;\ge\; C_{O_{j,k-1}},\qquad k=2,\dots,n_j,
$$

$$
\textbf{(Capacity)}\quad [S_i,C_i)\cap[S_{i'},C_{i'})=\varnothing
\quad\text{whenever } \mu_i=\mu_{i'},\ i\neq i',
$$

$$
\textbf{(Routing)}\quad \mu_i\in E(i),\qquad
\textbf{(No preemption)}\quad \text{each } i \text{ runs without interruption.}
$$

### 2.4 Objective

$$
\boxed{\;\min\; C_{\max}\;}
$$

This is the FJSP of Brandimarte (1993) and Hurink et al. (1994). RCFJSSP instances
are parsed for routing/timing only; additional resource constraints are **not**
enforced, so on those instances the relaxed (unlimited-resource) makespan is
targeted.

---

## 3. Game-Theoretic Model

The FJSP is cast as the strategic-form game

$$
\mathcal{G} \;=\; \bigl(\mathcal{J},\ \{S_i\}_{i\in\mathcal{J}},\ \{U_i\}_{i\in\mathcal{J}},\ \mathcal{C}\bigr),
$$

with **players** $\mathcal{J}$ (the jobs), **strategy sets** $S_i$, **payoffs**
$U_i$, and a **conflict relation** $\mathcal{C}$ (Section 6). Machines are *not*
players — they are the contested resources over which the players' conflicts are
resolved.

### 3.1 Player strategy

The strategy of job $i$ specifies, for each of its operations $O_{i,k}$, a machine
and a sequencing/priority decision:

$$
s_i \;=\; \bigl(\,\mu_{O_{i,k}},\ \pi_{O_{i,k}}\,\bigr)_{k=1}^{n_i}\;\in\;S_i,
$$

where $\mu_{O_{i,k}}\in E(O_{i,k})$ is the **machine choice** and
$\pi_{O_{i,k}}$ is the operation's **position** in the global dispatch order
(its priority). A **strategy profile** is the joint choice of all players,

$$
s=(s_1,\dots,s_\ell),\qquad s_{-i}=(s_1,\dots,s_{i-1},s_{i+1},\dots,s_\ell).
$$

---

## 4. Solution Representation

A strategy profile is encoded as the two aligned vectors $X=(\mathrm{OSV},\mathrm{MAV})$:

- **OSV — Operation Sequence Vector** $\sigma$: a permutation of all $n$ operations
  giving the order a list scheduler dispatches them. It must be
  **precedence-feasible**:

$$
\sigma^{-1}\!\bigl(O_{j,k-1}\bigr) < \sigma^{-1}\!\bigl(O_{j,k}\bigr)\qquad \forall j,\ \forall k\ge 2 .
$$

- **MAV — Machine Assignment Vector** $a$: for each operation, the index of the
  chosen eligible alternative, i.e. $\mu_i = E(i)[\,a_i\,]$.

Every move preserves precedence-feasibility, so the profile always decodes into a
feasible schedule. *(In code: `StrategyProfile.sequence` = OSV,
`StrategyProfile.routing` = MAV.)*

---

## 5. Decoder: the Active Schedule Builder (the "referee")

A profile is meaningless until decoded into a timed schedule. The decoder is an
**active, gap-insertion list scheduler**. Processing operations in OSV order, it
places operation $i$ of job $j$ on machine $r=\mu_i$ at the earliest feasible idle
gap at or after the job's release time:

$$
R_i \;=\; C_{\,\text{prev op of } j}\quad(R_i=0 \text{ for the first operation}),
$$

$$
S_i \;=\; \min\Bigl\{\,t \ge R_i \;:\; [\,t,\ t+p(i,r)\,]\ \text{fits an idle gap of machine } r \,\Bigr\}.
$$

Because the result is an **active schedule** (no operation can start earlier without
delaying another), its longest path equals $C_{\max}$, which makes the critical-path
analysis of Section 6 exact. *(In code: `ScheduleBuilder::build`.)*

---

## 6. Interaction, Conflict and the Critical Path

### 6.1 Conflict relation

Two jobs interact **indirectly through a shared machine**. Jobs $a$ and $b$ are in
conflict if two of their operations are dispatched on the same machine in adjacent
time order:

$$
\mathcal{C}_{ab}=
\begin{cases}
1, & \exists\, i\in a,\ i'\in b\ \text{adjacent on the same machine},\\[2pt]
0, & \text{otherwise.}
\end{cases}
$$

Granting one job an earlier slot delays its rival; thus an improvement for one
player is frequently a loss for another — a **negative externality**, which is the
engine that hands the bottleneck from job to job as the makespan falls.

### 6.2 Critical operations

In the active schedule, define the **tail** (longest path from an operation to the
sink) recursively over the job-successor and machine-successor:

$$
\mathrm{tail}(i) \;=\; p(i,\mu_i) \;+\; \max\bigl(\,\mathrm{tail}(\mathrm{jobSucc}\,i),\ \mathrm{tail}(\mathrm{machSucc}\,i)\,\bigr),
$$

with $\mathrm{tail}(\cdot)=0$ past the sink. An operation is **critical** iff it lies
on a longest path:

$$
i \ \text{is critical} \iff S_i + \mathrm{tail}(i) = C_{\max}.
$$

Only critical operations can change $C_{\max}$. The owner of a critical operation is
the **mover**; the job whose operation is its immediate machine neighbour is the
**rival**. *(In code: `GameSolver::criticalOperations`.)*

---

## 7. The Payoff Function

There is a single payoff used for every player and instance. For job $i$ define:

$$
C_i = \text{completion},\qquad
W_i = C_i - \sum_{k=1}^{n_i} p\bigl(O_{i,k},\mu_{O_{i,k}}\bigr)\ \ (\text{waiting}),
$$

$$
\mathrm{Conf}_i = \sum_{k=1}^{n_i}\ \mathrm{load}\bigl(\mu_{O_{i,k}}\bigr),
\qquad
\mathrm{load}(r) = \!\!\sum_{i'\,:\,\mu_{i'}=r}\!\! p(i',r),
$$

i.e. $\mathrm{Conf}_i$ sums, over the job's operations, the total processing booked
on each chosen machine (a busier machine ⇒ more contention). The **cost** and
**payoff** are

$$
\mathrm{cost}_i \;=\; \alpha\,C_i + \beta\,W_i + \gamma\,\mathrm{Conf}_i + \delta\,C_{\max},
\qquad
\boxed{\,U_i \;=\; \dfrac{1}{1+\mathrm{cost}_i}\,}
$$

with weights, as implemented,

$$
\alpha = 1.0,\quad \beta = 0.3,\quad \gamma = 0.05,\quad \delta = 0.5.
$$

Since $U_i\in(0,1]$, payoffs are comparable across jobs and instances. The term
$\delta\,C_{\max}$ is a **shared global coupling** that ties each player's payoff to
the social objective. *(In code: `PayoffFunction::forPlayer`.)*

---

## 8. Equilibrium, Best Response and the Acceptance Potential

### 8.1 Best response and Nash stability

The best response of player $i$ to the others' fixed strategies is

$$
\mathrm{BR}_i(s_{-i}) \;=\; \arg\max_{s_i'\in S_i}\; U_i\bigl(s_i',\,s_{-i}\bigr).
$$

A profile $s^\*$ is a **pure-strategy Nash equilibrium** if no player can improve
unilaterally:

$$
U_i\bigl(s_i^\*,\,s_{-i}^\*\bigr)\ \ge\ U_i\bigl(s_i,\,s_{-i}^\*\bigr)
\qquad \forall i\in\mathcal{J},\ \forall s_i\in S_i .
$$

The method strengthens this to **pairwise Nash stability**: no single job *and no
rival pair of jobs* can jointly improve.

### 8.2 Global acceptance potential

To guarantee monotone makespan reduction and termination, a deviation is **accepted
only if it lowers a makespan-dominated global potential** $\Phi$:

$$
\Phi(s) \;=\; C_{\max}(s)\cdot 10^{6} \;+\; \sum_{j\in\mathcal{J}} C_j(s).
$$

Makespan dominates lexicographically; total completion breaks ties toward balanced,
more improvable schedules. Because $\Phi$ is bounded below and strictly decreases on
every accepted move, the descent terminates at a $\Phi$-local optimum that is also
(pairwise) Nash-stable. *(In code: `PayoffFunction::fitness`; the per-job payoff
$U_i$ of Section 7 is computed for analysis and reporting — e.g. the bimatrix games
of Section 9.3 — while $\Phi$ governs acceptance.)*

---

## 9. The Two-Player Interaction Game (core of the method)

Rather than only unilateral moves, **every critical operation first plays a
two-player game** against its machine-neighbour rival; a single-job move is used
only as a fallback.

### 9.1 The three joint responses

Let $u$ be a critical (mover) operation of job $i$ on machine $r$, and $w$ its
machine-neighbour operation of rival job $i'$.

**(R2) Order swap — sequence game.** The two rivals exchange order on machine $r$
(the later operation is pulled ahead of the earlier):

$$
\sigma' = \mathrm{swap\_order}(\sigma; u, w),\qquad a' = a .
$$

**(R3) Mutual reroute — routing game.** Both rivals jointly re-pick machines, moving
to the **joint best response** over their $|E(u)|\times|E(w)|$ payoff bimatrix:

$$
\bigl(a'_u, a'_w\bigr) = \arg\min_{(b,c)}\ \Phi\bigl(s \mid \mu_u\!=\!E(u)[b],\,\mu_w\!=\!E(w)[c]\bigr),\qquad \sigma'=\sigma .
$$

(Bounded per sweep to at most $24$ pairs and $|E(u)|\,|E(w)|\le 36$ cells.)

**(R4) Combined reroute + resequence.** The mover **re-routes while the pair swaps
order** — both dimensions change in one joint move:

$$
a'_u = b\ (b\neq a_u),\qquad \sigma' = \mathrm{swap\_order}(\sigma; u, w).
$$

This expresses the *"I yield the machine, you take the earlier slot"* bargain that
neither a pure swap nor a pure reroute alone can represent.

### 9.2 Move selection and stopping rule

Across all critical operations and all three responses, the single deviation with
the largest decrease in $\Phi$ is applied. When no two-player response improves
$\Phi$, a **single-job fallback** is tried (re-route the critical op to another
eligible machine, or re-sequence it within its precedence window). The descent
stops at a **pairwise Nash-stable** profile:

$$
\Delta_i = 0\ \ \forall i \quad\text{and}\quad \Delta_{\{i,i'\}} = 0\ \ \forall\,\mathcal{C}_{ii'}=1,
$$

where $\Delta$ denotes the best achievable $\Phi$-decrease for that player or pair.
*(In code: `GameSolver::descend`, move kinds $2,3,4$ plus the solo bucket.)*

### 9.3 The game made explicit (reporting)

For each contested machine the per-instance log prints the **normal-form bimatrix**
$\bigl(U_i,U_{i'}\bigr)$ over the rivals' eligible machines, marks the cell actually
played, and marks **every pure-strategy Nash equilibrium**, so the game each
bottleneck encodes is directly inspectable. *(In code:
`InstanceReport::writePairGame`.)*

### 9.4 Worked numerical example (two jobs, one bimatrix, every step)

**Instance.** Two jobs, four machines. Each job has a fixed first operation on its
own dedicated machine and a **flexible second operation** that competes for the
shared machines $M_1,M_2$.

| Job | Op | Eligible (machine : time) |
|---|---|---|
| $J_1$ | $O_{11}$ | $M_3 : 2$ (dedicated) |
| $J_1$ | $O_{12}$ | $M_1 : 4$ **or** $M_2 : 6$ ← strategy |
| $J_2$ | $O_{21}$ | $M_4 : 3$ (dedicated) |
| $J_2$ | $O_{22}$ | $M_1 : 3$ **or** $M_2 : 5$ ← strategy |

Dispatch order (OSV): $[\,O_{11},O_{21},O_{12},O_{22}\,]$; releases $O_{12}$ at
$t=2$, $O_{22}$ at $t=3$. The two-player game: $J_1$ routes $O_{12}$, $J_2$ routes
$O_{22}$ — a $2\times2$ bimatrix.

**Step 1 — decode + payoff for cell $(M_1,M_1)$.**

| Op | Machine | Release | Start | Finish |
|---|---|---|---|---|
| $O_{11}$ | $M_3$ | 0 | 0 | 2 |
| $O_{21}$ | $M_4$ | 0 | 0 | 3 |
| $O_{12}$ | $M_1$ | 2 | 2 | 6 |
| $O_{22}$ | $M_1$ | 3 | 6 | 9 |

$C_1=6,\ C_2=9,\ C_{\max}=9$. Processing totals $6,6$; $W_1=0,\ W_2=3$. Loads
$\mathrm{load}(M_1)=7,\ \mathrm{load}(M_3)=2,\ \mathrm{load}(M_4)=3$; conflicts
$\mathrm{Conf}_1=2+7=9,\ \mathrm{Conf}_2=3+7=10$.

$$
\mathrm{cost}_1=6+0+0.05(9)+0.5(9)=10.95\Rightarrow U_1=\tfrac1{11.95}=0.0837,
$$
$$
\mathrm{cost}_2=9+0.3(3)+0.05(10)+0.5(9)=14.90\Rightarrow U_2=\tfrac1{15.90}=0.0629.
$$

**Step 2 — the other three cells (same procedure).**

| Cell | $C_1,C_2,C_{\max}$ | $\mathrm{Conf}_1,\mathrm{Conf}_2$ | $U_1,\ U_2$ |
|---|---|---|---|
| $(M_1,M_1)$ | $6,9,9$ | $9,10$ | $0.0837,\ 0.0629$ |
| $(M_1,M_2)$ | $6,8,8$ | $6,8$ | $0.0885,\ 0.0746$ |
| $(M_2,M_1)$ | $8,6,8$ | $8,6$ | $0.0746,\ 0.0885$ |
| $(M_2,M_2)$ | $8,13,13$ | $13,14$ | $0.0619,\ 0.0441$ |

**Step 3 — the Nash bimatrix** (each cell $U_1,U_2$; $C_{\max}$ below):

| $J_1\backslash J_2$ | $J_2:M_1$ | $J_2:M_2$ |
|---|---|---|
| $J_1:M_1$ | $(0.0837,0.0629)$, $C_{\max}=9$ | $(0.0885,0.0746)$, $C_{\max}=8$ **★NASH** |
| $J_1:M_2$ | $(0.0746,0.0885)$, $C_{\max}=8$ | $(0.0619,0.0441)$, $C_{\max}=13$ |

**Step 4 — best responses.** $J_1$: prefers $M_1$ in both columns
($0.0837>0.0746$; $0.0885>0.0619$) ⇒ $M_1$ dominant. $J_2$: best is $M_2$ when
$J_1=M_1$ ($0.0746>0.0629$), best is $M_1$ when $J_1=M_2$ ($0.0885>0.0441$). The
unique pure Nash equilibrium is $(M_1,M_2)$ with $C_{\max}=8$ — which is also the
minimum makespan: **here selfish stability and social optimum coincide.**

**Step 5 — dynamics.** From random start $(M_2,M_2)$ ($C_{\max}=13$): $J_1$
best-responds $M_2\!\to\!M_1$ → $(M_1,M_2)$, $C_{\max}=8$; $J_2$ already optimal.
Equilibrium reached; makespan fell $13\to8$ by selfish moves alone.

### 9.5 When selfish play and the makespan optimum diverge (price of anarchy)

The coupling term $\delta\,C_{\max}$ is what *aligns* private and social incentives.
Removing it (or the congestion term $\gamma\,\mathrm{Conf}$) exposes a **price of
anarchy** $\mathrm{PoA}=C_{\max}(\text{worst Nash})/C_{\max}(\text{optimum})>1$.

**Instance.** Three single-operation jobs, machines $M_1,M_2$, OSV
$[\,J_1,J_2,J_3\,]$:

$$
J_1:\{M_1\!:\!2,\ M_2\!:\!2\},\qquad J_2:\{M_1\!:\!3,\ M_2\!:\!8\},\qquad J_3:\{M_1\!:\!3,\ M_2\!:\!8\}.
$$

$J_2$ strongly prefers $M_1$ (dominant), so fix $J_2\!\to\!M_1$ and read the
**$J_1$-vs-$J_3$ bimatrix**. Decoding the four cells:

| Cell $(J_1,J_3)$ | $C_1,C_2,C_3,C_{\max}$ |
|---|---|
| $(M_1,M_1)$ | $2,5,8,\ \mathbf{8}$ |
| $(M_1,M_2)$ | $2,5,8,\ \mathbf{8}$ |
| $(M_2,M_1)$ | $2,3,6,\ \mathbf{6}$ |
| $(M_2,M_2)$ | $2,3,10,\ \mathbf{10}$ |

**Social optimum:** $(M_2,M_1)$ with $C_{\max}=6$ ($J_1$ steps aside to $M_2$ so
$J_2,J_3$ share $M_1$). This is provably optimal (any split forces an $M_2$ op of
length $8$).

**(a) Pure completion-time payoff** ($\gamma=\delta=0$, i.e. $U_i=1/(1+C_i)$). Since
$C_1=2$ in *every* cell, **$J_1$ is completely indifferent**:

| $J_1\backslash J_3$ | $J_3:M_1$ | $J_3:M_2$ |
|---|---|---|
| $J_1:M_1$ | $(0.333,0.111)$, $C_{\max}=8$ **NASH (weak)** | $(0.333,0.111)$, $C_{\max}=8$ |
| $J_1:M_2$ | $(0.333,0.143)$, $C_{\max}=6$ **NASH** | $(0.333,0.091)$, $C_{\max}=10$ |

$J_3$ always prefers $M_1$ (its own completion is lower). Because $J_1$ has no
incentive to vacate $M_1$, the inefficient profile $(M_1,M_1)$ with $C_{\max}=8$ is
a (weak) Nash equilibrium alongside the efficient $(M_2,M_1)$. **Price of anarchy
$=8/6\approx1.33$:** purely completion-selfish jobs can rationally settle on a
$33\%$-worse schedule.

**(b) The implemented payoff** ($\gamma=0.05,\ \delta=0.5$). Now the loads and the
shared makespan enter $J_1$'s cost and **break its indifference**. Recomputing
$U_1=1/(1+C_1+0.05\,\mathrm{Conf}_1+0.5\,C_{\max})$:

| $J_1\backslash J_3$ | $J_3:M_1$ | $J_3:M_2$ |
|---|---|---|
| $J_1:M_1$ | $(0.1351,0.0746)$, $C_{\max}=8$ | $(0.1379,0.0746)$, $C_{\max}=8$ |
| $J_1:M_2$ | $(0.1639,0.0971)$, $C_{\max}=6$ **★NASH** | $(0.1176,0.0606)$, $C_{\max}=10$ |

Now $J_1$ *strictly* prefers $M_2$ whenever $J_3$ is on $M_1$ ($0.1639>0.1351$),
because the lower makespan ($6$ vs $8$) and lighter load reduce its own cost. The
unique strict Nash equilibrium is $(M_2,M_1)$ with $C_{\max}=6$ — **the optimum;
$\mathrm{PoA}=1$.**

**Interpretation.** The $\gamma\,\mathrm{Conf}_i$ and $\delta\,C_{\max}$ terms are
*coordination devices*: they internalise the externality a job imposes on the shop,
so that the selfish best response also lowers the makespan. This is precisely why
the method, despite letting jobs act in their own interest, drives the makespan
toward the best-known values rather than toward anarchic equilibria.

### 9.6 All player strategies and interactions on the running example

We reuse the two-job instance of §9.4 and start from the **congested profile**
$(M_1,M_1)$ — both flexible operations routed to $M_1$ — because it puts the two
players into direct conflict, exposing every interaction and every move.

**Starting state $\sigma_0=(M_1,M_1)$.**

| Op | Machine | Start | Finish |
|---|---|---|---|
| $O_{11}$ | $M_3$ | 0 | 2 |
| $O_{21}$ | $M_4$ | 0 | 3 |
| $O_{12}$ | $M_1$ | 2 | 6 |
| $O_{22}$ | $M_1$ | 6 | 9 |

$C_1=6,\ C_2=9,\ C_{\max}=9,\ \sum_j C_j=15$, so $\Phi(\sigma_0)=9\cdot10^6+15$.

#### (A) The players' strategies (concrete)

$$
\textbf{MAV}=\bigl(\,O_{11}\!\to\!M_3,\ O_{12}\!\to\!M_1,\ O_{21}\!\to\!M_4,\ O_{22}\!\to\!M_1\,\bigr),
\qquad
\textbf{OSV}=[\,O_{11},O_{21},O_{12},O_{22}\,].
$$

- **Strategy of player $J_1$:** $\;s_1=\bigl(\mu_{O_{12}}=M_1,\ \pi_{O_{12}}=\text{slot }3\bigr)$. Its **strategy set** is $S_1=\{M_1,M_2\}\times\{\text{legal positions of }O_{12}\}$.
- **Strategy of player $J_2$:** $\;s_2=\bigl(\mu_{O_{22}}=M_1,\ \pi_{O_{22}}=\text{slot }4\bigr)$, with $S_2=\{M_1,M_2\}\times\{\text{legal positions of }O_{22}\}$.

(The dedicated first operations $O_{11},O_{21}$ are not strategic — single eligible
machine — so each player's decision reduces to *which machine and which queue slot
for its flexible operation*.)

#### (B) The six interactions, with numbers from $\sigma_0$

| # | Interaction | Concrete evidence in $\sigma_0$ |
|---|---|---|
| 1 | **Machine-slot competition** | $O_{12}$ and $O_{22}$ both demand $M_1$; capacity 1 grants $O_{12}$ the slot $[2,6]$ and pushes $O_{22}$ to $[6,9]$. |
| 2 | **Waiting-time conflict** | $O_{22}$ is ready at $t=3$ but cannot start until $6$ (waits for $O_{12}$): $W_2=C_2-\text{proc}_2=9-6=3$. $J_1$'s choice directly created $J_2$'s wait. |
| 3 | **Machine congestion** | $\mathrm{load}(M_1)=4+3=7$ (the busiest machine); both feel it: $\mathrm{Conf}_1=2+7=9$, $\mathrm{Conf}_2=3+7=10$. |
| 4 | **Sequence conflict** | $O_{12}$ precedes $O_{22}$ in the OSV, so $J_1$ is dispatched first; reordering would shift *both* jobs' start times (see move R2). |
| 5 | **Priority competition** | The earlier $M_1$ slot (finishing sooner) is contested; the OSV currently grants priority to $J_1$. $J_2$ would prefer it. |
| 6 | **Critical-machine rivalry** | $M_1$ is the bottleneck: $O_{22}$ on $M_1$ ends at $9=C_{\max}$. The two jobs clash precisely on the machine that fixes the makespan. |

The mover is the critical operation $O_{22}$ (job $J_2$); its **machine neighbour**
is $O_{12}$ (job $J_1$). The pair $(O_{22},O_{12})$ is what the three responses act
on.

#### (C) The three player moves (best responses), each computed

**R2 — Order swap (OSV only).** Pull $O_{22}$ ahead of $O_{12}$ on $M_1$:
$\text{OSV}\to[\,O_{11},O_{21},O_{22},O_{12}\,]$, MAV unchanged.

| Op | Machine | Start | Finish |
|---|---|---|---|
| $O_{22}$ | $M_1$ | 3 | 6 |
| $O_{12}$ | $M_1$ | 6 | 10 |

$C_1=10,\ C_2=6,\ C_{\max}=10$. **Worse** ($9\to10$): the swap alone just hands the
bottleneck to $J_1$. **Rejected** ($\Phi$ rises).

**R3 — Mutual reroute (MAV only).** Evaluate the routing bimatrix (§9.4) and take
the joint best response; here $J_2$ leaves the contested machine, $O_{22}\!\to\!M_2$
(OSV unchanged):

| Op | Machine | Start | Finish |
|---|---|---|---|
| $O_{12}$ | $M_1$ | 2 | 6 |
| $O_{22}$ | $M_2$ | 3 | 8 |

$C_1=6,\ C_2=8,\ C_{\max}=8$. **Improves** ($9\to8$). $U_1:0.0837\to0.0885$,
$U_2:0.0629\to0.0746$ — *both players gain*, the congestion on $M_1$ dissolved.

**R4 — Combined reroute + resequence (MAV *and* OSV).** $O_{22}\!\to\!M_2$ **and**
swap the pair's order, $\text{OSV}\to[\,O_{11},O_{21},O_{22},O_{12}\,]$:

| Op | Machine | Start | Finish |
|---|---|---|---|
| $O_{22}$ | $M_2$ | 3 | 8 |
| $O_{12}$ | $M_1$ | 2 | 6 |

$C_1=6,\ C_2=8,\ C_{\max}=8$. **Improves** ($9\to8$); same quality as R3 here.

**Selection.** The solver scores all three by $\Phi=C_{\max}\cdot10^6+\sum_j C_j$:

| Move | $C_{\max}$ | $\sum_j C_j$ | $\Phi$ | verdict |
|---|---|---|---|---|
| R2 swap | 10 | 16 | $10{,}000{,}016$ | rejected (worse) |
| R3 mutual reroute | 8 | 14 | $8{,}000{,}014$ | **accepted** |
| R4 reroute+resequence | 8 | 14 | $8{,}000{,}014$ | **accepted** |

It applies the best improving move (R3 or R4, tied here) and reaches the Nash /
optimum $(M_1,M_2)$ with $C_{\max}=8$. The single-job **fallback** (only $J_2$
re-routing $O_{22}\!\to\!M_2$, no swap) coincides with R3 in this instance, so it is
never needed — the two-player interaction already resolved the clash. The lesson:
the **swap alone is harmful, but combined with a reroute the same reordering becomes
useful** — which is exactly why the combined move R4 is in the repertoire.

---

## 10. Outer Optimisation Layer

A single game converges to *a* Nash-stable schedule, not necessarily the global
optimum. A multi-start layer with learning searches for the best one.

### 10.1 Multi-start and seed cycle

$R=\texttt{totalRun}=50$ independent runs are performed (override via `FJS_RUNS`):

- **Run $0$** — fully **random** profile (the required random initialisation);
- **Run $1$** — **task-pool** constructor (Section 10.2);
- **Later runs** cycle through **belief**, **Global** greedy, **Local** greedy,
  **task-pool**, and **random** seeds.

### 10.2 Task-pool constructor (Giffler–Thompson, earliest completion)

At each step, over every ready operation and its eligible machines, dispatch the
pair that **completes earliest**:

$$
(i^\*,r^\*) = \arg\min_{i\ \text{ready},\ r\in E(i)}\ \Bigl[\ \max\bigl(\mathrm{free}(r),\ \mathrm{ready}(\mathrm{job}\,i)\bigr) + p(i,r)\ \Bigr].
$$

This grows the OSV and fixes the MAV one slot at a time, yielding an active,
congestion-aware basin. *(In code: `TaskPool::build`.)*

### 10.3 Iterated Local Search (ILS)

Each run repeatedly perturbs its incumbent by
$\kappa=\texttt{kickStrength}=\max(4,\lfloor n/8\rfloor)$ random re-routes/
re-sequences (re-routes biased by beliefs, Section 10.4) and re-descends, keeping
the run's best, until

$$
\texttt{ILS\_PATIENCE} = 60 + \lfloor n/4 \rfloor
$$

consecutive kicks fail to improve. *(In code: `RandomKick::apply`.)*

### 10.4 Belief learning — fictitious play

A pool of the best **distinct** equilibria is maintained. For every operation $i$
and machine $r$, the belief is the Laplace-smoothed empirical frequency of that
assignment across the elite pool:

$$
\mathrm{belief}(i,r) \;=\; \frac{\#\{\text{elites routing } i \text{ to } r\} + \lambda}{|\text{pool}| + \lambda\,|E(i)|},\qquad \lambda = 0.5 .
$$

New runs and ILS kicks sample routings from $\mathrm{belief}(i,\cdot)$, so players
converge on the machine assignments good solutions agree upon. The beliefs do **not**
alter the payoff. *(In code: `BeliefModel`.)*

### 10.5 Greedy seeds

- **Global**: $\ \mu_i=\arg\min_{r\in E(i)}\bigl[\mathrm{load}(r)+p(i,r)\bigr]$ (load balancing).
- **Local**: $\ \mu_i=\arg\min_{r\in E(i)} p(i,r)$ (shortest processing time).

### 10.6 Selection of the reported solution

After all runs, the schedule with the smallest $\Phi$ (hence smallest $C_{\max}$,
ties broken by $\sum_j C_j$) is reported and compared with the best-known value.

---

## 11. Algorithm and Complexity

### 11.1 Pseudocode

```
JC-NCGS(instance):
  belief ← ∅ ;  best ← ∞
  for run = 0 … R-1:
     state ← seed(run, belief)                      # random | task-pool | belief | global | local
     descend(state)                                 # two-player-first best response → pairwise Nash
     runBest ← state ;  stagnant ← 0
     while stagnant < ILS_PATIENCE:                  # iterated local search to convergence
        work ← kick(runBest, belief, κ)
        descend(work)
        if Φ(work) < Φ(runBest):  runBest ← work ; stagnant ← 0
        else                      stagnant ← stagnant + 1
     best ← min_Φ(best, runBest)
     belief.consider(runBest)                        # fictitious-play update
  return best

descend(state):
  repeat:
     decode state ;  find critical ops + machine-neighbour rivals
     candidates ← { R2 swap, R3 mutual reroute, R4 reroute+resequence }  over all critical ops
     if no candidate lowers Φ:  candidates ← single-job re-route / re-sequence   # fallback
     if best candidate lowers Φ:  apply it
     else: break                                     # pairwise Nash-stable
```

### 11.2 Cost of one descent step

Let $n=|\Omega|$, $\bar f=\max_i |E(i)|$ (flexibility), and let $K$ be the number of
critical operations. One decode is $O(n\log n)$. A step evaluates
$O\!\bigl(K\,(\,1 + \bar f^{2} + \bar f\,)\bigr)$ candidate decodes (swap + bounded
mutual reroute + combined), i.e. $O(K\,\bar f^{2})$ decodes, so a step costs
$O\!\bigl(K\,\bar f^{2}\, n\log n\bigr)$. The mutual-reroute bound (≤24 pairs, ≤36
cells) keeps this practical on highly flexible instances.

---

## 12. Notation

| Symbol | Meaning |
|---|---|
| $\mathcal{J},\mathcal{M},\Omega$ | jobs (players), machines, operations |
| $O_{j,k}$ | $k$-th operation of job $j$ |
| $E(i),\,p(i,r)$ | eligible machines of $i$; processing time of $i$ on $r$ |
| $\mu_i,\,S_i,\,C_i$ | assigned machine, start, completion of operation $i$ |
| $C_j,\,C_{\max}$ | job completion; makespan |
| $\sigma$ (OSV), $a$ (MAV) | dispatch order; machine-alternative indices |
| $s_i,\,s_{-i},\,S_i$ | strategy of $i$; others' strategies; strategy set |
| $U_i,\,\mathrm{cost}_i$ | payoff and cost of player $i$ |
| $W_i,\,\mathrm{Conf}_i,\,\mathrm{load}(r)$ | waiting, conflict, machine load |
| $\Phi$ | makespan-dominated acceptance potential |
| $\mathcal{C}_{ab}$ | conflict relation between jobs $a,b$ |
| $\alpha,\beta,\gamma,\delta,\lambda$ | payoff weights $(1,0.3,0.05,0.5)$; belief smoothing $(0.5)$ |
| $R,\kappa$ | number of runs $(50)$; ILS kick strength $\max(4,n/8)$ |

---

## 13. Novelty

1. **Two-player interaction as the primary operator.** Unlike unilateral best-
   response game-scheduling methods, *every critical operation first plays a
   two-player subgame* against its machine-neighbour rival; the lone move is a
   fallback.
2. **A combined two-player MAV+OSV move (R4).** A single joint deviation that
   **re-routes one player while the pair swaps order**, changing routing *and*
   sequencing together — a move pure-routing or pure-sequencing neighbourhoods
   cannot express.
3. **Explicit per-bottleneck bimatrix games.** The method extracts a normal-form
   2-player game at each contested machine and marks its pure Nash equilibria,
   giving an interpretable, game-theoretic account of every bottleneck.
4. **Conflict-restricted player set.** Only critical / conflicting jobs act, which
   focuses computation on the interactions that actually fix $C_{\max}$.
5. **Hybrid payoff + potential acceptance.** A private–social payoff $U_i$ models
   the economics of each player, while a makespan-dominated potential $\Phi$
   guarantees monotone makespan reduction and termination.

---

## 14. Scope and Honest Limitations

- Move acceptance uses the global potential $\Phi$ (makespan-dominated), so the
  reported objective is never degraded; the per-job payoff $U_i$ is the economic
  model and the analysis/reporting layer. The method therefore behaves as a
  **potential-based best-response search** rather than a purely selfish game in
  which each agent's own $U_i$ alone drives acceptance. A pure-selfish variant
  (acceptance on $U_i$) would trade makespan quality for stronger Nash stability.
- A heuristic can **match but never beat** a proven optimum; reaching best-known
  values on the hardest, highly flexible instances would require longer runs, a
  full N7 critical-block neighbourhood, or an exact CP/MILP polishing step.
- On RCFJSSP instances additional resource constraints are not enforced; the
  reported makespan is for the relaxed problem.

---

## References

- P. Brandimarte (1993). *Routing and scheduling in a flexible job shop by tabu search.* Annals of Operations Research 41, 157–183.
- J. Hurink, B. Jurisch, M. Thole (1994). *Tabu search for the job-shop scheduling problem with multi-purpose machines.* OR Spektrum 15, 205–215.
- G. A. Kasapidis et al. (2025). *A unified solution framework for flexible job shop scheduling problems with multiple resource constraints.* EJOR 320, 479–495.
- R. Reijnen et al. (2026). *Job shop scheduling benchmark: environments and instances for learning and non-learning methods.* Annals of Mathematics and AI.
