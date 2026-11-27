#pragma once

#include <cstdint>
#include "../../../position.h"
#include "../../../moves.h"

namespace Polyglot
{
    extern const uint64_t VALUES[781];

    uint64_t hash(const Position& position);
    Move decodeMove(uint16_t encoded, const Position& position);
}