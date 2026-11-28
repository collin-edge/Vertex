#pragma once

#include <atomic>
#include <memory>
#include <vector>

#include "search_types.h"
#include "../position.h"
#include "objects/transposition_tables.h"
#include "opening_book/opening_book.h"
#include "search_thread.h"

class Search
{
private:
    std::atomic<bool> stopSearch = false;

    SearchSettings settings;

    TranspositionTable transposition{settings.hashSize};
    QuiescenceTranspositionTable quiescence_tt{settings.qHashSize};

    std::vector<std::unique_ptr<SearchThread>> workers;

    OpeningBook openingBook;

    void resizeWorkers(int count);

public:
    Search();
    ~Search();

    void applySettings(SearchSettings settings);

    SearchResult findBestMove(const Position& position, const SearchLimits& limits);

    void stop();

    bool DEBUG = true;
};
