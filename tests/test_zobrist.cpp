#include <catch2/catch_test_macros.hpp>
#include "core/Board.hpp"
#include "core/Zobrist.hpp"

using namespace draughts::core;

TEST_CASE("Zobrist: two freshly-reset boards hash identically") {
    Board a; a.resetStandard();
    Board b; b.resetStandard();
    REQUIRE(a.pieceHash() == b.pieceHash());
    REQUIRE(a.pieceHash() != 0u);
}

TEST_CASE("Zobrist: different positions hash differently") {
    Board a; a.resetStandard();
    Board b; b.resetStandard();
    b.removePiece(5);
    b.setPiece(2, Color::Red, PieceKind::Man);
    REQUIRE(a.pieceHash() != b.pieceHash());
}

TEST_CASE("Zobrist: set then remove restores hash") {
    Board b; b.resetStandard();
    const auto before = b.pieceHash();
    REQUIRE(b.empty(25));
    b.setPiece(25, Color::Red, PieceKind::King);
    REQUIRE(b.pieceHash() != before);
    b.removePiece(25);
    REQUIRE(b.pieceHash() == before);
}

TEST_CASE("Zobrist: promotion changes hash") {
    Board b; b.clear();
    b.setPiece(25, Color::Red, PieceKind::Man);
    const auto asMan = b.pieceHash();
    b.promote(25);
    const auto asKing = b.pieceHash();
    REQUIRE(asMan != asKing);
    REQUIRE(asKing != 0u);
}

TEST_CASE("Zobrist: recompute matches incremental updates") {
    Board b; b.resetStandard();
    // Apply a few random changes to exercise the incremental XOR path.
    b.removePiece(3);
    b.setPiece(15, Color::Red, PieceKind::King);
    b.setPiece(27, Color::Yellow, PieceKind::Man);
    const auto incremental = b.pieceHash();

    b.recomputeHash();
    REQUIRE(b.pieceHash() == incremental);
}
