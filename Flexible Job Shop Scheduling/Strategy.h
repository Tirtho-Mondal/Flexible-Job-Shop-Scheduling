#pragma once
// ============================================================================
//  Strategy.h
//  ---------------------------------------------------------------------------
//  The STRATEGY of a single job-player.
//
//  For each job-player, a strategy is its decision about:
//      * which machine will process each of its operations,
//      * where each operation is placed in the operation sequence (OSV),
//      * how it interacts with other jobs - i.e. the waiting time, machine
//        conflict and makespan contribution that decision produces.
//
//  A Strategy is therefore ONE player's slice of a StrategyProfile (the joint
//  decision of all players) once that profile has been decoded into a schedule.
//  It is the per-player view the payoff scores and the report explains.
//
//  OOP: a CLASS holding per-operation choices (OperationChoice) plus the
//  player's interaction metrics, exposed through const accessors.
// ============================================================================

#include <vector>
#include <string>

using namespace std;

namespace fjs {

// One operation's decision inside a job-player's strategy.
class OperationChoice {
public:
    int    globalId         = -1;   // flat operation id
    string operationLabel;          // e.g. "O(2,3)"
    int    machine          = -1;   // chosen machine (0-based; +1 for display)
    int    processingTime   = 0;    // processing time on the chosen machine
    int    sequencePosition = -1;   // index of this operation in the OSV
    int    start            = 0;    // start time in the decoded schedule
    int    finish           = 0;    // finish time in the decoded schedule
};

// Forward declarations - the factory's inputs live in their own headers.
class Instance;
class StrategyProfile;
class Schedule;
class PayoffFunction;

class Strategy {
public:
    Strategy(int jobIndex, const string& label);

    // ---- public data: who plays this strategy and its operation choices ----
    int    jobIndex = 0;
    vector<OperationChoice> choices;

    // The interaction outcome of this strategy (read from the decoded schedule).
    double completion = 0;   // C_i
    double waiting    = 0;   // W_i
    double conflict   = 0;   // Conf_i
    double payoff     = 0;   // U_i

    void addChoice(const OperationChoice& c) { choices.push_back(c); }
    void setInteraction(double c, double w, double cf, double u);

    const string& label() const { return name; }   // e.g. "J3"

    // Extract one job-player's strategy from a decoded strategy profile.
    static Strategy fromProfile(const Instance& inst, const StrategyProfile& profile,
                                const Schedule& sched, const PayoffFunction& payoff,
                                int job);

private:
    string name;   // player label, e.g. "J3"
};

} // namespace fjs
