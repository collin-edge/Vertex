#pragma once

#include <cstdint>

extern const uint64_t PAWN_ATTACKS[2][64];
extern const uint64_t KNIGHT_ATTACKS[64];
extern const uint64_t KING_ATTACKS[64]; // not castling

extern const uint64_t BISHOP_MASKS[64];
extern const uint64_t BISHOP_MAGICS[64];
extern const int BISHOP_SHIFTS[64];
extern const uint64_t* const BISHOP_ATTACK_TABLE[64];

extern const uint64_t ROOK_MASKS[64];
extern const uint64_t ROOK_MAGICS[64];
extern const int ROOK_SHIFTS[64];
extern const uint64_t* const ROOK_ATTACK_TABLE[64];