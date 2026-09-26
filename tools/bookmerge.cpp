// tools/bookmerge.cpp
// Reads a raw book file (from bookgen), deduplicates positions, keeps the
// top-5 most-frequent moves per position, emits a compact binary book.

#include "core/Types.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace draughts;

namespace {

constexpr int kTopMoves = 5;

// ?? Input format (matches tools/bookgen.cpp) ??????????????????????????????
struct RawRecord {
    std::uint64_t hash;
    std::uint64_t captured;
    std::uint8_t  from;
    std::uint8_t  to;
    std::uint8_t  isPromotion;
    std::uint8_t  pad[13];
};
static_assert(sizeof(RawRecord) == 32, "");

struct RawHeader {
    char          magic[4];     // "DBKG"
    std::uint32_t version;
    std::uint32_t recordSize;
    std::uint64_t recordCount;
    std::uint8_t  ruleVariant;
    std::uint8_t  pad[7];
};
static_assert(sizeof(RawHeader) == 32, "");

// ?? Output format ?????????????????????????????????????????????????????????
// Header: 32 bytes
//   magic "DBKM", version u32, entryCount u64, ruleVariant u8, pad[15]
//
// Entry: 48 bytes each
//   hash          u64            (8)
//   weights[5]    u32            (20)
//   from[5]       u8             (5)
//   to[5]         u8             (5)
//   isPromotion[5] u8            (5)
//   moveCount     u8             (1)
//   pad[4]        u8             (4)
// Total: 48 bytes

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

// Per-position aggregation: one move is identified by (from, to, captured).
struct MoveKey {
    std::uint8_t  from;
    std::uint8_t  to;
    std::uint64_t captured;
    std::uint8_t  isPromotion;

    bool operator==(const MoveKey& o) const noexcept {
        return from == o.from && to == o.to &&
               captured == o.captured && isPromotion == o.isPromotion;
    }
};

struct MoveKeyHash {
    std::size_t operator()(const MoveKey& k) const noexcept {
        std::uint64_t h = k.from;
        h = h * 1000003ULL ^ k.to;
        h = h * 1000003ULL ^ k.captured;
        h = h * 1000003ULL ^ k.isPromotion;
        return static_cast<std::size_t>(h);
    }
};

struct PositionInfo {
    std::unordered_map<MoveKey, std::uint32_t, MoveKeyHash> moveCounts;
    std::uint32_t totalVisits{0};
};

struct Options {
    std::string   in;
    std::string   out;
    std::size_t   maxPositions = 1000000;
    std::uint32_t minVisits    = 1;    // require at least N plays to keep a position
};

Options parseArgs(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto nextStr = [&]() -> std::string { return (i + 1 < argc) ? std::string(argv[++i]) : std::string{}; };
        auto nextInt = [&]() -> int { return (i + 1 < argc) ? std::stoi(argv[++i]) : 0; };

        if      (a == "--in")            o.in           = nextStr();
        else if (a == "--out")           o.out          = nextStr();
        else if (a == "--max-positions") o.maxPositions = static_cast<std::size_t>(std::stoull(nextStr()));
        else if (a == "--min-visits")    o.minVisits    = static_cast<std::uint32_t>(nextInt());
        else if (a == "--help" || a == "-h") {
            std::printf("usage: bookmerge --in RAW.bin --out BOOK.bin [--max-positions N] [--min-visits N]\n");
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
    if (o.in.empty() || o.out.empty()) {
        std::fprintf(stderr, "error: --in and --out required\n");
        return 2;
    }

    std::printf("bookmerge\n");
    std::printf("  in             %s\n", o.in.c_str());
    std::printf("  out            %s\n", o.out.c_str());
    std::printf("  max-positions  %zu\n", o.maxPositions);
    std::printf("  min-visits     %u\n", o.minVisits);
    std::printf("\n");

    std::ifstream f(o.in, std::ios::binary);
    if (!f) { std::fprintf(stderr, "cannot open %s\n", o.in.c_str()); return 1; }

    RawHeader hdr{};
    f.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (std::memcmp(hdr.magic, "DBKG", 4) != 0 || hdr.version != 1 ||
        hdr.recordSize != sizeof(RawRecord)) {
        std::fprintf(stderr, "bad input format\n"); return 1;
    }

    std::printf("Reading %llu raw records...\n",
                (unsigned long long)hdr.recordCount);
    std::fflush(stdout);

    // Aggregation map: position hash -> per-move counts.
    std::unordered_map<std::uint64_t, PositionInfo> table;
    table.reserve(static_cast<std::size_t>(hdr.recordCount / 2));

    RawRecord rec{};
    std::uint64_t read = 0;
    while (f.read(reinterpret_cast<char*>(&rec), sizeof(rec))) {
        ++read;
        if ((read % 500000) == 0) {
            std::printf("  read %llu / %llu (%.1f%%)\n",
                        (unsigned long long)read,
                        (unsigned long long)hdr.recordCount,
                        100.0 * read / hdr.recordCount);
            std::fflush(stdout);
        }

        PositionInfo& info = table[rec.hash];
        info.totalVisits++;

        MoveKey k{};
        k.from        = rec.from;
        k.to          = rec.to;
        k.captured    = rec.captured;
        k.isPromotion = rec.isPromotion;
        info.moveCounts[k]++;
    }
    f.close();

    std::printf("\nAggregated %zu distinct positions from %llu records.\n",
                table.size(), (unsigned long long)read);

    // Filter: drop positions with fewer than minVisits total plays.
    std::vector<std::pair<std::uint64_t, PositionInfo*>> kept;
    kept.reserve(table.size());
    for (auto& kv : table) {
        if (kv.second.totalVisits >= o.minVisits)
            kept.emplace_back(kv.first, &kv.second);
    }
    std::printf("After min-visits filter: %zu positions.\n", kept.size());

    // Sort by total visits, keep the top maxPositions.
    std::sort(kept.begin(), kept.end(),
              [](auto& a, auto& b) {
                  return a.second->totalVisits > b.second->totalVisits;
              });
    if (kept.size() > o.maxPositions) {
        kept.resize(o.maxPositions);
        std::printf("Capped to %zu most-visited positions.\n", o.maxPositions);
    }

    // Build output entries.
    std::vector<MergedEntry> entries;
    entries.reserve(kept.size());

    for (auto& [hash, info] : kept) {
        // Sort moves by count descending, take top-5.
        std::vector<std::pair<MoveKey, std::uint32_t>> moves(
            info->moveCounts.begin(), info->moveCounts.end());
        std::sort(moves.begin(), moves.end(),
                  [](auto& a, auto& b) { return a.second > b.second; });
        if (moves.size() > static_cast<std::size_t>(kTopMoves))
            moves.resize(kTopMoves);

        // Compute weights as percentages of the total captured by top moves.
        std::uint32_t totalTop = 0;
        for (auto& m : moves) totalTop += m.second;
        if (totalTop == 0) continue;

        MergedEntry e{};
        e.hash = hash;
        for (std::size_t i = 0; i < moves.size(); ++i) {
            // Weight as a 0-1000 scale.
            e.weights[i]     = static_cast<std::uint32_t>(
                (moves[i].second * 1000ULL) / totalTop);
            e.from[i]        = moves[i].first.from;
            e.to[i]          = moves[i].first.to;
            e.isPromotion[i] = moves[i].first.isPromotion;
        }
        e.moveCount = static_cast<std::uint8_t>(moves.size());
        entries.push_back(e);
    }

    std::printf("\nWriting %zu entries...\n", entries.size());

    MergedHeader mh{};
    std::memcpy(mh.magic, "DBKM", 4);
    mh.version     = 1;
    mh.entryCount  = entries.size();
    mh.ruleVariant = hdr.ruleVariant;

    std::ofstream out(o.out, std::ios::binary);
    if (!out) { std::fprintf(stderr, "cannot open %s for writing\n", o.out.c_str()); return 1; }

    out.write(reinterpret_cast<const char*>(&mh), sizeof(mh));
    out.write(reinterpret_cast<const char*>(entries.data()),
              static_cast<std::streamsize>(entries.size() * sizeof(MergedEntry)));
    out.close();

    std::printf("Done. Wrote %zu entries to %s\n", entries.size(), o.out.c_str());
    return 0;
}
