#pragma once
// ============================================================================
//  SolveResult.h
//  ---------------------------------------------------------------------------
//  The data carried OUT of one solved instance (shared by both optimization
//  layers and the report writers):
//    * MoveRecord  - one accepted best-response deviation (a trace row);
//    * SolveResult - the outcome: best schedule, the trace, and the
//                    game-theoretic certification (Nash stability).
//  CLASSES (never structs), in keeping with the project's OOP style.
// ============================================================================

#include "StrategyProfile.h"
#include <vector>
#include <string>

using namespace std;

namespace fjs {

// One accepted deviation - one row of the per-instance table.
class MoveRecord {
public:
    int         iteration = 0;     // 1-based global move counter
    int         run       = 0;     // which run produced it
    int         job       = 0;     // the player that moved (0-based)
    string action;            // human-readable description of the move
    double      oldCost   = 0;     // makespan before the move
    double      newCost   = 0;     // makespan after the move
    int         makespan  = 0;     // makespan after the move (== newCost)
    long long   sumCompletion = 0; // sum of all completion times after the move

    // ---- two-player interaction detail (a swap / mutual reroute between two rival jobs) ----
    int    rival          = -1;    // the rival job in a two-player move, else -1 (solo move)
    int    contestMachine = -1;    // the contested machine (0-based) for a two-player move
    string moveType;          // "reroute" | "resequence" | "swap" | "mutual" | "reroute+swap" | ...
    string layer;             // which game layer produced it: "L1(SCL)" routing game |
                              //   "L2(ODL)" sequencing game. Empty = derive from moveType.
    int    moverCBefore   = 0, moverCAfter = 0;   // mover job's completion C before/after
    int    rivalCBefore   = 0, rivalCAfter = 0;   // rival job's completion C before/after

    // ---- full per-iteration detail (captured only for the first few moves) ----
    bool   hasDetail = false;
    int    moverOp = -1, rivalOp = -1;            // operation global ids involved
    int    moverAltBefore = -1, moverAltAfter = -1;
    int    rivalAltBefore = -1, rivalAltAfter = -1;
    StrategyProfile stateBefore;                  // the profile just before this move
};

// Everything the reports need about one solved instance.
class SolveResult {
public:
    string name;
    int numJobs = 0, numMachines = 0, numOperations = 0;

    int       initialMakespan = 0;   // makespan of the very first random profile
    StrategyProfile initialState;          // that first random profile (for reporting)

    int       bestMakespan = 0;
    long long bestTotalCompletion = 0;
    StrategyProfile bestState;             // the best profile found over all runs

    bool equilibriumReached = false; // at least one local optimum / equilibrium
    int  runsRun       = 0;          // number of independent runs performed
    int  acceptedMoves = 0;          // total improving moves over the whole run
    long evaluations   = 0;          // schedule decodes performed
    vector<int> runBests;       // best makespan of each individual run

    // ---- game-theoretic certification of the reported best schedule ----
    bool nashStable          = false; // true iff no profitable deviation
    int  profitableDeviations = -1;   // count of profitable deviations (-1 = not checked)

    vector<MoveRecord> trace;   // capped trace for the per-instance log
};

} // namespace fjs
