#pragma once
// ============================================================================
//  Player.h
//  ---------------------------------------------------------------------------
//  THE PLAYER of the non-cooperative scheduling game.
//
//  In this game theory model a JOB is a player: a self-interested agent that
//  competes with the other jobs for machine time and tries to maximise its own
//  payoff (finish early, wait little, avoid congested machines). The Job class
//  holds the player's immutable problem data (its route of operations); the
//  Player class is the GAME-THEORETIC AGENT wrapped around that data - it owns a
//  Strategy (its decision) and knows how to read its own payoff and whether it
//  could do better by deviating on its own.
//
//      Player  = the agent (this class)
//      Strategy = the action the agent currently plays (Strategy.h)
//      U_i      = the payoff the agent receives (PayoffFunction.h)
//
//  Keeping the agent separate from the problem data and from the search engine
//  is what makes the "job = player" interpretation explicit in the code.
// ============================================================================

#include <string>

using namespace std;

namespace fjs {

class Instance;
class StrategyProfile;
class Schedule;
class PayoffFunction;
class Strategy;

class Player {
public:
    Player(const Instance& inst, int jobIndex);

    // ---- identity ------------------------------------------------------
    int           index = 0;                          // 0-based job/player id (public data)
    const string& label() const { return name; }      // e.g. "J3"
    int           operationCount() const;

    // ---- the action this player currently plays ------------------------
    // Extract this player's Strategy (its machine routing + queue positions and
    // its interaction outcome) from a decoded strategy profile.
    Strategy strategy(const StrategyProfile& profile, const Schedule& sched,
                      const PayoffFunction& payoff) const;

    // ---- the player's selfish outcome under a schedule -----------------
    int    completion(const Schedule& sched) const;                          // C_i
    double waiting(const Schedule& sched, const PayoffFunction& payoff) const; // W_i
    double conflict(const Schedule& sched, const PayoffFunction& payoff) const;// Conf_i
    double utility(const Schedule& sched, const PayoffFunction& payoff) const; // U_i

    // ---- best response -------------------------------------------------
    // Can this player improve the schedule by changing its OWN strategy alone -
    // i.e. by unilaterally re-routing one of its operations to another eligible
    // machine? Returns true if such a profitable deviation exists; when `how` is
    // given it is filled with a human-readable description of the first one.
    bool canImprove(const StrategyProfile& profile, const PayoffFunction& payoff,
                    string* how = nullptr) const;

private:
    const Instance* inst;
    string name;   // player label, e.g. "J3"
};

} // namespace fjs
