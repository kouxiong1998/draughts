#include <catch2/catch_test_macros.hpp>
#include "core/Board.hpp"
#include "core/Zobrist.hpp"
#include "ai/TranspositionTable.hpp"

using namespace draughts;

TEST_CASE("TT: empty probe returns false") {
    ai::TranspositionTable tt(1u << 10);
    ai::TTEntry e{};
    REQUIRE_FALSE(tt.probe(0xDEADBEEFCAFEBABEull, e));
}

TEST_CASE("TT: store then probe returns the same entry") {
    ai::TranspositionTable tt(1u << 10);
    const std::uint64_t hash = 0x123456789ABCDEF0ull;
    core::Move best{};
    best.from = 12;
    best.to   = 23;

    tt.store(hash, /*depth=*/7, /*score=*/42, ai::TTFlag::Exact, best);

    ai::TTEntry e{};
    REQUIRE(tt.probe(hash, e));
    REQUIRE(e.hash     == hash);
    REQUIRE(e.depth    == 7);
    REQUIRE(e.score    == 42);
    REQUIRE(e.flag     == static_cast<std::uint8_t>(ai::TTFlag::Exact));
    REQUIRE(e.from     == 12);
    REQUIRE(e.to       == 23);
}

TEST_CASE("TT: probe for a different hash misses") {
    ai::TranspositionTable tt(1u << 10);
    core::Move best{};
    best.from = 1;
    best.to   = 2;
    tt.store(0xAAAAAAAAAAAAAAAAull, 3, 10, ai::TTFlag::Exact, best);

    ai::TTEntry e{};
    REQUIRE_FALSE(tt.probe(0xBBBBBBBBBBBBBBBBull, e));
}

TEST_CASE("TT: depth-preferred replacement") {
    ai::TranspositionTable tt(1u << 10);
    const std::uint64_t hash = 0x5555555555555555ull;
    core::Move m1{}; m1.from = 5;  m1.to = 10;
    core::Move m2{}; m2.from = 15; m2.to = 20;

    tt.store(hash, /*depth=*/10, /*score=*/100, ai::TTFlag::Exact, m1);
    // Shallower store for the same position must NOT overwrite.
    tt.store(hash, /*depth=*/ 3, /*score=*/ 50, ai::TTFlag::Exact, m2);

    ai::TTEntry e{};
    REQUIRE(tt.probe(hash, e));
    REQUIRE(e.depth == 10);
    REQUIRE(e.from  == 5);

    // Deeper store must overwrite.
    tt.store(hash, /*depth=*/12, /*score=*/90, ai::TTFlag::Exact, m2);
    REQUIRE(tt.probe(hash, e));
    REQUIRE(e.depth == 12);
    REQUIRE(e.from  == 15);
}

TEST_CASE("TT: clear empties all entries") {
    ai::TranspositionTable tt(1u << 10);
    core::Move m{};
    for (std::uint64_t i = 0; i < 100; ++i)
        tt.store(0x1000 + i, 5, 1, ai::TTFlag::Exact, m);
    REQUIRE(tt.usedCount() > 0);

    tt.clear();
    REQUIRE(tt.usedCount() == 0);
}
