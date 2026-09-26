// tools/bookgen.cpp
// Opening-book generator. Plays N self-play games at fixed search depth,
// records every (position, chosen move) pair up to a ply limit.
//
// Writes incrementally: after each game, records are appended to the
// output file and the file is flushed. If the process is killed or the
// machine restarts, run the same command again and bookgen resumes.
//
// Output format (version 2):
//   Header (32 bytes):
//     magic[4]         = "DBKG"
//     version          = 2
//     recordSize       = 32
//     gamesCompleted   = number of games written to the file so far
//     ruleVariant      = 0 MaxCapture / 1 FreeCapture
//     pad[7]
//   Records (32 bytes each) follow immediately after the header, in
//   completion order. bookmerge reads them until EOF.

#include "core/GameEngine.hpp"
#include "core/Zobrist.hpp"
#include "ai/Search.hpp"
#include "ai/TranspositionTable.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace draughts;

namespace {

struct Options {
    int           games    = 5000;
    int           depth    = 10;
    int           stopPly  = 12;
    int           threads  = 8;
    std::string   out      = "raw.bin";
    core::RuleSet rules    = core::RuleSet::InternationalMaxCapture;
    std::uint64_t seed     = 0xC0FFEEull;
    bool          fresh    = false;
};

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
    char          magic[4];       // "DBKG"
    std::uint32_t version;        // 2
    std::uint32_t recordSize;     // sizeof(Record)
    std::uint64_t gamesCompleted;
    std::uint8_t  ruleVariant;
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

struct SharedWriter {
    std::fstream                file;
    std::mutex                  mtx;
    std::atomic<int>            gamesDone{0};
    std::atomic<bool>           abort{false};
    std::atomic<std::uint64_t>  records{0};
};

void threadWorker(const Options& opts,
                  std::atomic<int>& nextGame,
                  SharedWriter& writer)
{
    ai::TranspositionTable tt(1u << 20);      // 16 MB per worker
    std::atomic<bool>      stopFlag{false};
    ai::Search search(&tt, nullptr, &stopFlag, nullptr);

    ai::TimeBudget budget;
    budget.soft       = std::chrono::seconds(600);
    budget.hard       = std::chrono::seconds(600);
    budget.minimum    = std::chrono::milliseconds(0);
    budget.maxDepth   = opts.depth;
    // Wider randomization (A step): 9 candidates within 60 cp of the best
    // move. This gives the book much broader opening coverage because
    // bookgen's AI explores more distinct opening sequences per game.
    budget.randomTopN = 9;
    budget.randomEps  = 60;

    while (!writer.abort.load(std::memory_order_relaxed)) {
        const int g = nextGame.fetch_add(1, std::memory_order_relaxed);
        if (g >= opts.games) break;

        // Deterministic per-game seed: game N always plays the same way.
        search.setSeed(opts.seed * 1000003ULL +
                       static_cast<std::uint64_t>(g));

        // Alternate starting color so the book covers both Red-first and
        // Yellow-first games. Even games start with Red, odd with Yellow.
        // Without this the book only matches one of the two Settings options.
        core::GameEngine engine;
        const core::Color firstSide =
            (g % 2 == 0) ? core::Color::Red : core::Color::Yellow;
        engine.newGame(opts.rules, firstSide);

        std::vector<Record> records;
        records.reserve(static_cast<std::size_t>(opts.stopPly));

        for (int ply = 0; ply < opts.stopPly; ++ply) {
            if (engine.result() != core::GameResult::Ongoing) break;

            const auto h = positionHash(engine.board(), engine.sideToMove(),
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
            records.push_back(r);

            if (!engine.tryApply(stats.bestMove)) break;
        }

        // Append this game's records to the file (serialized under mutex).
        {
            std::lock_guard lk(writer.mtx);
            writer.file.write(
                reinterpret_cast<const char*>(records.data()),
                static_cast<std::streamsize>(records.size() * sizeof(Record)));
            writer.file.flush();
            writer.records.fetch_add(records.size(),
                                     std::memory_order_relaxed);
        }
        writer.gamesDone.fetch_add(1, std::memory_order_relaxed);
    }
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
        else if (a == "--fresh")    o.fresh    = true;
        else if (a == "--rules") {
            const auto v = nextStr();
            if      (v == "on")  o.rules = core::RuleSet::InternationalMaxCapture;
            else if (v == "off") o.rules = core::RuleSet::InternationalFreeCapture;
            else { std::fprintf(stderr, "bad --rules (on|off)\n"); std::exit(2); }
        } else if (a == "--help" || a == "-h") {
            std::printf(
                "usage: bookgen [--games N] [--depth N] [--stop-ply N]\n"
                "               [--threads N] [--out FILE] [--rules on|off]\n"
                "               [--seed N] [--fresh]\n");
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
    std::printf("  fresh      %s\n", o.fresh ? "yes" : "no");
    std::printf("\n");

    // ?? Detect existing file & read header if present ?????????????????????
    int        startGame = 0;
    bool       resume    = false;
    FileHeader existing{};

    if (!o.fresh && std::filesystem::exists(o.out)) {
        std::ifstream in(o.out, std::ios::binary);
        if (in && in.read(reinterpret_cast<char*>(&existing), sizeof(existing))) {
            const bool magicOk = std::memcmp(existing.magic, "DBKG", 4) == 0;
            const bool verOk   = existing.version == 2;
            const bool sizeOk  = existing.recordSize == sizeof(Record);
            const bool rulesOk = existing.ruleVariant ==
                ((o.rules == core::RuleSet::InternationalFreeCapture) ? 1 : 0);
            if (magicOk && verOk && sizeOk && rulesOk) {
                startGame = static_cast<int>(existing.gamesCompleted);
                resume    = true;
                std::printf("Resuming from game %d/%d (%.1f%%)\n",
                            startGame, o.games,
                            100.0 * startGame / std::max(1, o.games));
            } else {
                std::printf("Existing file has incompatible format or rule "
                            "variant - starting fresh.\n");
            }
        }
    }

    if (startGame >= o.games) {
        std::printf("Nothing to do: file already contains %d games.\n", startGame);
        return 0;
    }

    // ?? Open output file, write fresh header if needed ???????????????????
    std::fstream file;

    if (!resume) {
        file.open(o.out, std::ios::binary | std::ios::out | std::ios::trunc);
        if (!file) { std::fprintf(stderr, "cannot create %s\n", o.out.c_str()); return 1; }
        FileHeader hdr{};
        std::memcpy(hdr.magic, "DBKG", 4);
        hdr.version        = 2;
        hdr.recordSize     = sizeof(Record);
        hdr.gamesCompleted = 0;
        hdr.ruleVariant    = (o.rules == core::RuleSet::InternationalFreeCapture) ? 1 : 0;
        file.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
        file.flush();
        file.close();
    }

    file.open(o.out, std::ios::binary | std::ios::in | std::ios::out);
    if (!file) { std::fprintf(stderr, "cannot open %s\n", o.out.c_str()); return 1; }
    file.seekp(0, std::ios::end);

    SharedWriter writer;
    writer.file = std::move(file);

    std::atomic<int> nextGame{startGame};
    const auto       t0 = std::chrono::steady_clock::now();

    std::atomic<bool> progressStop{false};
    std::thread progressThread([&]{
        while (!progressStop.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            if (progressStop.load()) break;
            const auto sec = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - t0).count();
            const int    done = writer.gamesDone.load();
            const double pct  = 100.0 * (startGame + done) / std::max(1, o.games);
            std::printf("  [%llds] %d/%d games (%.1f%%)  records=%llu\n",
                        (long long)sec, startGame + done, o.games, pct,
                        (unsigned long long)writer.records.load());
            std::fflush(stdout);
        }
    });

    // ?? Spawn workers ????????????????????????????????????????????????????
    const int nThreads = std::max(1, o.threads);
    std::vector<std::thread> threads;
    threads.reserve(static_cast<std::size_t>(nThreads));
    for (int t = 0; t < nThreads; ++t) {
        threads.emplace_back([&]{ threadWorker(o, nextGame, writer); });
    }
    for (auto& th : threads) th.join();
    progressStop.store(true);
    progressThread.join();

    // ?? Update header with final gamesCompleted ??????????????????????????
    const int finalGames = startGame + writer.gamesDone.load();
    writer.file.seekp(static_cast<std::streamoff>(offsetof(FileHeader, gamesCompleted)));
    writer.file.write(reinterpret_cast<const char*>(&finalGames), sizeof(finalGames));
    writer.file.flush();
    writer.file.close();

    const auto sec = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - t0).count();
    std::printf("\nDone. %d total games, %llu records in %llds\n",
                finalGames,
                (unsigned long long)writer.records.load(),
                (long long)sec);
    std::printf("wrote %s\n", o.out.c_str());
    return 0;
}
