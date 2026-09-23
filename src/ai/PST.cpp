#include "PST.hpp"

#include <algorithm>
#include <atomic>
#include <fstream>
#include <mutex>
#include <sstream>

namespace draughts::ai::pst {

namespace {

// Two independent weight tables, one per rule variant. Initialized lazily
// from defaultWeights() the first time active() is called. Loading a custom
// weights file overwrites one of these in place ? safe because loading
// happens before any search starts.
std::array<Weights, 2>& tables() {
    static std::array<Weights, 2> t{
        Weights{},
        Weights{},
    };
    static std::once_flag init;
    std::call_once(init, [&]{
        t[0] = defaultWeights(core::RuleSet::InternationalMaxCapture);
        t[1] = defaultWeights(core::RuleSet::InternationalFreeCapture);
    });
    return t;
}

std::atomic<int>& activeIndex() {
    static std::atomic<int> idx{0};
    return idx;
}

std::size_t indexOf(core::RuleSet rules) noexcept {
    return rules == core::RuleSet::InternationalFreeCapture ? 1u : 0u;
}

// Iterate a value list into a fixed-size array. Returns count actually filled.
std::size_t fillFromStream(std::istream& in,
                            std::array<int, core::kNumPlayableSquares>& out)
{
    std::size_t n = 0;
    int v;
    while (n < out.size() && (in >> v)) out[n++] = v;
    return n;
}

} // namespace

Weights defaultWeights(core::RuleSet rules) {
    // Both variants start from the same hand-tuned values. The distinction
    // matters only after a tuner has overwritten one of them.
    (void)rules;

    Weights w{};

    // Men ? advance toward promotion, penalise camping on the back rank.
    const int menDefaults[core::kNumPlayableSquares] = {
        // row 0 (Red's back rank ? defensive)
        -10, -10, -10, -10, -10,
        // row 1
         -6,  -6,  -6,  -6,  -6,
        // row 2
         -2,   0,  -2,   0,  -2,
        // row 3
          2,   2,   4,   2,   2,
        // row 4 (centre)
          6,   6,   6,   6,   6,
        // row 5
         10,  10,  10,  10,  10,
        // row 6
         14,  14,  14,  14,  14,
        // row 7
         18,  18,  18,  18,  18,
        // row 8
         24,  24,  24,  24,  24,
        // row 9 (promotion row ? should be King, but just in case)
         30,  30,  30,  30,  30,
    };
    for (std::size_t i = 0; i < core::kNumPlayableSquares; ++i)
        w.men[i] = menDefaults[i];

    // Kings ? small centralisation bonus.
    const int kingDefaults[core::kNumPlayableSquares] = {
         6,  8, 10,  8,  6,
         8, 10, 12, 10,  8,
        10, 12, 14, 12, 10,
        10, 12, 14, 12, 10,
        10, 12, 14, 12, 10,
        10, 12, 14, 12, 10,
        10, 12, 14, 12, 10,
        10, 12, 14, 12, 10,
         8, 10, 12, 10,  8,
         6,  8, 10,  8,  6,
    };
    for (std::size_t i = 0; i < core::kNumPlayableSquares; ++i)
        w.kings[i] = kingDefaults[i];

    return w;
}

void setActive(core::RuleSet rules) noexcept {
    activeIndex().store(static_cast<int>(indexOf(rules)),
                        std::memory_order_relaxed);
}

const Weights& active() noexcept {
    return tables()[activeIndex().load(std::memory_order_relaxed)];
}

Weights& mutableWeightsFor(core::RuleSet rules) noexcept {
    return tables()[indexOf(rules)];
}

Weights loadFromFile(const std::string& path, bool& ok) {
    ok = false;

    std::ifstream in(path);
    if (!in) return defaultWeights(core::RuleSet::InternationalMaxCapture);

    Weights w{};
    bool haveMen   = false;
    bool haveKings = false;

    std::string line;
    while (std::getline(in, line)) {
        // Strip comments and trim.
        const auto hashPos = line.find('#');
        if (hashPos != std::string::npos) line.resize(hashPos);

        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        // Trim leading/trailing whitespace from key.
        while (!key.empty() && std::isspace(static_cast<unsigned char>(key.back())))  key.pop_back();
        while (!key.empty() && std::isspace(static_cast<unsigned char>(key.front()))) key.erase(key.begin());

        std::istringstream vs(val);
        if (key == "men") {
            if (fillFromStream(vs, w.men) == w.men.size()) haveMen = true;
        } else if (key == "kings") {
            if (fillFromStream(vs, w.kings) == w.kings.size()) haveKings = true;
        }
    }

    ok = haveMen && haveKings;
    return ok ? w : defaultWeights(core::RuleSet::InternationalMaxCapture);
}

void saveToFile(const std::string& path, const Weights& w) {
    std::ofstream out(path);
    if (!out) return;

    out << "# Draughts PST weights\n";
    out << "# Format: 50 space-separated integers per array, row-major from\n";
    out << "# Red's back rank (row 0) to row 9.\n";
    out << "# Yellow is mirrored via bc::mirrorSquare().\n\n";

    auto write = [&](const char* name,
                     const std::array<int, core::kNumPlayableSquares>& a) {
        out << name << " = ";
        for (std::size_t i = 0; i < a.size(); ++i) {
            out << a[i];
            if (i + 1 < a.size()) out << ' ';
        }
        out << '\n';
    };

    write("men",   w.men);
    write("kings", w.kings);
}

} // namespace draughts::ai::pst
