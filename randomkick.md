# Random Kick — the ILS perturbation (with a numerical example)

The **random kick** (`RandomKick::apply`) is the perturbation step of the Iterated Local
Search (ILS). After the two-layer game descends to a **local optimum (a Nash equilibrium)**,
no single move can improve it — so the kick makes a few small random changes to **escape**
that optimum, and the game is then replayed. This document explains exactly how the kick
works and walks through a small numerical example.

---

## 1. What a kick does

The kick applies **`strength`** small random changes to the strategy profile
$s=(\mathbf r,\boldsymbol\pi)$ = (routing MAV, sequence OSV). Each change:

1. **Pick a random operation** $g$.
2. **Flip a coin:**
   - if $g$ has **more than one** eligible machine **and** coin $=0$ → **REROUTE** (give it a
     different machine), else
   - → **RESEQUENCE** (move it to a random legal slot in its precedence window).

**Kick strength**
$$
\text{strength}=\max(\text{kick\_min},\ \lfloor N/\text{kick\_div}\rfloor)\quad(\text{default }\max(4,\ N/8)),
$$
where $N$ is the number of operations. In **memetic mode** (after a crossover) the kick is
reduced to $\max(1,\lfloor\text{strength}/3\rfloor)$ — a *light* kick.

---

## 2. What "strength" means (the size of the jump)

**Strength = how many operations the kick changes in one perturbation** — i.e. the *size* of
the jump out of the current solution. The kick loop runs **`strength` times**, and each
iteration changes **one** random operation (reroute or resequence). So:

- `strength = 1` → change **1** operation → a tiny nudge
- `strength = 6` → change **6** operations → a moderate jump
- `strength = 30` → change **30** operations → a large disturbance (≈ a random restart)

$$\text{strength} = \text{number of operations perturbed in one kick.}$$

It **scales with instance size** via $\text{strength}=\max(\text{kick\_min},\ \lfloor N/\text{kick\_div}\rfloor)$
(default $\max(4,N/8)$), so bigger instances get bigger kicks. *Example:* mk01 (~55 ops)
→ $\max(4,\ \lfloor55/8\rfloor)=6$ operations changed per kick. In memetic mode it is reduced
to $\max(1,\lfloor\text{strength}/3\rfloor)$ — a light kick, because the crossover already did
most of the disturbing.

**Why it matters (the balance).** The kick must be big enough to **escape** the current local
optimum but small enough to **stay near** the good region:

| Strength | Effect |
|---|---|
| **too small** (e.g. 1) | can't escape — the game descends right back to the same equilibrium |
| **just right** (4–8) | escapes but stays in the promising region → finds a *nearby, better* optimum |
| **too big** (e.g. 30) | destroys the solution → basically a random restart, losing all progress |

**Valley analogy:** if the local search is "walk downhill to the valley bottom", then
*strength is the jump distance* before you walk down again — too small lands you back in the
same valley, too big lands you anywhere on the map, and a medium jump lands you in a *nearby
(possibly deeper) valley*.

**Tuning:** `kick_min` sets the minimum strength; `kick_div` sets the divisor — a **smaller
`kick_div` gives stronger kicks**.

---

## 3. Algorithm (pseudocode of `RandomKick::apply`)

```
apply(profile, strength):
    repeat  strength  times:
        g ← a uniformly random operation
        if  g has >1 machine  and  coin()==0:
            # ---- REROUTE ----
            if the elite pool is ready:
                new machine ← sample from g's ELITE FREQUENCIES   (aimed kick)
            else:
                new machine ← a uniformly random eligible machine  (blind kick)
            profile.reroute(g, new machine)
        else:
            # ---- RESEQUENCE ----
            window = ( predecessor position , successor position )   # precedence window
            if the window has no room:  skip
            target ← a random slot strictly inside the window
            move g in the dispatch order from its position to target
```

Every change preserves **precedence-feasibility** (a job's operations stay in route order),
so the kicked profile always decodes to a valid schedule.

---

## 4. A worked numerical example

### Tiny instance
3 jobs, 5 operations (global ids 0–4), machines M1–M3:

| gid | operation | eligible (machine : time) |
|---|---|---|
| 0 | O(1,1) | M1:3 , M2:5 |
| 1 | O(1,2) | M2:4  (only 1 machine) |
| 2 | O(2,1) | M1:2 , M3:6 |
| 3 | O(2,2) | M3:4  (only 1 machine) |
| 4 | O(3,1) | M1:5 , M2:3 |

### Profile BEFORE the kick
```
routing (MAV) :  gid0→M1  gid1→M2  gid2→M1  gid3→M3  gid4→M1     (alt indices: [0,0,0,0,0])
sequence (OSV):  [ 0 , 2 , 4 , 1 , 3 ]  =  O(1,1) O(2,1) O(3,1) O(1,2) O(2,2)
```

### Apply a kick of `strength = 2`  →  two random changes

**Change 1 — REROUTE**
```
pick random op   → gid = 2  (O(2,1))
coin = 0, and O(2,1) has 2 machines  → REROUTE
pick a new machine → alt 1 = M3
effect:  gid2 :  M1  →  M3
routing becomes:  [0, 0, 1, 0, 0]
```

**Change 2 — RESEQUENCE**
```
pick random op   → gid = 4  (O(3,1))
coin = 1  → RESEQUENCE
O(3,1) is at position 2 in the sequence.
its precedence window: no predecessor, no successor (single-op job) → may move to slots 0..4.
pick a random target slot → 0
move O(3,1) from position 2 to position 0:
   [0,2,4,1,3]  →  remove 4  →  [0,2,1,3]  →  insert 4 at front  →  [4,0,2,1,3]
sequence becomes:  [ 4 , 0 , 2 , 1 , 3 ]
```

### Profile AFTER the kick
```
routing (MAV) :  gid0→M1  gid1→M2  gid2→M3  gid3→M3  gid4→M1     (O(2,1) moved to M3)
sequence (OSV):  [ 4 , 0 , 2 , 1 , 3 ]  =  O(3,1) O(1,1) O(2,1) O(1,2) O(2,2)
```

Two small perturbations — **one machine changed, one dispatch position changed** — and the
profile is still precedence-feasible.

### What happens next
The kicked profile is returned to the **two-layer game (local search)**, which re-descends to
a new equilibrium. If its makespan is lower it is kept as the new incumbent; otherwise it is
discarded, and ILS kicks again — until `ils_patience` kicks pass with no improvement.

---

## 5. Two important details

- **Aimed vs blind reroute.** When the **elite pool** is ready, the new machine is *sampled
  from the elite frequencies* (the machines the best equilibria favour), so the kick is
  **aimed** toward promising assignments rather than uniformly random.
- **Precedence-safe resequence.** The target slot is drawn only from the operation's window
  `(predecessor position, successor position)`, so a job's operations never get out of order.
  If the window has no room, that change is skipped.

---

## 6. Code mapping

| Concept | Code |
|---|---|
| the kick | `RandomKick::apply(profile, strength, elitePool)` |
| reroute (change machine) | `profile.reroute(gid, newAlt)` |
| aimed reroute (elite freq.) | `elitePool->sampleAlternative(gid, rng)` |
| resequence (move in order) | `profile.resequenced(pos, target)` + `sequence.swap` |
| precedence window | `predPos`, `succPos` (predecessor / successor positions) |
| strength | `max(kick_min, ops/kick_div)`; memetic uses `max(1, strength/3)` |

---

## 7. One-line summary

**A random kick = repeat `strength` times: grab a random operation and either give it a new
machine (reroute) or slide it to a new legal position in the dispatch order (resequence) — a
handful of small, feasibility-preserving random changes that nudge the solution out of its
current equilibrium so the game can search again.**
