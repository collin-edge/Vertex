#include "search_stack.h"
#include <algorithm>

Move SearchStack::nextMove()
{
    if (moveIndex >= moves.count)
        return NO_MOVE;

    int bestIndex = moveIndex;

    for (int i = moveIndex + 1; i < moves.count; ++i)
    {
        if (moveScores[i] > moveScores[bestIndex])
            bestIndex = i;
    }

    std::swap(moves[moveIndex], moves[bestIndex]);
    std::swap(moveScores[moveIndex], moveScores[bestIndex]);

    return moves[moveIndex++];
}