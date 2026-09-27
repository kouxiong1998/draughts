#include <catch2/catch_test_macros.hpp>
#include "core/Board.hpp"
#include "ai/Evaluator.hpp"
#include <bit>

using namespace draughts;

TEST_CASE("Evaluator: opening position is symmetric") {
    core::Board b; b.resetStandard();
    // Both sides start with 20 men. Evaluation from Red's side should be
    // the exact negation of the evaluation from Yellow's side, and near 0.
    const int redEval    = ai::evaluate(b, core::Color::Red, core::RuleSet::InternationalMaxCapture);
    const int yellowEval = ai::evaluate(b, core::Color::Yellow, core::RuleSet::InternationalMaxCapture);
    REQUIRE(redEval == -yellowEval);

    // Opening should be roughly balanced (small PST differences only).
    REQUIRE(std::abs(redEval) < 30);
}

TEST_CASE("Evaluator: extra man favors the owner") {
    core::Board b; b.resetStandard();
    // Remove a Yellow man; Red is now up a man.
    b.removePiece(35);   // some yellow square near the top of their setup

    const int redEval    = ai::evaluate(b, core::Color::Red, core::RuleSet::InternationalMaxCapture);
    const int yellowEval = ai::evaluate(b, core::Color::Yellow, core::RuleSet::InternationalMaxCapture);

    // Red should be ahead by ~100 cp (one man) in both evaluations.
    REQUIRE(redEval > 60);
    REQUIRE(yellowEval < -60);
}

TEST_CASE("Evaluator: extra king favors the owner") {
    core::Board b; b.clear();
    b.setPiece(10, core::Color::Red,    core::PieceKind::King);
    b.setPiece(40, core::Color::Yellow, core::PieceKind::King);
    // Add one extra Red man so the position is clearly Red-favored.
    b.setPiece(25, core::Color::Red,    core::PieceKind::Man);

    const int redEval = ai::evaluate(b, core::Color::Red, core::RuleSet::InternationalMaxCapture);
    REQUIRE(redEval > 80);   // at least a man's worth of advantage
}

TEST_CASE("Evaluator: negamax convention - positive = good for mover") {
    core::Board b; b.resetStandard();
    b.removePiece(30);   // remove a Yellow man

    const int redPerspective    = ai::evaluate(b, core::Color::Red, core::RuleSet::InternationalMaxCapture);
    const int yellowPerspective = ai::evaluate(b, core::Color::Yellow, core::RuleSet::InternationalMaxCapture);

    // Red is ahead, so evaluating with Red as the mover is positive;
    // evaluating with Yellow as the mover is negative.
    REQUIRE(redPerspective    > 0);
    REQUIRE(yellowPerspective < 0);
}


TEST_CASE("Evaluator: 180-degree mirror negates eval exactly") {
    // Symmetry check: rotate the board 180 degrees AND swap colors.
    // Evaluation must flip sign exactly. Any PST or term asymmetry
    // would break this and cause a Red/Yellow strength bias.
    core::Board b;
    b.resetStandard();
    b.removePiece(11);
    b.removePiece(38);
    b.setPiece(22, core::Color::Red, core::PieceKind::King);

    const int redEval = ai::evaluate(b, core::Color::Red,
                                     core::RuleSet::InternationalMaxCapture);

    core::Board m;
    m.clear();
    core::Bitboard bb = b.occupied();
    while (bb) {
        const auto sq = static_cast<core::Square>(std::countr_zero(bb));
        bb &= bb - 1;
        const auto p = b.at(sq);
        m.setPiece(core::bc::mirrorSquare(sq), core::opposite(p.color), p.kind);
    }

    const int mirrorEval = ai::evaluate(m, core::Color::Yellow,
                                        core::RuleSet::InternationalMaxCapture);

    REQUIRE(redEval == mirrorEval);
}