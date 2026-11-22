#include "search.h"
#include "../move_generation/movegen.h"
#include <iostream>

constexpr int INF = 32000;

int Search::search(Position& position, int depth, int alpha, int beta)
{
    if (stopSearch.load(std::memory_order_relaxed))
        return 0;

    ++nodes;

    if (depth == 0)
        return Evaluation::evaluate(position);

    MoveList moves = MoveGenerator::generateLegalMoves(position);

    int bestScore = -INF;

    for (Move m : moves)
    {
        UndoInfo undo = position.makeMove(m);

        int score = -search(position, depth - 1, -beta, -alpha);

        position.undoMove(undo);

        if (stopSearch.load(std::memory_order_relaxed))
            return 0;

        if (score > bestScore)
            bestScore = score;

        if (score > alpha)
            alpha = score;

        if (alpha >= beta)
            break;
    }

    return bestScore;
}

Move Search::findBestMove(const Position& position, SearchLimits l)
{
    stopSearch.store(false, std::memory_order_relaxed);

    Position copy = position;
    limits = l;

    nodes = 0;

    MoveList moves = MoveGenerator::generateLegalMoves(copy);

    if (moves.count == 0)
        return -1;

    Move bestMove = moves[0];
    int maxDepth = limits.depth > 0 ? limits.depth : 64;

    for (int depth = 1; depth <= maxDepth; depth++)
    {
        if (stopSearch.load(std::memory_order_relaxed))
            break;

        int bestScore = -INF;
        int alpha = -INF;

        Move iterationBestMove = bestMove;

        for (Move m : moves)
        {
            UndoInfo undo = copy.makeMove(m);

            int score = -search(copy, depth - 1, -INF, -alpha);

            copy.undoMove(undo);

            if (stopSearch.load(std::memory_order_relaxed))
                break;

            if (score > bestScore)
            {
                bestScore = score;
                iterationBestMove = m;
            }

            if (score > alpha)
                alpha = score;
        }

        if (stopSearch.load(std::memory_order_relaxed))
            break;

        bestMove = iterationBestMove;

        if (DEBUG)
        {
            std::cout
                << "info depth " << depth
                << " score cp " << bestScore
                << " nodes " << nodes
                << std::endl;
        }
    }

    return bestMove;
}

void Search::stop()
{
    stopSearch.store(true, std::memory_order_relaxed);
}