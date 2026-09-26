// tools/bookgen.cpp
// Opening-book generator. Plays N self-play games at fixed search depth,
// records every (position, chosen move) pair up to a ply limit.
//
// Output format: binary file with 32-byte header + N records (32 bytes each).

#include "core/GameEngine.hpp"
#include "core/Zobrist.hpp"
#include "ai/Search.hpp"
#include "ai/TranspositionTable.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

using namespace draughts;

namespace {

struct Options {
    int           games    = 50000;
    int           depth    = 10;
    int           stopPly  = 16;
    int           threads  = 20;
    std::string   out      = "raw.bin";
    core::RuleSet rules    = core::RuleSet::InternationalMaxCapture;
    std::uint64_t seed     = 0xC0FFEEull;
};

// Compact on-disk record. 32 bytes for cache alignment.
struct Record {
    std::uint64_t hash;
    std::uint64_t captured;
    std::uint8_t  from;
    std::uint8_t  to;
    std::uint8_t  isPromotion;
    std::uint8_t  pad[13];
};
static_assert(sizeof(Record) == 32, "Record must be 32 bytes");

struct FileHeader {
    char          magic[4];    // "DBKG"
    std::uint32_t version;     // 1
    std::uint32_t recordSize;  // sizeof(Record)
    std::uint64_t recordCount;
    std::uint8_t  ruleVariant; // 0 = MaxCapture, 1 = FreeCapture
    std::uint8_t  pad[7];
};
static_assert(sizeof(FileHeader) == 32, "Header must be 32 bytes");

std::uint64_t positionHash(const core::Board& b, core::Color side,
                           core::RuleSet rules) noexcept
{
    std::uint64_t h = b.pieceHash();
    h ^= core::Zobrist::instance().side(side);
    h ^= static_cast<std::uint64_t>(rules) << 61;
    return h;
}

// Play a batch of games. Returns every (position, chosen move) pair
// recorded along the way.
std::vector<Record> playBatch(int                numGames,
                              const Options&     opts,
                              std::atomic<int>*  gamesDone,
                              std::atomic<bool>* abort)
{
    std::vector<Record> out;
    out.reserve(static_cast<std::size_t>(numGames) * opts.stopPly);

    // Per-thread search context. 16 MB TT is plenty for depth-10 search.
    ai::TranspositionTable tt(1u << 20);
    std::atomic<bool>      stopFlag{false};
    ai::Search search(&tt, /*tb=*/nullptr, &stopFlag, /*progress=*/nullptr);

    ai::TimeBudget budget;
    // Generous time ceiling - depth is the only real stopping criterion.
    budget.soft     = std::chrono::seconds(60);
    budget.hard     = std::chrono::seconds(60);
    budget.minimum  = std::chrono::milliseconds(0);
    budget.maxDepth = opts.depth;
    // Randomize among near-best moves to give the book variety.
    budget.randomTopN = 5;
    budget.randomEps  = 30;

    for (int g = 0; g < numGames; ++g) {
        if (abort->load(std::memory_order_relaxed)) break;

        core::GameEngine engine;
        engine.newGame(opts.rules, core::Color::Red);

        for (int ply = 0; ply < opts.stopPly; ++ply) {
            if (engine.result() != core::GameResult::Ongoing) break;

            const std::uint64_t h = positionHash(engine.board(),
                                                  engine.sideToMove(),
                                                  engine.rules());

            const auto stats = search.think(engine.board(),
                                            engine.sideToMove(),
                                            engine.rules(),
                                            budget,
                                            /*threadId=*/0);

            if (!stats.completed) break;

            Record r{};
            r.hash        = h;
            r.from        = stats.bestMove.from;
            r.to          = stats.bestMove.to;
            r.isPromotion = stats.bestMove.isPromotion ? 1 : 0;
            r.captured    = stats.bestMove.captured;
            out.push_back(r);

            if (!engine.tryApply(stats.bestMove)) break;
        }

        if (gamesDone) gamesDone->fetch_add(1, std::memory_order_relaxed);
    }

    return out;
}

Options parseArgs(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto nextInt = [&]() -> int { return (i + 1 < argc) ? std::stoi(argv[++i]) : 0; };
        auto nextStr = [&]() -> std::string { return (i + 1 < argc) ? std::string(argv[++i]) : std::string{}; };

        if      (a == "--games")    o.games    = nextInt();
        else if (a == "--depth")    o.depth    = nextInt();
        else if (a == "--stop-ply") o.stopPly  = nextInt();
        else if (a == "--threads")  o.threads  = nextInt();
        else if (a == "--out")      o.out      = nextStr();
        else if (a == "--seed")     o.seed     = std::stoull(nextStr());
        else if (a == "--rules") {
            const auto v = nextStr();
            if      (v == "on")  o.rules = core::RuleSet::InternationalMaxCapture;
            else if (v == "off") o.rules = core::RuleSet::InternationalFreeCapture;
            else { std::fprintf(stderr, "bad --rules (on|off)\n"); std::exit(2); }
        } else if (a == "--help" || a == "-h") {
            std::printf(
                "usage: bookgen [--games N] [--depth N] [--stop-ply N]\n"
                "               [--threads N] [--out FILE] [--rules on|off] [--seed N]\n");
            std::exit(0);
        } else {
            std::fprintf(stderr, "unknown arg: %s\n", a.c_str());
            std::exit(2);
        }
    }
    return o;
}

} // namespace

int main(int argc, char** argv) {
    const Options o = parseArgs(argc, argv);

    std::printf("bookgen\n");
    std::printf("  games      %d\n", o.games);
    std::printf("  depth      %d\n", o.depth);
    std::printf("  stop-ply   %d\n", o.stopPly);
    std::printf("  threads    %d\n", o.threads);
    std::printf("  out        %s\n", o.out.c_str());
    std::printf("  rules      %s\n",
        o.rules == core::RuleSet::InternationalMaxCapture
            ? "on (majority)" : "off (free capture)");
    std::printf("  seed       0x%llx\n", (unsigned long long)o.seed);
    std::printf("\n");

    std::atomic<int>  gamesDone{0};
    std::atomic<bool> abort{false};

    const int nThreads  = std::max(1, o.threads);
    const int perThread = o.games / nThreads;

    std::vector<std::vector<Record>> results(static_cast<std::size_t>(nThreads));
    std::vector<std::thread>         threads;
    threads.reserve(static_cast<std::size_t>(nThreads));

    const auto t0 = std::chrono::steady_clock::now();

    std::atomic<bool> progressStop{false};
    std::thread progressThread([&]{
        while (!progressStop.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            if (progressStop.load()) break;
            const auto sec = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - t0).count();
            const int done = gamesDone.load();
            const double pct = 100.0 * done / o.games;
            std::printf("  [%llds] %d/%d games (%.1f%%)\n",
                        (long long)sec, done, o.games, pct);
            std::fflush(stdout);
        }
    });

    for (int t = 0; t < nThreads; ++t) {
        const int g = (t == nThreads - 1)
            ? (o.games - perThread * (nThreads - 1))
            : perThread;
        threads.emplace_back([&, t, g]{
            results[static_cast<std::size_t>(t)] =
                playBatch(g, o, &gamesDone, &abort);
        });
    }

    for (auto& th : threads) th.join();
    progressStop.store(true);
    progressThread.join();

    const auto t1  = std::chrono::steady_clock::now();
    const auto sec = std::chrono::duration_cast<std::chrono::seconds>(t1 - t0).count();

    std::size_t total = 0;
    for (const auto& v : results) total += v.size();

    std::printf("\nDone. %zu records in %llds\n", total, (long long)sec);

    std::ofstream f(o.out, std::ios::binary);
    if (!f) { std::fprintf(stderr, "cannot open %s\n", o.out.c_str()); return 1; }

    FileHeader hdr{};
    std::memcpy(hdr.magic, "DBKG", 4);
    hdr.version     = 1;
    hdr.recordSize  = sizeof(Record);
    hdr.recordCount = total;
    hdr.ruleVariant = (o.rules == core::RuleSet::InternationalFreeCapture) ? 1 : 0;
    f.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));

    for (const auto& v : results) {
        f.write(reinterpret_cast<const char*>(v.data()),
                static_cast<std::streamsize>(v.size() * sizeof(Record)));
    }
    f.close();

    std::printf("wrote %s (%zu records)\n", o.out.c_str(), total);
    return 0;
}
