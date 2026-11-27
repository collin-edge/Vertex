#pragma once

#include "../../moves.h"
#include "../../types.h"
#include "../move_ordering.h"
#include "../../move_generation/movegen.h"
#include "search_heuristics.h"

enum class MoveStage
{
    TT_MOVE,
    GOOD_TACTICALS,
    KILLERS,
    QUIETS,
    BAD_TACTICALS,
    DONE
};

struct SearchStack
{
private:
    std::array<int, MoveList::MAX_MOVES> moveScores{};
    int moveIndex = 0;
    int killerIndex = 0;
    MoveStage stage = MoveStage::TT_MOVE;

    MoveList moves;
    bool stageInitialized = false;

    bool quiescenceInitialized = false;
    int quiescenceMoveIndex = 0;

    MoveList goodTacticals;
    MoveList badTacticals;

    Move nextScoredMove(MoveList& list);

    // a list of the special moves like Transposition, Killer heuristic moves, that when we search other moves, we don't repeat it
    std::array<Move, 3> searchedMoves{};
    int searchedMoveCount = 0;

    bool alreadySearched(Move move) const;
    void addSearchedMove(Move move);

public:
    void resetMovePicker();

    Move nextMove(const Position& position, const MoveGenInfo& info, Move ttMove, const SearchHeuristics& heuristics, int ply);
    Move quiescenceNextMove(const Position& position, const MoveGenInfo& info, bool inCheck);

    MoveStage getStage() const
    {
        return stage;
    }
};