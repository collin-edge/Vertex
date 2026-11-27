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

enum class MoveGenType
{
    ALL,
    TACTICAL,
    QUIETS
};

namespace MoveGenerator
{
    MoveGenInfo calculateInfo(const Position& position);

    void king(MoveList& moves, const Position& position, const MoveGenInfo& info, MoveGenType type);
    void pawn(MoveList& moves, const Position& position, const MoveGenInfo& info, MoveGenType type);
    void knight(MoveList& moves, const Position& position, const MoveGenInfo& info, MoveGenType type);
    void bishop(MoveList& moves, const Position& position, const MoveGenInfo& info, MoveGenType type);
    void rook(MoveList& moves, const Position& position, const MoveGenInfo& info, MoveGenType type);
    void queen(MoveList& moves, const Position& position, const MoveGenInfo& info, MoveGenType type);

    void generateLegalMoves(const Position& position, MoveGenInfo info, MoveList& moves, MoveGenType type);
    bool isLegal(const Position& position, MoveGenInfo info, Move move);
};