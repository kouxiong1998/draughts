#include <catch2/catch_test_macros.hpp>
#include "core/Board.hpp"
#include "core/BoardConstants.hpp"
#include "core/MoveGenerator.hpp"

using namespace draughts::core;

TEST_CASE("Promotion: man reaching last row becomes King") {
    Board b; b.clear();
    // Red man on row 8 (one step from row 9 = promotion).
    // Square 40 = row 8, col 0 (playable since (8+0) odd = playable).
    const Square from = static_cast<Square>(40);
    b.setPiece(from, Color::Red, PieceKind::Man);
    // Yellow has a legal move somewhere so it isn't stalemate on its turn,
    // but for this test we only need Red's move.
    b.setPiece(static_cast<Square>(5), Color::Yellow, PieceKind::Man);

    const auto moves = generateLegalMoves(b, Color::Red, RuleSet::InternationalMaxCapture);

    // Find the move that lands on row 9 (promotion row for Red).
    bool foundPromotion = false;
    for (std::size_t i = 0; i < moves.size(); ++i) {
        const auto& m = moves[i];
        if (m.from != from) continue;
        if (bc::rowOf(m.to) == 9) {
            foundPromotion = true;
            REQUIRE(m.isPromotion);
        }
    }
    REQUIRE(foundPromotion);
}

TEST_CASE("Promotion: quiet move to row 9 promotes") {
    Board b; b.clear();
    const Square from = static_cast<Square>(40);  // row 8
    b.setPiece(from, Color::Red, PieceKind::Man);
    b.setPiece(static_cast<Square>(5), Color::Yellow, PieceKind::Man);

    const auto moves = generateLegalMoves(b, Color::Red, RuleSet::InternationalMaxCapture);
    bool applied = false;
    for (std::size_t i = 0; i < moves.size(); ++i) {
        const auto& m = moves[i];
        if (m.from != from) continue;
        if (bc::rowOf(m.to) != 9) continue;

        // Apply manually and confirm the piece is now a King.
        Board next = b;
        const auto p = next.at(m.from);
        next.removePiece(m.from);
        next.setPiece(m.to, p.color, p.kind);
        if (m.isPromotion) next.promote(m.to);

        REQUIRE(next.isKing(m.to));
        REQUIRE(next.hasPiece(Color::Red, m.to));
        applied = true;
    }
    REQUIRE(applied);
}

TEST_CASE("Promotion: no false promotion when not reaching the row") {
    Board b; b.clear();
    const Square from = static_cast<Square>(35);  // row 7, col 0
    b.setPiece(from, Color::Red, PieceKind::Man);
    b.setPiece(static_cast<Square>(5), Color::Yellow, PieceKind::Man);

    const auto moves = generateLegalMoves(b, Color::Red, RuleSet::InternationalMaxCapture);
    for (std::size_t i = 0; i < moves.size(); ++i) {
        const auto& m = moves[i];
        if (m.from != from) continue;
        REQUIRE(bc::rowOf(m.to) != 9);
        REQUIRE_FALSE(m.isPromotion);
    }
}

TEST_CASE("Promotion: Yellow promotes on row 0") {
    Board b; b.clear();
    // Yellow man on row 1 (one step from row 0 = promotion row for Yellow).
    // Square 6 = row 1, col 1 (playable).
    const Square from = static_cast<Square>(6);
    b.setPiece(from, Color::Yellow, PieceKind::Man);
    b.setPiece(static_cast<Square>(44), Color::Red, PieceKind::Man);

    const auto moves = generateLegalMoves(b, Color::Yellow, RuleSet::InternationalMaxCapture);
    bool foundPromotion = false;
    for (std::size_t i = 0; i < moves.size(); ++i) {
        const auto& m = moves[i];
        if (m.from != from) continue;
        if (bc::rowOf(m.to) == 0) {
            foundPromotion = true;
            REQUIRE(m.isPromotion);
        }
    }
    REQUIRE(foundPromotion);
}
