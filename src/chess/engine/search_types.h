#pragma once

#include "../types.h"
#include <vector>

namespace SearchConfig
{
    constexpr int MAX_PLY = 64;
}

struct SearchSettings
{
    int threads = 1;
    std::size_t hashSize = 64;
    std::size_t qHashSize = 8;
};

struct SearchLimits
{
    int wtime = -1;
    int btime = -1;

    int winc = 0;
    int binc = 0;

    int movesToGo = 0;

    int depth = -1;
    int mate = 0;

    uint64_t nodes = 0;

    int moveTime = -1;

    bool infinite = false;
    bool ponder = false;

    std::vector<Move> searchMoves;
};

struct SearchResult
{
    Move bestMove = NO_MOVE;
    int score = 0;
    int depth = 0;
    uint64_t nodes = 0;
};