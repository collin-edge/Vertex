#pragma once

#include "../types.h"

struct Position;

namespace Attacks
{
    extern Bitboard BETWEEN[64][64];
    extern Bitboard BISHOP_RAYS[64][4];
    extern Bitboard ROOK_RAYS[64][4];

    void buildTables();

    Bitboard getPawnAttacks(Color color, Square square);
    Bitboard getKnightAttacks(Square square);
    Bitboard getKingAttacks(Square square);
    Bitboard getRookAttacks(Square square, Bitboard occupied); // basically left/right up/down attacks
    Bitboard getBishopAttacks(Square square, Bitboard occupied); // diagonals

    Bitboard getAttackers(const Position& position, Square square, Color attacker, Bitboard occupied);
    bool isSquareAttacked(const Position& position, Square square, Color attacker, Bitboard occupied);
}