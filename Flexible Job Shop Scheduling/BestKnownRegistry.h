#pragma once
// ============================================================================
//  BestKnownRegistry.h
//  ---------------------------------------------------------------------------
//  A small lookup table of best-known / optimal makespans from the literature,
//  so every result can be reported next to the benchmark target and a gap %
//  computed.  The values are the BKS columns of Kasapidis et al. (2025),
//  Appendix B (Brandimarte BRData and Hurink edata/rdata/vdata).
//
//  Instances without a published value here (e.g. Hurink sdata or the
//  resource-constrained RCFJSSP set) simply report "N/A".
//
//  Keys are canonicalised as "<group>/<name>" where the name is lower-cased
//  with leading zeros stripped from its trailing number (so file "Mk01" and
//  table entry "mk1" match, as do "la01" and "la1").
// ============================================================================

#include <string>
#include <unordered_map>

using namespace std;

namespace fjs {

class BestKnownRegistry {
public:
    BestKnownRegistry();

    // Returns the best-known makespan for (group, instanceName), or -1 if the
    // instance is not in the table.
    int lookup(const string& group, const string& instanceName) const;

    // Expose the canonical form so reports can show it if useful.
    static string canonicalName(const string& name);

private:
    void add(const string& group, const string& name, int value);
    unordered_map<string, int> table_;
};

} // namespace fjs
