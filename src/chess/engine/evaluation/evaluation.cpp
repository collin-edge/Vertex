#include "evaluation.h"
#include "pst.h"

int material(const Position& position)
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
            case PieceType::PAWN: score += 100 * ((sideToMove == col) ? 1 : -1);
            case PieceType::KNIGHT: score += 320 * ((sideToMove == col) ? 1 : -1);
            case PieceType::BISHOP: score += 330 * ((sideToMove == col) ? 1 : -1);
            case PieceType::ROOK: score += 500 * ((sideToMove == col) ? 1 : -1);
            case PieceType::QUEEN: score += 900 * ((sideToMove == col) ? 1 : -1);
        }

        occupied &= occupied - 1;
    }

    return score;
}

int pst(const Position& position)
{
    Color sideToMove = position.getSideToMove();

    int score = 0;

    int endgameWeight = PST::gamePhase(position);

    Bitboard occupied = position.getOccupied(sideToMove);
    while (occupied)
    {
        unsigned long target;
        _BitScanForward64(&target, occupied);

        PieceType p = Pieces::getType(position.getPiece(target));

        score += PST::pstScore(target, p, position, endgameWeight);

        occupied &= occupied - 1;
    }

    return score;
}

int Evaluation::evaluate(const Position& position)
{
    int score = 0;

    score += material(position);
    score += pst(position);

    return score;
}