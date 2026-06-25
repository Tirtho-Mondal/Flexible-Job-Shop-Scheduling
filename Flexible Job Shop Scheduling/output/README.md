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
| brandimarte | 1 | 1 | 1 | 0 | 0.00 |

## Full results

| Instance | Group | Jobs | Mch | Ops | Init Cmax | Our Cmax | Best-known | Gap % | Eq |
|---|---|---:|---:|---:|---:|---:|---:|---:|:--:|
| Mk01 | brandimarte | 10 | 6 | 55 | 115 | 40 | 40 | 0.00 | Y |

