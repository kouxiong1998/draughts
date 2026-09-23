#include "Zobrist.hpp"

namespace draughts::core {

    namespace {
        constexpr std::uint64_t kSeed = 0x9E3779B97F4A7C15ULL;   // golden-ratio constant

        constexpr std::uint64_t splitmix64(std::uint64_t& x) noexcept {
            x += 0x9E3779B97F4A7C15ULL;
            std::uint64_t z = x;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
            return z ^ (z >> 31);
        }
    } // namespace

    Zobrist::Zobrist(std::uint64_t seed) noexcept {
        std::uint64_t state = seed;
        for (auto& byColor : piece_)
            for (auto& byKind : byColor)
                for (auto& cell : byKind) cell = splitmix64(state);
        for (auto& s : side_) s = splitmix64(state);
    }

    const Zobrist& Zobrist::instance() noexcept {
        static const Zobrist table{ kSeed };
        return table;
    }

} // namespace draughts::core