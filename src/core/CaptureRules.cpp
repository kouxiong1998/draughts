#include "CaptureRules.hpp"
#include "BoardConstants.hpp"
#include <algorithm>

namespace draughts::core {
namespace {

struct ChainCtx {
    const Board* board{nullptr};
    Color        side{Color::Red};
    RuleSet      rules{RuleSet::InternationalMaxCapture};

    Bitboard     captured{};
    Square       from{kInvalidSquare};
    MoveSpan*    out{nullptr};
};

inline bool oppositeDir(int a, int b) noexcept {
    if (a < 0 || b < 0) return false;
    return (a + b) == 3;   // NE(0)<->SW(3), NW(1)<->SE(2)
}

void manChain(ChainCtx& ctx, Square cursor, int lastDir) {
    bool extended = false;
    for (int d = 0; d < bc::kNumDirs; ++d) {
        if (ctx.rules == RuleSet::InternationalFreeCapture && oppositeDir(d, lastDir))
            continue;

        const Square victim = bc::step(cursor, d);
        if (victim == kInvalidSquare) continue;
        if (!ctx.board->hasPiece(opposite(ctx.side), victim)) continue;
        if (ctx.captured & Board::bit(victim)) continue;

        const Square landing = bc::step(victim, d);
        if (landing == kInvalidSquare) continue;
        if (!ctx.board->empty(landing) && !(ctx.captured & Board::bit(landing)))
            continue;

        extended = true;
        const Bitboard savedCaptured = ctx.captured;
        ctx.captured |= Board::bit(victim);
        manChain(ctx, landing, d);
        ctx.captured = savedCaptured;
    }
    if (!extended) {
        if (ctx.captured == 0) return;
        Move m;
        m.from        = ctx.from;
        m.to          = cursor;
        m.captured    = ctx.captured;
        m.isPromotion = bc::rowOf(cursor) == bc::promotionRow(ctx.side);
        ctx.out->push_back(m);
    }
}

void kingChain(ChainCtx& ctx, Square cursor, int lastDir) {
    bool extended = false;
    for (int d = 0; d < bc::kNumDirs; ++d) {
        if (ctx.rules == RuleSet::InternationalFreeCapture && oppositeDir(d, lastDir))
            continue;

        Square victim = kInvalidSquare;
        for (int k = 0; k < kBoardSize; ++k) {
            const Square s = bc::ray(cursor, d, k);
            if (s == kInvalidSquare) break;
            if (ctx.captured & Board::bit(s)) continue;
            if (ctx.board->hasPiece(opposite(ctx.side), s)) { victim = s; break; }
            if (!ctx.board->empty(s)) break;
        }
        if (victim == kInvalidSquare) continue;

        for (int k = 0; k < kBoardSize; ++k) {
            const Square landing = bc::ray(victim, d, k);
            if (landing == kInvalidSquare) break;
            if (!ctx.board->empty(landing) && !(ctx.captured & Board::bit(landing))) break;

            extended = true;
            const Bitboard saved = ctx.captured;
            ctx.captured |= Board::bit(victim);
            kingChain(ctx, landing, d);
            ctx.captured = saved;
        }
    }
    if (!extended) {
        if (ctx.captured == 0) return;
        Move m;
        m.from        = ctx.from;
        m.to          = cursor;
        m.captured    = ctx.captured;
        m.isPromotion = false;
        ctx.out->push_back(m);
    }
}

} // namespace

void generateCapturesFrom(const Board& board,
                          Square       sq,
                          Color        side,
                          RuleSet      rules,
                          MoveSpan&    out) noexcept
{
    const Piece p = board.at(sq);
    if (p.empty() || p.color != side) return;

    ChainCtx ctx;
    ctx.board = &board;
    ctx.side  = side;
    ctx.rules = rules;
    ctx.from  = sq;
    ctx.out   = &out;

    if (p.kind == PieceKind::Man) manChain(ctx, sq, -1);
    else                          kingChain(ctx, sq, -1);
}

} // namespace draughts::core
