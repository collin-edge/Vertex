#include "search.h"
#include "../move_generation/movegen.h"
#include <iostream>
#include <utility>
#include <chrono>

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

int Search::search(Position& position, int depth, int alpha, int beta, int ply)
{
    if (stopSearch.load(std::memory_order_relaxed))
        return 0;

    ++nodes;

    int originalAlpha = alpha;
    uint64_t key = position.getZobristKey();
    TTEntry ttEntry;

    Move ttMove = NO_MOVE;

    if (transposition.probe(key, ttEntry))
    {
        ttMove = ttEntry.bestMove;
        if (ttEntry.depth >= depth)
        {
            int ttscore = normalizeFromTT(ttEntry.score, ply, SearchConfig::MAX_PLY);
            
            if (ttEntry.bound == TTBound::EXACT) 
                return ttscore;
            if (ttEntry.bound == TTBound::LOWER_BOUND) 
                if (ttscore >= beta) return ttscore;
            if (ttEntry.bound == TTBound::UPPER_BOUND)
                if (ttscore <= alpha) return ttscore;
        }
    }

    if (ply >= SearchConfig::MAX_PLY - 1)
        return Evaluation::evaluate(position);

    if (depth <= 0)
        return quiescence(position, alpha, beta, ply);
        
    SearchStack& ss = stack[ply];

    MoveGenInfo info = MoveGenerator::calculateInfo(position);

    ss.resetMovePicker();

    int bestScore = -INF;
    Move bestMove = NO_MOVE;
    bool foundMove = false;

    Move quietMovesSearched[MoveList::MAX_MOVES];
    int quietMoveCount = 0;

    while (true)
    {
        Move m = ss.nextMove(position, info, ttMove, heuristics, ply);

        if (m == NO_MOVE)
            break;

        foundMove = true;

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
        Square start = MovePacker::getStart(m);
        auto flag = MovePacker::getFlag(m);

        Piece captured = position.getPiece(end);

        bool quiet = captured == Pieces::EMPTY && flag != MovePacker::Flag::EN_PASSANT && !MovePacker::isPromotion(flag);

        if (alpha >= beta)
        {
            if (quiet)
            {
                heuristics.updateKillers(ply, m);

                int bonus = depth * depth;

                heuristics.updateHistory(
                    position.getSideToMove(),
                    start,
                    end,
                    bonus
                );

                for (int i = 0; i < quietMoveCount; ++i)
                {
                    Move previous = quietMovesSearched[i];

                    heuristics.updateHistory(
                        position.getSideToMove(),
                        MovePacker::getStart(previous),
                        MovePacker::getEnd(previous),
                        -bonus
                    );
                }
            }

            break;
        }
        else if (quiet)
        {
            quietMovesSearched[quietMoveCount++] = m;
        }
    }

    if (!foundMove)
    {
        if (info.checkerCount() > 0)
            return -MATE_SCORE + ply;

        return 0;
    }

    // now store in tt

    TTBound bound = TTBound::EXACT;

    if (bestScore <= originalAlpha) bound = TTBound::UPPER_BOUND;
    else if (bestScore >= beta) bound = TTBound::LOWER_BOUND;

    transposition.store({
        key,
        bestMove,
        static_cast<int16_t>(normalizeToTT(bestScore, ply, SearchConfig::MAX_PLY)),
        static_cast<uint8_t>(depth),
        bound
    });

    return bestScore;
}

int Search::quiescence(Position& position, int alpha, int beta, int ply, int qply)
{
    if (stopSearch.load(std::memory_order_relaxed))
        return 0;

    ++nodes;

    if (ply >= SearchConfig::MAX_PLY - 1)
        return Evaluation::evaluate(position);

    int originalAlpha = alpha;

    uint64_t key = position.getZobristKey();

    MoveGenInfo info = MoveGenerator::calculateInfo(position);
    bool inCheck = (info.checkerCount() > 0);

    int remainingDepth = std::max(0, MAX_QUIESCENCE_DEPTH - qply);

    QTTMode mode = inCheck ? QTTMode::IN_CHECK : QTTMode::NORMAL;

    bool cacheable = qply < MAX_QUIESCENCE_DEPTH;

    QTTEntry ttEntry;

    if (cacheable && quiescence_tt.probe(key, mode, ttEntry))
    {
        if (ttEntry.depth >= remainingDepth)
        {
            int ttScore = normalizeFromTT(ttEntry.score, ply, SearchConfig::MAX_PLY);

            if (ttEntry.bound == TTBound::EXACT)
                return ttScore;
            if (ttEntry.bound == TTBound::LOWER_BOUND && ttScore >= beta)
                return ttScore;
            if (ttEntry.bound == TTBound::UPPER_BOUND && ttScore <= alpha)
                return ttScore;
        }
    }

    int bestScore = -INF;

    if (!inCheck)
    {
        int standPat = Evaluation::evaluate(position);

        if (standPat >= beta)
        {
            if (cacheable)
            {
                quiescence_tt.store({
                    key,
                    static_cast<int16_t>(normalizeToTT(standPat, ply, SearchConfig::MAX_PLY)),
                    static_cast<uint8_t>(remainingDepth),
                    TTBound::LOWER_BOUND,
                    mode
                });
            }

            return standPat;
        }

        if (standPat > alpha)
            alpha = standPat;

        bestScore = standPat;

        if (qply >= MAX_QUIESCENCE_DEPTH)
            return bestScore;
    }

    SearchStack& ss = stack[ply];
    ss.resetMovePicker();

    bool foundMove = false;

    while (true)
    {
        Move m = ss.quiescenceNextMove(position, info, inCheck);

        if (m == NO_MOVE)
            break;

        foundMove = true;

        UndoInfo undo = position.makeMove(m);

        int score = -quiescence(position, -beta, -alpha, ply + 1, qply + 1);

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

    if (inCheck && !foundMove)
        return -MATE_SCORE + ply;

    if (cacheable)
    {
        TTBound bound = TTBound::EXACT;

        if (bestScore <= originalAlpha)
            bound = TTBound::UPPER_BOUND;
        else if (bestScore >= beta)
            bound = TTBound::LOWER_BOUND;

        quiescence_tt.store({
            key,
            static_cast<int16_t>(normalizeToTT(bestScore, ply, SearchConfig::MAX_PLY)),
            static_cast<uint8_t>(remainingDepth),
            bound,
            mode
        });
    }

    return bestScore;
}

Move Search::findBestMove(const Position& position, SearchLimits l)
{
    stopSearch.store(false, std::memory_order_relaxed);

    Position copy = position;
    limits = l;
    nodes = 0;

    transposition.newSearch();
    quiescence_tt.newSearch();
    heuristics.clearKillers();

    uint64_t key = copy.getZobristKey();
    TTEntry ttEntry;

    Move ttMove = NO_MOVE;

    if (transposition.probe(key, ttEntry))
        ttMove = ttEntry.bestMove;

    Move bestMove = NO_MOVE;

    int maxDepth = limits.depth > 0 ? limits.depth : 64;

    for (int depth = 1; depth <= maxDepth; ++depth)
    {
        auto iterationStart = std::chrono::steady_clock::now();

        if (stopSearch.load(std::memory_order_relaxed))
            break;

        SearchStack& root = stack[0];

        MoveGenInfo info = MoveGenerator::calculateInfo(copy);

        root.resetMovePicker();

        int bestScore = -INF;
        int alpha = -INF;

        Move iterationBestMove = NO_MOVE;

        while (true)
        {
            Move m = root.nextMove(copy, info, ttMove, heuristics, 0);

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

        ttMove = bestMove;

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