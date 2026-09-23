#include <catch2/catch_test_macros.hpp>
#include "core/Board.hpp"
#include "core/Perft.hpp"

using namespace draughts::core;

// Reference values are the FMJD-published counts for the standard
// International rules and machine-verified counts for the free-capture
// variant (generated with tools/perft_main.cpp).
TEST_CASE("Perft: standard opening, max-capture ON") {
    Board b; b.resetStandard();
    REQUIRE(perft(b, Color::Red, RuleSet::InternationalMaxCapture, 1).nodes ==      9);
    REQUIRE(perft(b, Color::Red, RuleSet::InternationalMaxCapture, 2).nodes ==     81);
    REQUIRE(perft(b, Color::Red, RuleSet::InternationalMaxCapture, 3).nodes ==    658);
    REQUIRE(perft(b, Color::Red, RuleSet::InternationalMaxCapture, 4).nodes ==   4265);
    REQUIRE(perft(b, Color::Red, RuleSet::InternationalMaxCapture, 5).nodes ==  27117);
    REQUIRE(perft(b, Color::Red, RuleSet::InternationalMaxCapture, 6).nodes == 167140);
}

TEST_CASE("Perft: standard opening, max-capture OFF") {
    Board b; b.resetStandard();
    REQUIRE(perft(b, Color::Red, RuleSet::InternationalFreeCapture, 1).nodes ==      9);
    REQUIRE(perft(b, Color::Red, RuleSet::InternationalFreeCapture, 2).nodes ==     81);
    REQUIRE(perft(b, Color::Red, RuleSet::InternationalFreeCapture, 3).nodes ==    658);
    REQUIRE(perft(b, Color::Red, RuleSet::InternationalFreeCapture, 4).nodes ==   4265);
    REQUIRE(perft(b, Color::Red, RuleSet::InternationalFreeCapture, 5).nodes ==  27132);
    REQUIRE(perft(b, Color::Red, RuleSet::InternationalFreeCapture, 6).nodes == 168316);
}
