#include <catch2/catch_test_macros.hpp>
#include "core/Board.hpp"
#include "core/BoardConstants.hpp"
#include "core/MoveGenerator.hpp"

using namespace draughts::core;

TEST_CASE("Flying king: moves any distance on an empty diagonal") {
    Board b; b.clear();
    // Red king on square 25 (centre-ish).
    const Square from = static_cast<Square>(25);
    b.setPiece(from, Color::Red, PieceKind::King);
    b.setPiece(static_cast<Square>(5), Color::Yellow, PieceKind::Man);

    const auto moves = generateLegalMoves(b, Color::Red, RuleSet::InternationalMaxCapture);

    // A king on an empty board should have several quiet moves,
    // each of length 1..9 depending on how far it can fly.
    int kingMoves = 0;
    for (std::size_t i = 0; i < moves.size(); ++i) {
        const auto& m = moves[i];
        if (m.from != from) continue;
        REQUIRE_FALSE(m.isCapture());
        ++kingMoves;
    }
    REQUIRE(kingMoves >= 4);   // at least one move in each diagonal direction
}

TEST_CASE("Flying king: blocked by own piece stops range") {
    Board b; b.clear();
    const Square king = static_cast<Square>(25);
    b.setPiece(king, Color::Red, PieceKind::King);

    // Place a red man two squares NE from the king on the same diagonal.
    // Walk NE from square 25 until we find the 2nd square.
    Square step1 = bc::step(king, bc::NE);
    Square step2 = bc::step(step1, bc::NE);
    REQUIRE(step1 != kInvalidSquare);
    REQUIRE(step2 != kInvalidSquare);
    b.setPiece(step2, Color::Red, PieceKind::Man);

    b.setPiece(static_cast<Square>(5), Color::Yellow, PieceKind::Man);

    const auto moves = generateLegalMoves(b, Color::Red, RuleSet::InternationalMaxCapture);
    for (std::size_t i = 0; i < moves.size(); ++i) {
        const auto& m = moves[i];
        if (m.from != king) continue;
        // The king must not land on `step2` or beyond in the NE direction.
        // The only NE move allowed is to `step1`.
        if (bc::colOf(m.to) > bc::colOf(king)
            && bc::rowOf(m.to) < bc::rowOf(king)) {
            REQUIRE(m.to == step1);
        }
    }
}

TEST_CASE("Flying king: capture requires empty landing squares") {
    Board b; b.clear();
    const Square king = static_cast<Square>(25);
    b.setPiece(king, Color::Red, PieceKind::King);

    // Yellow man two squares NE. Empty squares between (one square).
    Square step1 = bc::step(king, bc::NE);
    Square step2 = bc::step(step1, bc::NE);
    Square step3 = bc::step(step2, bc::NE);
    REQUIRE(step3 != kInvalidSquare);
    b.setPiece(step2, Color::Yellow, PieceKind::Man);

    // Also give yellow a legal move so the board isn't a stalemate
    // when it's Red's turn (defensive ? we only search Red's moves).
    b.setPiece(static_cast<Square>(5), Color::Yellow, PieceKind::King);

    const auto moves = generateLegalMoves(b, Color::Red, RuleSet::InternationalMaxCapture);
    bool foundCapture = false;
    for (std::size_t i = 0; i < moves.size(); ++i) {
        const auto& m = moves[i];
        if (m.from != king || !m.isCapture()) continue;
        REQUIRE(m.captured == Board::bit(step2));
        // Landing squares beyond the victim ? step3, and further along
        // the NE diagonal while they're empty.
        REQUIRE(bc::colOf(m.to) >= bc::colOf(step3));
        foundCapture = true;
    }
    REQUIRE(foundCapture);
}

TEST_CASE("Flying king: does not capture through an occupied square") {
    Board b; b.clear();
    const Square king = static_cast<Square>(25);
    b.setPiece(king, Color::Red, PieceKind::King);

    // Yellow man two squares NE, but a red man sits one square NE ?
    // the diagonal is blocked, so no capture is possible in that direction.
    Square step1 = bc::step(king, bc::NE);
    Square step2 = bc::step(step1, bc::NE);
    REQUIRE(step2 != kInvalidSquare);
    b.setPiece(step1, Color::Red, PieceKind::Man);
    b.setPiece(step2, Color::Yellow, PieceKind::Man);

    b.setPiece(static_cast<Square>(5), Color::Yellow, PieceKind::King);

    const auto moves = generateLegalMoves(b, Color::Red, RuleSet::InternationalMaxCapture);
    for (std::size_t i = 0; i < moves.size(); ++i) {
        const auto& m = moves[i];
        if (m.from != king) continue;
        // No capture of step2 is allowed.
        REQUIRE((m.captured & Board::bit(step2)) == 0);
    }
}

TEST_CASE("Flying king: captures both directions (forward and backward)") {
    // King at 22 (row 4, col 5) has all four diagonal directions available.
    // Victim SW of 22 = square 27. Under international rules a king may
    // capture in any direction, forward or backward.
    Board b; b.clear();
    constexpr Square king = 22;
    b.setPiece(king, Color::Red, PieceKind::King);

    const Square victim  = bc::step(king, bc::SW);
    REQUIRE(victim != kInvalidSquare);
    const Square landing = bc::step(victim, bc::SW);
    REQUIRE(landing != kInvalidSquare);
    b.setPiece(victim, Color::Yellow, PieceKind::Man);

    b.setPiece(static_cast<Square>(5), Color::Yellow, PieceKind::King);

    const auto moves = generateLegalMoves(b, Color::Red, RuleSet::InternationalMaxCapture);
    bool found = false;
    for (std::size_t i = 0; i < moves.size(); ++i) {
        const auto& m = moves[i];
        if (m.from != king || !m.isCapture()) continue;
        if (m.captured == Board::bit(victim)) { found = true; break; }
    }
    REQUIRE(found);
}
