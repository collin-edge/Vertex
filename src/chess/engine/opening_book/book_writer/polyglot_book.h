#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct PolyglotEntry
{
    uint64_t key;
    uint16_t move;
    uint16_t weight;
    uint32_t learn;
};

class PolyglotBook
{
private:
    std::unordered_map<uint64_t, std::vector<PolyglotEntry>> entries;

public:
    bool open(const std::string& filename);

    void close();

    const std::vector<PolyglotEntry>* find(uint64_t key) const;
};