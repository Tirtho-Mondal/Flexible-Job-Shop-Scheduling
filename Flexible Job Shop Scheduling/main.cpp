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
#include "AlgorithmConfig.h"

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <cstdlib>
#include <cctype>
#include <functional>
#include <fstream>
#include <set>
#include <system_error>

#ifdef _WIN32
// Minimal declaration instead of <windows.h>, which clashes with std::byte under
// `using namespace std;` (ambiguous-symbol errors). GetModuleFileNameA lives in
// kernel32, linked by default.
extern "C" __declspec(dllimport) unsigned long __stdcall
GetModuleFileNameA(void* hModule, char* lpFilename, unsigned long nSize);
#endif

using namespace std;
namespace fs = filesystem;
using namespace fjs;

static string toLower(string s) {
    for (char& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

// Folder that contains the running executable - so data/ and the *.txt settings are
// found regardless of the working directory the program was launched from (e.g. when
// Visual Studio runs the exe from a build sub-folder).
static fs::path exeDirectory() {
#ifdef _WIN32
    char buf[1024];
    unsigned long n = GetModuleFileNameA(nullptr, buf, (unsigned long)sizeof(buf));
    if (n > 0 && n < sizeof(buf)) return fs::path(string(buf, n)).parent_path();
#endif
    return {};
}

// Find a file by name, starting at each `start` directory and walking up to `levels`
// parents. Returns the first existing match, or an empty path.
static fs::path findFileUpwards(const string& name, const vector<fs::path>& starts, int levels = 6) {
    for (const fs::path& s : starts) {
        if (s.empty()) continue;
        error_code ec;
        fs::path d = fs::absolute(s, ec);
        if (ec) continue;
        for (int i = 0; i <= levels; ++i) {
            const fs::path cand = d / name;
            if (fs::is_regular_file(cand)) return cand;
            if (!d.has_parent_path() || d.parent_path() == d) break;
            d = d.parent_path();
        }
    }
    return {};
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
    // Walk up from BOTH the current directory and the executable's directory,
    // looking for a data/ (or input/) folder that actually contains instances.
    for (const fs::path& start : { fs::current_path(), exeDirectory() }) {
        if (start.empty()) continue;
        error_code ec;
        fs::path d = fs::absolute(start, ec);
        if (ec) continue;
        for (int i = 0; i <= 6; ++i) {
            for (const char* folder : { "data", "input" }) {
                const fs::path cand = d / folder;
                if (fs::is_directory(cand)) {
                    for (auto& entry : fs::recursive_directory_iterator(cand, ec))
                        if (!ec && entry.is_regular_file() && looksLikeInstance(entry.path()))
                            return cand;
                }
            }
            if (!d.has_parent_path() || d.parent_path() == d) break;
            d = d.parent_path();
        }
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

// ---- optional dataset selector (DatasetSetting.txt) -----------------------
// If a DatasetSetting.txt is present, only the datasets it names are executed.
// Look for it next to the exe / data folder (or wherever FJS_DATASET points).
static fs::path locateDatasetSetting(const fs::path& dataDir) {
    if (const char* env = getenv("FJS_DATASET")) {
        fs::path p(env);
        if (fs::is_regular_file(p)) return p;
    }
    // Search from the current dir, the exe's dir, the data folder and its parent,
    // walking up parents - so it's found wherever the exe is launched from.
    const vector<fs::path> starts = {
        fs::current_path(), exeDirectory(), dataDir, dataDir.parent_path()
    };
    return findFileUpwards("DatasetSetting.txt", starts);
}

// Split a string on commas / whitespace into lower-cased tokens.
static vector<string> splitTokens(const string& s) {
    vector<string> out;
    string t;
    for (char c : s) {
        if (c == ',' || isspace((unsigned char)c)) { if (!t.empty()) { out.push_back(toLower(t)); t.clear(); } }
        else t.push_back(c);
    }
    if (!t.empty()) out.push_back(toLower(t));
    return out;
}

// Read the wanted datasets. The file is one name per line, grouped under
// "# Folder" headers:
//        # Hurink/edata
//        abz5
//        abz6
// A comment line that is a SINGLE token (e.g. "# Hurink/edata") sets the folder
// SCOPE for the bare names beneath it, so "abz5" there means Hurink/edata/abz5.
// Prose comments (with spaces) and blank lines are ignored. A name that already
// contains a '/' is taken as written. Names are lower-cased.
static set<string> readDatasetTokens(const fs::path& file) {
    set<string> tokens;
    ifstream in(file);
    string line, scope;
    while (getline(in, line)) {
        const size_t hash = line.find('#');
        const vector<string> code = splitTokens(hash == string::npos ? line : line.substr(0, hash));
        if (code.empty()) {                                  // comment-only or blank line
            if (hash != string::npos) {
                const vector<string> cmt = splitTokens(line.substr(hash + 1));
                if (cmt.size() == 1) scope = cmt[0];         // "# Folder" header sets the scope
            }
            continue;
        }
        for (const string& tok : code) {                     // real names on this line
            const bool qualified = tok.find_first_of("/\\:") != string::npos;
            tokens.insert((!qualified && !scope.empty()) ? scope + "/" + tok : tok);
        }
    }
    return tokens;
}

// True if `file` belongs to a requested dataset. A token may be:
//   * a plain name        -> matches the instance's name (stem) OR any folder in
//                            its path, e.g. "brandimarte", "vdata", "Mk01".
//   * "folder/instance"    -> matches that instance only inside that folder, and
//   * "a/b/instance"        -> nested: every "a","b" must be a folder on the path
//                            and the stem must equal the last segment, e.g.
//                            "hurink/edata/abz5" (sub-folders that share names).
static bool matchesDataset(const fs::path& file, const set<string>& tokens) {
    const string stem = toLower(file.stem().string());
    vector<string> parts;
    for (const auto& part : file) parts.push_back(toLower(part.string()));
    auto hasFolder = [&](const string& f) {
        for (const string& p : parts) if (p == f) return true;
        return false;
    };
    for (const string& t : tokens) {
        // split the token into segments on '/', '\\' or ':'
        vector<string> seg; string s;
        for (char c : t) {
            if (c == '/' || c == '\\' || c == ':') { if (!s.empty()) { seg.push_back(s); s.clear(); } }
            else s.push_back(c);
        }
        if (!s.empty()) seg.push_back(s);
        if (seg.empty()) continue;

        if (seg.size() == 1) {                           // plain: stem OR any folder
            if (stem == seg[0] || hasFolder(seg[0])) return true;
        } else {                                          // folder(s)/instance
            if (stem != seg.back()) continue;
            bool ok = true;
            for (size_t i = 0; i + 1 < seg.size(); ++i) if (!hasFolder(seg[i])) { ok = false; break; }
            if (ok) return true;
        }
    }
    return false;
}

// ---- optional algorithm parameters (AlgorithmSetting.txt) -----------------
// "key value" or "key = value" per line; '#' comments and blanks ignored. Looks
// next to the exe / data folder (or wherever FJS_ALGO points). Missing file or
// missing keys keep the built-in defaults.
static AlgorithmConfig loadAlgorithmConfig(const fs::path& dataDir, fs::path& usedFile) {
    AlgorithmConfig cfg;
    fs::path file;
    if (const char* env = getenv("FJS_ALGO")) { fs::path p(env); if (fs::is_regular_file(p)) file = p; }
    if (file.empty()) {
        const vector<fs::path> starts = {
            fs::current_path(), exeDirectory(), dataDir, dataDir.parent_path()
        };
        file = findFileUpwards("AlgorithmSetting.txt", starts);
    }
    usedFile = file;
    if (file.empty()) return cfg;

    ifstream in(file);
    string line;
    while (getline(in, line)) {
        const size_t hash = line.find('#');
        if (hash != string::npos) line.erase(hash);
        for (char& c : line) if (c == '=') c = ' ';
        const vector<string> tok = splitTokens(line);   // [key, value, ...]
        if (tok.size() < 2) continue;
        const string& k = tok[0]; const string& v = tok[1];
        try {
            if      (k == "alpha")            cfg.alpha           = stod(v);
            else if (k == "beta")             cfg.beta            = stod(v);
            else if (k == "gamma")            cfg.gamma           = stod(v);
            else if (k == "delta")            cfg.delta           = stod(v);
            else if (k == "tau")              cfg.tau             = stod(v);
            else if (k == "acceptance")       cfg.selfish         = (v == "selfish") ? 1 : 0;
            else if (k == "selfish")          cfg.selfish         = stoi(v);
            else if (k == "inertia")          cfg.inertia         = stod(v);
            else if (k == "crossover")        cfg.crossover       = stoi(v);
            else if (k == "crossover_type")   cfg.crossoverType   = (v == "pox") ? 0 : (v == "oox") ? 2 : 1;
            else if (k == "runs")             cfg.runs            = stoi(v);
            else if (k == "belief_pool")      cfg.beliefPool      = stoi(v);
            else if (k == "ils_patience")     cfg.ilsPatienceBase = stoi(v);
            else if (k == "ils_patience_div") cfg.ilsPatienceDiv  = stoi(v);
            else if (k == "kick_min")         cfg.kickMin         = stoi(v);
            else if (k == "kick_div")         cfg.kickDiv         = stoi(v);
            else if (k == "trace_rows")       cfg.traceRows       = stoi(v);
        } catch (...) { /* ignore a malformed value, keep the default */ }
    }
    return cfg;
}

int main() {
    const fs::path dataDir = locateDataFolder();
    if (dataDir.empty()) {
        cerr << "Couldn't find a data/ folder with .fjs or .txt instances.\n"
                "Point FJS_DATA at one and try again.\n";
        return 1;
    }
    // Keep the results next to the instances: output/ is created as a SIBLING of
    // the data/ folder (same parent), not in the current working directory.
    const fs::path outDir = fs::absolute(dataDir).parent_path() / "output";
    fs::create_directories(outDir);
    fs::create_directories(outDir / "ganchart");

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

    // If DatasetSetting.txt names one or more datasets, keep only those; an empty
    // or absent file means "run everything" (the default).
    const fs::path settingFile = locateDatasetSetting(dataDir);
    const set<string> wanted = settingFile.empty() ? set<string>{}
                                                   : readDatasetTokens(settingFile);
    if (!wanted.empty()) {
        const size_t before = instances.size();
        instances.erase(remove_if(instances.begin(), instances.end(),
            [&](const fs::path& f){ return !matchesDataset(f, wanted); }), instances.end());
        cout << "Dataset filter: " << settingFile.string() << "  (";
        bool first = true;
        for (const string& t : wanted) { cout << (first ? "" : ", ") << t; first = false; }
        cout << ")\n"
             << "Selected " << instances.size() << " of " << before << " instances.\n\n";
        if (instances.empty()) {
            cerr << "No instances match DatasetSetting.txt - check the names against the\n"
                    "data/ folder (e.g. brandimarte, vdata, hurink, or an instance like Mk01).\n";
            return 1;
        }
    } else {
        cout << "Dataset filter: " << (settingFile.empty()
                ? "(DatasetSetting.txt not found - running ALL instances)"
                : settingFile.string() + " (empty - running ALL instances)") << "\n";
    }

    // Load all tunable parameters (payoff weights + search control).
    fs::path algoFile;
    const AlgorithmConfig algo = loadAlgorithmConfig(dataDir, algoFile);

    cout << "Data folder : " << dataDir.string() << "\n"
         << "Output dir  : " << outDir.string() << "\n"
         << "Instances   : " << instances.size() << "\n"
         << "Algo config : " << (algoFile.empty() ? string("(built-in defaults)") : algoFile.string()) << "\n"
         << "  mode     : " << (algo.selfish ? "PURE-SELFISH non-cooperative game"
                                              : "coordinated makespan engine") << "\n"
         << "  payoff   : alpha=" << algo.alpha << " beta=" << algo.beta
         << " gamma=" << algo.gamma << " delta=" << algo.delta << " tau=" << algo.tau << "\n"
         << "  perturb  : " << (algo.crossover
                ? (algo.crossoverType == 0 ? "CROSSOVER=POX (memetic) + light kick"
                 : algo.crossoverType == 2 ? "CROSSOVER=OOX one-point (memetic) + light kick"
                 :                           "CROSSOVER=OUX payoff-guided (memetic) + light kick")
                : "random kick (ILS)") << "\n"
         << "  search   : runs=" << algo.runs << " belief_pool=" << algo.beliefPool
         << " ils_patience=" << algo.ilsPatienceBase << "(+ops/" << algo.ilsPatienceDiv << ")"
         << " kick=max(" << algo.kickMin << ",ops/" << algo.kickDiv << ")"
         << " trace_rows=" << algo.traceRows << "\n\n";

    FjsInstanceReader reader;
    PayoffFunction    payoff(algo.alpha, algo.beta, algo.gamma, algo.delta, algo.tau);
    BestKnownRegistry literature;
    GlobalReport      summary((outDir / "allresult.txt").string());

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
            GameSolver solver(inst, payoff, seed, algo);
            const SolveResult result = solver.solve();

            const int bks = literature.lookup(family, name);
            InstanceReport::write((outDir / (family + "_" + name + "_log.txt")).string(),
                                  inst, result, payoff, bks, algo.selfish != 0);
            summary.append(inst, result, bks);

            // Gantt chart of the best schedule (one colour per job).
            Schedule best = ScheduleBuilder::build(inst, result.bestState);
            GanttChart::write((outDir / "ganchart" / (family + "_" + name + ".svg")).string(), inst, best);

            cout << "Cmax=" << result.bestMakespan;
            if (bks >= 0) cout << " (BKS " << bks << ")";
            cout << "\n";
        } catch (const exception& ex) {
            // One bad file shouldn't sink the whole batch - report it and move on.
            cout << "ERROR: " << ex.what() << "\n";
        }
    }

    summary.writeReadme((outDir / "README.md").string());
    CodeExplanation::write((outDir / "code_explanation.md").string(), payoff);

    cout << "\nAll done - see " << outDir.string()
         << " (allresult.txt, README.md, per-instance logs).\n";
    return 0;
}
