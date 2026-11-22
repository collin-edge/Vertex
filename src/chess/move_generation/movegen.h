#pragma once

#include "../types.h"
#include "../moves.h"
#include <intrin.h>

struct Position;

struct MoveGenInfo
{
    Square kingSquare;

    Bitboard checkers = 0;
    Bitboard evasionMask = ~0ULL;

    Bitboard pinMasks[64];

    Bitboard kingAttackers = 0;

    int checkerCount() const
    {
        return __popcnt64(checkers);
    }
};

namespace MoveGenerator
{
    MoveGenInfo calculateInfo(const Position& position);

    void king(MoveList& moves, const Position& position, const MoveGenInfo& info);
    void pawn(MoveList& moves, const Position& position, const MoveGenInfo& info);
    void knight(MoveList& moves, const Position& position, const MoveGenInfo& info);
    void bishop(MoveList& moves, const Position& position, const MoveGenInfo& info);
    void rook(MoveList& moves, const Position& position, const MoveGenInfo& info);
    void queen(MoveList& moves, const Position& position, const MoveGenInfo& info);

    MoveList generateLegalMoves(const Position& position);
};