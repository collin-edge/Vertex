#include "search_stack.h"

Move SearchStack::nextScoredMove(MoveList& list)
{
    while (moveIndex < list.count)
    {
        int bestIndex = moveIndex;

        for (int i = moveIndex + 1; i < list.count; ++i)
        {
            if (moveScores[i] > moveScores[bestIndex])
                bestIndex = i;
        }

        std::swap(list[moveIndex], list[bestIndex]);
        std::swap(moveScores[moveIndex], moveScores[bestIndex]);

        Move move = list[moveIndex++];

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

                if (ttMove != NO_MOVE && MoveGenerator::isLegal(position, info, ttMove))
                {
                    addSearchedMove(ttMove);
                    return ttMove;
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

                    goodTacticals.count = 0;
                    badTacticals.count = 0;

                    for (Move m : moves)
                    {
                        if (MovePacker::isPromotion(MovePacker::getFlag(m)))
                        {
                            goodTacticals.append(m);
                            continue;
                        }

                        if (MoveOrdering::see(m, position) >= 0)
                            goodTacticals.append(m);
                        else
                            badTacticals.append(m);
                    }

                    for (int i = 0; i < goodTacticals.count; ++i)
                    {
                        moveScores[i] = MoveOrdering::scoreTactical(goodTacticals[i], position);
                    }

                    stageInitialized = true;
                }

                Move move = nextScoredMove(goodTacticals);

                if (move != NO_MOVE)
                    return move;

                stage = MoveStage::KILLERS;
                stageInitialized = false;

                break;
            }
            case MoveStage::KILLERS:
            {
                while (killerIndex < 2)
                {
                    Move killer = heuristics.getKiller(ply, killerIndex++);

                    if (killer == NO_MOVE || alreadySearched(killer) || !MoveGenerator::isLegal(position, info, killer)) 
                        continue;

                    Square end = MovePacker::getEnd(killer);

                    if (MovePacker::isPromotion(MovePacker::getFlag(killer))) continue;
                    if (position.getPiece(end) != Pieces::EMPTY) continue;

                    addSearchedMove(killer);
                    return killer;
                }

                stage = MoveStage::QUIETS;
                stageInitialized = false;
                moveIndex = 0;

                break;
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

                Move move = nextScoredMove(moves);

                if (move != NO_MOVE)
                    return move;

                stage = MoveStage::BAD_TACTICALS;
                stageInitialized = false;
                moveIndex = 0;

                break;
            }
            case MoveStage::BAD_TACTICALS:
            {
                if (!stageInitialized)
                {
                    moveIndex = 0;

                    for (int i = 0; i < badTacticals.count; ++i)
                    {
                        moveScores[i] = MoveOrdering::scoreTactical(badTacticals[i], position);
                    }

                    stageInitialized = true;
                }

                Move move = nextScoredMove(badTacticals);

                if (move != NO_MOVE)
                    return move;

                stage = MoveStage::DONE;
                break;
            }
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

    return nextScoredMove(moves);
}