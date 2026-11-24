#pragma once

#include "../moves.h"
#include "../types.h"
#include "move_ordering.h"

struct SearchStack
{
private:
    std::array<int, MoveList::MAX_MOVES> moveScores{};
    int moveIndex = 0;

public:
    MoveList moves;
    Move killerMoves[2] = {NO_MOVE, NO_MOVE};

    void resetMovePicker()
    {
        moveIndex = 0;
    }

    void setMoveScore(int index, int score)
    {
        moveScores[index] = score;
    }

    Move nextMove();
};