// TODO: FINISH

#pragma once

#include <cstdint>
#include <fstream>
#include <string>

#include "../../types.h"

class OpeningBook
{
private:
    std::ifstream file;
    uint64_t entryCount = 0;

    static constexpr uint64_t ENTRY_SIZE = 10;

public:
    bool open(const std::string& filename);
    void close();

    bool probe(uint64_t key, Move& move);
};
