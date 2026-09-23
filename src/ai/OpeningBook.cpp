#include "OpeningBook.hpp"
#include "core/Notation.hpp"
#include "core/Zobrist.hpp"
#include "core/MoveGenerator.hpp"

#include <algorithm>
#include <bit>
#include <fstream>
#include <sstream>

namespace draughts::ai {

namespace {

std::uint64_t positionHash(const core::Board& board,
                           core::Color        side,
                           core::RuleSet      rules) noexcept
{
    std::uint64_t h = board.pieceHash();
    h ^= core::Zobrist::instance().side(side);
    h ^= static_cast<std::uint64_t>(rules) << 61;
    return h;
}

void applyMove(core::Board& board, const core::Move& m) noexcept {
    core::Bitboard cap = m.captured;
    while (cap) {
        const auto s = static_cast<core::Square>(std::countr_zero(cap));
        cap &= cap - 1;
        board.removePiece(s);
    }
    const auto p = board.at(m.from);
    board.removePiece(m.from);
    board.setPiece(m.to, p.color, p.kind);
    if (p.kind == core::PieceKind::Man && m.isPromotion) board.promote(m.to);
}

} // namespace

bool OpeningBook::loadFromFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) return false;
    std::stringstream ss;
    ss << in.rdbuf();
    loadFromString(ss.str());
    return true;
}

void OpeningBook::loadFromString(const std::string& text) {
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        // Strip comments.
        const auto hashPos = line.find('#');
        if (hashPos != std::string::npos) line.resize(hashPos);

        // Trim whitespace.
        const auto b = line.find_first_not_of(" \t\r\n");
        if (b == std::string::npos) continue;
        const auto e = line.find_last_not_of(" \t\r\n");
        line = line.substr(b, e - b + 1);
        if (line.empty()) continue;

        parseAndInsert(line);
    }
}

void OpeningBook::parseAndInsert(const std::string& line) {
    std::istringstream in(line);
    std::vector<std::string> tokens;
    std::string tok;
    while (in >> tok) tokens.push_back(tok);
    if (tokens.size() < 2) return;   // A single move teaches nothing new.

    core::Board board;
    board.resetStandard();
    core::Color side = core::Color::Red;
    constexpr core::RuleSet rules = core::RuleSet::InternationalMaxCapture;

    bool ok = true;
    for (std::size_t i = 0; i + 1 < tokens.size(); ++i) {
        const auto mv = core::parseMove(board, side, rules, tokens[i]);
        if (!mv) { ok = false; break; }

        const Key key{positionHash(board, side, rules), side, rules};
        auto& list = table_[key];
        const bool dup = std::any_of(list.begin(), list.end(),
            [&](const core::Move& m) {
                return m.from == mv->from && m.to == mv->to
                    && m.captured == mv->captured;
            });
        if (!dup) list.push_back(*mv);

        applyMove(board, *mv);
        side = core::opposite(side);
    }
    if (ok) ++lineCount_;
}

std::optional<core::Move> OpeningBook::pickMove(
    const core::Board& board,
    core::Color        side,
    core::RuleSet      rules,
    std::mt19937_64&   rng) const
{
    const Key key{positionHash(board, side, rules), side, rules};
    const auto it = table_.find(key);
    if (it == table_.end() || it->second.empty()) return std::nullopt;

    // Cross-check against currently legal moves, in case the book file was
    // written for a different rule variant.
    const auto legal = core::generateLegalMoves(board, side, rules);
    std::vector<core::Move> choices;
    choices.reserve(it->second.size());
    for (const auto& recorded : it->second) {
        for (std::size_t i = 0; i < legal.size(); ++i) {
            const auto& l = legal[i];
            if (l.from == recorded.from && l.to == recorded.to
                && l.captured == recorded.captured) {
                choices.push_back(l);
                break;
            }
        }
    }
    if (choices.empty()) return std::nullopt;

    std::uniform_int_distribution<std::size_t> dist(0, choices.size() - 1);
    return choices[dist(rng)];
}

} // namespace draughts::ai
