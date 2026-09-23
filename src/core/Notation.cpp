#include "Notation.hpp"
#include "BoardConstants.hpp"
#include "MoveGenerator.hpp"

#include <array>
#include <charconv>
#include <sstream>
#include <vector>

namespace draughts::core {

namespace {

std::optional<Square> parseSquareToken(std::string_view s) {
    if (s.empty()) return std::nullopt;
    int n = 0;
    const auto* b = s.data();
    const auto* e = s.data() + s.size();
    const auto r = std::from_chars(b, e, n);
    if (r.ec != std::errc{} || r.ptr != e) return std::nullopt;
    if (n < 1 || n > kNumPlayableSquares) return std::nullopt;
    return static_cast<Square>(n - 1);
}

void splitInto(std::string_view s, char sep, std::vector<std::string_view>& out) {
    out.clear();
    std::size_t start = 0;
    while (start <= s.size()) {
        const std::size_t pos = s.find(sep, start);
        if (pos == std::string_view::npos) {
            out.push_back(s.substr(start));
            return;
        }
        out.push_back(s.substr(start, pos - start));
        start = pos + 1;
    }
}

} // namespace

std::string formatMove(const Board& boardBefore, Color side, const Move& move) {
    std::ostringstream os;
    auto sq1 = [](Square s) { return static_cast<int>(s) + 1; };

    if (!move.isCapture()) {
        os << sq1(move.from) << '-' << sq1(move.to);
        return os.str();
    }

    std::array<Square, 32> landings{};
    const bool ok = expandChainLandings(boardBefore, side, move,
                                        landings.data(),
                                        static_cast<int>(landings.size()));
    os << sq1(move.from);
    if (ok) {
        for (std::size_t i = 1; i < landings.size(); ++i) {
            if (landings[i] == kInvalidSquare) break;
            os << 'x' << sq1(landings[i]);
            if (landings[i] == move.to) break;
        }
    } else {
        os << 'x' << sq1(move.to);
    }
    return os.str();
}

std::string formatMoveNumber(int ply, Color mover) {
    std::ostringstream os;
    const int n = ply / 2 + 1;
    os << n << (mover == Color::Red ? "." : "...");
    return os.str();
}

std::optional<Move> parseMove(const Board&     board,
                              Color            side,
                              RuleSet          rules,
                              std::string_view token)
{
    if (token.empty()) return std::nullopt;

    const bool isCapture = token.find('x') != std::string_view::npos;
    const bool isQuiet   = !isCapture && token.find('-') != std::string_view::npos;
    if (!isCapture && !isQuiet) return std::nullopt;

    std::vector<std::string_view> parts;
    splitInto(token, isCapture ? 'x' : '-', parts);
    if (parts.size() < 2) return std::nullopt;

    std::vector<Square> path;
    path.reserve(parts.size());
    for (const auto& p : parts) {
        const auto sq = parseSquareToken(p);
        if (!sq) return std::nullopt;
        path.push_back(*sq);
    }

    const auto moves = generateLegalMoves(board, side, rules);

    if (isQuiet) {
        if (path.size() != 2) return std::nullopt;
        for (std::size_t i = 0; i < moves.size(); ++i) {
            const auto& m = moves[i];
            if (!m.isCapture() && m.from == path[0] && m.to == path[1]) return m;
        }
        return std::nullopt;
    }

    // Capture chain: match from, to, and every intermediate landing.
    for (std::size_t i = 0; i < moves.size(); ++i) {
        const auto& m = moves[i];
        if (!m.isCapture()) continue;
        if (m.from != path.front() || m.to != path.back()) continue;

        std::array<Square, 32> landings{};
        if (!expandChainLandings(board, side, m, landings.data(),
                                 static_cast<int>(landings.size()))) continue;

        bool ok = true;
        for (std::size_t k = 0; k < path.size(); ++k) {
            if (landings[k] != path[k]) { ok = false; break; }
        }
        if (ok) return m;
    }
    return std::nullopt;
}

} // namespace draughts::core
