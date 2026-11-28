#include "search_thread.h"

#include <utility>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <chrono>

#include "evaluation/evaluation.h"

namespace
{
    constexpr int INF = 32000;
    constexpr int MATE_SCORE = 10000;
    constexpr int MAX_QUIESCENCE_DEPTH = 8;
    constexpr int ABSOLUTE_QUIESCENCE_DEPTH = 10;

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

    int calculateReduction(int depth, int moveCount, int historyScore)
    {
        int reduction = static_cast<int>(0.75 + (std::log(depth) * std::log(moveCount)) / 2.25);

        if (historyScore > 200)
            --reduction;
        else if (historyScore < -200)
            ++reduction;

        return std::clamp(reduction, 1, depth - 1);
    }
}

SearchThread::SearchThread(TranspositionTable& tt, QuiescenceTranspositionTable& qtt, std::atomic<bool>& stopSearch)
: transposition(tt), qtt(qtt), stopSearch(stopSearch)
{
    thread = std::thread(&SearchThread::workerLoop, this);
}

SearchThread::~SearchThread()
{
    join();
}

void SearchThread::workerLoop()
{
    while (true)
    {
        {
            std::unique_lock<std::mutex> lock(mutex);

            condition.wait(lock, [this]
            {
                return taskPending || shutdown;
            });

            if (shutdown)
                return;

            taskPending = false;
        }

        searchLoop();

        {
            std::lock_guard<std::mutex> lock(mutex);

            searching = false;
        }

        condition.notify_all();
    }
}

void SearchThread::start(Position position, const SearchLimits& limits)
{
    std::unique_lock<std::mutex> lock(mutex);

    condition.wait(lock, [this]
    {
        return (!searching && !taskPending) || shutdown;
    });

    if (shutdown)
        return;

    this->position = std::move(position);
    this->limits = limits;

    nodes = 0;
    result = SearchResult{};

    searching = true;
    taskPending = true;

    lock.unlock();

    condition.notify_one();
}

void SearchThread::wait()
{
    std::unique_lock<std::mutex> lock(mutex);

    condition.wait(lock, [this]
    {
        return !searching && !taskPending;
    });
}

void SearchThread::join()
{
    {
        std::lock_guard<std::mutex> lock(mutex);

        shutdown = true;
    }

    condition.notify_one();

    if (thread.joinable())
        thread.join();
}

SearchResult SearchThread::getResult() const
{
    std::lock_guard<std::mutex> lock(mutex);

    return result;
}

void SearchThread::searchLoop()
{
    heuristics.clearKillers();

    MoveGenInfo info = MoveGenerator::calculateInfo(position);

    uint64_t key = position.getZobristKey();
    TTEntry ttEntry;

    Move ttMove = NO_MOVE;

    if (transposition.probe(key, ttEntry))
        ttMove = ttEntry.bestMove;

    Move bestMove = NO_MOVE;

    int maxDepth = limits.depth > 0 ? limits.depth : 64;

    int previousScore = 0;
    int aspirationWindow = 25;

    for (int depth = 1; depth <= maxDepth; ++depth)
    {
        auto iterationStart = std::chrono::steady_clock::now();

        if (stopSearch.load(std::memory_order_relaxed))
            break;

        int alpha = -INF;
        int beta = INF;

        if (depth >= 2)
        {
            alpha = previousScore - aspirationWindow;
            beta = previousScore + aspirationWindow;
        }

        Move iterationBestMove = NO_MOVE;
        int bestScore = -INF;

        while (true)
        {
            SearchStack& root = stack[0];
            root.resetMovePicker();

            bestScore = -INF;
            iterationBestMove = NO_MOVE;

            int currentAlpha = alpha;
            bool firstMove = true;

            while (true)
            {
                Move m = root.nextMove(position, info, ttMove, heuristics, 0);

                if (m == NO_MOVE)
                    break;

                UndoInfo undo = position.makeMove(m);

                int score;

                if (firstMove)
                {
                    score = -search(position, depth - 1, -beta, -currentAlpha, 1);

                    firstMove = false;
                }
                else
                {
                    score = -search(position, depth - 1, -currentAlpha - 1, -currentAlpha, 1);

                    if (score > currentAlpha && score < beta)
                    {
                        score = -search(position, depth - 1, -beta, -currentAlpha, 1);
                    }
                }

                position.undoMove(undo);

                if (stopSearch.load(std::memory_order_relaxed))
                    break;

                if (score > bestScore)
                {
                    bestScore = score;
                    iterationBestMove = m;
                }

                if (score > currentAlpha)
                    currentAlpha = score;
            }

            if (stopSearch.load(std::memory_order_relaxed))
                break;

            if (bestScore <= alpha)
            {
                alpha -= aspirationWindow;
                aspirationWindow *= 2;
                continue;
            }

            if (bestScore >= beta)
            {
                beta += aspirationWindow;
                aspirationWindow *= 2;
                continue;
            }

            break;
        }

        if (stopSearch.load(std::memory_order_relaxed))
            break;

        bestMove = iterationBestMove;
        ttMove = bestMove;
        previousScore = bestScore;

        aspirationWindow = 25;

        result.bestMove = bestMove;
        result.score = bestScore;
        result.depth = depth;
        result.nodes = nodes;

        auto iterationEnd = std::chrono::steady_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(iterationEnd - iterationStart).count();

        std::cout
            << "info depth " << depth
            << " score cp " << bestScore
            << " nodes " << nodes
            << " time " << elapsed
            << std::endl;
    }

    result.nodes = nodes;
}

int SearchThread::search(Position& position, int depth, int alpha, int beta, int ply)
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
    {
        MoveGenInfo info = MoveGenerator::calculateInfo(position);

        if (info.checkerCount() > 0)
            return 0;

        return Evaluation::evaluate(position, info);
    }

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

    int moveCount = 0;

    bool firstMove = true;

    while (true)
    {
        Move m = ss.nextMove(position, info, ttMove, heuristics, ply);

        if (m == NO_MOVE)
            break;

        foundMove = true;

        ++moveCount;

        Square end = MovePacker::getEnd(m);
        auto flag = MovePacker::getFlag(m);
        Piece captured = position.getPiece(end);
        bool quiet = (captured == Pieces::EMPTY && flag != MovePacker::Flag::EN_PASSANT && !MovePacker::isPromotion(flag));

        UndoInfo undo = position.makeMove(m);

        // late move reduction implementation
        int newDepth = depth - 1;
        int score;

        bool canReduce = moveCount >= 4 && depth >= 3 && quiet && info.checkerCount() == 0;

        if (firstMove)
        {
            // first move search with the full window
            score = -search(position, newDepth, -beta, -alpha, ply + 1);

            firstMove = false;
        }
        else
        {
            // everything else we search with a smaller window to see if it could really best the first alpha
            if (canReduce)
            {
                int reduction = calculateReduction(depth, moveCount, heuristics.getHistory(position.getSideToMove(), MovePacker::getStart(m), end));
                int reducedDepth = std::max(0, newDepth - reduction);

                score = -search(position, reducedDepth, -alpha - 1, -alpha, ply + 1);

                // verify at full depth if it looks promising
                if (score > alpha)
                {
                    score = -search(position, newDepth, -alpha - 1, -alpha, ply + 1);
                }
            }
            else
            {
                score = -search(position, newDepth, -alpha - 1, -alpha, ply + 1);
            }

            // if it beats alpha but doesn't cause a cutoff, determine its actual score using a full window
            if (score > alpha && score < beta)
            {
                score = -search(position, newDepth, -beta, -alpha,ply + 1);
            }
        }

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

        Square start = MovePacker::getStart(m);

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

int SearchThread::quiescence(Position& position, int alpha, int beta, int ply, int qply)
{
    if (stopSearch.load(std::memory_order_relaxed))
        return 0;

    ++nodes;

    if (ply >= SearchConfig::MAX_PLY - 1)
        return Evaluation::evaluate(position);

    int originalAlpha = alpha;
    uint64_t key = position.getZobristKey();

    MoveGenInfo info = MoveGenerator::calculateInfo(position);
    bool inCheck = info.checkerCount() > 0;

    int remainingDepth = std::max(0, MAX_QUIESCENCE_DEPTH - qply);
    QTTMode mode = inCheck ? QTTMode::IN_CHECK : QTTMode::NORMAL;

    if (qply >= ABSOLUTE_QUIESCENCE_DEPTH)
    {
        MoveList moves;
        moves.count = 0;

        MoveGenerator::generateLegalMoves(position, info, moves, MoveGenType::ALL);

        if (moves.count == 0)
        {
            if (inCheck)
                return -MATE_SCORE + ply;

            return 0;
        }

        return Evaluation::evaluate(position, info);
    }

    QTTEntry ttEntry;

    if (qtt.probe(key, mode, ttEntry))
    {
        if (ttEntry.depth >= remainingDepth)
        {
            int ttScore = normalizeFromTT(
                ttEntry.score,
                ply,
                SearchConfig::MAX_PLY
            );

            if (ttEntry.bound == TTBound::EXACT)
                return ttScore;

            if (ttEntry.bound == TTBound::LOWER_BOUND)
                if (ttScore >= beta) return ttScore;

            if (ttEntry.bound == TTBound::UPPER_BOUND)
                if (ttScore <= alpha) return ttScore;
        }
    }

    int bestScore = -INF;
    int standPat = -INF;

    if (!inCheck)
    {
        standPat = Evaluation::evaluate(position, info);

        if (standPat >= beta)
        {
            qtt.store({
                key,
                static_cast<int16_t>(normalizeToTT(standPat, ply, SearchConfig::MAX_PLY)),
                static_cast<uint8_t>(remainingDepth),
                TTBound::LOWER_BOUND,
                mode
            });

            return beta;
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

        auto flag = MovePacker::getFlag(m);
        bool promotion = MovePacker::isPromotion(flag);
        int capturedValue = MoveOrdering::capturedPieceValue(m, position);

        constexpr int DELTA_MARGIN = 200;

        if (!inCheck && !promotion && capturedValue > 0)
        {
            if (standPat + capturedValue + DELTA_MARGIN < alpha)
                continue;
        }

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
            return beta;
    }

    if (inCheck && !foundMove)
        return -MATE_SCORE + ply;

    TTBound bound = TTBound::EXACT;

    if (bestScore <= originalAlpha)
        bound = TTBound::UPPER_BOUND;
    else if (bestScore >= beta)
        bound = TTBound::LOWER_BOUND;

    qtt.store({
        key,
        static_cast<int16_t>(normalizeToTT(bestScore, ply, SearchConfig::MAX_PLY)),
        static_cast<uint8_t>(remainingDepth),
        bound,
        mode
    });

    return bestScore;
}