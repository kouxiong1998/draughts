#include <catch2/catch_test_macros.hpp>
#include "core/Board.hpp"
#include "core/BoardConstants.hpp"
#include "core/MoveGenerator.hpp"

using namespace draughts::core;

TEST_CASE("FreeCapture: king MUST continue chain while victims remain") {
    // Construct a straight-line chain on the NE diagonal:
    //
    //   King(27) . V1(18) . V2(9) . landing(4)
    //
    // One empty square between the king and V1, one between V1 and V2,
    // and an empty landing square beyond V2. The king MUST capture both.
    // There is no legal move that captures only V1 and stops.
    Board b; b.clear();
    constexpr Square king = 27;

    const Square step1 = bc::step(king, bc::NE);   // 22
    const Square v1    = bc::step(step1, bc::NE);  // 18
    const Square step2 = bc::step(v1, bc::NE);     // 13
    const Square v2    = bc::step(step2, bc::NE);  // 9
    const Square land  = bc::step(v2, bc::NE);     // 4

    REQUIRE(step1 != kInvalidSquare);
    REQUIRE(v1    != kInvalidSquare);
    REQUIRE(step2 != kInvalidSquare);
    REQUIRE(v2    != kInvalidSquare);
    REQUIRE(land  != kInvalidSquare);   // V2 must have landing room

    b.setPiece(king, Color::Yellow, PieceKind::King);
    b.setPiece(v1,   Color::Red,    PieceKind::Man);
    b.setPiece(v2,   Color::Red,    PieceKind::Man);

    const auto moves = generateLegalMoves(b, Color::Yellow,
                                          RuleSet::InternationalFreeCapture);

    REQUIRE(!moves.empty());
    const Bitboard expected = Board::bit(v1) | Board::bit(v2);
    for (std::size_t i = 0; i < moves.size(); ++i) {
        const auto& m = moves[i];
        REQUIRE(m.isCapture());
        REQUIRE(m.from     == king);
        REQUIRE(m.captured == expected);
    }
}

TEST_CASE("FreeCapture: man MUST continue chain while victims remain") {
    //   Man(27) . V1(22) . V2(13) . landing(9)
    // Adjacent captures, one empty landing between victims.
    Board b; b.clear();
    constexpr Square man = 27;

    const Square v1     = bc::step(man,  bc::NE);   // 22
    const Square land1  = bc::step(v1,   bc::NE);   // 18
    const Square v2     = bc::step(land1, bc::NE);  // 13
    const Square land2  = bc::step(v2,   bc::NE);   // 9

    REQUIRE(v1    != kInvalidSquare);
    REQUIRE(land1 != kInvalidSquare);
    REQUIRE(v2    != kInvalidSquare);
    REQUIRE(land2 != kInvalidSquare);

    b.setPiece(man, Color::Red,    PieceKind::Man);
    b.setPiece(v1,  Color::Yellow, PieceKind::Man);
    b.setPiece(v2,  Color::Yellow, PieceKind::Man);

    const auto moves = generateLegalMoves(b, Color::Red,
                                          RuleSet::InternationalFreeCapture);

    REQUIRE(!moves.empty());
    const Bitboard expected = Board::bit(v1) | Board::bit(v2);
    for (std::size_t i = 0; i < moves.size(); ++i) {
        const auto& m = moves[i];
        REQUIRE(m.isCapture());
        REQUIRE(m.from     == man);
        REQUIRE(m.captured == expected);
    }
}

TEST_CASE("MaxCapture ON: FMJD rules - no direction restriction in chain") {
    // Same king position as the FreeCapture test, under international rules.
    Board b; b.clear();
    constexpr Square king = 27;

    const Square step1 = bc::step(king, bc::NE);
    const Square v1    = bc::step(step1, bc::NE);
    const Square step2 = bc::step(v1, bc::NE);
    const Square v2    = bc::step(step2, bc::NE);
    const Square land  = bc::step(v2, bc::NE);

    REQUIRE(land != kInvalidSquare);

    b.setPiece(king, Color::Yellow, PieceKind::King);
    b.setPiece(v1,   Color::Red,    PieceKind::Man);
    b.setPiece(v2,   Color::Red,    PieceKind::Man);

    const auto moves = generateLegalMoves(b, Color::Yellow,
                                          RuleSet::InternationalMaxCapture);
    REQUIRE(!moves.empty());
    const Bitboard expected = Board::bit(v1) | Board::bit(v2);
    for (std::size_t i = 0; i < moves.size(); ++i) {
        REQUIRE(moves[i].captured == expected);
    }
}
