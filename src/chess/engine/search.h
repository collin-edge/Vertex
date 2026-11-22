#pragma once

#include <atomic>
#include <vector>

#include "evaluation.h"
#include "../position.h"
#include "../moves.h"

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

class Search
{
private:
    std::atomic<bool> stopSearch = false;
    SearchLimits limits;
    uint64_t nodes = 0;
    int currentDepth = 0;

    int search(Position& position, int depth, int alpha, int beta);

public:
    Move findBestMove(const Position& position, SearchLimits l);
    bool DEBUG = true;

    void stop();
};