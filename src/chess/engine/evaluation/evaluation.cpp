#include "evaluation.h"
#include "pst.h"
#include "../../move_generation/attacks.h"
#include "../../move_generation/movegen.h"
#include <algorithm>
#include <array>
#include <intrin.h>

constexpr Bitboard FILE_MASKS[8] = {
    0x0101010101010101ULL, 0x0202020202020202ULL,
    0x0404040404040404ULL, 0x0808080808080808ULL,
    0x1010101010101010ULL, 0x2020202020202020ULL,
    0x4040404040404040ULL, 0x8080808080808080ULL
};

constexpr Bitboard ADJACENT_FILES[8] = {
    FILE_MASKS[1], FILE_MASKS[0] | FILE_MASKS[2],
    FILE_MASKS[1] | FILE_MASKS[3], FILE_MASKS[2] | FILE_MASKS[4],
    FILE_MASKS[3] | FILE_MASKS[5], FILE_MASKS[4] | FILE_MASKS[6],
    FILE_MASKS[5] | FILE_MASKS[7], FILE_MASKS[6]
};

constexpr Bitboard passedPawnMask(Color color, Square square)
{
    int file = square % 8;
    int rank = square / 8;
    int firstRank = color == Color::WHITE ? rank + 1 : 0;
    int lastRank = color == Color::WHITE ? 8 : rank;
    Bitboard mask = 0;

    for (int targetRank = firstRank; targetRank < lastRank; ++targetRank)
    {
        for (int targetFile = std::max(0, file - 1); targetFile <= std::min(7, file + 1); ++targetFile)
            mask |= 1ULL << (targetRank * 8 + targetFile);
    }

    return mask;
}

constexpr std::array<std::array<Bitboard, 64>, 2> buildPassedPawnMasks()
{
    std::array<std::array<Bitboard, 64>, 2> masks{};

    for (int square = 0; square < 64; ++square)
    {
        masks[0][square] = passedPawnMask(Color::WHITE, static_cast<Square>(square));
        masks[1][square] = passedPawnMask(Color::BLACK, static_cast<Square>(square));
    }

    return masks;
}

constexpr auto PASSED_PAWN_MASKS = buildPassedPawnMasks();

int sideSign(Color color)
{
    return color == Color::WHITE ? 1 : -1;
}

bool isPassedPawn(Square square, Color color, Bitboard enemyPawns)
{
    int colorIndex = color == Color::WHITE ? 0 : 1;
    return (enemyPawns & PASSED_PAWN_MASKS[colorIndex][square]) == 0;
}

int pawnStructure(const Position& position)
{
    int score = 0;

    for (Color color : {Color::WHITE, Color::BLACK})
    {
        int sign = sideSign(color);
        Bitboard pawns = position.getBitboard(color, PieceType::PAWN);
        Bitboard ownPawns = pawns;

        for (int file = 0; file < 8; ++file)
        {
            int count = __popcnt64(pawns & FILE_MASKS[file]);
            if (count > 1)
                score -= sign * Weights::DOUBLED_PAWN_PENALTY * (count - 1);

            if (__popcnt64(pawns & ADJACENT_FILES[file]) == 0)
            {
                int isolatedCount = __popcnt64(pawns & FILE_MASKS[file]);
                score -= sign * Weights::ISOLATED_PAWN_PENALTY * isolatedCount;
            }
        }

        Bitboard enemyPawns = position.getBitboard(color == Color::WHITE ? Color::BLACK : Color::WHITE, PieceType::PAWN);

        while (ownPawns)
        {
            unsigned long square;
            _BitScanForward64(&square, ownPawns);

            if (isPassedPawn(static_cast<Square>(square), color, enemyPawns))
            {
                int rank = static_cast<int>(square) / 8;
                int advancement = color == Color::WHITE ? rank : 7 - rank;
                score += sign * Weights::PASSED_PAWN_BONUS[advancement];
            }

            ownPawns &= ownPawns - 1;
        }
    }

    return score;
}

int bishopPair(const Position& position, int eg)
{
    int score = 0;
    int bishopPair = (Weights::BISHOP_PAIR_MG * (256 - eg) + Weights::BISHOP_PAIR_EG * eg) / 256;

    if (__popcnt64(position.getBitboard(Color::WHITE, PieceType::BISHOP)) >= 2) score += bishopPair;
    if (__popcnt64(position.getBitboard(Color::BLACK, PieceType::BISHOP)) >= 2) score -= bishopPair;

    return score;
}

int pieceMobility(const Position& position)
{
    int score = 0;
    Bitboard occupied = position.getOccupied();

    for (Color color : {Color::WHITE, Color::BLACK})
    {
        int sign = sideSign(color);
        Bitboard friendly = position.getOccupied(color);

        Bitboard knights = position.getBitboard(color, PieceType::KNIGHT);
        while (knights)
        {
            unsigned long square;
            _BitScanForward64(&square, knights);
            int mobility = __popcnt64(Attacks::getKnightAttacks(static_cast<Square>(square)) & ~friendly);
            score += sign * mobility * Weights::KNIGHT_MOBILITY;
            knights &= knights - 1;
        }

        Bitboard bishops = position.getBitboard(color, PieceType::BISHOP);
        while (bishops)
        {
            unsigned long square;
            _BitScanForward64(&square, bishops);
            int mobility = __popcnt64(Attacks::getBishopAttacks(static_cast<Square>(square), occupied) & ~friendly);
            score += sign * mobility * Weights::BISHOP_MOBILITY;
            bishops &= bishops - 1;
        }

        Bitboard rooks = position.getBitboard(color, PieceType::ROOK);
        while (rooks)
        {
            unsigned long square;
            _BitScanForward64(&square, rooks);
            int mobility = __popcnt64(Attacks::getRookAttacks(static_cast<Square>(square), occupied) & ~friendly);
            score += sign * mobility * Weights::ROOK_MOBILITY;
            rooks &= rooks - 1;
        }

        Bitboard queens = position.getBitboard(color, PieceType::QUEEN);
        while (queens)
        {
            unsigned long square;
            _BitScanForward64(&square, queens);
            int mobility = __popcnt64((Attacks::getBishopAttacks(static_cast<Square>(square), occupied) | Attacks::getRookAttacks(static_cast<Square>(square), occupied)) & ~friendly);
            score += sign * mobility * Weights::QUEEN_MOBILITY;
            queens &= queens - 1;
        }
    }

    return score;
}

int kingSafety(const Position& position, int eg, const MoveGenInfo* info)
{
    int score = 0;

    for (Color color : {Color::WHITE, Color::BLACK})
    {
        int sign = sideSign(color);
        Color enemy = color == Color::WHITE ? Color::BLACK : Color::WHITE;
        Bitboard king = position.getBitboard(color, PieceType::KING);
        
        if (king == 0)
            continue;

        Square kingSquare;
        if (info != nullptr && color == position.getSideToMove())
        {
            kingSquare = info->kingSquare;
        }
        else
        {
            unsigned long square;
            _BitScanForward64(&square, king);
            kingSquare = static_cast<Square>(square);
        }

        int file = kingSquare % 8;
        int rank = kingSquare / 8;

        int shieldRank = color == Color::WHITE ? rank + 1 : rank - 1;
        if (shieldRank >= 0 && shieldRank < 8)
        {
            for (int f = std::max(0, file - 1); f <= std::min(7, file + 1); ++f)
            {
                if (!(position.getBitboard(color, PieceType::PAWN) & (1ULL << (shieldRank * 8 + f))))
                    score -= sign * Weights::WEAK_PAWN_SHIELD_PENALTY;
            }
        }

        for (int f = std::max(0, file - 1); f <= std::min(7, file + 1); ++f)
        {
            bool ownPawnOnFile = (position.getBitboard(color, PieceType::PAWN) & FILE_MASKS[f]) != 0;
            if (!ownPawnOnFile)
                score -= sign * Weights::OPEN_FILE_NEAR_KING_PENALTY;
        }

        Bitboard attackerPieces = 0;
        Bitboard kingZone = Attacks::getKingAttacks(kingSquare) | (1ULL << kingSquare);
        while (kingZone)
        {
            unsigned long square;
            _BitScanForward64(&square, kingZone);
            attackerPieces |= Attacks::getAttackers(position, static_cast<Square>(square), enemy, position.getOccupied());
            kingZone &= kingZone - 1;
        }

        int attackers = __popcnt64(attackerPieces);
        if (attackers > 0)
            score -= sign * attackers * Weights::KING_ATTACKER_WEIGHT;
    }

    return score * (256 - eg) / 256;
}

int material(const Position& position)
{
    int score = 0;
    constexpr PieceType PIECE_TYPES[] = {
        PieceType::PAWN, PieceType::KNIGHT, PieceType::BISHOP, PieceType::ROOK, PieceType::QUEEN
    };
    constexpr int PIECE_VALUES[] = {
        Weights::PAWN_VALUE, Weights::KNIGHT_VALUE, Weights::BISHOP_VALUE, Weights::ROOK_VALUE, Weights::QUEEN_VALUE
    };

    for (Color color : {Color::WHITE, Color::BLACK})
    {
        int sign = sideSign(color);
        for (int i = 0; i < 5; ++i)
        {
            score += sign * __popcnt64(position.getBitboard(color, PIECE_TYPES[i])) * PIECE_VALUES[i];
        }
    }

    return score;
}

int pst(const Position& position, int eg)
{
    int score = 0;

    for (Color color : {Color::WHITE, Color::BLACK})
    {
        Bitboard occupied = position.getOccupied(color);
        while (occupied)
        {
            unsigned long target;
            _BitScanForward64(&target, occupied);

            Piece p = position.getPiece(static_cast<Square>(target));
            PieceType type = Pieces::getType(p);
            score += sideSign(color) * PST::pstScore(static_cast<Square>(target), type, color, eg);

            occupied &= occupied - 1;
        }
    }

    return score;
}

int evaluatePosition(const Position& position, const MoveGenInfo* info)
{
    int eg = PST::endgameWeight(PST::gamePhase(position));
    int score =
        material(position)
      + pst(position, eg)
      + pawnStructure(position)
      + bishopPair(position, eg)
      + pieceMobility(position)
      + kingSafety(position, eg, info);

    return position.getSideToMove() == Color::WHITE ? score : -score;
}

int Evaluation::evaluate(const Position& position)
{
    return evaluatePosition(position, nullptr);
}

int Evaluation::evaluate(const Position& position, const MoveGenInfo& info)
{
    return evaluatePosition(position, &info);
}