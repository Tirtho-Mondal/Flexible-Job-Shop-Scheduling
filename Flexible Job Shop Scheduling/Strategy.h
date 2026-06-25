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

    void addChoice(const OperationChoice& c) { choices_.push_back(c); }
    void setInteraction(double completion, double waiting,
                        double conflict, double payoff);

    int                            jobIndex() const { return jobIndex_; }
    const string&                  label()    const { return label_; }
    const vector<OperationChoice>& choices()  const { return choices_; }

    // The interaction outcome of this strategy (read from the decoded schedule).
    double completion() const { return completion_; }   // C_i
    double waiting()    const { return waiting_; }       // W_i
    double conflict()   const { return conflict_; }      // Conf_i
    double payoff()     const { return payoff_; }        // U_i

    // Extract one job-player's strategy from a decoded strategy profile.
    static Strategy fromProfile(const Instance& inst, const StrategyProfile& profile,
                                const Schedule& sched, const PayoffFunction& payoff,
                                int job);

private:
    int    jobIndex_;
    string label_;
    vector<OperationChoice> choices_;
    double completion_ = 0, waiting_ = 0, conflict_ = 0, payoff_ = 0;
};

} // namespace fjs
