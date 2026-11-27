#pragma once

#include <atomic>
#include <vector>

#include "search_config.h"
#include "evaluation/evaluation.h"
#include "../position.h"
#include "../moves.h"
#include "move_ordering.h"
#include "objects/transposition_tables.h"
#include "objects/search_stack.h"
#include "objects/search_heuristics.h"
#include "opening_book/opening_book.h"

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

    SearchStack stack[SearchConfig::MAX_PLY];
    SearchHeuristics heuristics;
    TranspositionTable transposition{64};
    QuiescenceTranspositionTable quiescence_tt{8};

    OpeningBook openingBook;

    int search(Position& position, int depth, int alpha, int beta, int ply = 0);
    int quiescence(Position& position, int alpha, int beta, int ply, int qply = 0);

public:
    Search();
    
    Move findBestMove(const Position& position, SearchLimits l);
    bool DEBUG = true;

    void stop();
};