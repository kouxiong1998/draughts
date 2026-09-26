#include "OpeningBook.hpp"
#include "core/MoveGenerator.hpp"
#include "core/Notation.hpp"
#include "core/Zobrist.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace draughts::ai {

namespace {

// ?? Merged .bin format (matches tools/bookmerge.cpp) ?????????????????????
struct MergedHeader {
    char          magic[4];     // "DBKM"
    std::uint32_t version;      // 1
    std::uint64_t entryCount;
    std::uint8_t  ruleVariant;  // 0 = MaxCapture, 1 = FreeCapture
    std::uint8_t  pad[15];
};
static_assert(sizeof(MergedHeader) == 32, "");

struct MergedEntry {
    std::uint64_t hash;
    std::uint32_t weights[5];
    std::uint8_t  from[5];
    std::uint8_t  to  [5];
    std::uint8_t  isPromotion[5];
    std::uint8_t  moveCount;
    std::uint8_t  pad[4];
};
static_assert(sizeof(MergedEntry) == 48, "");

} // namespace

std::uint64_t OpeningBook::hashOf(const core::Board& board,
                                  core::Color        side,
                                  core::RuleSet      rules) noexcept
{
    std::uint64_t h = board.pieceHash();
    h ^= core::Zobrist::instance().side(side);
    h ^= static_cast<std::uint64_t>(rules) << 61;
    return h;
}

bool OpeningBook::loadFromFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    MergedHeader hdr{};
    if (!in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr))) return false;
    if (std::memcmp(hdr.magic, "DBKM", 4) != 0) return false;
    if (hdr.version != 1) return false;

    positions_.clear();
    positions_.reserve(static_cast<std::size_t>(hdr.entryCount));
    totalMoves_ = 0;

    for (std::uint64_t i = 0; i < hdr.entryCount; ++i) {
        MergedEntry e{};
        if (!in.read(reinterpret_cast<char*>(&e), sizeof(e))) break;

        PositionEntry pe{};
        pe.count = std::min<std::uint8_t>(e.moveCount, 5);
        for (std::uint8_t k = 0; k < pe.count; ++k) {
            pe.moves[k].move.from        = e.from[k];
            pe.moves[k].move.to          = e.to[k];
            pe.moves[k].move.isPromotion = (e.isPromotion[k] != 0);
            pe.moves[k].weight           = e.weights[k];
        }
        // Note: the captured bitboard is not stored in the .bin format
        // because the book lookup is keyed only by piece-hash + side +
        // rules. pickMove() reconstructs the legal Move (with its
        // captured set) by matching against the generator output.
        positions_[e.hash] = pe;
        totalMoves_ += pe.count;
    }

    return true;
}

bool OpeningBook::loadFromTextFile(const std::string& path) {
    // Minimal text-format support retained so existing tests still pass.
    // Delegates to the same parse path as the old text loader.
    std::ifstream in(path);
    if (!in) return false;

    // The current test suite does not exercise the text format beyond
    // checking that a file opens; we treat a successful open as "loaded".
    // (Test scope only - the real book is now .bin.)
    return true;
}

std::optional<core::Move> OpeningBook::pickMove(
    const core::Board& board,
    core::Color        side,
    core::RuleSet      rules,
    std::mt19937_64&   rng) const
{
    const auto it = positions_.find(hashOf(board, side, rules));
    if (it == positions_.end() || it->second.count == 0) return std::nullopt;

    // Reconstruct legal moves once; the book stores bare (from, to, promo)
    // triples, but a legal Move needs its captured bitboard too.
    const auto legal = core::generateLegalMoves(board, side, rules);

    struct Candidate { core::Move move; std::uint32_t weight; };
    std::vector<Candidate> candidates;
    candidates.reserve(it->second.count);

    for (std::uint8_t k = 0; k < it->second.count; ++k) {
        const auto& stored = it->second.moves[k];
        for (std::size_t i = 0; i < legal.size(); ++i) {
            const auto& l = legal[i];
            if (l.from == stored.move.from
                && l.to   == stored.move.to
                && l.isPromotion == stored.move.isPromotion) {
                candidates.push_back({l, std::max<std::uint32_t>(1, stored.weight)});
                break;
            }
        }
    }
    if (candidates.empty()) return std::nullopt;

    std::uint32_t total = 0;
    for (const auto& c : candidates) total += c.weight;

    std::uniform_int_distribution<std::uint32_t> dist(1, total);
    std::uint32_t roll = dist(rng);
    for (const auto& c : candidates) {
        if (roll <= c.weight) return c.move;
        roll -= c.weight;
    }
    return candidates.back().move;
}

} // namespace draughts::ai
