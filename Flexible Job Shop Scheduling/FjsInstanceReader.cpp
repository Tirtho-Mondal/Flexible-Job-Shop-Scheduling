// ============================================================================
//  FjsInstanceReader.cpp - .fjs parsing for both FJSSP and RCFJSSP layouts.
// ============================================================================
#include "FjsInstanceReader.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <stdexcept>
#include <filesystem>

using namespace std;

namespace fjs {

Instance FjsInstanceReader::read(const string& path, const string& group) const {
    ifstream in(path);
    if (!in) throw runtime_error("cannot open file: " + path);

    // ---- read the header line and count how many numbers it holds --------
    string headerLine;
    while (getline(in, headerLine)) {
        bool blank = headerLine.find_first_not_of(" \t\r\n") == string::npos;
        if (!blank) break;
    }
    vector<double> header;
    { istringstream hs(headerLine); double v; while (hs >> v) header.push_back(v); }
    if (header.size() < 2)
        throw runtime_error("malformed header in: " + path);

    const int numJobs     = (int)header[0];
    const int numMachines = (int)header[1];
    const bool resourceConstrained = header.size() >= 7;

    // ---- read every remaining number into one flat token stream ----------
    vector<double> tok;
    { double v; while (in >> v) tok.push_back(v); }
    size_t p = 0;
    auto next = [&]() -> int {
        if (p >= tok.size()) throw runtime_error("unexpected end of data in: " + path);
        return (int)tok[p++];
    };

    Instance inst;
    inst.setMachineCount(numMachines);
    inst.setName(filesystem::path(path).stem().string());
    inst.setGroup(group);

    if (resourceConstrained) {
        const int numUtil = (int)header[3];
        const int numTool = (int)header[4];
        const int numWip  = (int)header[5];
        const int numArb  = (int)header[6];
        // Skip the resources section: buffer sizes (one per machine), then the
        // utility, tool, WIP-buffer and arbitrary-resource definition lines.
        p += (size_t)numMachines;   // limited-capacity buffer sizes
        p += (size_t)2 * numUtil;   // id + limit
        p += (size_t)2 * numTool;   // id + copies
        p += (size_t)2 * numWip;    // id + capacity
        p += (size_t)4 * numArb;    // id + wipUid + start + max

        for (int j = 0; j < numJobs; ++j) {
            Job& job = inst.addJob();
            const int reqCount = next();  p += (size_t)2 * reqCount;  // required resources
            const int prodCount = next(); p += (size_t)2 * prodCount; // produced resources
            const int numOps = next();
            for (int o = 0; o < numOps; ++o) {
                next();                              // tool UID (ignored)
                const int numAlt = next();
                Operation& op = job.addOperation(inst.nextGlobalOperationId());
                inst.bumpGlobalOperationId();
                for (int a = 0; a < numAlt; ++a) {
                    const int machine = next();
                    const int time    = next();
                    p += (size_t)numUtil;       // utility consumption rates (ignored)
                    op.addAlternative(machine - 1, time);
                }
            }
        }
    } else {
        for (int j = 0; j < numJobs; ++j) {
            Job& job = inst.addJob();
            const int numOps = next();
            for (int o = 0; o < numOps; ++o) {
                const int numAlt = next();
                Operation& op = job.addOperation(inst.nextGlobalOperationId());
                inst.bumpGlobalOperationId();
                for (int a = 0; a < numAlt; ++a) {
                    const int machine = next();
                    const int time    = next();
                    op.addAlternative(machine - 1, time);
                }
            }
        }
    }

    inst.finalise();
    return inst;
}

} // namespace fjs
