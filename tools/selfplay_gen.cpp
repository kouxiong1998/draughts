// tools/selfplay_gen.cpp
// Self-play generator: plays N games of AI vs AI under a chosen rule
// variant, writes (position, side-to-move, result) records to a binary
// file. The output is consumed by tools/tuner.

#include "core/GameEngine.hpp"
#include "core/MoveGenerator.hpp"
#include "ai/AIEngine.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

using namespace draughts;

namespace {

struct Options {
    int          games      = 200;
    int          moveMs     = 100;    // time budget per move
    int          maxPlies   = 200;
    std::string  outPath    = "selfplay.bin";
    core::RuleSet rules     = core::RuleSet::InternationalMaxCapture;
    std::uint64_t seed      = 0xC0FFEEULL;
};

// One record: 32 bytes, aligned.
struct Record {
    std::uint64_t red;          // bitboard
    std::uint64_t yellow;       // bitboard
    std::uint64_t kings;        // bitboard
    std::uint8_t  sideToMove;   // 0=Red, 1=Yellow
    std::uint8_t  result;       // 0=loss, 1=draw, 2=win (from side to move)
    std::uint8_t  pad0[6];
};
static_assert(sizeof(Record) == 32, "Record must be 32 bytes");

struct FileHeader {
    char          magic[4];     // "DSPG"
    std::uint32_t version;      // 1
    std::uint32_t recordSize;   // sizeof(Record)
    std::uint64_t recordCount;
    std::uint8_t  ruleVariant;  // 0=MaxCapture, 1=FreeCapture
    std::uint8_t  pad0[7];
};
static_assert(sizeof(FileHeader) == 32, "Header must be 32 bytes");

Options parseArgs(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto nextInt  = [&]() -> int          { return (i + 1 < argc) ? std::stoi(argv[++i]) : 0; };
        auto nextStr  = [&]() -> std::string  { return (i + 1 < argc) ? std::string(argv[++i]) : std::string{}; };
        if      (a == "--games")   o.games   = nextInt();
        else if (a == "--move-ms") o.moveMs  = nextInt();
        else if (a == "--max-plies") o.maxPlies = nextInt();
        else if (a == "--out")     o.outPath = nextStr();
        else if (a == "--seed")    o.seed    = std::stoull(nextStr());
        else if (a == "--rules") {
            const auto v = nextStr();
            if      (v == "on")  o.rules = core::RuleSet::InternationalMaxCapture;
            else if (v == "off") o.rules = core::RuleSet::InternationalFreeCapture;
            else { std::fprintf(stderr, "bad --rules (use on|off)\n"); std::exit(2); }
        } else if (a == "--help" || a == "-h") {
            std::printf(
                "usage: selfplay_gen [--games N] [--move-ms N] [--max-plies N]\n"
                "                     [--out FILE] [--rules on|off] [--seed N]\n");
            std::exit(0);
        } else {
            std::fprintf(stderr, "unknown arg: %s\n", a.c_str());
            std::exit(2);
        }
    }
    return o;
}

std::uint8_t resultFromPerspective(core::GameResult r, core::Color side) {
    switch (r) {
        case core::GameResult::Draw:        return 1;
        case core::GameResult::RedWins:     return (side == core::Color::Red) ? 2 : 0;
        case core::GameResult::YellowWins:  return (side == core::Color::Yellow) ? 2 : 0;
        case core::GameResult::Ongoing:     return 1;   // truncated game ? treat as draw
    }
    return 1;
}

// Play one game, collecting (board, side) snapshots before every move.
// Returns the labeled records with the final result attached.
std::vector<Record> playOneGame(const Options& o, ai::AIEngine& ai) {
    core::GameEngine engine;
    engine.newGame(o.rules, core::Color::Red);

    ai::TimeBudget budget;
    budget.soft    = std::chrono::milliseconds(o.moveMs);
    budget.hard    = std::chrono::milliseconds(o.moveMs + 50);
    budget.minimum = std::chrono::milliseconds(10);

    struct Snapshot {
        core::Board board;
        core::Color side;
    };
    std::vector<Snapshot> positions;
    positions.reserve(static_cast<std::size_t>(o.maxPlies));

    // Synchronous search wrapper.
    std::atomic<bool> gotMove{false};
    core::Move        chosen{};
    ai.setDoneCallback([&](const core::Move& m, const ai::SearchStats&) {
        chosen  = m;
        gotMove.store(true, std::memory_order_release);
    });

    for (int ply = 0; ply < o.maxPlies; ++ply) {
        if (engine.result() != core::GameResult::Ongoing) break;

        positions.push_back({engine.board(), engine.sideToMove()});

        gotMove.store(false, std::memory_order_release);
        ai.think(engine.board(), engine.sideToMove(), engine.rules(), budget);

        const auto start = std::chrono::steady_clock::now();
        while (!gotMove.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            if (std::chrono::steady_clock::now() - start > std::chrono::seconds(5)) {
                break;
            }
        }
        ai.stop();

        if (!gotMove.load()) {
            // Fallback: play the first legal move so we always advance.
            const auto moves = core::generateLegalMoves(engine.board(),
                                                        engine.sideToMove(),
                                                        engine.rules());
            if (moves.empty()) break;
            chosen = moves[0];
        }

        if (!engine.tryApply(chosen)) break;
    }

    std::vector<Record> out;
    out.reserve(positions.size());
    for (const auto& p : positions) {
        Record r{};
        r.red        = engine.board().occupied() & engine.board().colorMask(core::Color::Red);
        // Simpler: read the piece bitboards directly from the snapshot board.
        r.red        = p.board.colorMask(core::Color::Red);
        r.yellow     = p.board.colorMask(core::Color::Yellow);
        r.kings      = p.board.allKings();
        r.sideToMove = (p.side == core::Color::Red) ? 0 : 1;
        r.result     = resultFromPerspective(engine.result(), p.side);
        out.push_back(r);
    }
    return out;
}

} // namespace

int main(int argc, char** argv) {
    const Options o = parseArgs(argc, argv);

    std::printf("selfplay_gen\n");
    std::printf("  games       %d\n", o.games);
    std::printf("  move-ms     %d\n", o.moveMs);
    std::printf("  max-plies   %d\n", o.maxPlies);
    std::printf("  out         %s\n", o.outPath.c_str());
    std::printf("  rules       %s\n", o.rules == core::RuleSet::InternationalMaxCapture
                                         ? "on (majority)" : "off (free capture)");
    std::printf("  seed        0x%llx\n", (unsigned long long)o.seed);
    std::printf("\n");

    ai::AIEngine ai;
    ai.setThreadCount(4);
    ai.setOpeningBook(nullptr);   // no book during training ? want raw positions

    std::ofstream out(o.outPath, std::ios::binary);
    if (!out) {
        std::fprintf(stderr, "cannot open %s for writing\n", o.outPath.c_str());
        return 1;
    }

    // Reserve header space; we write record count at the end.
    FileHeader hdr{};
    std::memcpy(hdr.magic, "DSPG", 4);
    hdr.version      = 1;
    hdr.recordSize   = sizeof(Record);
    hdr.recordCount  = 0;
    hdr.ruleVariant  = (o.rules == core::RuleSet::InternationalMaxCapture) ? 0 : 1;
    out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));

    std::uint64_t totalRecords = 0;
    int redWins = 0, yellowWins = 0, draws = 0;

    const auto t0 = std::chrono::steady_clock::now();
    for (int g = 0; g < o.games; ++g) {
        const auto recs = playOneGame(o, ai);

        for (const auto& r : recs)
            out.write(reinterpret_cast<const char*>(&r), sizeof(Record));
        totalRecords += recs.size();

        // Determine result for stats ? peek the last record's result.
        if (!recs.empty()) {
            const auto& last = recs.back();
            const auto s = (last.sideToMove == 0) ? core::Color::Red : core::Color::Yellow;
            switch (last.result) {
                case 0: // side to move lost
                    if (s == core::Color::Red) ++yellowWins; else ++redWins;
                    break;
                case 1: ++draws; break;
                case 2:
                    if (s == core::Color::Red) ++redWins; else ++yellowWins;
                    break;
            }
        }

        if ((g + 1) % 10 == 0 || g + 1 == o.games) {
            const auto sec = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - t0).count();
            std::printf("  game %5d/%d  records=%llu  R/W/D=%d/%d/%d  elapsed=%llds\n",
                        g + 1, o.games,
                        (unsigned long long)totalRecords,
                        redWins, yellowWins, draws,
                        (long long)sec);
            std::fflush(stdout);
        }
    }

    // Rewrite header with final record count.
    hdr.recordCount = totalRecords;
    out.seekp(0, std::ios::beg);
    out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    out.close();

    std::printf("\nDone. %llu records written to %s\n",
                (unsigned long long)totalRecords, o.outPath.c_str());
    return 0;
}
