// TODO: FINISH

#include "opening_book.h"

bool OpeningBook::open(const std::string& filename)
{
    close();

    file.open(filename, std::ios::binary);

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

        file.read(reinterpret_cast<char*>(&storedKey), sizeof(storedKey));
        file.read(reinterpret_cast<char*>(&storedMove), sizeof(storedMove));

        if (!file)
        {
            file.clear();
            return false;
        }

        if (storedKey == key)
        {
            move = storedMove;
            return true;
        }

        if (storedKey < key)
            left = mid + 1;
        else
            right = mid;
    }

    return false;
}