#include "GameEngine.hpp"
#include "MoveGenerator.hpp"
#include "Zobrist.hpp"
#include <bit>
#include <cassert>

namespace draughts::core {

    GameEngine::GameEngine() { newGame(); }
    GameEngine::GameEngine(RuleSet rules) { newGame(rules); }

    void GameEngine::resetInternal() {
        history_.clear();
        cursor_ = 0;
        kings3v1Plies_ = 0;
        kings2v1Plies_ = 0;
        result_ = GameResult::Ongoing;
    }

    void GameEngine::newGame(RuleSet rules, Color first) {
        board_.resetStandard();
        state_.reset(first, rules);
        resetInternal();
        state_.notePosition(positionHash());
    }

    std::uint64_t GameEngine::positionHash() const noexcept {
        const auto& z = Zobrist::instance();
        std::uint64_t h = board_.pieceHash();
        h ^= z.side(state_.sideToMove());
        h ^= static_cast<std::uint64_t>(state_.rules()) << 60;   // mix rule variant
        return h;
    }

    void GameEngine::applyMoveToBoard(const Move& m, bool wasPromoted) noexcept {
        const Piece p = board_.at(m.from);
        if (p.empty()) return;   // defensive

        Bitboard cap = m.captured;
        while (cap) {
            const Square c = static_cast<Square>(std::countr_zero(cap));
            cap &= cap - 1;
            board_.removePiece(c);
        }

        board_.removePiece(m.from);
        if (wasPromoted) {
            board_.setPiece(m.to, p.color, PieceKind::King);
        } else {
            board_.setPiece(m.to, p.color, p.kind);
        }
    }

    void GameEngine::undoMoveFromBoard(const Move& m, bool wasPromoted) noexcept {
        const Piece moved = board_.at(m.to);
        if (moved.empty()) return;   // defensive
        const Color c = moved.color;

        board_.removePiece(m.to);
        if (wasPromoted) {
            board_.setPiece(m.from, c, PieceKind::Man);
        } else {
            board_.setPiece(m.from, c, moved.kind);
        }

        const Color opp = opposite(c);
        Bitboard cap = m.captured;
        while (cap) {
            const Square s = static_cast<Square>(std::countr_zero(cap));
            cap &= cap - 1;
            board_.setPiece(s, opp, PieceKind::Man);
        }
    }

    bool GameEngine::tryApply(const Move& move) noexcept {
        if (result_ != GameResult::Ongoing) return false;

        const Color mover = state_.sideToMove();
        const Piece p = board_.at(move.from);
        if (p.empty() || p.color != mover) return false;

        // Match against the engine's own generated move list, and
        // use the generator's Move (not the caller's) for state
        // changes and history. This guarantees isPromotion is correct
        // even if the caller passed a move with a stale promotion flag.
        const MoveSpan legal = legalMoves();
        bool legalFound = false;
        Move matched = move;
        for (std::size_t i = 0; i < legal.size(); ++i) {
            const Move& c = legal[i];
            if (c.from == move.from && c.to == move.to && c.captured == move.captured) {
                matched = c;
                legalFound = true; break;
            }
        }
        if (!legalFound) return false;

        // Truncate any redone future before committing.
        if (cursor_ < history_.size()) history_.resize(cursor_);

        const bool movedKing = p.kind == PieceKind::King;
        const bool wasPromoted = (!movedKing)
            && (p.kind == PieceKind::Man)
            && (bc::rowOf(matched.to) == bc::promotionRow(p.color));
        const bool wasCap = matched.isCapture();

        applyMoveToBoard(matched, wasPromoted);
        state_.applyMoveBookkeeping(mover, movedKing, wasCap);
        state_.notePosition(positionHash());

        history_.push_back(MoveRecord{ matched, mover, movedKing, wasCap, wasPromoted, positionHash() });
        ++cursor_;

        updateSmallEndgameCounters(mover, movedKing, wasCap);
        updateResultAfterMove();
        return true;
    }

    bool GameEngine::undo() noexcept {
        if (!canUndo()) return false;
        const MoveRecord& rec = history_[cursor_ - 1];
        undoMoveFromBoard(rec.move, rec.wasPromoted);
        --cursor_;

        // Rebuild state cleanly from scratch (cheap ??? game is short).
        state_.reset(history_.empty() ? Color::Red : history_[0].mover,
            state_.rules());
        // Replay bookkeeping only (do not re-apply moves to board).
        for (std::size_t i = 0; i < cursor_; ++i) {
            const auto& r = history_[i];
            state_.applyMoveBookkeeping(r.mover, r.movedKing, r.wasCapture);
            state_.notePosition(r.positionHashAfter);
        }
        result_ = GameResult::Ongoing;
        kings3v1Plies_ = 0;
        kings2v1Plies_ = 0;
    return true;
}

    bool GameEngine::redo() noexcept {
        if (!canRedo()) return false;
        const MoveRecord& rec = history_[cursor_];
        applyMoveToBoard(rec.move, rec.wasPromoted);
        state_.applyMoveBookkeeping(rec.mover, rec.movedKing, rec.wasCapture);
        state_.notePosition(rec.positionHashAfter);
        ++cursor_;
        updateSmallEndgameCounters(rec.mover, rec.movedKing, rec.wasCapture);
        updateResultAfterMove();
        return true;
    }

    void GameEngine::resign() noexcept {
        result_ = (state_.sideToMove() == Color::Red) ? GameResult::YellowWins
            : GameResult::RedWins;
    }

    void GameEngine::agreeDraw() noexcept { result_ = GameResult::Draw; }

    void GameEngine::updateSmallEndgameCounters(Color mover, bool movedKing, bool wasCapture) noexcept {
        if (wasCapture || !movedKing) { kings3v1Plies_ = kings2v1Plies_ = 0; return; }

        const int rK = board_.count(Color::Red, PieceKind::King);
        const int yK = board_.count(Color::Yellow, PieceKind::King);
        const int rM = board_.count(Color::Red, PieceKind::Man);
        const int yM = board_.count(Color::Yellow, PieceKind::Man);
        const bool kingsOnly = (rM == 0 && yM == 0);

        // 4-piece rule: when exactly 4 pieces are on the board, count plies.
    // 40 plies (= 20 moves by both sides) with no capture ? draw.
    const int totalPieces = rK + yK + rM + yM;
    if (wasCapture || totalPieces != 4) {
        fourPiecePlies_ = 0;
    } else {
        ++fourPiecePlies_;
    }

    if (!kingsOnly) { kings3v1Plies_ = kings2v1Plies_ = 0; return; }

        if (rK + yK == 4 && (rK == 3 || yK == 3)) { ++kings3v1Plies_; kings2v1Plies_ = 0; }
        else if (rK + yK == 3 && (rK == 2 || yK == 2)) { ++kings2v1Plies_; kings3v1Plies_ = 0; }
        else { kings3v1Plies_ = kings2v1Plies_ = 0; }
        (void)mover;
    }

    void GameEngine::updateResultAfterMove() {
        // No legal moves for side to move ??? that side loses.
        const MoveSpan legal = legalMoves();
        if (legal.empty()) {
            result_ = (state_.sideToMove() == Color::Red) ? GameResult::YellowWins
                : GameResult::RedWins;
            return;
        }

        // Threefold repetition.
        if (state_.isThreefoldRepetition(positionHash())) {
            result_ = GameResult::Draw; return;
        }

        // 25-move rule (king-only halfmove clock).
        if (state_.isTwentyFiveMoveDraw()) { result_ = GameResult::Draw; return; }

        // FMJD small-endgame move limits: 3v1 = 16 moves (=32 plies),
        // 2v1 = 5 moves (=10 plies). Both sides must have completed that many
        // king-only plies with no capture.
        if (kings3v1Plies_ >= 32) { result_ = GameResult::Draw; return; }
        if (kings2v1Plies_ >= 10) { result_ = GameResult::Draw; return; }

        // 4-piece rule: 20 moves (40 plies) with no capture at exactly
        // 4 pieces on the board ? draw.
        if (fourPiecePlies_ >= 40) { result_ = GameResult::Draw; return; }
    }

} // namespace draughts::core
