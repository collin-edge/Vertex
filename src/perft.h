#pragma once

#include "chess/move_generation/movegen.h"
#include "chess/position.h"
#include <string>

namespace Perft
{
    uint64_t nodeCount(Position& position, int depth)
    {
        if (depth == 0)
            return 1;

        MoveList moves = MoveGenerator::generateLegalMoves(position);

        uint64_t nodes = 0;

        for (Move m : moves)
        {
            UndoInfo undo = position.makeMove(m);

            nodes += nodeCount(position, depth - 1);

            position.undoMove(undo);
        }

        return nodes;
    }
}