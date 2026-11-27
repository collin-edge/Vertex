#include "polyglot_book.h"

#include <fstream>
#include <iostream>

namespace
{
    uint64_t readBE64(const uint8_t* b)
    {
        uint64_t value = 0;

        for (int i = 0; i < 8; ++i)
            value = (value << 8) | b[i];

        return value;
    }

    uint32_t readBE32(const uint8_t* b)
    {
        uint32_t value = 0;

        for (int i = 0; i < 4; ++i)
            value = (value << 8) | b[i];

        return value;
    }

    uint16_t readBE16(const uint8_t* b)
    {
        return (uint16_t(b[0]) << 8) | b[1];
    }
}

bool PolyglotBook::open(const std::string& filename)
{
    close();

    size_t totalEntries = 0;

    std::ifstream file(filename, std::ios::binary);

    if (!file)
        return false;

    uint8_t buffer[16];

    while (file.read(reinterpret_cast<char*>(buffer), sizeof(buffer)))
    {
        PolyglotEntry entry{
            readBE64(buffer),
            readBE16(buffer + 8),
            readBE16(buffer + 10),
            readBE32(buffer + 12)
        };

        entries[entry.key].push_back(entry);

        ++totalEntries;
    }

    if (!file.eof())
    {
        close();
        return false;
    }

    return true;
}

void PolyglotBook::close()
{
    entries.clear();
}

const std::vector<PolyglotEntry>* PolyglotBook::find(uint64_t key) const
{
    auto it = entries.find(key);

    if (it == entries.end())
        return nullptr;

    return &it->second;
}
