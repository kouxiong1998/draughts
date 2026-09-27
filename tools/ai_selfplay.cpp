// tools/ai_selfplay.cpp
// AI vs AI self-play: play N games with random openings, print W/L/D summary.
// Used to measure engine health (color bias, draw rate, game length) and as
// the single-process half of version-vs-version comparison.

#include "core/GameEngine.hpp"
#include "core/Notation.hpp"
#include "ai/AIEngine.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <thread>

using namespace draughts;

namespace {

struct Opts {
    int           games        = 10;
    int           maxPlies     = 200;
    int           timeMs       = 500;
    int           openingPlies = 2;
    int           threads      = 4;
    std::uint64_t seed         = 12345;
    bool          maxCapture   = true;
    bool          quiet        = false;
};

void usage(const char* argv0) {
    std::printf(
        "Usage: %s [--games N] [--plies N] [--time MS] [--open N]\n"
        "          [--seed S] [--mode on|off] [--threads N] [--quiet]\n"
        "Defaults: games=10 plies=200 time=500 open=2 seed=12345 mode=on\n",
        argv0);
}

Opts parse(int argc, char** argv) {
    Opts o;
    for (int i = 1; i < argc; ++i) {
        auto need = [&](const char* flag) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "missing value for %s\n", flag);
                std::exit(2);
            }
            return argv[++i];
        };
        if      (!std::strcmp(argv[i], "--games"))  o.games        = std::atoi(need("--games"));
        else if (!std::strcmp(argv[i], "--plies"))  o.maxPlies     = std::atoi(need("--plies"));
        else if (!std::strcmp(argv[i], "--time"))   o.timeMs       = std::atoi(need("--time"));
        else if (!std::strcmp(argv[i], "--open"))   o.openingPlies = std::atoi(need("--open"));
        else if (!std::strcmp(argv[i], "--seed"))   o.seed         = std::strtoull(need("--seed"), nullptr, 10);
        else if (!std::strcmp(argv[i], "--mode")) {
            const char* m = need("--mode");
            o.maxCapture = (std::strcmp(m, "off") != 0);
        }
        else if (!std::strcmp(argv[i], "--threads")) o.threads = std::atoi(need("--threads"));
        else if (!std::strcmp(argv[i], "--quiet"))  o.quiet = true;
        else if (!std::strcmp(argv[i], "-h") || !std::strcmp(argv[i], "--help")) {
            usage(argv[0]); std::exit(0);
        }
        else {
            std::fprintf(stderr, "unknown arg: %s\n", argv[i]);
            usage(argv[0]);
            std::exit(2);
        }
    }
    return o;
}

} // namespace

int main(int argc, char** argv) {
    const Opts o = parse(argc, argv);
    const core::RuleSet rules = o.maxCapture
        ? core::RuleSet::InternationalMaxCapture
        : core::RuleSet::InternationalFreeCapture;

    std::mt19937_64 rng(o.seed);

    ai::AIEngine ai;
    ai.setThreadCount(o.threads);

    ai::TimeBudget budget;
    budget.soft    = std::chrono::milliseconds(o.timeMs);
    budget.hard    = std::chrono::milliseconds(o.timeMs * 3 / 2);
    budget.minimum = std::chrono::milliseconds(std::min(50, o.timeMs / 10));

    std::atomic<bool> done{false};
    core::Move        lastChosen{};
    ai::SearchStats   lastStats{};

    ai.setDoneCallback([&](const core::Move& m, const ai::SearchStats& s) {
        lastChosen = m;
        lastStats  = s;
        done.store(true);
    });

    int redWins = 0, yellowWins = 0, draws = 0, aborted = 0;

    std::printf("ai_selfplay: %d games, %d ms/move, %d opening plies, %s\n\n",
                o.games, o.timeMs, o.openingPlies,
                o.maxCapture ? "MaxCapture=ON" : "MaxCapture=OFF");
    std::fflush(stdout);

    for (int g = 1; g <= o.games; ++g) {
        ai.clearTT();

        core::GameEngine engine;
        engine.newGame(rules, core::Color::Red);

        for (int p = 0; p < o.openingPlies
                       && engine.result() == core::GameResult::Ongoing; ++p) {
            const auto ms = engine.legalMoves();
            if (ms.empty()) break;
            std::uniform_int_distribution<std::size_t> d(0, ms.size() - 1);
            if (!engine.tryApply(ms[d(rng)])) break;
        }

        int ply = 0;
        for (; ply < o.maxPlies
                && engine.result() == core::GameResult::Ongoing; ++ply) {
            const core::Board preBoard = engine.board();
            const core::Color mover    = engine.sideToMove();

            done.store(false);
            ai.think(preBoard, mover, engine.rules(), budget);

            const auto start = std::chrono::steady_clock::now();
            while (!done.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                if (std::chrono::steady_clock::now() - start > std::chrono::seconds(10)) {
                    std::fprintf(stderr, "game %d ply %d: AI timeout\n", g, ply);
                    break;
                }
            }
            ai.stop();

            if (!engine.tryApply(lastChosen)) {
                std::fprintf(stderr, "game %d ply %d: illegal move rejected\n", g, ply);
                aborted++;
                break;
            }

            if (!o.quiet) {
                const auto notation = core::formatMove(preBoard, mover, lastChosen);
                std::printf("  g%-3d ply %3d  %-6s  %-10s  d%2d  s%6d  n%llu\n",
                            g, ply,
                            mover == core::Color::Red ? "Red" : "Yellow",
                            notation.c_str(),
                            lastStats.depth, lastStats.score,
                            (unsigned long long)lastStats.nodes);
                std::fflush(stdout);
            }
        }

        const auto r = engine.result();
        const char* tag = "Aborted";
        if      (r == core::GameResult::RedWins)    { tag = "Red wins";    redWins++;    }
        else if (r == core::GameResult::YellowWins) { tag = "Yellow wins"; yellowWins++; }
        else if (r == core::GameResult::Draw)       { tag = "Draw";        draws++;      }
        else                                        { aborted++;                          }
        std::printf("Game %d/%d: %s after %d plies\n", g, o.games, tag, ply);
        std::fflush(stdout);
    }

    std::printf("\nSummary: %d games | Red %d | Yellow %d | Draw %d | Aborted %d\n",
                o.games, redWins, yellowWins, draws, aborted);
    return 0;
}