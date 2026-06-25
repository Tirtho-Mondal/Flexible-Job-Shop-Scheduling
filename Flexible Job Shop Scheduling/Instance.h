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

    // ---- Identity (public data) ----------------------------------------
    string name;
    string group;

    // ---- Builder API used by the reader --------------------------------
    void  setName(const string& n)  { name = n; }
    void  setGroup(const string& g) { group = g; }
    void  setMachineCount(int m);
    Job&  addJob();                 // appends a new player
    int   nextGlobalOperationId() const { return nextGlobalId; }
    void  bumpGlobalOperationId() { ++nextGlobalId; }
    void  finalise();               // build the flat operation index

    // ---- Sizes ---------------------------------------------------------
    int numJobs()         const { return (int)jobs.size(); }
    int numMachines()     const { return (int)machines.size(); }
    int totalOperations() const { return nextGlobalId; }

    // ---- Access --------------------------------------------------------
    Job&             job(int i)              { return jobs[i]; }
    const Job&       job(int i)        const { return jobs[i]; }
    const Machine&   machine(int m)    const { return machines[m]; }

    // Resolve a flat operation id back to its owning Operation object.
    Operation&       operationByGlobalId(int gid);
    const Operation& operationByGlobalId(int gid) const;
    int globalIdJob(int gid)      const { return opJob[gid]; }
    int globalIdPosition(int gid) const { return opPos[gid]; }

    // A loose horizon used to normalise / bound times: the total work of the
    // shop, which is always an upper bound on any sensible makespan.
    int totalWork() const;

private:
    vector<Machine> machines;
    vector<Job>     jobs;
    int                  nextGlobalId = 0;

    vector<int> opJob;   // globalId -> owning job index
    vector<int> opPos;   // globalId -> position within that job
};

} // namespace fjs
