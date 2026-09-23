#pragma once
/// @file TranspositionTable.hpp
/// @brief Fixed-size, thread-safe hash table that caches previously-searched
///        positions keyed by Zobrist hash.
///
/// Concurrency: striped mutexes. Each stripe covers a contiguous run of
/// buckets, so any given bucket is protected by exactly one lock. With
/// 4096 stripes and ~8 threads, contention is negligible.

#include "core/Types.hpp"

#include <array>
#include <cstdint>
#include <mutex>
#include <vector>

namespace draughts::ai {

enum class TTFlag : std::uint8_t {
    Empty = 0,
    Exact = 1,
    LowerBound = 2,
    UpperBound = 3,
};

struct TTEntry {
    std::uint64_t hash{0};
    std::int32_t  score{0};
    std::int16_t  depth{-1};
    std::uint8_t  flag{static_cast<std::uint8_t>(TTFlag::Empty)};
    std::uint8_t  from{core::kInvalidSquare};
    std::uint8_t  to  {core::kInvalidSquare};
    std::uint8_t  pad0{0};
    std::uint64_t captured{0};
};

class TranspositionTable {
public:
    explicit TranspositionTable(std::size_t entryCount = (1u << 20));

    void clear() noexcept;

    [[nodiscard]] bool probe(std::uint64_t hash, TTEntry& out) const noexcept;

    void store(std::uint64_t hash, int depth, int score, TTFlag flag,
               const core::Move& bestMove) noexcept;

    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] std::size_t usedCount() const noexcept;

private:
    static constexpr std::size_t kStripeCount = 4096;
    mutable std::array<std::mutex, kStripeCount> stripes_{};
    std::vector<TTEntry> entries_;
    std::size_t          mask_{0};

    [[nodiscard]] std::mutex& lockFor(std::size_t idx) const noexcept {
        return stripes_[idx & (kStripeCount - 1)];
    }
};

} // namespace draughts::ai
