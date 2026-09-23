#include "SaveGame.hpp"
#include "core/Notation.hpp"
#include "core/MoveGenerator.hpp"
#include "core/BoardConstants.hpp"

#include <QFile>
#include <QStringList>
#include <QTextStream>

#include <array>
#include <charconv>
#include <optional>

namespace draughts::ui {

namespace {

QString colorName(core::Color c) {
    return c == core::Color::Red ? QStringLiteral("red") : QStringLiteral("yellow");
}

std::optional<core::Color> parseColor(const QString& s) {
    if (s == "red")    return core::Color::Red;
    if (s == "yellow") return core::Color::Yellow;
    return std::nullopt;
}

std::optional<core::RuleSet> parseRules(const QString& s) {
    if (s == "maxcapture") return core::RuleSet::InternationalMaxCapture;
    if (s == "freecapture") return core::RuleSet::InternationalFreeCapture;
    return std::nullopt;
}

/// Parse a square label like "32" into a Square (0-based).
std::optional<core::Square> parseSquare(const QString& s) {
    bool ok = false;
    const int n = s.toInt(&ok);
    if (!ok || n < 1 || n > core::kNumPlayableSquares) return std::nullopt;
    return static_cast<core::Square>(n - 1);
}

/// Try to resolve a notation token (e.g. "32-28" or "32x19x10") into one
/// of the legal moves generated for the current position.
std::optional<core::Move> resolveMove(const core::Board&   board,
                                      core::Color          side,
                                      core::RuleSet        rules,
                                      const QString&       token,
                                      QString*             errOut)
{
    // Two cases: quiet move "32-28", capture chain "32x19x10".
    if (token.contains('-')) {
        const auto parts = token.split('-');
        if (parts.size() != 2) { if (errOut) *errOut = "malformed quiet move: " + token; return std::nullopt; }
        const auto from = parseSquare(parts[0]);
        const auto to   = parseSquare(parts[1]);
        if (!from || !to) { if (errOut) *errOut = "bad square in: " + token; return std::nullopt; }

        const auto moves = core::generateLegalMoves(board, side, rules);
        for (std::size_t i = 0; i < moves.size(); ++i) {
            const auto& m = moves[i];
            if (!m.isCapture() && m.from == *from && m.to == *to) return m;
        }
        if (errOut) *errOut = "no legal quiet move: " + token;
        return std::nullopt;
    }

    if (token.contains('x')) {
        const auto parts = token.split('x');
        if (parts.size() < 2) { if (errOut) *errOut = "malformed capture: " + token; return std::nullopt; }

        std::vector<core::Square> targetPath;
        targetPath.reserve(static_cast<std::size_t>(parts.size()));
        for (const auto& p : parts) {
            const auto sq = parseSquare(p);
            if (!sq) { if (errOut) *errOut = "bad square in: " + token; return std::nullopt; }
            targetPath.push_back(*sq);
        }

        const auto moves = core::generateLegalMoves(board, side, rules);
        std::optional<core::Move> match;
        int ambiguities = 0;
        for (std::size_t i = 0; i < moves.size(); ++i) {
            const auto& m = moves[i];
            if (!m.isCapture()) continue;
            if (m.from != targetPath.front()) continue;
            if (m.to   != targetPath.back())  continue;

            // Full path check.
            std::array<core::Square, 32> path{};
            if (!core::expandChainLandings(board, side, m, path.data(),
                                           static_cast<int>(path.size()))) {
                continue;
            }
            bool ok = true;
            for (std::size_t k = 0; k < targetPath.size(); ++k) {
                if (path[k] == core::kInvalidSquare || path[k] != targetPath[k]) {
                    ok = false; break;
                }
            }
            if (!ok) continue;

            if (match) { ++ambiguities; }
            else match = m;
        }
        if (!match) { if (errOut) *errOut = "no legal capture: " + token; return std::nullopt; }
        if (ambiguities > 0) {
            // Rare: multiple chains landing on the same set of squares.
            // Fall back to the first legal match ? deterministic but
            // loses full fidelity. Notation fully disambiguates in practice.
        }
        return match;
    }

    if (errOut) *errOut = "unrecognised token: " + token;
    return std::nullopt;
}

} // namespace

bool SaveGame::save(const core::GameEngine& engine,
                    core::Color             playerColor,
                    const QString&          path,
                    QString*                errorOut)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorOut) *errorOut = f.errorString();
        return false;
    }

    QTextStream out(&f);
    out << "DRAUGHTS 1\n";
    out << "rules=" << (engine.rules() == core::RuleSet::InternationalMaxCapture
                             ? "maxcapture" : "freecapture") << "\n";
    out << "first=" << colorName(core::Color::Red) << "\n";  // informational
    out << "playerColor=" << colorName(playerColor) << "\n";
    out << "moves:\n";

    core::Board board; board.resetStandard();
    core::Color side = core::Color::Red;

    const auto& hist = engine.history();
    const auto  cursor = engine.historyCursor();

    for (std::size_t i = 0; i < cursor; ++i) {
        const auto& rec = hist[i];
        out << QString::fromStdString(core::formatMove(board, side, rec.move)) << "\n";

        // Advance board for next iteration's notation.
        core::Board next = board;
        core::Bitboard cap = rec.move.captured;
        while (cap) {
            const auto s = static_cast<core::Square>(std::countr_zero(cap));
            cap &= cap - 1;
            next.removePiece(s);
        }
        const auto p = next.at(rec.move.from);
        next.removePiece(rec.move.from);
        next.setPiece(rec.move.to, p.color, p.kind);
        if (p.kind == core::PieceKind::Man && rec.move.isPromotion)
            next.promote(rec.move.to);
        board = next;
        side  = core::opposite(side);
    }
    out.flush();
    f.close();
    return true;
}

bool SaveGame::load(core::GameEngine& engine,
                    core::Color&      playerColorOut,
                    const QString&    path,
                    QString*          errorOut)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorOut) *errorOut = f.errorString();
        return false;
    }

    QTextStream in(&f);
    const QString header = in.readLine().trimmed();
    if (!header.startsWith("DRAUGHTS")) {
        if (errorOut) *errorOut = "not a Draughts save file";
        return false;
    }

    core::RuleSet rules = core::RuleSet::InternationalMaxCapture;
    core::Color   playerColor = core::Color::Yellow;

    QString line;
    while (!in.atEnd()) {
        line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        if (line == "moves:") break;
        const int eq = line.indexOf('=');
        if (eq < 0) continue;
        const QString key = line.left(eq);
        const QString val = line.mid(eq + 1);
        if (key == "rules") {
            const auto r = parseRules(val);
            if (r) rules = *r;
        } else if (key == "playerColor") {
            const auto c = parseColor(val);
            if (c) playerColor = *c;
        }
    }

    // Fresh engine with the right rule set, first move always Red.
    engine.newGame(rules, core::Color::Red);

    // Replay every following non-empty line.
    while (!in.atEnd()) {
        const QString token = in.readLine().trimmed();
        if (token.isEmpty() || token.startsWith('#')) continue;

        const auto move = resolveMove(engine.board(), engine.sideToMove(),
                                      engine.rules(), token, errorOut);
        if (!move) return false;

        if (!engine.tryApply(*move)) {
            if (errorOut) *errorOut = "engine refused legal move: " + token;
            return false;
        }
    }

    playerColorOut = playerColor;
    return true;
}

} // namespace draughts::ui
