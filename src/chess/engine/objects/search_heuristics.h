#pragma once

#include "../search_config.h"
#include "../../types.h"

class SearchHeuristics
{
private:
    int history[2][64][64] = {};

    Move killerMoves[SearchConfig::MAX_PLY][2] = {NO_MOVE, NO_MOVE};

public:
    void updateHistory(Color color, Square from, Square to, int bonus);
    int getHistory(Color color, Square from, Square to) const;

    void updateKillers(int ply, Move move);
    Move getKiller(int ply, int index) const;
    void clearKillers();

    void clear();
};