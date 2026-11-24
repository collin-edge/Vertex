#include "move_ordering.h"
#include <algorithm>

void MoveOrdering::orderMoves(MoveList& moves, const Position& position, Move ttMove, const int history [2][64][64], const Move* killerMoves)
{
    std::array<int, MoveList::MAX_MOVES> scores{};

    for (int i = 0; i < moves.count; ++i)
    {
        scores[i] = scoreMove(moves[i], position, ttMove, history, killerMoves);
    }

    for (int i = 0; i < moves.count - 1; ++i)
    {
        int best = i;

        for (int j = i + 1; j < moves.count; ++j)
        {
            if (scores[j] > scores[best]) best = j;
        }

        std::swap(moves[i], moves[best]);
        std::swap(scores[i], scores[best]);
    }
}

int MoveOrdering::scoreMove(Move move, const Position& position, Move ttMove, const int history [2][64][64], const Move* killerMoves)
{
    // these two take priority over all
    constexpr int TT_MOVE_SCORE = 1000000;
    constexpr int PROMOTION_SCORE = 900000;
    constexpr int KILLER_1_SCORE = 700000;
    constexpr int KILLER_2_SCORE = 600000;

    if (move == ttMove) return TT_MOVE_SCORE;

    auto flag = MovePacker::getFlag(move);

    // ok if it's not those then we do it as normal
    // using MVV-LVA, a pawn capturing a queen is better than a queen capturing a pawn
    // formula is victimScore * 10 - attackerScore

    Square from = MovePacker::getStart(move);
    Square to = MovePacker::getEnd(move);

    Piece attacker = position.getPiece(from);
    Piece victim = position.getPiece(to);

    int captureScore = 0;

    if (flag == MovePacker::Flag::EN_PASSANT)
    {
        int capturedSquare = to + (position.getSideToMove() == Color::WHITE ? -8 : 8);
        victim = position.getPiece(static_cast<Square>(capturedSquare));
    }

    if (victim != Pieces::EMPTY)
    {
        int attackerScore = pieceValue(Pieces::getType(attacker));
        int victimScore = pieceValue(Pieces::getType(victim));

        captureScore = victimScore * 10 - attackerScore;
    }

    if (MovePacker::isPromotion(flag)) return PROMOTION_SCORE + captureScore;

    if (killerMoves != nullptr)
    {
        if (move == killerMoves[0]) return KILLER_1_SCORE;
        if (move == killerMoves[1]) return KILLER_2_SCORE;
    }

    if (victim != Pieces::EMPTY) return captureScore;

    int col = (position.getSideToMove() == Color::WHITE) ? 0 : 1;
    return history[col][from][to];
}