// ============================================================================
//  BestKnownRegistry.cpp - the embedded literature best-known table.
// ============================================================================
#include "BestKnownRegistry.h"
#include <cctype>
#include <vector>

using namespace std;

namespace fjs {

string BestKnownRegistry::canonicalName(const string& name) {
    string lower;
    for (char c : name) lower += (char)tolower((unsigned char)c);
    // split into alphabetic prefix and trailing digits
    size_t i = 0;
    while (i < lower.size() && !isdigit((unsigned char)lower[i])) ++i;
    string prefix = lower.substr(0, i);
    string digits = lower.substr(i);
    if (digits.empty()) return prefix;
    long n = 0; for (char c : digits) if (isdigit((unsigned char)c)) n = n * 10 + (c - '0');
    return prefix + to_string(n);
}

void BestKnownRegistry::add(const string& group, const string& name, int value) {
    table_[group + "/" + canonicalName(name)] = value;
}

int BestKnownRegistry::lookup(const string& group, const string& instanceName) const {
    auto it = table_.find(group + "/" + canonicalName(instanceName));
    return it == table_.end() ? -1 : it->second;
}

BestKnownRegistry::BestKnownRegistry() {
    // ---- Brandimarte BRData (Mk01..Mk15) -------------------------------
    const int mk[15] = {40,26,204,60,172,57,139,523,307,193,609,508,386,694,333};
    for (int i = 0; i < 15; ++i) add("brandimarte", "mk" + to_string(i + 1), mk[i]);

    // Helper to load a whole Hurink group (la1..la40, mt6/10/20, abz5..9,
    // car1..8, orb1..10) from arrays of BKS values.
    auto loadGroup = [&](const string& g,
                         const vector<int>& la,   // 40 values
                         int mt6, int mt10, int mt20,
                         const vector<int>& abz,  // abz5..abz9
                         const vector<int>& car,  // car1..car8
                         const vector<int>& orb)  // orb1..orb10
    {
        for (int i = 0; i < (int)la.size(); ++i) add(g, "la" + to_string(i + 1), la[i]);
        // Fisher-Thompson 6x6 / 10x10 / 20x5 - shipped under several names
        // (mt06/mt10/mt20, ft06/ft10/ft20, m06/m10/m20): register all aliases.
        for (const char* pfx : {"mt", "ft", "m"}) {
            add(g, string(pfx) + "6", mt6);
            add(g, string(pfx) + "10", mt10);
            add(g, string(pfx) + "20", mt20);
        }
        for (int i = 0; i < (int)abz.size(); ++i) add(g, "abz" + to_string(i + 5), abz[i]);
        for (int i = 0; i < (int)car.size(); ++i) add(g, "car" + to_string(i + 1), car[i]);
        for (int i = 0; i < (int)orb.size(); ++i) add(g, "orb" + to_string(i + 1), orb[i]);
    };

    // ---- Hurink edata --------------------------------------------------
    loadGroup("edata",
        {609,655,550,568,503,833,762,845,878,866,1103,960,1053,1123,1111,
         892,707,842,796,857,1009,880,950,908,936,1106,1181,1142,1107,1193,
         1532,1698,1547,1599,1736,1160,1397,1141,1184,1144},
        55, 871, 1088,
        {1167,925,610,637,644},
        {6176,6327,6856,7789,7229,7990,6123,7689},
        {977,865,951,984,842,958,387,894,933,933});

    // ---- Hurink rdata --------------------------------------------------
    loadGroup("rdata",
        {571,529,477,502,457,799,749,765,853,804,1071,936,1038,1070,1089,
         717,646,666,700,756,829,753,832,801,782,1059,1087,1077,996,1072,
         1520,1658,1498,1535,1550,1023,1062,954,1011,955},
        47, 686, 1022,
        {954,807,527,540,539},
        {5035,5986,5623,6515,5615,6147,4425,5692},
        {746,696,712,753,639,754,302,639,694,742});

    // ---- Hurink vdata --------------------------------------------------
    loadGroup("vdata",
        {570,529,477,502,457,799,749,765,853,804,1071,936,1038,1070,1089,
         717,646,663,617,756,802,734,811,775,753,1053,1084,1069,994,1069,
         1520,1658,1497,1535,1549,948,986,943,922,955},
        47, 655, 1022,
        {859,742,495,510,499},
        {5005,5929,5598,6514,4913,5486,4281,4613},
        {695,620,648,753,584,715,275,573,659,681});
}

} // namespace fjs
