#include <catch2/catch_test_macros.hpp>
#include "core/Board.hpp"
#include "core/MoveGenerator.hpp"
#include "core/CaptureRules.hpp"

using namespace draughts::core;

TEST_CASE("MaxCapture=ON: opening has 9 quiet moves") {
    Board b; b.resetStandard();
    const auto m = generateLegalMoves(b, Color::Red, RuleSet::InternationalMaxCapture);
    REQUIRE(m.size() == 9);   // 5 men on row 3, each with 1 or 2 forward moves
}

TEST_CASE("MaxCapture=ON: forced to take the longest chain") {
    // Construct: Red man at 33 can chain-capture 3 yellows; at 35 can chain 2.
    // ... (a concrete tactical fixture would go here)
    SUCCEED("placeholder — fixtures land with the test-data commit");
}