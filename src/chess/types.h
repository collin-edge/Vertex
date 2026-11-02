#pragma once

#include <cstdint>

// uh just a simple class to list the bazillion of types

using Bitboard = uint64_t;
using Square = uint8_t;
using Piece = uint8_t;
using Move = uint16_t;

Square NO_SQUARE = 64; // since squares range from 0-63, 64 can be the integer for no square.

enum class Color : uint8_t
{
    WHITE = 0,
    BLACK = 8
};

enum class PieceType : uint8_t
{
    KING = 1,
    PAWN = 2,
    KNIGHT = 3,
    BISHOP = 4,
    ROOK = 5,
    QUEEN = 6
};

constexpr uint8_t WHITE_KINGSIDE = 1 << 0;
constexpr uint8_t WHITE_QUEENSIDE = 1 << 1;
constexpr uint8_t BLACK_KINGSIDE = 1 << 2;
constexpr uint8_t BLACK_QUEENSIDE = 1 << 3;