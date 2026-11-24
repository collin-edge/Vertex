#pragma once

#include <vector>
#include <cstddef>
#include <cassert>
#include "../types.h"

enum class TTBound : uint8_t
{
    EXACT,
    LOWER_BOUND,
    UPPER_BOUND
};

enum class TTSearchType : uint8_t
{
    NORMAL,
    QUIESCENCE
};

struct TTEntry
{
    uint64_t key = 0;

    Move bestMove = NO_MOVE;

    int16_t score = 0;
    uint8_t depth = 0;

    TTBound bound = TTBound::EXACT;
    TTSearchType searchType = TTSearchType::NORMAL;

    bool occupied = false;
};

constexpr std::size_t CLUSTER_SIZE = 4;

struct Cluster
{
    TTEntry entries[CLUSTER_SIZE];
};

struct TranspositionTable
{
private:
    std::vector<Cluster> clusters;

    std::size_t index(uint64_t key) const
    {
        assert(!clusters.empty());

        return key % clusters.size();
    }

public:
    TranspositionTable(std::size_t sizeMB)
    {
        assert(sizeMB > 0);
        clusters.resize((sizeMB * 1024 * 1024) / sizeof(Cluster));
    }

    void store(const TTEntry& entry)
    {
        Cluster& cluster = clusters[index(entry.key)];

        TTEntry* replacement = nullptr;

        for (TTEntry& existing : cluster.entries)
        {
            if (existing.occupied && existing.key == entry.key)
            {
                replacement = &existing;
                break;
            }

            if (!existing.occupied)
            {
                replacement = &existing;
                break;
            }

            if (replacement == nullptr || existing.depth < replacement->depth)
            {
                replacement = &existing;
            }
        }

        if (replacement != nullptr)
        {
            *replacement = entry;
            replacement->occupied = true;
        }
    }

    bool probe(uint64_t key, TTEntry& entry) const
    {
        const Cluster& cluster = clusters[index(key)];

        for (const TTEntry& existing : cluster.entries)
        {
            if (existing.occupied && existing.key == key)
            {
                entry = existing;
                return true;
            }
        }

        return false;
    }

    void clear()
    {
        for (Cluster& cluster : clusters)
        {
            for (TTEntry& entry : cluster.entries)
            {
                entry.occupied = false;
            }
        }
    }
};