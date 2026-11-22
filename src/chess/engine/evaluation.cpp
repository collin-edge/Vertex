#include "evaluation.h"

int materialEvaluation(const Position& position)
{
    Color sideToMove = position.getSideToMove();

    int score = 0;

    Bitboard occupied = position.getOccupied();
    while (occupied)
    {
        unsigned long target;
        _BitScanForward64(&target, occupied);

        Piece p = position.getPiece(target);
        Color col = Pieces::getColor(p);

        switch (Pieces::getType(p))
        {
            case PieceType::PAWN: score += 100 * (sideToMove == col) ? 1 : -1; break;
            case PieceType::KNIGHT: score += 200 * (sideToMove == col) ? 1 : -1; break;
            case PieceType::BISHOP: score += 300 * (sideToMove == col) ? 1 : -1; break;
            case PieceType::ROOK: score += 500 * (sideToMove == col) ? 1 : -1; break;
            case PieceType::QUEEN: score += 900 * (sideToMove == col) ? 1 : -1; break;
        }

        occupied &= occupied - 1;
    }

    return score;
}

int Evaluation::evaluate(const Position& position)
{
    int score = 0;

    score += materialEvaluation(position);

    return score;
}