# Solution Representation — OSV + MAV (with a worked example)

A complete solution to the Flexible Job-Shop Scheduling Problem is encoded by **two aligned
vectors**, together forming the **strategy profile** $s=(\text{MAV},\text{OSV})$:

- **MAV — Machine Assignment Vector (the *routing* decision)**
  which eligible machine each operation runs on.
- **OSV — Operation Sequence Vector (the *sequencing* decision)**
  the order in which operations are dispatched onto the machines.

These are exactly the **two decisions** the flexible job shop requires, and they are the two
halves of each job-player's game strategy (MAV = routing game, OSV = sequencing game).

---

## 1. What each vector is

### OSV — Operation Sequence Vector
An ordering of **all** operations. It is written as a list of **job ids**, where the **k-th
occurrence of job _j_** denotes the **k-th operation** of job _j_ (its route order). This makes
the sequence **precedence-feasible by construction** — a job's operations always appear in order.

```
OSV = [ 1 , 2 , 3 , 1 , 2 ]
        │   │   │   │   └── 2nd time job2 appears → O(2,2)
        │   │   │   └────── 2nd time job1 appears → O(1,2)
        │   │   └────────── 1st time job3 appears → O(3,1)
        │   └────────────── 1st time job2 appears → O(2,1)
        └────────────────── 1st time job1 appears → O(1,1)
```

### MAV — Machine Assignment Vector
Aligned position-by-position with the OSV. Each entry is the **index of the chosen eligible
alternative** (1 = first eligible machine, 2 = second, …) for the operation at that OSV position.
Storing the *alternative index* (not the raw machine number) keeps it valid for any eligibility set.

```
OSV = [ 1 , 2 , 3 , 1 , 2 ]
MAV = [ 1 , 1 , 2 , 1 , 1 ]     (which eligible machine each op uses)
```

*(Internally the solver stores `routing[gid]` = alternative index keyed by the operation's flat
global id, and `sequence` = the list of global ids; the printed OSV/MAV above are the
human-readable view.)*

---

## 2. The example instance

3 jobs, 3 machines, 5 operations. Eligible machines (machine : time), in eligibility order:

| operation | alt 1 | alt 2 |
|---|---|---|
| O(1,1) | M1:3 | M2:5 |
| O(1,2) | M2:4 | M3:2 |
| O(2,1) | M1:2 | M3:6 |
| O(2,2) | M3:4 | M1:7 |
| O(3,1) | M1:5 | M2:3 |

Jobs: **Job 1** = O(1,1)→O(1,2); **Job 2** = O(2,1)→O(2,2); **Job 3** = O(3,1).

---

## 3. A worked solution

```
OSV = [ 1 , 2 , 3 , 1 , 2 ]      (dispatch order, as job ids)
MAV = [ 1 , 1 , 2 , 1 , 1 ]      (chosen eligible-machine index, aligned)
```

**Reading the two vectors together** (position by position):

| pos | OSV (job) | operation | MAV (alt) | → machine : time |
|---|---|---|---|---|
| 1 | 1 | O(1,1) | 1 | **M1 : 3** |
| 2 | 2 | O(2,1) | 1 | **M1 : 2** |
| 3 | 3 | O(3,1) | 2 | **M2 : 3** |
| 4 | 1 | O(1,2) | 1 | **M2 : 4** |
| 5 | 2 | O(2,2) | 1 | **M3 : 4** |

So the **MAV (routing)** says: O(1,1)→M1, O(2,1)→M1, O(3,1)→M2, O(1,2)→M2, O(2,2)→M3.
The **OSV (sequencing)** says: dispatch them in the order O(1,1), O(2,1), O(3,1), O(1,2), O(2,2).

---

## 4. Decoding the solution into a schedule

The decoder places each operation **as early as possible** (respecting its job-predecessor and
the chosen machine's availability), in OSV order:

```
1. O(1,1) → M1, ready at 0            → M1 [0,3]        C(J1)=3
2. O(2,1) → M1, ready at 0, M1 busy   → M1 [3,5]        C(J2)=5
3. O(3,1) → M2, ready at 0            → M2 [0,3]        C(J3)=3
4. O(1,2) → M2, ready at 3 (after O(1,1)), M2 busy [0,3] → M2 [3,7]   C(J1)=7
5. O(2,2) → M3, ready at 5 (after O(2,1))               → M3 [5,9]   C(J2)=9
```

**Completions:** C(J1)=7, C(J2)=9, C(J3)=3 → **makespan $C_{\max}=9$.**

### Gantt of this solution
```
        0    1    2    3    4    5    6    7    8    9
  M1 : |==O(1,1)==|==O(2,1)==|
  M2 : |==O(3,1)==|====O(1,2)=====|
  M3 :                     |=====O(2,2)=====|
```

---

## 5. Why the encoding is always feasible

- **Precedence (OSV):** because the OSV lists job ids and the k-th occurrence = the k-th
  operation, every job's operations are automatically in route order — no infeasible ordering can
  be written. Every solver move (resequence, swap, crossover) preserves this.
- **Eligibility (MAV):** each MAV entry is an index into that operation's **eligible** list, so it
  can only ever point to a machine the operation is allowed to run on.

Together they guarantee the pair (MAV, OSV) **always decodes to a valid schedule.**

---

## 6. Mapping to the game model

| Vector | Decision | Game layer | Player's strategy part |
|---|---|---|---|
| **MAV** | routing (which machine) | **Layer 1 — Routing Game** | which machine each operation uses |
| **OSV** | sequencing (which order) | **Layer 2 — Sequencing Game** | its operations' dispatch positions |

A **move** in the game edits one of these vectors:
- **reroute** = change one MAV entry (a routing move),
- **resequence / swap** = change the OSV order (a sequencing move).

The complete pair $s=(\text{MAV},\text{OSV})$ is the **strategy profile** — every player's
strategy bundled together — which the decoder turns into the timed schedule scored by the payoff.

---

## 7. One-line summary

**A solution = two aligned vectors: the MAV (which eligible machine each operation uses) and the
OSV (the job-id dispatch order, whose k-th occurrence of a job is that job's k-th operation).
Read together they assign every operation a machine and a start time via the as-early-as-possible
decoder — e.g. OSV=[1,2,3,1,2], MAV=[1,1,2,1,1] decodes to a schedule with makespan 9.**
