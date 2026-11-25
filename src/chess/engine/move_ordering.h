#pragma once

#include "../types.h"
#include "../moves.h"
#include "../position.h"

// Alpha-Beta pruning is a lot stronger whenever moves that are likely to be really good are the first ones searched, and more irrelevant moves are last
namespace MoveOrdering
{
    void orderMoves(MoveList& moves, const Position& position, Move ttMove, const int history [2][64][64], const Move* killerMoves = nullptr);
    int scoreMove(Move move, const Position& position, Move ttMove, const int history [2][64][64], const Move* killerMoves = nullptr);

    int scoreTactical(Move move, const Position& position);

    constexpr int pieceValue(PieceType piece)
    {
        switch (piece)
        {
            case PieceType::PAWN: return 100;
            case PieceType::KNIGHT: return 320;
            case PieceType::BISHOP: return 330;
            case PieceType::ROOK: return 500;
            case PieceType::QUEEN: return 900;
            case PieceType::KING: return 20000;
            default: return 0;
        }
    }
}