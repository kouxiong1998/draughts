#include "TranspositionTable.hpp"
#include "core/Board.hpp"

#include <bit>

namespace draughts::ai {

TranspositionTable::TranspositionTable(std::size_t entryCount) {
    const std::size_t p2 = std::bit_floor(entryCount);
    entries_.assign(p2 == 0 ? 1 : p2, TTEntry{});
    mask_ = entries_.size() - 1;
}

void TranspositionTable::clear() noexcept {
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        std::lock_guard lk(lockFor(i));
        entries_[i] = TTEntry{};
    }
}

bool TranspositionTable::probe(std::uint64_t hash, TTEntry& out) const noexcept {
    const std::size_t idx = static_cast<std::size_t>(hash & mask_);
    std::lock_guard lk(lockFor(idx));
    const TTEntry& e = entries_[idx];
    if (e.hash != hash) return false;
    if (e.flag == static_cast<std::uint8_t>(TTFlag::Empty)) return false;
    out = e;
    return true;
}

void TranspositionTable::store(std::uint64_t hash, int depth, int score,
                               TTFlag flag, const core::Move& bestMove) noexcept
{
    const std::size_t idx = static_cast<std::size_t>(hash & mask_);
    std::lock_guard lk(lockFor(idx));
    TTEntry& e = entries_[idx];

    // Depth-preferred: keep deeper entries at the same position. Always
    // replace entries for a different position, or equal/shallower depth.
    if (e.flag != static_cast<std::uint8_t>(TTFlag::Empty) &&
        e.hash == hash && depth < e.depth) {
        return;
    }

    e.hash     = hash;
    e.score    = score;
    e.depth    = static_cast<std::int16_t>(depth);
    e.flag     = static_cast<std::uint8_t>(flag);
    e.from     = bestMove.from;
    e.to       = bestMove.to;
    e.captured = bestMove.captured;
}

std::size_t TranspositionTable::usedCount() const noexcept {
    std::size_t n = 0;
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        std::lock_guard lk(lockFor(i));
        if (entries_[i].flag != static_cast<std::uint8_t>(TTFlag::Empty)) ++n;
    }
    return n;
}

} // namespace draughts::ai
