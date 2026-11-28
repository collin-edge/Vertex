
#pragma once

#include <vector>
#include <cstdint>
#include <cassert>
#include <climits>

#include "../../types.h"

enum class TTBound : uint8_t
{
    EXACT,
    LOWER_BOUND,
    UPPER_BOUND
};

struct TTEntry
{
    uint64_t key = 0;

    Move bestMove = NO_MOVE;

    int16_t score = 0;
    uint8_t depth = 0;

    TTBound bound = TTBound::EXACT;

    uint8_t generation = 0;

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

    uint8_t generation = 0;

    std::size_t index(uint64_t key) const
    {
        assert(!clusters.empty());
        return key % clusters.size();
    }

    uint8_t age(const TTEntry& entry) const
    {
        return static_cast<uint8_t>(generation - entry.generation);
    }

    int replacementValue(const TTEntry& entry) const
    {
        return static_cast<int>(entry.depth) - 2 * static_cast<int>(age(entry));
    }

public:
    TranspositionTable(std::size_t sizeMB)
    {
        resize(sizeMB);
    }

    void newSearch()
    {
        ++generation;
    }

    void store(TTEntry entry)
    {
        Cluster& cluster = clusters[index(entry.key)];

        for (TTEntry& existing : cluster.entries)
        {
            if (existing.occupied && existing.key == entry.key)
            {
                if (entry.depth >= existing.depth || (entry.bound == TTBound::EXACT && existing.bound != TTBound::EXACT))
                {
                    entry.generation = generation;
                    entry.occupied = true;
                    existing = entry;
                }
                else
                {
                    existing.generation = generation;
                }

                return;
            }
        }

        for (TTEntry& existing : cluster.entries)
        {
            if (!existing.occupied)
            {
                entry.generation = generation;
                entry.occupied = true;
                existing = entry;
                return;
            }
        }

        TTEntry* replacement = nullptr;
        int worstValue = INT_MAX;

        for (TTEntry& existing : cluster.entries)
        {
            int value = replacementValue(existing);

            if (value < worstValue)
            {
                worstValue = value;
                replacement = &existing;
            }
        }

        if (replacement != nullptr)
        {
            entry.generation = generation;
            entry.occupied = true;
            *replacement = entry;
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

        generation = 0;
    }

    void resize(std::size_t sizeMB)
    {
        assert(sizeMB > 0);

        std::vector<Cluster> newClusters((sizeMB * 1024 * 1024) / sizeof(Cluster));

        clusters.swap(newClusters);
        generation = 0;
    }
};

// quiescence tables

enum class QTTMode : uint8_t
{
    NORMAL,
    IN_CHECK
};

struct QTTEntry
{
    uint64_t key = 0;

    int16_t score = 0;
    uint8_t depth = 0;

    TTBound bound = TTBound::EXACT;
    QTTMode mode = QTTMode::NORMAL;

    uint8_t generation = 0;

    bool occupied = false;
};

struct QTTCluster
{
    QTTEntry entries[CLUSTER_SIZE];
};

struct QuiescenceTranspositionTable
{
private:
    std::vector<QTTCluster> clusters;

    uint8_t generation = 0;

    std::size_t index(uint64_t key) const
    {
        assert(!clusters.empty());
        return key % clusters.size();
    }

    uint8_t age(const QTTEntry& entry) const
    {
        return static_cast<uint8_t>(generation - entry.generation);
    }

    int replacementValue(const QTTEntry& entry) const
    {
        return static_cast<int>(entry.depth) - 2 * static_cast<int>(age(entry));
    }

public:
    explicit QuiescenceTranspositionTable(std::size_t sizeMB)
    {
        resize(sizeMB);
    }

    void newSearch()
    {
        ++generation;
    }

    void store(QTTEntry entry)
    {
        QTTCluster& cluster = clusters[index(entry.key)];

        for (QTTEntry& existing : cluster.entries)
        {
            if (existing.occupied && existing.key == entry.key && existing.mode == entry.mode)
            {
                if (entry.depth >= existing.depth || (entry.bound == TTBound::EXACT && existing.bound != TTBound::EXACT))
                {
                    entry.generation = generation;
                    entry.occupied = true;
                    existing = entry;
                }
                else
                {
                    existing.generation = generation;
                }

                return;
            }
        }

        for (QTTEntry& existing : cluster.entries)
        {
            if (!existing.occupied)
            {
                entry.generation = generation;
                entry.occupied = true;
                existing = entry;
                return;
            }
        }

        QTTEntry* replacement = nullptr;
        int worstValue = INT_MAX;

        for (QTTEntry& existing : cluster.entries)
        {
            int value = replacementValue(existing);

            if (value < worstValue)
            {
                worstValue = value;
                replacement = &existing;
            }
        }

        if (replacement != nullptr)
        {
            entry.generation = generation;
            entry.occupied = true;
            *replacement = entry;
        }
    }

    bool probe(uint64_t key, QTTMode mode, QTTEntry& entry) const
    {
        const QTTCluster& cluster = clusters[index(key)];

        for (const QTTEntry& existing : cluster.entries)
        {
            if (existing.occupied && existing.key == key && existing.mode == mode)
            {
                entry = existing;
                return true;
            }
        }

        return false;
    }

    void clear()
    {
        for (QTTCluster& cluster : clusters)
        {
            for (QTTEntry& entry : cluster.entries)
            {
                entry.occupied = false;
            }
        }

        generation = 0;
    }

    void resize(std::size_t sizeMB)
    {
        assert(sizeMB > 0);

        std::vector<QTTCluster> newClusters((sizeMB * 1024 * 1024) / sizeof(QTTCluster));

        clusters.swap(newClusters);
        generation = 0;
    }
};