# Crossover — Numerical Examples for All Operators (OSV / MAV representation)

This document works through **every crossover operator** (POX, OUX, OOX, PWX, RMX, WGX, CPX)
on the **same two parents**, using the **OSV + MAV** solution representation, the **elite pool**,
and the **payoff functions**. All numbers are computed on one small instance so the arithmetic
can be followed by hand.

**Solution representation** (see `solution.md`):
- **OSV** — Operation Sequence Vector: the dispatch order written as **job ids**; the k-th
  occurrence of a job is that job's k-th operation (precedence-feasible by construction).
- **MAV** — Machine Assignment Vector: aligned to the OSV, each entry is the **chosen
  eligible-machine index** (1 = first eligible machine, 2 = second, …).

---

## 1. The instance

3 jobs, 3 machines, 5 operations. Eligible machines (machine : time), in eligibility order:

| operation | alt 1 | alt 2 |
|---|---|---|
| O(1,1) | M1:3 | M2:5 |
| O(1,2) | M2:4 | M3:2 |
| O(2,1) | M1:2 | M3:6 |
| O(2,2) | M3:4 | M1:7 |
| O(3,1) | M1:5 | M2:3 |

Jobs: **J1** = O(1,1)→O(1,2); **J2** = O(2,1)→O(2,2); **J3** = O(3,1).

---

## 2. The payoff functions

**Own-interest cost** of job $i$ (lower = better):
$$
\mathrm{own}_i=\alpha\,C_i+\beta\,W_i+\gamma\,\mathrm{Conf}_i+\tau\,\mathrm{Toll}_i,
$$
- $C_i$ = completion time; $W_i=C_i-\sum(\text{its processing times})$ = waiting;
- $\mathrm{Conf}_i=\sum_{O\in i}\mathrm{load}(\mu(O))$, $\mathrm{load}(k)$ = total processing on machine $k$;
- $\mathrm{Toll}_i=\sum_{O\in i}p(O)\cdot(\text{\# later rival ops on the same machine})$ (Pigouvian toll).

**Stable payoff** ($w=\max(1,\delta)$):
$$
U_i=\frac{1}{1+w\,C_{\max}+\dfrac{\mathrm{own}_i}{1+\mathrm{own}_i}}\ \ (\delta>0),
\qquad
U_i=\frac{1}{1+\mathrm{own}_i}\ \ (\delta=0,\ \text{selfish}).
$$

Weights used here: $\alpha=1,\ \beta=0.3,\ \gamma=0.05,\ \tau=0,\ \delta=0.5\Rightarrow w=1$.

---

## 3. The elite pool and the two parents

Crossover recombines **two parents from the search**: **Parent 1 = `runBest`** (the run's best
equilibrium) and **Parent 2 = a random elite** from the **ElitePlay pool**. Suppose the pool holds
`E1(Cmax 10)`, `E2(Cmax 13)`, `E3(Cmax 11)`; run-best = `E1`, random draw = `E2`.

### Parent A = `runBest` = E1  (all first-choice machines)
```
OSV(A) = [ 1 , 2 , 3 , 1 , 2 ]        = O(1,1) O(2,1) O(3,1) O(1,2) O(2,2)
MAV(A) = [ 1 , 1 , 1 , 1 , 1 ]        (every op uses its 1st eligible machine)
```
Decode → machines: O(1,1)→M1, O(2,1)→M1, O(3,1)→M1, O(1,2)→M2, O(2,2)→M3
```
O(1,1) M1 [0,3]   O(2,1) M1 [3,5]   O(3,1) M1 [5,10]   O(1,2) M2 [3,7]   O(2,2) M3 [5,9]
```
**C = (J1:7, J2:9, J3:10), Cmax = 10.**

### Parent B = random elite = E2  (all second-choice machines)
```
OSV(B) = [ 3 , 1 , 2 , 2 , 1 ]        = O(3,1) O(1,1) O(2,1) O(2,2) O(1,2)
MAV(B) = [ 2 , 2 , 2 , 2 , 2 ]        (every op uses its 2nd eligible machine)
```
Decode → machines: O(3,1)→M2, O(1,1)→M2, O(2,1)→M3, O(2,2)→M1, O(1,2)→M3
```
O(3,1) M2 [0,3]   O(1,1) M2 [3,8]   O(2,1) M3 [0,6]   O(2,2) M1 [6,13]   O(1,2) M3 [8,10]
```
**C = (J1:10, J2:13, J3:3), Cmax = 13.**

---

## 4. Payoff calculation for both parents

### Parent A  (loads: M1=3+2+5=10, M2=4, M3=4)
| Job | C | W | Conf | $\mathrm{own}=C+0.3W+0.05\,\mathrm{Conf}$ | $U$ (Cmax 10) |
|---|---|---|---|---|---|
| J1 | 7 | 0 | 14 | 7 + 0 + 0.70 = **7.70** | 1/(11+0.885)=**0.0841** |
| J2 | 9 | 3 | 14 | 9 + 0.9 + 0.70 = **10.60** | 1/(11+0.914)=**0.0839** |
| J3 | 10 | 5 | 10 | 10 + 1.5 + 0.50 = **12.00** | 1/(11+0.923)=**0.0839** |

### Parent B  (loads: M1=7, M2=3+5=8, M3=6+2=8)
| Job | C | W | Conf | $\mathrm{own}$ | $U$ (Cmax 13) |
|---|---|---|---|---|---|
| J1 | 10 | 3 | 16 | 10 + 0.9 + 0.80 = **11.70** | 1/(14+0.921)=**0.0670** |
| J2 | 13 | 0 | 15 | 13 + 0 + 0.75 = **13.75** | 1/(14+0.932)=**0.0670** |
| J3 | 3 | 0 | 8 | 3 + 0 + 0.40 = **3.40** | 1/(14+0.773)=**0.0677** |

> **KEY INSIGHT.** Under the stable payoff, **every job's $U_j$ is higher in Parent A** (A has the
> lower makespan, and the makespan dominates $U_j$). Selecting by $U_j$ would send **all** jobs to
> A → no recombination. So the payoff-guided crossovers select by the **own-interest cost
> $\mathrm{own}_j$**, which *does* discriminate:
>
> | Job | $\mathrm{own}_j$(A) | $\mathrm{own}_j$(B) | happier (lower) in |
> |---|---|---|---|
> | J1 | 7.70 | 11.70 | **A** |
> | J2 | 10.60 | 13.75 | **A** |
> | J3 | 12.00 | 3.40 | **B** |

---

## 5. How a payoff-guided child is assembled (shared step)

OUX / PWX / RMX / WGX / CPX decide a **partition** `fromA[job] ∈ {A,B}` (which parent each job
inherits its whole strategy from), then build the child's OSV and MAV:

1. **MAV:** each operation takes its machine index from its job's chosen parent.
2. **OSV:** keep the `fromA`-jobs' operations at **Parent A's positions**; fill the remaining
   slots with the other jobs' operations in **Parent B's order**. (Each job's ops stay in route
   order → the child OSV is precedence-feasible.)

---

## 6. The operators, one by one

### 6.1 POX — precedence-preserving / uniform (NOT payoff-guided)
- **MAV:** each operation takes its machine index from A or B by a fair coin.
- **OSV:** a random subset of jobs keeps A's positions; the rest come from B.

*Sample draw:* coins `(A,B,A,B,A)` for O(1,1),O(1,2),O(2,1),O(2,2),O(3,1); job subset `S={J1,J3}` from A.
```
child OSV = [ 1 , 2 , 3 , 1 , 2 ]      (J1,J3 at A-positions, J2 from B)
child MAV = [ 1 , 1 , 1 , 2 , 2 ]      (O(1,1)A O(2,1)A O(3,1)A | O(1,2)B O(2,2)B)
```
POX is **random** (no payoff) — different coins give different children; it maximises diversity.

### 6.2 OUX — order-uniform, payoff-guided (deterministic)
`fromA[j] = A if own_j(A) ≤ own_j(B) else B`:
```
J1: 7.70 ≤ 11.70 → A      J2: 10.60 ≤ 13.75 → A      J3: 12.00 ≤ 3.40? no → B
fromA = { J1:A , J2:A , J3:B }
```
Build the child (J1,J2 machines from A, J3's machine from B; O(3,1) alt 1→**alt 2** = M2):
```
child OSV = [ 1 , 2 , 3 , 1 , 2 ]
child MAV = [ 1 , 1 , 2 , 1 , 1 ]      (only O(3,1) changed: A(alt1=M1) → B(alt2=M2))
```
**Decode the OUX child:**
```
O(1,1) M1 [0,3]   O(2,1) M1 [3,5]   O(3,1) M2 [0,3]   O(1,2) M2 [3,7]   O(2,2) M3 [5,9]
C = (J1:7, J2:9, J3:3),  Cmax = 9
```
✅ **The child (Cmax 9) beats BOTH parents (10 and 13)** — it kept A's good J1/J2 routing and took
B's good J3 routing (O(3,1)→M2 frees the overloaded M1). *(This child is exactly the solution in
`solution.md`.)*

### 6.3 OOX — order-based one-point (NOT payoff-guided)
Pick a cut `c`; OSV prefix from A (with A's MAV), remainder from B in order (with B's MAV),
skipping placed operations.

*Sample cut `c = 3`:* A prefix = O(1,1) O(2,1) O(3,1) (all A/alt 1); remainder from B order
skipping those = O(2,2) O(1,2) (B/alt 2):
```
child OSV = [ 1 , 2 , 3 , 2 , 1 ]
child MAV = [ 1 , 1 , 1 , 2 , 2 ]      (prefix A: alt 1 ; suffix B: alt 2)
```

### 6.4 PWX — payoff-WEIGHTED (roulette OUX), payoff-guided (stochastic)
Inherit from A with probability $P(\text{A})=\dfrac{\mathrm{own}_j(B)}{\mathrm{own}_j(A)+\mathrm{own}_j(B)}$:
```
J1: 11.70/19.40 = 0.60 → 60% A     J2: 13.75/24.35 = 0.57 → 57% A     J3: 3.40/15.40 = 0.22 → 22% A
```
The happier parent is *more likely* but not certain. *One sampled draw* (rolls 0.40,0.72,0.55):
`fromA = {J1:A, J2:B, J3:B}`:
```
child OSV = [ 1 , 3 , 2 , 1 , 2 ]      (J1 at A-positions; J2,J3 from B order)
child MAV = [ 1 , 2 , 2 , 1 , 2 ]      (O(1,1)A O(3,1)B O(2,1)B O(1,2)A O(2,2)B)
```
Over many ILS iterations PWX samples many partitions (that diversity is why it led the runs-50 ablation).

### 6.5 RMX — regret-matching, payoff-guided
$\mathrm{regret}_j(P)=\mathrm{own}_j(P)-\min_{j\text{'s single reroutes}}\mathrm{own}_j$
(how much own cost the job could still shave by best-responding). Inherit from the **lower-regret** parent.

*Worked case — J3:*
- In **A**, O(3,1) is on M1 (own₃=12.00); rerouting it to **M2** gives own₃=3.35
  → $\mathrm{regret}_3(A)=12.00-3.35=\mathbf{8.65}$.
- In **B**, O(3,1) is already on M2 (own₃=3.40, near best) → $\mathrm{regret}_3(B)\approx\mathbf{0}$.
- Lower regret in B → **J3 inherits from B.** J1,J2 are near best in A → inherit from A.
```
fromA = { J1:A , J2:A , J3:B }        (agrees with OUX here; criterion = best-response gap)
child OSV = [ 1 , 2 , 3 , 1 , 2 ]
child MAV = [ 1 , 1 , 2 , 1 , 1 ]      Cmax = 9
```

### 6.6 WGX — welfare-guided (social cost), payoff-guided (novel)
Inherit by the **internalised social cost** $\psi_j=\mathrm{own}_j+\mathrm{Toll}_j$.
Tolls (from the schedules): **Toll(A)=(6,2,0)**, **Toll(B)=(0,6,3)**.

| Job | $\psi(A)=\mathrm{own}(A)+\mathrm{Toll}(A)$ | $\psi(B)$ | lower |
|---|---|---|---|
| J1 | 7.70+6 = 13.70 | 11.70+0 = 11.70 | **B** |
| J2 | 10.60+2 = 12.60 | 13.75+6 = 19.75 | **A** |
| J3 | 12.00+0 = 12.00 | 3.40+3 = 6.40 | **B** |

`fromA = {J1:B, J2:A, J3:B}` — **different from OUX** (the externality flips **J1 to B**):
```
child OSV = [ 3 , 2 , 1 , 1 , 2 ]      (J2 at A-positions; J1,J3 from B order)
child MAV = [ 2 , 1 , 2 , 2 , 1 ]      (O(3,1)B O(2,1)A O(1,1)B O(1,2)B O(2,2)A)   Cmax = 10
```

### 6.7 CPX — coalitional payoff, payoff-guided (novel)
Jobs sharing a machine in either parent form a coalition; each coalition inherits from the parent
with lower coalition total own cost. In **A**, M1 carries J1,J2,J3 → **{J1,J2,J3}** is one coalition.
Totals: $\sum\mathrm{own}(A)=30.30$, $\sum\mathrm{own}(B)=28.85$ → the coalition inherits from **B**:
```
fromA ≈ { all : B }  (± 15% per-job exploration flip)
child OSV = [ 3 , 1 , 2 , 2 , 1 ]  = Parent B
child MAV = [ 2 , 2 , 2 , 2 , 2 ]                                             Cmax = 13
```
On this **densely-coupled** instance CPX collapses to one coalition → it just copies the better
(by own-cost sum) parent — a documented limitation; CPX needs *sparse* interaction to recombine.

---

## 7. Summary — partitions and children (OSV / MAV)

| Operator | `fromA` | child OSV | child MAV | Cmax |
|---|---|---|---|---|
| Parent A | — | [1,2,3,1,2] | [1,1,1,1,1] | 10 |
| Parent B | — | [3,1,2,2,1] | [2,2,2,2,2] | 13 |
| POX | random | [1,2,3,1,2] | [1,1,1,2,2] | varies |
| **OUX** | J1:A J2:A J3:B | [1,2,3,1,2] | [1,1,2,1,1] | **9** ✅ |
| OOX | cut=3 | [1,2,3,2,1] | [1,1,1,2,2] | varies |
| PWX | J1:A J2:B J3:B (sample) | [1,3,2,1,2] | [1,2,2,1,2] | varies |
| RMX | J1:A J2:A J3:B | [1,2,3,1,2] | [1,1,2,1,1] | 9 |
| WGX | J1:B J2:A J3:B | [3,2,1,1,2] | [2,1,2,2,1] | 10 |
| CPX | all:B | [3,1,2,2,1] | [2,2,2,2,2] | 13 |

---

## 8. Key takeaways

1. **A solution = (OSV, MAV):** OSV = job-id dispatch order (k-th occurrence = k-th operation),
   MAV = chosen eligible-machine index aligned to it.
2. **Payoff-guided crossovers select by $\mathrm{own}_j$, not $U_j$** — the stable $U_j$ is
   makespan-dominated and would not discriminate between jobs.
3. **OUX/PWX/RMX** send each job to where it is individually better (own cost / regret);
   **WGX** adds the externality (social cost); **CPX** keeps interacting coalitions together.
4. **Recombination can beat both parents** — the OUX child reached **Cmax 9** (MAV=[1,1,2,1,1])
   vs parents 10 and 13, by combining A's J1/J2 routing with B's J3 routing.
5. **CPX needs sparse interaction** — on densely-coupled instances it collapses to one coalition
   and copies the better parent (an honest, documented limitation).
