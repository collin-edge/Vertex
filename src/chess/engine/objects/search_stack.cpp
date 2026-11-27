#include "search_stack.h"

Move SearchStack::nextScoredMove()
{
    while (moveIndex < moves.count)
    {
        int bestIndex = moveIndex;

        for (int i = moveIndex + 1; i < moves.count; ++i)
        {
            if (moveScores[i] > moveScores[bestIndex])
                bestIndex = i;
        }

        std::swap(moves[moveIndex], moves[bestIndex]);
        std::swap(moveScores[moveIndex], moveScores[bestIndex]);

        Move move = moves[moveIndex++];

        if (!alreadySearched(move))
        {
            addSearchedMove(move);
            return move;
        }
    }

    return NO_MOVE;
}

void SearchStack::resetMovePicker()
{
    moveIndex = 0;
    killerIndex = 0;
    searchedMoveCount = 0;

    stage = MoveStage::TT_MOVE;
    stageInitialized = false;

    moves.count = 0;

    quiescenceInitialized = false;
    quiescenceMoveIndex = 0;
}

bool SearchStack::alreadySearched(Move move) const
{
    for (int i = 0; i < searchedMoveCount; ++i)
    {
        if (searchedMoves[i] == move)
            return true;
    }

    return false;
}

void SearchStack::addSearchedMove(Move move)
{
    if (move == NO_MOVE || alreadySearched(move))
        return;

    if (searchedMoveCount < searchedMoves.size())
        searchedMoves[searchedMoveCount++] = move;
}

Move SearchStack::nextMove(const Position& position, const MoveGenInfo& info, Move ttMove, const SearchHeuristics& heuristics, int ply)
{
    while (stage != MoveStage::DONE)
    {
        switch (stage)
        {
            case MoveStage::TT_MOVE:
            {
                stage = MoveStage::GOOD_TACTICALS;

                if (ttMove != NO_MOVE)
                {
                    MoveList legalMoves;
                    legalMoves.count = 0;

                    MoveGenerator::generateLegalMoves(position, info, legalMoves, MoveGenType::ALL);

                    bool legal = false;

                    for (int i = 0; i < legalMoves.count; ++i)
                    {
                        if (legalMoves[i] == ttMove)
                        {
                            legal = true;
                            break;
                        }
                    }

                    if (legal)
                    {
                        addSearchedMove(ttMove);
                        return ttMove;
                    }
                }

                break;
            }
            case MoveStage::GOOD_TACTICALS:
            {
                if (!stageInitialized)
                {
                    moves.count = 0;
                    moveIndex = 0;

                    MoveGenerator::generateLegalMoves(position, info, moves, MoveGenType::TACTICAL);

                    for (int i = 0; i < moves.count; ++i)
                    {
                        moveScores[i] = MoveOrdering::scoreTactical(moves[i], position);
                    }

                    stageInitialized = true;
                }

                Move move = nextScoredMove();

                if (move != NO_MOVE)
                    return move;

                stage = MoveStage::KILLERS;
                stageInitialized = false;

                break;
            }
            case MoveStage::KILLERS:
            {
                if (!stageInitialized)
                {
                    moves.count = 0;
                    moveIndex = 0;

                    MoveGenerator::generateLegalMoves(position, info, moves, MoveGenType::QUIETS);

                    for (int i = 0; i < moves.count; ++i)
                    {
                        moveScores[i] = heuristics.getHistory(position.getSideToMove(), MovePacker::getStart(moves[i]), MovePacker::getEnd(moves[i]));
                    }

                    stageInitialized = true;
                }

                while (killerIndex < 2)
                {
                    Move killer = heuristics.getKiller(ply, killerIndex++);

                    bool legal = false;

                    for (int i = 0; i < moves.count; ++i)
                    {
                        if (moves[i] == killer)
                        {
                            legal = true;
                            break;
                        }
                    }

                    if (legal && !alreadySearched(killer))
                    {
                        addSearchedMove(killer);
                        return killer;
                    }
                }

                stage = MoveStage::QUIETS;
                moveIndex = 0;
                continue;
            }
            case MoveStage::QUIETS:
            {
                if (!stageInitialized)
                {
                    moves.count = 0;
                    moveIndex = 0;

                    MoveGenerator::generateLegalMoves(position, info, moves, MoveGenType::QUIETS);

                    for (int i = 0; i < moves.count; ++i)
                    {
                        moveScores[i] = heuristics.getHistory(position.getSideToMove(), MovePacker::getStart(moves[i]), MovePacker::getEnd(moves[i]));
                    }

                    stageInitialized = true;
                }

                Move move = nextScoredMove();

                if (move != NO_MOVE)
                    return move;

                stage = MoveStage::BAD_TACTICALS;
                stageInitialized = false;

                break;
            }
            case MoveStage::BAD_TACTICALS:
                stage = MoveStage::DONE;
                break;
            case MoveStage::DONE:
                return NO_MOVE;
        }
    }

    return NO_MOVE;
}

Move SearchStack::quiescenceNextMove(const Position& position, const MoveGenInfo& info, bool inCheck)
{
    if (!quiescenceInitialized)
    {
        moves.count = 0;
        moveIndex = 0;

        if (inCheck)
        {
            MoveGenerator::generateLegalMoves(position, info, moves, MoveGenType::ALL);
        }
        else
        {
            MoveGenerator::generateLegalMoves(position, info, moves, MoveGenType::TACTICAL);
        }

        for (int i = 0; i < moves.count; ++i)
        {
            moveScores[i] = MoveOrdering::scoreTactical(moves[i], position);
        }

        quiescenceInitialized = true;
    }

    return nextScoredMove();
}