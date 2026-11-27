#pragma once

#include "../../types.h"
#include "../../position.h"

struct MoveGenInfo;

namespace Weights
{
    // basic piece values
    constexpr int PAWN_VALUE = 100;
    constexpr int KNIGHT_VALUE = 320;
    constexpr int BISHOP_VALUE = 330;
    constexpr int ROOK_VALUE = 500;
    constexpr int QUEEN_VALUE = 900;

    // bishop pairs
    constexpr int BISHOP_PAIR_MG = 35;
    constexpr int BISHOP_PAIR_EG = 50;

    // pawn structure
    constexpr int DOUBLED_PAWN_PENALTY = 15;
    constexpr int ISOLATED_PAWN_PENALTY = 20;
    constexpr int BACKWARD_PAWN_PENALTY = 15;
    constexpr int PASSED_PAWN_BONUS[8] =
    {
        0, 5, 10, 20, 35, 60, 100, 0
    };

    // mobility
    constexpr int KNIGHT_MOBILITY = 4;
    constexpr int BISHOP_MOBILITY = 4;
    constexpr int ROOK_MOBILITY = 2;
    constexpr int QUEEN_MOBILITY = 1;

    // king safety
    constexpr int KING_ATTACKER_WEIGHT = 10;
    constexpr int OPEN_FILE_NEAR_KING_PENALTY = 20;
    constexpr int WEAK_PAWN_SHIELD_PENALTY = 30;
}

namespace Evaluation
{
    int evaluate(const Position& position);
    int evaluate(const Position& position, const MoveGenInfo& info);
}