# Game-Theoretic Flexible Job Shop Scheduling

Each **job is a player** in a non-cooperative game. A player's strategy is the
machine it picks for each of its operations plus where those operations sit in
the shared dispatch order. Every player is **selfish**: it minimises only its own
completion time `C_i` (payoff `u_i = -C_i`). The shop **makespan** `Cmax = max_i C_i`
is what we report against the literature best-known values.

Every instance starts from a **random** strategy profile; the jobs then play
**best-response dynamics** (with random restarts) until no job can finish earlier
- a **Nash equilibrium**. The makespan-critical job always has an incentive to
deviate, so selfish play drives the makespan down.

## How to run

The program takes no console input. It auto-detects the `data/` folder, solves
every `.fjs` instance and writes everything under `output/`:

- `output/allresult.txt` - cumulative table (our makespan vs best-known),
- `output/<instance>_log.txt` - per-instance trace + final schedule,
- `output/README.md` - this summary,
- `output/code_explanation.md` - how the code and the game model fit together.

## Summary by benchmark group

| Group | Instances | With BKS | Matched | Beaten | Avg gap % (where known) |
|---|---:|---:|---:|---:|---:|
| brandimarte | 15 | 15 | 6 | 0 | 3.61 |

## Full results

| Instance | Group | Jobs | Mch | Ops | Init Cmax | Our Cmax | Best-known | Gap % | Eq |
|---|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| mk01 | brandimarte | 10 | 6 | 55 | 86 | 40 | 40 | 0.00 | Y |
| mk02 | brandimarte | 10 | 6 | 58 | 62 | 27 | 26 | 3.85 | Y |
| mk03 | brandimarte | 15 | 8 | 150 | 351 | 204 | 204 | 0.00 | Y |
| mk04 | brandimarte | 15 | 8 | 90 | 142 | 62 | 60 | 3.33 | Y |
| mk05 | brandimarte | 15 | 4 | 106 | 307 | 173 | 172 | 0.58 | Y |
| mk06 | brandimarte | 10 | 15 | 150 | 194 | 61 | 57 | 7.02 | Y |
| mk07 | brandimarte | 20 | 5 | 100 | 278 | 140 | 139 | 0.72 | Y |
| mk08 | brandimarte | 20 | 10 | 225 | 743 | 523 | 523 | 0.00 | Y |
| mk09 | brandimarte | 20 | 10 | 240 | 559 | 307 | 307 | 0.00 | Y |
| mk10 | brandimarte | 20 | 15 | 240 | 510 | 221 | 193 | 14.51 | Y |
| mk11 | brandimarte | 30 | 5 | 179 | 886 | 611 | 609 | 0.33 | Y |
| mk12 | brandimarte | 30 | 10 | 193 | 960 | 508 | 508 | 0.00 | Y |
| mk13 | brandimarte | 30 | 10 | 231 | 918 | 426 | 386 | 10.36 | Y |
| mk14 | brandimarte | 30 | 15 | 277 | 1271 | 694 | 694 | 0.00 | Y |
| mk15 | brandimarte | 30 | 15 | 284 | 636 | 378 | 333 | 13.51 | Y |

