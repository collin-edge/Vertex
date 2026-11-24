#pragma once

#include "../../types.h"
#include "../../position.h"

// uh thanks chessprogramming.org

namespace PST
{
    constexpr int king_MIDDLEGAME[64] = {
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -20,-30,-30,-40,-40,-30,-30,-20,
        -10,-20,-20,-20,-20,-20,-20,-10,
        20, 20,  0,  0,  0,  0, 20, 20,
        20, 30, 10,  0,  0, 10, 30, 20
    };

    constexpr int king_ENDGAME[64] = {
        -50,-40,-30,-20,-20,-30,-40,-50,
        -30,-20,-10,  0,  0,-10,-20,-30,
        -30,-10, 20, 30, 30, 20,-10,-30,
        -30,-10, 30, 40, 40, 30,-10,-30,
        -30,-10, 30, 40, 40, 30,-10,-30,
        -30,-10, 20, 30, 30, 20,-10,-30,
        -30,-30,  0,  0,  0,  0,-30,-30,
        -50,-30,-30,-30,-30,-30,-30,-50
    };

    constexpr int pawn[64] = {
        0,  0,  0,  0,  0,  0,  0,  0,
        50, 50, 50, 50, 50, 50, 50, 50,
        10, 10, 20, 30, 30, 20, 10, 10,
        5,  5, 10, 25, 25, 10,  5,  5,
        0,  0,  0, 20, 20,  0,  0,  0,
        5, -5,-10,  0,  0,-10, -5,  5,
        5, 10, 10,-20,-20, 10, 10,  5,
        0,  0,  0,  0,  0,  0,  0,  0
    };

    constexpr int knight[64] = {
        -50,-40,-30,-30,-30,-30,-40,-50,
        -40,-20,  0,  0,  0,  0,-20,-40,
        -30,  0, 10, 15, 15, 10,  0,-30,
        -30,  5, 15, 20, 20, 15,  5,-30,
        -30,  0, 15, 20, 20, 15,  0,-30,
        -30,  5, 10, 15, 15, 10,  5,-30,
        -40,-20,  0,  5,  5,  0,-20,-40,
        -50,-40,-30,-30,-30,-30,-40,-50,
    };

    constexpr int bishop[64] = {
        -20,-10,-10,-10,-10,-10,-10,-20,
        -10,  0,  0,  0,  0,  0,  0,-10,
        -10,  0,  5, 10, 10,  5,  0,-10,
        -10,  5,  5, 10, 10,  5,  5,-10,
        -10,  0, 10, 10, 10, 10,  0,-10,
        -10, 10, 10, 10, 10, 10, 10,-10,
        -10,  5,  0,  0,  0,  0,  5,-10,
        -20,-10,-10,-10,-10,-10,-10,-20,
    };

    constexpr int rook[64] = {
        0,  0,  0,  0,  0,  0,  0,  0,
        5, 10, 10, 10, 10, 10, 10,  5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        0,  0,  0,  5,  5,  0,  0,  0
    };

    constexpr int queen[64] = {
        -20,-10,-10, -5, -5,-10,-10,-20,
        -10,  0,  0,  0,  0,  0,  0,-10,
        -10,  0,  5,  5,  5,  5,  0,-10,
        -5,  0,  5,  5,  5,  5,  0, -5,
        0,  0,  5,  5,  5,  5,  0, -5,
        -10,  5,  5,  5,  5,  5,  0,-10,
        -10,  0,  5,  0,  0,  0,  0,-10,
        -20,-10,-10, -5, -5,-10,-10,-20
    };

    // we use the amount of material left on the board to interpolate between the values of king middlegame and king endgame
    int gamePhase(const Position& position)
    {
        int phase = 0;

        phase += __popcnt64(position.getBitboard(Pieces::makePiece(Color::WHITE, PieceType::KNIGHT)));
        phase += __popcnt64(position.getBitboard(Pieces::makePiece(Color::BLACK, PieceType::KNIGHT)));

        phase += __popcnt64(position.getBitboard(Pieces::makePiece(Color::WHITE, PieceType::BISHOP)));
        phase += __popcnt64(position.getBitboard(Pieces::makePiece(Color::BLACK, PieceType::BISHOP)));

        phase += 2 * __popcnt64(position.getBitboard(Pieces::makePiece(Color::WHITE, PieceType::ROOK)));
        phase += 2 * __popcnt64(position.getBitboard(Pieces::makePiece(Color::BLACK, PieceType::ROOK)));

        phase += 4 * __popcnt64(position.getBitboard(Pieces::makePiece(Color::WHITE, PieceType::QUEEN)));
        phase += 4 * __popcnt64(position.getBitboard(Pieces::makePiece(Color::BLACK, PieceType::QUEEN)));

        return std::min(phase, 24);
    }

    int endgameWeight(int phase)
    {
        if (phase >= 16)
            return 0;

        if (phase <= 0)
            return 256;

        int t = (16 - phase) * 256 / 16;

        return (t * t * (768 - 2 * t)) / (256 * 256);
    }

    int pstScore(Square square, PieceType piece, const Position& position, int endgameWeight)
    {
        // mirror the tables for black
        if (position.getSideToMove() == Color::BLACK)
            square ^= 56;

        switch (piece)
        {
            case PieceType::KING:
            {
                // do special stuff

                int mg = PST::king_MIDDLEGAME[square];
                int eg = PST::king_ENDGAME[square];

                return (mg * (256 - endgameWeight) + eg * endgameWeight) / 256;
            }
            case PieceType::PAWN: return pawn[square];
            case PieceType::KNIGHT: return knight[square];
            case PieceType::BISHOP: return bishop[square];
            case PieceType::ROOK: return rook[square];
            case PieceType::QUEEN: return queen[square];
            default: return 0;
        }
    }
}