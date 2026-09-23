#include <catch2/catch_test_macros.hpp>
#include "core/Board.hpp"
#include "core/BoardConstants.hpp"
#include "core/MoveGenerator.hpp"

using namespace draughts::core;

TEST_CASE("FreeCapture: shorter chain is legal when a longer exists") {
    Board b; b.clear();
    constexpr Square red  = 22;
    constexpr Square neV1 = 18;
    constexpr Square neV2 = 9;
    constexpr Square seV  = 28;

    b.setPiece(red,  Color::Red,    PieceKind::Man);
    b.setPiece(neV1, Color::Yellow, PieceKind::Man);
    b.setPiece(neV2, Color::Yellow, PieceKind::Man);
    b.setPiece(seV,  Color::Yellow, PieceKind::Man);
    b.setPiece(5,    Color::Yellow, PieceKind::King);

    const auto off = generateLegalMoves(b, Color::Red, RuleSet::InternationalFreeCapture);
    const auto on  = generateLegalMoves(b, Color::Red, RuleSet::InternationalMaxCapture);

    bool offShort = false, offLong = false;
    for (std::size_t i = 0; i < off.size(); ++i) {
        const auto& m = off[i];
        if (m.from != red || !m.isCapture()) continue;
        if (m.captureCount() == 1 && m.captured == Board::bit(seV))  offShort = true;
        if (m.captureCount() == 2
            && m.captured == (Board::bit(neV1) | Board::bit(neV2)))  offLong = true;
    }
    REQUIRE(offShort);
    REQUIRE(offLong);

    int onCount = 0;
    for (std::size_t i = 0; i < on.size(); ++i) {
        const auto& m = on[i];
        if (m.from != red) continue;
        REQUIRE(m.captureCount() == 2);
        ++onCount;
    }
    REQUIRE(onCount > 0);
}

TEST_CASE("FreeCapture: king chains in a non-reverse direction") {
    // King at 22 captures NE (18), lands beyond, then captures SE.
    // Perpendicular direction change is allowed in BOTH variants.
    Board b; b.clear();
    constexpr Square king = 22;
    constexpr Square neV  = 18;

    b.setPiece(king, Color::Red,    PieceKind::King);
    b.setPiece(neV,  Color::Yellow, PieceKind::Man);
    b.setPiece(5,    Color::Yellow, PieceKind::Man);

    const auto off = generateLegalMoves(b, Color::Red, RuleSet::InternationalFreeCapture);
    bool found = false;
    for (std::size_t i = 0; i < off.size(); ++i) {
        const auto& m = off[i];
        if (m.from != king) continue;
        if (m.captured == Board::bit(neV)) { found = true; break; }
    }
    REQUIRE(found);
}

TEST_CASE("FreeCapture: opening position identical to MaxCapture") {
    Board b; b.resetStandard();
    const auto off = generateLegalMoves(b, Color::Red,
                                        RuleSet::InternationalFreeCapture);
    const auto on  = generateLegalMoves(b, Color::Red,
                                        RuleSet::InternationalMaxCapture);
    REQUIRE(off.size() == on.size());
    REQUIRE(off.size() == 9);
}

TEST_CASE("FreeCapture: men may capture backward") {
    Board b; b.clear();
    constexpr Square red   = 22;
    constexpr Square backV = 18;
    b.setPiece(red,   Color::Red,    PieceKind::Man);
    b.setPiece(backV, Color::Yellow, PieceKind::Man);
    b.setPiece(5,     Color::Yellow, PieceKind::Man);

    const auto moves = generateLegalMoves(b, Color::Red,
                                          RuleSet::InternationalFreeCapture);
    bool found = false;
    for (std::size_t i = 0; i < moves.size(); ++i) {
        const auto& m = moves[i];
        if (m.from != red) continue;
        if (m.captured == Board::bit(backV)) { found = true; break; }
    }
    REQUIRE(found);
}
