#include <catch2/catch_test_macros.hpp>
#include "core/Board.hpp"
#include "core/MoveGenerator.hpp"
#include "ai/AIEngine.hpp"
#include "ai/Search.hpp"
#include "ai/TranspositionTable.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace draughts;

namespace {

std::vector<core::Move> legalMovesFor(const core::Board& b,
                                      core::Color side,
                                      core::RuleSet rules)
{
    const auto span = core::generateLegalMoves(b, side, rules);
    std::vector<core::Move> out;
    for (std::size_t i = 0; i < span.size(); ++i) out.push_back(span[i]);
    return out;
}

bool containsMove(const std::vector<core::Move>& v, const core::Move& m) {
    for (const auto& x : v)
        if (x.from == m.from && x.to == m.to && x.captured == m.captured)
            return true;
    return false;
}

} // namespace

TEST_CASE("Search: returns a legal move from the opening") {
    core::Board b; b.resetStandard();
    std::atomic<bool> stop{false};

    ai::TranspositionTable tt(1u << 14);
    ai::Search real(&tt, /*tb=*/nullptr, &stop, nullptr);
    ai::TimeBudget budget;
    budget.soft = budget.hard = std::chrono::milliseconds(300);
    budget.minimum = std::chrono::milliseconds(0);

    const auto result = real.think(b, core::Color::Red,
                                   core::RuleSet::InternationalMaxCapture,
                                   budget, /*threadId=*/0);

    const auto legal = legalMovesFor(b, core::Color::Red,
                                     core::RuleSet::InternationalMaxCapture);
    REQUIRE(containsMove(legal, result.bestMove));
    REQUIRE(result.completed);
    REQUIRE(result.depth >= 1);
}

TEST_CASE("Search: returns a legal capture when forced") {
    core::Board b; b.clear();
    b.setPiece(22, core::Color::Red,    core::PieceKind::Man);
    b.setPiece(18, core::Color::Yellow, core::PieceKind::Man);
    b.setPiece( 5, core::Color::Yellow, core::PieceKind::Man);

    const auto legal = legalMovesFor(b, core::Color::Red,
                                     core::RuleSet::InternationalMaxCapture);
    REQUIRE(legal.size() >= 1);
    for (const auto& m : legal) REQUIRE(m.isCapture());

    std::atomic<bool> stop{false};
    ai::TranspositionTable tt(1u << 14);
    ai::Search s(&tt, /*tb=*/nullptr, &stop, nullptr);
    ai::TimeBudget budget;
    budget.soft = budget.hard = std::chrono::milliseconds(200);
    budget.minimum = std::chrono::milliseconds(0);

    const auto result = s.think(b, core::Color::Red,
                                core::RuleSet::InternationalMaxCapture,
                                budget, /*threadId=*/0);
    REQUIRE(containsMove(legal, result.bestMove));
}

TEST_CASE("Search: single-move position returns that move with mate-ish score") {
    core::Board b; b.clear();
    b.setPiece(22, core::Color::Red,    core::PieceKind::Man);
    b.setPiece(18, core::Color::Yellow, core::PieceKind::Man);

    const auto legal = legalMovesFor(b, core::Color::Red,
                                     core::RuleSet::InternationalMaxCapture);
    REQUIRE(legal.size() == 1);
    REQUIRE(legal[0].isCapture());

    std::atomic<bool> stop{false};
    ai::TranspositionTable tt(1u << 14);
    ai::Search s(&tt, /*tb=*/nullptr, &stop, nullptr);
    ai::TimeBudget budget;
    budget.soft = budget.hard = std::chrono::milliseconds(200);
    budget.minimum = std::chrono::milliseconds(0);

    const auto result = s.think(b, core::Color::Red,
                                core::RuleSet::InternationalMaxCapture,
                                budget, /*threadId=*/0);
    REQUIRE(result.bestMove.from == legal[0].from);
    REQUIRE(result.bestMove.to   == legal[0].to);
    REQUIRE(result.score > ai::kMateScore - 100);
}

TEST_CASE("Search: stop flag aborts promptly") {
    core::Board b; b.resetStandard();
    std::atomic<bool> stop{false};

    ai::TranspositionTable tt(1u << 14);
    ai::Search s(&tt, /*tb=*/nullptr, &stop, nullptr);
    ai::TimeBudget budget;
    budget.soft    = std::chrono::milliseconds(5000);
    budget.hard    = std::chrono::milliseconds(6000);
    budget.minimum = std::chrono::milliseconds(0);

    std::thread killer([&stop]{
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        stop.store(true);
    });

    const auto t0 = std::chrono::steady_clock::now();
    (void)s.think(b, core::Color::Red,
                  core::RuleSet::InternationalMaxCapture,
                  budget, /*threadId=*/0);
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();

    killer.join();
    REQUIRE(elapsedMs < 3000);
}
