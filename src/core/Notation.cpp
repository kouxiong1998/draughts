#include "Notation.hpp"
#include "BoardConstants.hpp"
#include "MoveGenerator.hpp"

#include <array>
#include <functional>
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
    return bc::fromDisplayLabel(n);
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
    auto lab = [](Square s) { return bc::displayLabel(s); };

    if (!move.isCapture()) {
        os << lab(move.from) << '-' << lab(move.to);
        return os.str();
    }

    std::array<Square, 32> landings{};
    const bool ok = expandChainLandings(boardBefore, side, move,
                                        landings.data(),
                                        static_cast<int>(landings.size()));
    os << lab(move.from);
    if (ok) {
        for (std::size_t i = 1; i < landings.size(); ++i) {
            if (landings[i] == kInvalidSquare) break;
            os << 'x' << lab(landings[i]);
            if (landings[i] == move.to) break;
        }
    } else {
        os << 'x' << lab(move.to);
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

namespace {

// Reconstruct the landing sequence of a capture chain:
//   [from, hop1, hop2, ..., to]
// Uses the board for geometry, respects ghost squares (already-captured
// pieces remain on the board and can be landed on). Returns empty vector
// on failure.
std::vector<Square> reconstructChain(const Board& board, Color side,
                                     const Move& move)
{
    std::vector<Square> path;
    if (!move.isCapture()) {
        path = {move.from, move.to};
        return path;
    }

    const Piece p = board.at(move.from);
    if (p.empty() || p.color != side) return path;

    const bool isKing = (p.kind == PieceKind::King);
    path.push_back(move.from);

    std::function<bool(Square, Bitboard)> rec =
        [&](Square cursor, Bitboard remaining) -> bool {
            if (remaining == 0) return cursor == move.to;

            const Bitboard ghosts = move.captured & ~remaining;

            for (int d = 0; d < bc::kNumDirs; ++d) {
                Square victim = kInvalidSquare;

                if (isKing) {
                    for (int k = 0; k < kBoardSize; ++k) {
                        const Square s = bc::ray(cursor, d, k);
                        if (s == kInvalidSquare) break;
                        if (remaining & Board::bit(s)) { victim = s; break; }
                        if (!board.empty(s) && !(ghosts & Board::bit(s))) break;
                    }
                } else {
                    const Square v = bc::step(cursor, d);
                    if (v != kInvalidSquare && (remaining & Board::bit(v)))
                        victim = v;
                }
                if (victim == kInvalidSquare) continue;

                if (isKing) {
                    for (int k = 0; k < kBoardSize; ++k) {
                        const Square landing = bc::ray(victim, d, k);
                        if (landing == kInvalidSquare) break;
                        if (!board.empty(landing)
                            && !(ghosts & Board::bit(landing))) break;
                        path.push_back(landing);
                        if (rec(landing, remaining & ~Board::bit(victim)))
                            return true;
                        path.pop_back();
                    }
                } else {
                    const Square landing = bc::step(victim, d);
                    if (landing == kInvalidSquare) continue;
                    path.push_back(landing);
                    if (rec(landing, remaining & ~Board::bit(victim)))
                        return true;
                    path.pop_back();
                }
            }
            return false;
        };

    if (!rec(move.from, move.captured)) path.clear();
    return path;
}

} // namespace

std::string formatMoveHistory(const Board& boardBefore, Color side, const Move& move) {
    std::ostringstream os;

    auto prefix = [](Color c, PieceKind k) -> std::string {
        std::string p = (c == Color::Red) ? "R" : "Y";
        if (k == PieceKind::King) p += "K";
        return p;
    };

    const Piece mover = boardBefore.at(move.from);
    if (mover.empty()) return "?";
    const Color c = mover.color;
    const PieceKind startKind = mover.kind;

    if (!move.isCapture()) {
        const bool promotes = (startKind == PieceKind::Man && move.isPromotion);
        const PieceKind endKind = promotes ? PieceKind::King : startKind;
        os << prefix(c, startKind) << bc::displayLabel(move.from)
           << " - "
           << prefix(c, endKind) << bc::displayLabel(move.to);
        return os.str();
    }

    // Use expandChainLandings (the same routine the animation uses).
    // It returns the full landing path starting with move.from and
    // ending with move.to.
    std::array<Square, 32> landings{};
    const bool ok = expandChainLandings(boardBefore, side, move,
                                        landings.data(),
                                        static_cast<int>(landings.size()));

    if (!ok) {
        os << prefix(c, startKind) << bc::displayLabel(move.from)
           << " X " << prefix(c, startKind) << bc::displayLabel(move.to);
        return os.str();
    }

    // A capture chain has exactly 1 + captureCount() elements. Do not
    // stop at the first occurrence of move.to - the first hop can
    // coincide with the final landing (e.g. 24 -> 33 -> ... -> 33).
    const std::size_t expected = 1
        + static_cast<std::size_t>(move.captureCount());
    os << prefix(c, startKind) << bc::displayLabel(landings[0]);
    for (std::size_t i = 1; i < expected && i < landings.size(); ++i) {
        const Square s = landings[i];
        if (s == kInvalidSquare) break;

        const bool isLastHop = (i + 1 == expected);
        const bool promotesHere = isLastHop
                                  && startKind == PieceKind::Man
                                  && move.isPromotion;
        const PieceKind hopKind = promotesHere ? PieceKind::King : startKind;

        os << " X " << prefix(c, hopKind) << bc::displayLabel(s);
    }
    return os.str();
}
} // namespace draughts::core
