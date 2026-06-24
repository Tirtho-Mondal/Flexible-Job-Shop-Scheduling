#define _CRT_SECURE_NO_WARNINGS
// ============================================================================
//  main.cpp - driver of the game-theoretic FJSSP solver.
//  ---------------------------------------------------------------------------
//  Takes NO console input.  On start-up it:
//    1. locates the data/ folder (override with the FJS_DATA env var),
//    2. (re)creates output/allresult.txt,
//    3. recursively finds every *.fjs instance,
//    4. for each one: parse -> random init -> selfish best-response game ->
//       write output/<group>_<name>_log.txt and append to output/allresult.txt,
//    5. writes output/README.md and output/code_explanation.md.
//
//  Build (from this folder) without Visual Studio, if desired:
//      g++  -std=c++17 -O2 *.cpp -o fjs_game
//      cl   /EHsc /std:c++17 /O2 *.cpp /Fe:fjs_game.exe
// ============================================================================

#include "FjsInstanceReader.h"
#include "GameSolver.h"
#include "PayoffFunction.h"
#include "BestKnownRegistry.h"
#include "InstanceReport.h"
#include "GlobalReport.h"
#include "CodeExplanation.h"

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <cstdlib>
#include <cctype>
#include <functional>

using namespace std;

namespace fs = filesystem;
using namespace fjs;

// Lower-case copy of a string (for case-insensitive path matching).
static string lower(string s) {
    for (char& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

// An instance file is any .fjs or .txt (the benchmarks ship under both names).
static bool isInstanceFile(const fs::path& p) {
    string ext = lower(p.extension().string());
    return ext == ".fjs" || ext == ".txt";
}

// Find the data folder: honour FJS_DATA, otherwise probe a few likely places.
static fs::path findDataFolder() {
    if (const char* e = getenv("FJS_DATA")) {
        fs::path p(e);
        if (fs::exists(p)) return p;
    }
    const char* candidates[] = {
        "data", "../data", "../../data", "../../../data", "../../../../data",
        "input", "../input", "../../input", "../../../input", "../../../../input"
    };
    for (const char* c : candidates) {
        fs::path p(c);
        if (fs::exists(p) && fs::is_directory(p)) {
            for (auto& e : fs::recursive_directory_iterator(p))
                if (e.is_regular_file() && isInstanceFile(e.path()))
                    return p;
        }
    }
    return fs::path();
}

// Decide which benchmark family a file belongs to (drives the best-known lookup
// and the report file name).
static string groupOf(const fs::path& file) {
    for (const auto& part : file) {
        string p = lower(part.string());
        if (p == "edata" || p == "rdata" || p == "sdata" || p == "vdata") return p;
        if (p == "brandimarte") return "brandimarte";
        if (p.find("rcfjssp") != string::npos) return "rcfjssp";
    }
    return file.parent_path().filename().string();
}

// Trailing integer of an instance name, for natural sorting within a group.
static int trailingNumber(const string& s) {
    int v = 0; bool any = false;
    for (char c : s) {
        if (isdigit((unsigned char)c)) { v = v * 10 + (c - '0'); any = true; }
        else if (any) break;
    }
    return any ? v : 0;
}

int main() {
    fs::path dataDir = findDataFolder();
    if (dataDir.empty()) {
        cerr << "ERROR: could not find a 'data' folder containing .fjs files.\n"
                     "Set the FJS_DATA environment variable to its path.\n";
        return 1;
    }
    fs::create_directories("output");

    // gather instances
    vector<fs::path> files;
    for (auto& e : fs::recursive_directory_iterator(dataDir))
        if (e.is_regular_file() && isInstanceFile(e.path()))
            files.push_back(e.path());
    sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b) {
        string ga = groupOf(a), gb = groupOf(b);
        if (ga != gb) return ga < gb;
        return trailingNumber(a.stem().string()) < trailingNumber(b.stem().string());
    });

    cout << "Data folder : " << dataDir.string() << "\n";
    cout << "Instances   : " << files.size() << "\n\n";

    FjsInstanceReader reader;
    PayoffFunction    payoff;
    BestKnownRegistry bestKnown;
    GlobalReport      global("output/allresult.txt");

    int done = 0;
    for (const fs::path& file : files) {
        const string group = groupOf(file);
        const string stem  = file.stem().string();
        cout << "[" << ++done << "/" << files.size() << "] "
                  << group << "/" << stem << " ... " << flush;
        try {
            Instance inst = reader.read(file.string(), group);

            // Reproducible per-instance seed (deterministic across runs).
            unsigned seed = (unsigned)(hash<string>{}(group + "/" + stem) ^ 0x9E3779B9u);
            GameSolver solver(inst, payoff, seed);
            SolveResult result = solver.solve();

            int bk = bestKnown.lookup(group, stem);
            InstanceReport::write("output/" + group + "_" + stem + "_log.txt",
                                  inst, result, payoff, bk);
            global.append(inst, result, bk);

            cout << "Cmax=" << result.bestMakespan;
            if (bk >= 0) cout << " (BKS " << bk << ")";
            cout << "\n";
        } catch (const exception& ex) {
            cout << "ERROR: " << ex.what() << "\n";
        }
    }

    global.writeReadme("output/README.md");
    CodeExplanation::write("output/code_explanation.md", payoff);

    cout << "\nDone. See output/allresult.txt, output/README.md and the per-instance logs.\n";
    return 0;
}
