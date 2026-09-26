// tools/bookfill.cpp
// Extends an existing .bin book by filling in "missing" positions up to
// a given ply depth. Every position reachable within `--max-ply` plies
// from the start position (both starting sides) is either already in
// the book or gets a depth-N search performed on it, with the top-5
// moves added.

#include "core/GameEngine.hpp"
#include "core/MoveGenerator.hpp"
#include "core/Zobrist.hpp"
#include "ai/Search.hpp"
#include "ai/TranspositionTable.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

using namespace draughts;

namespace {

constexpr int kTopMoves = 5;

struct MergedHeader {
    char          magic[4];     // "DBKM"
    std::uint32_t version;      // 1
    std::uint64_t entryCount;
    std::uint8_t  ruleVariant;
    std::uint8_t  pad[15];
};
static_assert(sizeof(MergedHeader) == 32, "");

struct MergedEntry {
    std::uint64_t hash;
    std::uint32_t weights[kTopMoves];
    std::uint8_t  from[kTopMoves];
    std::uint8_t  to  [kTopMoves];
    std::uint8_t  isPromotion[kTopMoves];
    std::uint8_t  moveCount;
    std::uint8_t  pad[4];
};
static_assert(sizeof(MergedEntry) == 48, "");

std::uint64_t positionHash(const core::Board& b, core::Color side,
                           core::RuleSet rules) noexcept
{
    std::uint64_t h = b.pieceHash();
    h ^= core::Zobrist::instance().side(side);
    h ^= static_cast<std::uint64_t>(rules) << 61;
    return h;
}

// One position waiting to be filled.
struct MissingPosition {
    core::Board board;
    core::Color side;
};

struct Options {
    std::string   in;
    std::string   out;
    int           depth    = 10;
    int           maxPly   = 4;
    core::RuleSet rules    = core::RuleSet::InternationalMaxCapture;
};

Options parseArgs(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto nextInt = [&](int d) { return (i + 1 < argc) ? std::stoi(argv[++i]) : d; };
        auto nextStr = [&]() { return (i + 1 < argc) ? std::string(argv[++i]) : std::string{}; };

        if      (a == "--in")      o.in     = nextStr();
        else if (a == "--out")     o.out    = nextStr();
        else if (a == "--depth")   o.depth  = nextInt(o.depth);
        else if (a == "--max-ply") o.maxPly = nextInt(o.maxPly);
        else if (a == "--rules") {
            const auto v = nextStr();
            if      (v == "on")  o.rules = core::RuleSet::InternationalMaxCapture;
            else if (v == "off") o.rules = core::RuleSet::InternationalFreeCapture;
            else { std::fprintf(stderr, "bad --rules\n"); std::exit(2); }
        } else if (a == "--help" || a == "-h") {
            std::printf(
                "usage: bookfill --in BOOK.bin --out BOOK.bin [--depth N] [--max-ply N] [--rules on|off]\n");
            std::exit(0);
        } else {
            std::fprintf(stderr, "unknown arg: %s\n", a.c_str());
            std::exit(2);
        }
    }
    return o;
}

// Apply a move to board in place.
void applyMove(core::Board& board, const core::Move& m) {
    core::Bitboard cap = m.captured;
    while (cap) {
        const auto s = static_cast<core::Square>(std::countr_zero(cap));
        cap &= cap - 1;
        board.removePiece(s);
    }
    const auto p = board.at(m.from);
    board.removePiece(m.from);
    board.setPiece(m.to, p.color, p.kind);
    if (p.kind == core::PieceKind::Man && m.isPromotion) board.promote(m.to);
}

// Walk the tree of legal positions up to maxPly, collecting positions
// not already in the book.
void collectMissing(const core::Board& board,
                    core::Color side,
                    core::RuleSet rules,
                    int plyLeft,
                    std::set<std::uint64_t>& seenPositions,
                    std::vector<MissingPosition>& missing,
                    bool skipEnqueue)  // if true, position already in book
{
    const auto h = positionHash(board, side, rules);
    const bool novel = (seenPositions.insert(h).second);   // true if new
    if (!novel) return;

    if (!skipEnqueue) missing.push_back({board, side});

    if (plyLeft <= 0) return;

    const auto moves = core::generateLegalMoves(board, side, rules);
    for (std::size_t i = 0; i < moves.size(); ++i) {
        core::Board next = board;
        applyMove(next, moves[i]);
        collectMissing(next, core::opposite(side), rules, plyLeft - 1,
                       seenPositions, missing, false);
    }
}

} // namespace

int main(int argc, char** argv) {
    const Options o = parseArgs(argc, argv);
    if (o.in.empty() || o.out.empty()) {
        std::fprintf(stderr, "error: --in and --out required\n");
        return 2;
    }

    std::printf("bookfill\n");
    std::printf("  in        %s\n", o.in.c_str());
    std::printf("  out       %s\n", o.out.c_str());
    std::printf("  depth     %d\n", o.depth);
    std::printf("  max-ply   %d\n", o.maxPly);
    std::printf("  rules     %s\n",
        o.rules == core::RuleSet::InternationalFreeCapture ? "off" : "on");
    std::printf("\n");

    // ?? Read existing entries ?????????????????????????????????????????????
    std::printf("Reading existing book...\n");
    std::vector<MergedEntry> entries;
    std::set<std::uint64_t> knownHashes;

    {
        std::ifstream f(o.in, std::ios::binary);
        if (!f) { std::fprintf(stderr, "cannot open %s\n", o.in.c_str()); return 1; }
        MergedHeader hdr{};
        if (!f.read(reinterpret_cast<char*>(&hdr), sizeof(hdr))) return 1;
        if (std::memcmp(hdr.magic, "DBKM", 4) != 0) {
            std::fprintf(stderr, "bad input format\n"); return 1;
        }
        entries.reserve(static_cast<std::size_t>(hdr.entryCount));
        for (std::uint64_t i = 0; i < hdr.entryCount; ++i) {
            MergedEntry e{};
            if (!f.read(reinterpret_cast<char*>(&e), sizeof(e))) break;
            entries.push_back(e);
            knownHashes.insert(e.hash);
        }
    }
    std::printf("  loaded %zu entries\n\n", entries.size());

    // ?? Walk the tree from start, both starting sides ?????????????????????
    std::vector<MissingPosition> missing;
    std::set<std::uint64_t>      visited;

    for (core::Color first : {core::Color::Red, core::Color::Yellow}) {
        core::Board b; b.resetStandard();
        collectMissing(b, first, o.rules, o.maxPly,
                       visited, missing, /*skipEnqueue=*/false);
    }

    // Filter: keep only positions not already in the book.
    std::vector<MissingPosition> toFill;
    for (auto& mp : missing) {
        if (!knownHashes.count(positionHash(mp.board, mp.side, o.rules)))
            toFill.push_back(mp);
    }
    std::printf("  %zu positions reachable within ply %d\n",
                missing.size(), o.maxPly);
    std::printf("  %zu already in book\n", missing.size() - toFill.size());
    std::printf("  %zu to fill\n\n", toFill.size());

    if (toFill.empty()) {
        std::printf("Nothing to fill. Copying input to output.\n");
        std::filesystem::copy_file(o.in, o.out,
                                   std::filesystem::copy_options::overwrite_existing);
        return 0;
    }

    // ?? Fill each missing position ????????????????????????????????????????
    ai::TranspositionTable tt(1u << 22);   // 64 MB for the fill search
    std::atomic<bool>      stopFlag{false};
    ai::Search search(&tt, nullptr, &stopFlag, nullptr);

    ai::TimeBudget budget;
    budget.soft     = std::chrono::seconds(120);
    budget.hard     = std::chrono::seconds(120);
    budget.minimum  = std::chrono::milliseconds(0);
    budget.maxDepth = o.depth;

    std::size_t filled = 0;
    for (auto& mp : toFill) {
        const auto scored = search.topMoves(mp.board, mp.side, o.rules,
                                            o.depth, kTopMoves);
        if (scored.empty()) continue;

        MergedEntry e{};
        e.hash = positionHash(mp.board, mp.side, o.rules);

        // Distribute weights linearly: best move gets highest weight.
        // Weights sum to 1000. A simple geometric falloff.
        std::uint32_t weightsSum = 0;
        std::vector<std::uint32_t> rawWeights;
        rawWeights.reserve(scored.size());
        for (std::size_t k = 0; k < scored.size(); ++k) {
            const std::uint32_t w = static_cast<std::uint32_t>(scored.size() - k);
            rawWeights.push_back(w);
            weightsSum += w;
        }
        for (std::size_t k = 0; k < scored.size(); ++k) {
            const std::uint32_t scaled = rawWeights[k] * 1000 / weightsSum;
            e.weights[k]      = std::max<std::uint32_t>(scaled, 1);
            e.from[k]         = scored[k].first.from;
            e.to[k]           = scored[k].first.to;
            e.isPromotion[k]  = scored[k].first.isPromotion ? 1 : 0;
        }
        e.moveCount = static_cast<std::uint8_t>(scored.size());

        entries.push_back(e);
        knownHashes.insert(e.hash);
        ++filled;

        if ((filled % 25) == 0 || filled == toFill.size()) {
            std::printf("  filled %zu / %zu\n", filled, toFill.size());
            std::fflush(stdout);
        }
    }

    // ?? Write merged output ???????????????????????????????????????????????
    std::printf("\nWriting %zu total entries to %s\n",
                entries.size(), o.out.c_str());

    MergedHeader mh{};
    std::memcpy(mh.magic, "DBKM", 4);
    mh.version     = 1;
    mh.entryCount  = entries.size();
    mh.ruleVariant = (o.rules == core::RuleSet::InternationalFreeCapture) ? 1 : 0;

    std::ofstream out(o.out, std::ios::binary);
    if (!out) { std::fprintf(stderr, "cannot write %s\n", o.out.c_str()); return 1; }
    out.write(reinterpret_cast<const char*>(&mh), sizeof(mh));
    out.write(reinterpret_cast<const char*>(entries.data()),
              static_cast<std::streamsize>(entries.size() * sizeof(MergedEntry)));
    out.close();

    std::printf("Done. Added %zu entries.\n", filled);
    return 0;
}
