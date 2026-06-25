#pragma once
// ============================================================================
//  Instance.h
//  ---------------------------------------------------------------------------
//  The whole FJSSP problem instance: the players (jobs), the shared resources
//  (machines) and a flat index of every operation.  This object is built once
//  by the reader and then treated as IMMUTABLE problem data - the mutable
//  decision (routing + dispatch order) lives in a separate StrategyProfile, so a
//  single Instance can be evaluated against any number of strategy profiles.
//
//  OOP pillars: ABSTRACTION (the rest of the program touches the problem only
//  through this clean interface) and ENCAPSULATION (private storage).
// ============================================================================

#include "Machine.h"
#include "Job.h"
#include <vector>
#include <string>

using namespace std;

namespace fjs {

class Instance {
public:
    Instance() = default;

    // ---- Builder API used by the reader --------------------------------
    void  setName(const string& n)  { name_ = n; }
    void  setGroup(const string& g) { group_ = g; }
    void  setMachineCount(int m);
    Job&  addJob();                 // appends a new player
    int   nextGlobalOperationId() const { return nextGlobalId_; }
    void  bumpGlobalOperationId() { ++nextGlobalId_; }
    void  finalise();               // build the flat operation index

    // ---- Identity ------------------------------------------------------
    const string& name()  const { return name_; }
    const string& group() const { return group_; }

    // ---- Sizes ---------------------------------------------------------
    int numJobs()         const { return (int)jobs_.size(); }
    int numMachines()     const { return (int)machines_.size(); }
    int totalOperations() const { return nextGlobalId_; }

    // ---- Access --------------------------------------------------------
    Job&             job(int i)              { return jobs_[i]; }
    const Job&       job(int i)        const { return jobs_[i]; }
    const Machine&   machine(int m)    const { return machines_[m]; }

    // Resolve a flat operation id back to its owning Operation object.
    Operation&       operationByGlobalId(int gid);
    const Operation& operationByGlobalId(int gid) const;
    int globalIdJob(int gid)      const { return opJob_[gid]; }
    int globalIdPosition(int gid) const { return opPos_[gid]; }

    // A loose horizon used to normalise / bound times: the total work of the
    // shop, which is always an upper bound on any sensible makespan.
    int totalWork() const;

private:
    string          name_;
    string          group_;
    vector<Machine> machines_;
    vector<Job>     jobs_;
    int                  nextGlobalId_ = 0;

    vector<int> opJob_;   // globalId -> owning job index
    vector<int> opPos_;   // globalId -> position within that job
};

} // namespace fjs
