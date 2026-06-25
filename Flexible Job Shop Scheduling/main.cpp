#define _CRT_SECURE_NO_WARNINGS   // we use getenv() below

// Entry point for the JC-NCGS scheduler.
//
// There's nothing to type in: point it at a folder of FJSSP instances (the
// data/ folder by default, or wherever FJS_DATA says) and it solves every
// instance it finds, writing the reports into output/. Each instance gets its
// own log; allresult.txt collects the makespan-vs-best-known comparison.

#include "FjsInstanceReader.h"
#include "GameSolver.h"
#include "PayoffFunction.h"
#include "BestKnownRegistry.h"
#include "InstanceReport.h"
#include "GlobalReport.h"
#include "CodeExplanation.h"
#include "ScheduleBuilder.h"
#include "GanttChart.h"

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

static string toLower(string s) {
    for (char& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

// We accept the original .fjs benchmarks as well as the same data saved as .txt.
static bool looksLikeInstance(const fs::path& p) {
    const string ext = toLower(p.extension().string());
    return ext == ".fjs" || ext == ".txt";
}

// Where do the instances live? Use FJS_DATA if it's set; otherwise walk up a few
// directories looking for a data/ (or input/) folder that actually contains some.
static fs::path locateDataFolder() {
    if (const char* env = getenv("FJS_DATA")) {
        fs::path p(env);
        if (fs::exists(p)) return p;
    }
    for (const char* guess : { "data", "../data", "../../data", "../../../data", "../../../../data",
                               "input", "../input", "../../input", "../../../input", "../../../../input" }) {
        fs::path p(guess);
        if (!fs::is_directory(p)) continue;
        for (auto& entry : fs::recursive_directory_iterator(p))
            if (entry.is_regular_file() && looksLikeInstance(entry.path()))
                return p;
    }
    return {};
}

// The benchmark family is essentially the folder the file sits in. We use it to
// look up the best-known value and to prefix the per-instance report name.
static string familyOf(const fs::path& file) {
    for (const auto& part : file) {
        const string name = toLower(part.string());
        if (name == "edata" || name == "rdata" || name == "sdata" || name == "vdata") return name;
        if (name == "brandimarte") return "brandimarte";
        if (name.find("rcfjssp") != string::npos) return "rcfjssp";
    }
    return file.parent_path().filename().string();
}

// Pull the trailing number out of a name so that, e.g., mk2 sorts before mk10.
static int instanceNumber(const string& name) {
    int value = 0;
    bool seenDigit = false;
    for (char c : name) {
        if (isdigit((unsigned char)c)) { value = value * 10 + (c - '0'); seenDigit = true; }
        else if (seenDigit) break;        // first non-digit after the number ends it
    }
    return seenDigit ? value : 0;
}

int main() {
    const fs::path dataDir = locateDataFolder();
    if (dataDir.empty()) {
        cerr << "Couldn't find a data/ folder with .fjs or .txt instances.\n"
                "Point FJS_DATA at one and try again.\n";
        return 1;
    }
    fs::create_directories("output");
    fs::create_directories("output/ganchart");

    // Grab every instance file, then order them family-by-family and in natural
    // numeric order so the console output and tables read sensibly.
    vector<fs::path> instances;
    for (auto& entry : fs::recursive_directory_iterator(dataDir))
        if (entry.is_regular_file() && looksLikeInstance(entry.path()))
            instances.push_back(entry.path());

    sort(instances.begin(), instances.end(), [](const fs::path& a, const fs::path& b) {
        const string fa = familyOf(a), fb = familyOf(b);
        if (fa != fb) return fa < fb;
        return instanceNumber(a.stem().string()) < instanceNumber(b.stem().string());
    });

    cout << "Data folder : " << dataDir.string() << "\n"
         << "Instances   : " << instances.size() << "\n\n";

    FjsInstanceReader reader;
    PayoffFunction    payoff;
    BestKnownRegistry literature;
    GlobalReport      summary("output/allresult.txt");

    int index = 0;
    for (const fs::path& file : instances) {
        const string family = familyOf(file);
        const string name   = file.stem().string();
        cout << "[" << ++index << "/" << instances.size() << "] "
             << family << "/" << name << " ... " << flush;

        try {
            Instance inst = reader.read(file.string(), family);

            // Same instance name -> same seed, so re-running reproduces the numbers.
            const unsigned seed = (unsigned)(hash<string>{}(family + "/" + name) ^ 0x9E3779B9u);
            GameSolver solver(inst, payoff, seed);
            const SolveResult result = solver.solve();

            const int bks = literature.lookup(family, name);
            InstanceReport::write("output/" + family + "_" + name + "_log.txt",
                                  inst, result, payoff, bks);
            summary.append(inst, result, bks);

            // Gantt chart of the best schedule (one colour per job).
            Schedule best = ScheduleBuilder::build(inst, result.bestState);
            GanttChart::write("output/ganchart/" + family + "_" + name + ".svg", inst, best);

            cout << "Cmax=" << result.bestMakespan;
            if (bks >= 0) cout << " (BKS " << bks << ")";
            cout << "\n";
        } catch (const exception& ex) {
            // One bad file shouldn't sink the whole batch - report it and move on.
            cout << "ERROR: " << ex.what() << "\n";
        }
    }

    summary.writeReadme("output/README.md");
    CodeExplanation::write("output/code_explanation.md", payoff);

    cout << "\nAll done - see output/ (allresult.txt, README.md, per-instance logs).\n";
    return 0;
}
