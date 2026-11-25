#include "search_heuristics.h"
#include "../../types.h"
#include <cmath>

void SearchHeuristics::updateHistory(Color color, Square from, Square to, int bonus)
{
    constexpr int HISTORY_MAX = 500;

    int col = (color == Color::WHITE) ? 0 : 1;
    history[col][from][to] += bonus - ((history[col][from][to] * std::abs(bonus)) / HISTORY_MAX);
}

int SearchHeuristics::getHistory(Color color, Square from, Square to) const
{
    if (!(color == Color::WHITE || color == Color::BLACK) || from < 0 || from > 63 || to < 0 || to > 63) return 0;

    int col = (color == Color::WHITE) ? 0 : 1;
    return history[col][from][to];
}

void SearchHeuristics::updateKillers(int ply, Move move)
{
    if (killerMoves[ply][0] == move)
        return;

    killerMoves[ply][1] = killerMoves[ply][0];
    killerMoves[ply][0] = move;
}

Move SearchHeuristics::getKiller(int ply, int index) const
{
    if (ply < 0 || ply >= SearchConfig::MAX_PLY || index < 0 || index > 1) return NO_MOVE;

    return killerMoves[ply][index];
}

void SearchHeuristics::clearKillers()
{
    for (int ply = 0; ply < SearchConfig::MAX_PLY; ++ply)
    {
        killerMoves[ply][0] = NO_MOVE;
        killerMoves[ply][1] = NO_MOVE;
    }
}

void SearchHeuristics::clear()
{
    clearKillers();
    
    for (int c = 0; c < 2; ++c)
    {
        for (int f = 0; f < 64; ++f)
        {
            for (int t = 0; t < 64; ++t)
            {
                history[c][f][t] = 0;
            }
        }
    }
}