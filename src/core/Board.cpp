#include "Board.hpp"
#include "Zobrist.hpp"
#include <cassert>
#include <sstream>

namespace draughts::core {

    void Board::clear() noexcept {
        red_ = yellow_ = kings_ = occupied_ = 0;
        hash_ = 0;
    }

    void Board::resetStandard() noexcept {
        clear();
        red_ = bc::kInitialRed;
        yellow_ = bc::kInitialYellow;
        occupied_ = red_ | yellow_;
        recomputeHash();
    }

    Piece Board::at(Square sq) const noexcept {
        const Bitboard b = bit(sq);
        if (!(occupied_ & b)) return { Color::Red, PieceKind::None };
        const Color c = (red_ & b) ? Color::Red : Color::Yellow;
        const PieceKind k = (kings_ & b) ? PieceKind::King : PieceKind::Man;
        return { c, k };
    }

    void Board::setPiece(Square sq, Color c, PieceKind k) noexcept {
        assert(k != PieceKind::None);
        const Bitboard b = bit(sq);
        // remove whatever was there
        if (occupied_ & b) removePiece(sq);
        occupied_ |= b;
        (c == Color::Red ? red_ : yellow_) |= b;
        if (k == PieceKind::King) kings_ |= b;
        hash_ ^= Zobrist::instance().piece(c, k, sq);
    }

    void Board::removePiece(Square sq) noexcept {
        const Bitboard b = bit(sq);
        if (!(occupied_ & b)) return;
        const Color c = (red_ & b) ? Color::Red : Color::Yellow;
        const PieceKind k = (kings_ & b) ? PieceKind::King : PieceKind::Man;
        hash_ ^= Zobrist::instance().piece(c, k, sq);
        red_ &= ~b;
        yellow_ &= ~b;
        kings_ &= ~b;
        occupied_ &= ~b;
    }

    void Board::promote(Square sq) noexcept {
        const Bitboard b = bit(sq);
        if (!(occupied_ & b) || (kings_ & b)) return;
        const Color c = (red_ & b) ? Color::Red : Color::Yellow;
        hash_ ^= Zobrist::instance().piece(c, PieceKind::Man, sq);
        kings_ |= b;
        hash_ ^= Zobrist::instance().piece(c, PieceKind::King, sq);
    }

    void Board::recomputeHash() noexcept {
        hash_ = 0;
        Bitboard all = occupied_;
        while (all) {
            const Square sq = static_cast<Square>(std::countr_zero(all));
            all &= all - 1;
            const Color c = (red_ & bit(sq)) ? Color::Red : Color::Yellow;
            const PieceKind k = (kings_ & bit(sq)) ? PieceKind::King : PieceKind::Man;
            hash_ ^= Zobrist::instance().piece(c, k, sq);
        }
    }

    int Board::count(Color c, PieceKind k) const noexcept {
        if (k == PieceKind::Man)   return std::popcount(menMask(c));
        if (k == PieceKind::King)  return std::popcount(kingsMask(c));
        return std::popcount(colorMask(c));
    }

    std::string Board::debugString() const {
        std::ostringstream os;
        auto dump = [&](Color c) {
            os << (c == Color::Red ? "W" : "B") << ':';
            bool first = true;
            for (int sq = 0; sq < kNumPlayableSquares; ++sq) {
                if (hasPiece(c, static_cast<Square>(sq))) {
                    if (!first) os << ',';
                    os << (sq + 1);
                    if (isKing(static_cast<Square>(sq))) os << 'K';
                    first = false;
                }
            }
            };
        dump(Color::Red);
        os << ':';
        dump(Color::Yellow);
        os << '.';
        return os.str();
    }

} // namespace draughts::core
