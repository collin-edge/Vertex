#pragma once

#include <array>
#include <cstdint>
#include <random>

namespace Zobrist
{
    std::mt19937_64 rng(0xC0ED6E);

    uint64_t PIECE_KEYS[12][64];
    uint64_t SIDE_TO_MOVE;
    uint64_t CASTLING_RIGHTS[16];
    uint64_t EN_PASSANT_FILE[8];

    void generate_keys()
    {
        // 12 keys for 12 piece types
        for (int i = 0; i < 12; ++i)
        {
            for (int j = 0; j < 64; ++j)
            {
                PIECE_KEYS[i][j] = rng();
            }
        }

        SIDE_TO_MOVE = rng();

        for (int i = 0; i < 16; ++i)
            CASTLING_RIGHTS[i] = rng();

        for (int i = 0; i < 8; ++i)
            EN_PASSANT_FILE[i] = rng();
    }
}