#pragma once

#include <array>
#include <cassert>
#include "types.h"

struct MoveList
{
    int count = 0;
    static constexpr std::size_t MAX_MOVES = 256;
    std::array<Move, MAX_MOVES> moves{};

    void append(Move move)
    {
        assert(count < MAX_MOVES);
        moves[count++] = move;
    }

    Move& operator[](std::size_t index)
    {
        return moves[index];
    }

    const Move& operator[](std::size_t index) const
    {
        return moves[index];
    }

    auto begin() 
    { 
        return moves.begin(); 
    }
    auto end() 
    {
        return moves.begin() + count; 
    }

    auto begin() const 
    { 
        return moves.begin(); 
    }
    auto end() const 
    { 
        return moves.begin() + count; 
    }
};

namespace MovePacker
{
    constexpr uint8_t PROMOTION_FLAG = 0b1000;

    // make my life easier because i don't wanna remember these values
    enum class Flag : uint8_t
    {
        NONE = 0,
        EN_PASSANT = 0b0001,
        DOUBLE_PAWN_PUSH = 0b0010,
        CASTLE_KINGSIDE = 0b0011,
        CASTLE_QUEENSIDE = 0b0100,

        // promotions: highest bit reserved so it's easier to tell
        KNIGHT = PROMOTION_FLAG | static_cast<uint8_t>(PieceType::KNIGHT),
        BISHOP = PROMOTION_FLAG | static_cast<uint8_t>(PieceType::BISHOP),
        ROOK = PROMOTION_FLAG | static_cast<uint8_t>(PieceType::ROOK),
        QUEEN = PROMOTION_FLAG | static_cast<uint8_t>(PieceType::QUEEN)
    };

    constexpr Move encodeMove(Square start, Square end, Flag flag = Flag::NONE)
    {
        return start | (end << 6) | (static_cast<uint16_t>(flag) << 12);
    }

    constexpr Square getStart(Move move)
    {
        return (move & 0x3F);
    }

    constexpr Square getEnd(Move move)
    {
        return ((move >> 6) & 0x3F);
    }

    constexpr Flag getFlag(Move move)
    {
        return static_cast<Flag>((move >> 12));
    }

    constexpr bool isPromotion(Flag flag)
    {
        return (static_cast<uint8_t>(flag) & PROMOTION_FLAG) != 0;
    }

    constexpr PieceType promotedPiece(Flag flag)
    {
        return static_cast<PieceType>(static_cast<uint8_t>(flag) & 0b0111);
    }
}
