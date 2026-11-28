#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "../position.h"
#include "objects/transposition_tables.h"
#include "objects/search_heuristics.h"
#include "objects/search_stack.h"
#include "search_types.h"

class SearchThread
{
private:
    std::thread thread;

    mutable std::mutex mutex;
    std::condition_variable condition;

    bool shutdown = false;
    bool taskPending = false;
    bool searching = false;

    TranspositionTable& transposition;
    QuiescenceTranspositionTable& qtt;
    std::atomic<bool>& stopSearch;

    Position position;
    SearchLimits limits;

    SearchStack stack[SearchConfig::MAX_PLY];
    SearchHeuristics heuristics;

    uint64_t nodes = 0;
    SearchResult result;

    void workerLoop();
    void searchLoop();

    int search(Position& position, int depth, int alpha, int beta, int ply = 0);
    int quiescence(Position& position, int alpha, int beta, int ply, int qply = 0);

public:
    SearchThread(TranspositionTable& tt, QuiescenceTranspositionTable& qtt, std::atomic<bool>& stopSearch);
    ~SearchThread();

    void start(Position position, const SearchLimits& limits);
    void wait();
    void join();

    SearchResult getResult() const;
};