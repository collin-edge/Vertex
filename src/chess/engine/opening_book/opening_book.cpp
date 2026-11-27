// TODO: FINISH

#include "opening_book.h"

#include <random>
#include <windows.h>
#include <filesystem>

std::filesystem::path getExecutableDirectory()
{
    char buffer[MAX_PATH];
    GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    return std::filesystem::path(buffer).parent_path();
}

bool OpeningBook::open(const std::string& filename)
{
    close();

    auto path = getExecutableDirectory() / filename;

    file.open(path, std::ios::binary);

    if (!file.is_open())
        return false;

    file.seekg(0, std::ios::end);

    std::streamoff fileSize = file.tellg();

    if (fileSize < 0 || fileSize % ENTRY_SIZE != 0)
    {
        close();
        return false;
    }

    entryCount = fileSize / ENTRY_SIZE;

    file.seekg(0, std::ios::beg);

    return true;
}

void OpeningBook::close()
{
    if (file.is_open())
        file.close();

    entryCount = 0;
}

bool OpeningBook::probe(uint64_t key, Move& move)
{
    if (!file.is_open() || entryCount == 0)
        return false;

    uint64_t left = 0;
    uint64_t right = entryCount;

    while (left < right)
    {
        uint64_t mid = left + (right - left) / 2;

        file.seekg(mid * ENTRY_SIZE, std::ios::beg);

        uint64_t storedKey;
        uint16_t storedMove;
        uint16_t storedWeight;

        file.read(reinterpret_cast<char*>(&storedKey), 8);
        file.read(reinterpret_cast<char*>(&storedMove), 2);
        file.read(reinterpret_cast<char*>(&storedWeight), 2);

        if (!file)
        {
            file.clear();
            return false;
        }

        if (storedKey < key)
            left = mid + 1;
        else
            right = mid;
    }

    if (left >= entryCount)
        return false;

    uint64_t totalWeight = 0;
    uint64_t end = left;

    while (end < entryCount)
    {
        file.seekg(end * ENTRY_SIZE, std::ios::beg);

        uint64_t storedKey;
        uint16_t storedMove;
        uint16_t storedWeight;

        file.read(reinterpret_cast<char*>(&storedKey), 8);
        file.read(reinterpret_cast<char*>(&storedMove), 2);
        file.read(reinterpret_cast<char*>(&storedWeight), 2);

        if (!file)
        {
            file.clear();
            return false;
        }

        if (storedKey != key)
            break;

        totalWeight += storedWeight;
        ++end;
    }

    if (end == left)
        return false;

    static thread_local std::mt19937 rng(std::random_device{}());

    if (totalWeight == 0)
    {
        std::uniform_int_distribution<uint64_t> dist(left, end - 1);

        uint64_t selected = dist(rng);

        file.seekg(selected * ENTRY_SIZE + 8, std::ios::beg);

        uint16_t storedMove;
        file.read(reinterpret_cast<char*>(&storedMove), 2);

        if (!file)
        {
            file.clear();
            return false;
        }

        move = static_cast<Move>(storedMove);
        return true;
    }

    std::uniform_int_distribution<uint64_t> dist(1, totalWeight);

    uint64_t randomWeight = dist(rng);

    for (uint64_t i = left; i < end; ++i)
    {
        file.seekg(i * ENTRY_SIZE + 10, std::ios::beg);

        uint16_t weight;
        file.read(reinterpret_cast<char*>(&weight), 2);

        if (!file)
        {
            file.clear();
            return false;
        }

        if (randomWeight <= weight)
        {
            file.seekg(i * ENTRY_SIZE + 8, std::ios::beg);

            uint16_t storedMove;
            file.read(reinterpret_cast<char*>(&storedMove), 2);

            if (!file)
            {
                file.clear();
                return false;
            }

            move = static_cast<Move>(storedMove);
            return true;
        }

        randomWeight -= weight;
    }

    return false;
}