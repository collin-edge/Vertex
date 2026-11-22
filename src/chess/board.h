#pragma once

#include "types.h"
#include <array>

namespace Pieces
{
    constexpr Piece EMPTY = 0;

    constexpr Piece makePiece(Color color, PieceType type)
    {
        return static_cast<Piece>(color) | static_cast<Piece>(type);
    }

    constexpr uint8_t COLOR_MASK = 0b1000;
    constexpr uint8_t PIECE_MASK = 0b0111;

    constexpr Color getColor(Piece piece)
    {
        return static_cast<Color>(piece & COLOR_MASK);
    }

    constexpr PieceType getType(Piece piece)
    {
        return static_cast<PieceType>(piece & PIECE_MASK);
    }

    constexpr int toBitboardIndex(Piece piece)
    {
        if (piece == EMPTY) return -1;

        int colorOffset = (piece & COLOR_MASK) ? 6 : 0;
        int pieceType = (piece & PIECE_MASK) - 1;

        return colorOffset + pieceType;
    }
}

struct Board
{
private:
    std::array<Piece, 64> mailbox{};
    std::array<Bitboard, 12> bitboards{};
    std::array<Bitboard, 2> occupied{};

public:
    void reset();

    void add(Piece piece, Square square);
    void remove(Square square);

    Piece get(Square square) const
    {
        return mailbox[square];
    }

    Bitboard getBitboard(Piece piece) const
    {
        int idx = Pieces::toBitboardIndex(piece);
        if (idx < 0) return 0;
        return bitboards[idx];
    }

    Bitboard getBitboard(Color color, PieceType type) const
    {
        return getBitboard(Pieces::makePiece(color, type));
    }

    Bitboard getOccupied(Color color) const
    {
        return occupied[static_cast<uint8_t>(color) / 8];
    }

    Bitboard getOccupied() const
    {
        return occupied[0] | occupied[1];
    }

    Bitboard getEmpty() const
    {
        return ~getOccupied();
    }
};