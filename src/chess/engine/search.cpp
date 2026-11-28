#include <chrono>
#include <iostream>

#include "search.h"
#include "../move_generation/movegen.h"

Search::Search()
{
    resizeWorkers(settings.threads);

    if (!openingBook.open("opening_book.bin"))
        std::cerr << "Failed to load opening book.\n";
    else
        std::cout << "Opening book loaded successfully.\n";
}

Search::~Search()
{
    stop();

    workers.clear();
}

void Search::resizeWorkers(int count)
{
    // WILL CHANGE LATER WHENEVER MULTITHREADING IS SUPPORTED
    // *technically*, you could increase this and have multiple threads. BUT the Transposition Tables aren't multithread safe
    // and i have no clue what would happen
    count = 1;

    workers.clear();

    for (int i = 0; i < count; ++i)
    {
        workers.push_back(
            std::make_unique<SearchThread>(
                transposition,
                quiescence_tt,
                stopSearch
            )
        );
    }
}

void Search::applySettings(SearchSettings s)
{
    stop();

    for (auto& worker : workers)
        worker->join();

    settings = s;

    transposition.resize(settings.hashSize);
    quiescence_tt.resize(settings.qHashSize);

    resizeWorkers(settings.threads);
}

SearchResult Search::findBestMove(const Position& position, const SearchLimits& limits)
{
    stopSearch.store(false, std::memory_order_relaxed);

    Move bookMove = NO_MOVE;
    MoveGenInfo info = MoveGenerator::calculateInfo(position);

    if (openingBook.probe(position.getZobristKey(), bookMove))
    {
        if (MoveGenerator::isLegal(position, info, bookMove))
        {
            SearchResult result;
            result.bestMove = bookMove;

            return result;
        }
    }

    transposition.newSearch();
    quiescence_tt.newSearch();

    for (auto& worker : workers)
        worker->start(position, limits);

    for (auto& worker : workers)
        worker->wait();

    return workers[0]->getResult();
}

void Search::stop()
{
    stopSearch.store(true, std::memory_order_relaxed);
}
