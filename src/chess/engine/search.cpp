#include "search.h"
#include "../move_generation/movegen.h"
#include <iostream>
#include <utility>
#include <chrono>
#include <cstdlib>

constexpr int INF = 32000;
constexpr int MATE_SCORE = 10000;

constexpr int MAX_QUIESCENCE_DEPTH = 8;

int normalizeToTT(int score, int ply, int MAX_PLY)
{
    if (score > MATE_SCORE - MAX_PLY)
        score += ply;
    else if (score < -MATE_SCORE + MAX_PLY)
        score -= ply;

    return score;
}

int normalizeFromTT(int score, int ply, int MAX_PLY)
{
    if (score > MATE_SCORE - MAX_PLY)
        score -= ply;
    else if (score < -MATE_SCORE + MAX_PLY)
        score += ply;

    return score;
}

void updateHistory(int& historyScore, int bonus)
{
    constexpr int HISTORY_MAX = 500;

    historyScore += bonus - ((historyScore * std::abs(bonus)) / HISTORY_MAX);
}

int Search::search(Position& position, int depth, int alpha, int beta, int ply)
{
    if (stopSearch.load(std::memory_order_relaxed))
        return 0;

    ++nodes;

    int originalAlpha = alpha;
    uint64_t key = position.getZobristKey();
    TTEntry ttEntry;

    if (transposition.probe(key, ttEntry))
    {
        if (ttEntry.depth >= depth && ttEntry.searchType == TTSearchType::NORMAL)
        {
            int ttscore = normalizeFromTT(ttEntry.score, ply, MAX_PLY);
            
            if (ttEntry.bound == TTBound::EXACT) 
                return ttscore;
            if (ttEntry.bound == TTBound::LOWER_BOUND) 
                if (ttscore >= beta) return ttscore;
            if (ttEntry.bound == TTBound::UPPER_BOUND)
                if (ttscore <= alpha) return ttscore;
        }
    }

    Move ttMove = ttEntry.bestMove;

    if (ply >= MAX_PLY - 1)
        return Evaluation::evaluate(position);

    if (depth <= 0)
        return quiescence(position, alpha, beta, ply);
        
    SearchStack& ss = stack[ply];

    MoveList& moves = ss.moves;
    moves.count = 0;

    MoveGenInfo info = MoveGenerator::calculateInfo(position);
    MoveGenerator::generateLegalMoves(position, info, moves);

    int bestScore = -INF;
    Move bestMove = NO_MOVE;

    if (moves.count == 0)
    {
        if (info.checkerCount() > 0)
            return -MATE_SCORE + ply;

        return 0;
    }

    ss.resetMovePicker();
    for (int i = 0; i < ss.moves.count; ++i)
    {
        ss.setMoveScore(i, MoveOrdering::scoreMove(ss.moves[i], position, ttMove, history, ss.killerMoves));
    }

    Move quietMovesSearched[MoveList::MAX_MOVES];
    int quietMoveCount = 0;

    while (true)
    {
        Move m = ss.nextMove();

        if (m == NO_MOVE)
            break;

        UndoInfo undo = position.makeMove(m);

        int score = -search(position, depth - 1, -beta, -alpha, ply + 1);

        position.undoMove(undo);

        if (stopSearch.load(std::memory_order_relaxed))
            return 0;

        if (score > bestScore)
        {
            bestScore = score;
            bestMove = m;
        }

        if (score > alpha)
            alpha = score;

        Square end = MovePacker::getEnd(m);
        auto flag = MovePacker::getFlag(m);
        bool quiet = (position.getPiece(end) == Pieces::EMPTY && flag != MovePacker::Flag::EN_PASSANT && !MovePacker::isPromotion(flag));

        if (alpha >= beta)
        {
            if (quiet)
            {
                Square start = MovePacker::getStart(m);
                updateKillers(ply, m);

                int col = (position.getSideToMove() == Color::WHITE) ? 0 : 1;

                int bonus = depth * depth;
                updateHistory(history[col][start][end], bonus);

                for (int i = 0; i < quietMoveCount; i++)
                {
                    Move previous = quietMovesSearched[i];

                    int penalty = -bonus;
                    updateHistory(history[col][MovePacker::getStart(previous)][MovePacker::getEnd(previous)], penalty);
                }
            }

            break;
        } 
        else if (quiet)
        {
            quietMovesSearched[quietMoveCount] = m;
            ++quietMoveCount;
        }
    }

    // now store in tt

    TTBound bound = TTBound::EXACT;

    if (bestScore <= originalAlpha) bound = TTBound::UPPER_BOUND;
    else if (bestScore >= beta) bound = TTBound::LOWER_BOUND;

    transposition.store({
        key, 
        bestMove, 
        static_cast<int16_t>(normalizeToTT(bestScore, ply, MAX_PLY)), 
        static_cast<uint8_t>(depth), 
        bound, 
        TTSearchType::NORMAL, 
        true
    });

    return bestScore;
}

int Search::quiescence(Position& position, int alpha, int beta, int ply, int qply)
{
    if (stopSearch.load(std::memory_order_relaxed))
        return 0;

    ++nodes;

    if (ply >= MAX_PLY - 1)
        return Evaluation::evaluate(position); // i feel like there's no way in hell that'd ever happen, but just incase

    int originalAlpha = alpha;
    uint64_t key = position.getZobristKey();
    TTEntry ttEntry;

    int remainingDepth = std::max(0, MAX_QUIESCENCE_DEPTH - qply);

    if (transposition.probe(key, ttEntry))
    {
        
        if (ttEntry.depth >= remainingDepth && ttEntry.searchType == TTSearchType::QUIESCENCE)
        {
            int ttscore = normalizeFromTT(ttEntry.score, ply, MAX_PLY);
            
            if (ttEntry.bound == TTBound::EXACT) 
                return ttscore;
            if (ttEntry.bound == TTBound::LOWER_BOUND) 
                if (ttscore >= beta) return ttscore;
            if (ttEntry.bound == TTBound::UPPER_BOUND)
                if (ttscore <= alpha) return ttscore;
        }
    }

    Move ttMove = ttEntry.bestMove;

    MoveGenInfo info = MoveGenerator::calculateInfo(position);
    bool inCheck = (info.checkerCount() > 0);

    if (qply >= MAX_QUIESCENCE_DEPTH && !inCheck)
        return Evaluation::evaluate(position);

    int bestScore = -INF;

    if (!inCheck)
    {
        int standPat = Evaluation::evaluate(position);

        if (standPat >= beta)
        {
            transposition.store({
                key, 
                NO_MOVE, 
                static_cast<int16_t>(normalizeToTT(standPat, ply, MAX_PLY)), 
                static_cast<uint8_t>(remainingDepth), 
                TTBound::LOWER_BOUND, 
                TTSearchType::QUIESCENCE, 
                true
            });

            return standPat;
        }

        if (standPat > alpha)
            alpha = standPat;

        bestScore = standPat;
    }

    SearchStack& ss = stack[ply];
    MoveList& moves = ss.moves;
    moves.count = 0;

    if (inCheck)
        MoveGenerator::generateLegalMoves(position, info, moves);
    else
        MoveGenerator::generateTacticalMoves(position, info, moves);

    if (moves.count == 0)
    {
        if (inCheck)
            return -MATE_SCORE + ply;

        return bestScore;
    }

    ss.resetMovePicker();
    for (int i = 0; i < ss.moves.count; ++i)
    {
        ss.setMoveScore(i, MoveOrdering::scoreMove(ss.moves[i], position, ttMove, history, ss.killerMoves));
    }

    Move bestMove = NO_MOVE;

    while (true)
    {
        Move m = ss.nextMove();

        if (m == NO_MOVE)
            break;

        UndoInfo undo = position.makeMove(m);

        int score = -quiescence(
            position, -beta, -alpha, ply + 1, qply + 1
        );

        position.undoMove(undo);

        if (stopSearch.load(std::memory_order_relaxed))
            return 0;

        if (score > bestScore)
        {
            bestScore = score;
            bestMove = m;
        }

        if (score > alpha)
            alpha = score;

        if (alpha >= beta)
            break;
    }

    // now store in tt

    TTBound bound = TTBound::EXACT;

    if (bestScore <= originalAlpha) bound = TTBound::UPPER_BOUND;
    else if (bestScore >= beta) bound = TTBound::LOWER_BOUND;

    transposition.store({
        key, 
        bestMove, 
        static_cast<int16_t>(normalizeToTT(bestScore, ply, MAX_PLY)), 
        static_cast<uint8_t>(remainingDepth), 
        bound, 
        TTSearchType::QUIESCENCE, 
        true
    });

    return bestScore;
}

Move Search::findBestMove(const Position& position, SearchLimits l)
{
    stopSearch.store(false, std::memory_order_relaxed);

    Position copy = position;
    limits = l;

    nodes = 0;

    SearchStack& root = stack[0];
    root.moves.count = 0;

    MoveGenInfo info = MoveGenerator::calculateInfo(copy);
    MoveGenerator::generateLegalMoves(copy, info, root.moves);

    MoveList& moves = root.moves;

    if (moves.count == 0)
    {
        if (info.checkerCount() > 0)
            return -MATE_SCORE;

        return 0;
    }

    for (int ply = 0; ply < MAX_PLY; ply++)
    {
        stack[ply].killerMoves[0] = NO_MOVE;
        stack[ply].killerMoves[1] = NO_MOVE;
    }

    uint64_t key = position.getZobristKey();
    TTEntry ttEntry;

    transposition.probe(key, ttEntry);

    Move ttMove = ttEntry.bestMove;

    Move bestMove = moves[0];
    int maxDepth = limits.depth > 0 ? limits.depth : 64;

    for (int depth = 1; depth <= maxDepth; depth++)
    {
        auto iterationStart = std::chrono::steady_clock::now();

        if (stopSearch.load(std::memory_order_relaxed))
            break;

        root.resetMovePicker();

        for (int i = 0; i < moves.count; ++i)
        {
            int score = MoveOrdering::scoreMove(moves[i], copy, ttMove, history, root.killerMoves);

            if (moves[i] == bestMove)
                score = 950000;

            root.setMoveScore(i, score);
        }

        int bestScore = -INF;
        int alpha = -INF;

        Move iterationBestMove = bestMove;

        while (true)
        {
            Move m = root.nextMove();

            if (m == NO_MOVE)
                break;

            UndoInfo undo = copy.makeMove(m);

            int score = -search(copy, depth - 1, -INF, -alpha, 1);

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

        auto iterationEnd = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(iterationEnd - iterationStart).count();

        if (DEBUG)
        {
            std::cout
                << "info depth " << depth
                << " score cp " << bestScore
                << " nodes " << nodes
                << " time " << elapsed
                << std::endl;
        }
    }

    return bestMove;
}
void Search::stop()
{
    stopSearch.store(true, std::memory_order_relaxed);
}

void Search::updateKillers(int ply, Move move)
{
    SearchStack& ss = stack[ply];

    if (ss.killerMoves[0] == move)
        return;

    ss.killerMoves[1] = ss.killerMoves[0];
    ss.killerMoves[0] = move;
}