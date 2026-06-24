#pragma once
// ============================================================================
//  FjsInstanceReader.h
//  ---------------------------------------------------------------------------
//  Reads a .fjs benchmark file into an Instance.
//
//  It understands the two flavours found in the data folder:
//    * the classic FJSSP format (Brandimarte 1993, Hurink 1994):
//          header = "nJobs nMachines [flexibility]"
//          one line per job = "nOps  (nEligible (machine time)... )..."
//    * the resource-constrained RCFJSSP format (Kasapidis et al. 2025):
//          a 9-number header, a resources section, then richer job lines.
//      For the makespan game we only need each operation's eligible machines
//      and processing times, so the resource information is parsed past and
//      ignored.
//
//  Machine ids in both formats are 1-based and are converted to 0-based here.
// ============================================================================

#include "Instance.h"
#include <string>

using namespace std;

namespace fjs {

class FjsInstanceReader {
public:
    // Read `path`; `group` (e.g. "brandimarte", "edata") is recorded on the
    // Instance so the best-known table can be looked up per benchmark family.
    Instance read(const string& path, const string& group) const;
};

} // namespace fjs
