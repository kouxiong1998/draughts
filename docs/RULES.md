# Rules

This document specifies both rule variants supported by the engine. The
canonical implementation lives in `src/core/CaptureRules.cpp`,
`src/core/MoveGenerator.cpp`, and `src/core/GameEngine.cpp`. The description
below is a formal restatement of the code, not an independent source.

Two rule variants are selected via the `RuleSet` enum:

- `InternationalMaxCapture` - standard FMJD rules with majority capture.
- `InternationalFreeCapture` - custom variant where any capture chain of
  consistent direction is legal.

Sections 1-9 apply to BOTH variants. Section 10 documents the difference.
Section 11 lists terminal conditions.

## 1. Board

- 10 x 10 grid, 100 squares.
- Squares alternate black and white. Only the 50 BLACK squares are playable.
- Playable squares are those where `(row + col) is odd`, with row 0 at the
  top and col 0 at the left.
- The longest playable diagonal runs from the bottom-left corner to the
  top-right corner.
- Standard starting position: each player has 20 men occupying the four
  rows nearest their own side, all on playable squares.
  - Red men start in rows 0-3 (top).
  - Yellow men start in rows 6-9 (bottom).
  - Red always moves first (unless changed in Settings).

## 2. Men - movement

- A man moves one square diagonally FORWARD to an empty playable square.
- "Forward" for Red is toward increasing row (down). For Yellow it is toward
  decreasing row (up).
- Men cannot move sideways or backward.

## 3. Men - capture

- A man captures by jumping diagonally over an ADJACENT enemy piece and
  landing on the empty square directly beyond it.
- A man may capture FORWARD or BACKWARD (even though it cannot move backward
  quietly).
- After landing, if another capture is available with the SAME piece, it must
  continue capturing in the same turn.
- Captured pieces are removed only at the END of the full capture chain.
  During the chain, they remain as "ghosts" that block movement (international
  standard).

## 4. Kings

- A man that ENDS its move on the promotion row is promoted to a King.
  - Red promotion row: row 9 (bottom).
  - Yellow promotion row: row 0 (top).
- A man that passes THROUGH the promotion row during a capture chain and
  continues is NOT promoted.
- Kings move any number of empty squares diagonally in any direction
  ("flying kings").
- Kings capture by flying along a diagonal, jumping over exactly one enemy
  piece, and landing on any empty square beyond the victim (still on the
  same diagonal, before the next piece or board edge).

## 5. Kings - capture

- A king may capture an enemy piece anywhere along a diagonal, provided
  every square between the king and the victim is empty.
- After capturing, the king lands on any empty square directly beyond the
  victim on the same diagonal.
- If another capture is then available with the same king, it must continue.
- Captured pieces remain as ghosts during the chain, exactly as for men.

## 6. Chains (multi-jump)

- A capture chain is a sequence of jumps made by one piece in one turn.
- Every intermediate landing square must be empty on the current
  ghost-aware board (i.e. not occupied by a still-present piece).
- Already-captured pieces behave as ghosts: they block movement but cannot
  themselves be captured again.
- Chains end when no further capture is available for the moving piece.

## 7. Turn

- Players alternate turns. Red begins unless configured otherwise.
- If the side to move has any capture available, it must capture. Quiet
  moves are only legal when no capture exists for that side.

## 8. Promotion

- Promotion happens when a man ENDS its move on the promotion row.
- If the man is mid-chain and the chain terminates on the promotion row, it
  promotes at the end of the chain.
- Kings never promote.

## 9. Draws

Three independent draw conditions, all implemented in `GameEngine`:

1. **25-move rule**: 25 consecutive moves (50 plies) by both sides with
   no capture and no man move. Both sides must be moving kings only.
2. **Threefold repetition**: the same position (by Zobrist hash of pieces
   plus side to move plus rule variant) occurs three times.
3. **FMJD small-endgame limits**:
   - 3 kings vs 1 king: draw after 16 moves by the stronger side
     (32 plies total).
   - 2 kings vs 1 king: draw after 5 moves by the stronger side
     (10 plies total).

## 10. Rule variant: max-capture ON vs OFF

### 10.1 InternationalMaxCapture (standard FMJD)

- **Majority rule**: a player MUST play the capture sequence that captures
  the greatest number of enemy pieces.
- If two or more chains capture the same maximum number of pieces, the
  player may choose between them.
- There is no restriction on the direction of successive capture steps
  within a chain.
- Men and kings capture forward and backward freely, subject only to the
  geometry described in sections 3-5.

### 10.2 InternationalFreeCapture (custom variant)

- A player may choose ANY capture chain, even if a longer one exists.
- Once the initial capture direction is chosen, the chain MUST stay
  consistent with that direction in the following sense:
  - After each capture step, the piece may continue in the SAME direction,
    or turn 90 degrees to either adjacent diagonal.
  - A 180-degree reversal (directly back the way you came) is NOT allowed.
  - This is the "no-immediate-reversal" rule.
- Quiet move restrictions are identical to the standard variant.
- The opening position produces the same 9 legal moves under both variants,
  because no captures exist in the opening.

Note: the "no-immediate-reversal" rule is symmetric for men and kings
despite the difference in their movement ranges. It applies to the
direction of the JUMP, not the number of squares travelled.

## 11. Terminal conditions

- **Win**: the side to move has no legal moves (all pieces captured or
  all remaining pieces are blocked).
- **Win**: the opponent resigns (Give Up button).
- **Draw**: any of the three conditions in section 9.
- **Draw**: both players agree via the Draw button.

## 12. Notes on implementation

- `RuleSet` is a single enum used by both the engine and the AI. There is
  no re-implementation of rule logic in the AI layer.
- All move generation routes through `core::generateLegalMoves()`. The AI
  calls exactly the same function the UI calls.
- Captured-piece "ghosts" are tracked by the `Move::captured` bitboard
  during chain expansion; they are removed only when
  `GameEngine::applyMoveToBoard` runs at end of turn.
- Promotion is a flag on `Move` (`Move::isPromotion`) set by the generator
  when the landing square is the promotion row.
