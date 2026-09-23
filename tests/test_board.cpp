#include <catch2/catch_test_macros.hpp>
#include "core/Board.hpp"
#include "core/BoardConstants.hpp"

using namespace draughts::core;

TEST_CASE("Board: initial setup has 20+20 pieces") {
    Board b; b.resetStandard();
    REQUIRE(b.count(Color::Red, PieceKind::Man) == 20);
    REQUIRE(b.count(Color::Yellow, PieceKind::Man) == 20);
    REQUIRE(b.count(Color::Red, PieceKind::King) == 0);
    REQUIRE(b.count(Color::Yellow, PieceKind::King) == 0);
}

TEST_CASE("Board: coordinate round-trip") {
    for (int sq = 0; sq < kNumPlayableSquares; ++sq) {
        const auto s = static_cast<Square>(sq);
        REQUIRE(bc::indexFromRowCol(bc::rowOf(s), bc::colOf(s)) == s);
    }
}

TEST_CASE("Board: piece set / remove / promote") {
    Board b; b.clear();
    b.setPiece(5, Color::Red, PieceKind::Man);
    REQUIRE(b.hasPiece(Color::Red, 5));
    b.promote(5);
    REQUIRE(b.isKing(5));
    b.removePiece(5);
    REQUIRE(b.empty(5));
}