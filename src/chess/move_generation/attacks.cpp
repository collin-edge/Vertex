#include "attacks.h"

#include "../position.h"
#include "attack_tables.h"
#include <algorithm>

namespace Attacks
{
    Bitboard BETWEEN[64][64]{};
    Bitboard BISHOP_RAYS[64][4]{};
    Bitboard ROOK_RAYS[64][4];
}

void Attacks::buildTables()
{
    for (int a = 0; a < 64; ++a)
    {
        for (int b = 0; b < 64; ++b)
        {
            if (a == b) continue;

            // we can narrow down the direction between A and B
            int aFile = a % 8;
            int aRank = a / 8;

            int bFile = b % 8;
            int bRank = b / 8;

            int fileDiff = bFile - aFile;
            int rankDiff = bRank - aRank;

            int fileStep = 0;
            int rankStep = 0;

            if (fileDiff == 0)
            {
                // same file, so vertical line
                rankStep = (rankDiff > 0) ? 1 : -1;
            }
            else if (rankDiff == 0)
            {
                // same rank, so horizontal line
                fileStep = (fileDiff > 0) ? 1 : -1;
            }
            else if (abs(fileDiff) == abs(rankDiff))
            {
                // diagonal
                fileStep = (fileDiff > 0) ? 1 : -1;
                rankStep = (rankDiff > 0) ? 1 : -1;
            }
            else
            {
                // not aligned
                continue;
            }

            int file = aFile + fileStep;
            int rank = aRank + rankStep;

            while (file != bFile || rank != bRank)
            {
                int idx = rank * 8 + file;
                BETWEEN[a][b] |= 1ULL << idx;

                file += fileStep;
                rank += rankStep;
            }
        }
    }

    const int ROOK_DIRS[4][2] = {
        {-1, 0}, {1, 0}, {0, -1}, {0, 1}
    };

    const int BISHOP_DIRS[4][2] = {
        {-1, -1}, {-1, 1}, {1, -1}, {1, 1}
    };

    for (int i = 0; i < 64; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            ROOK_RAYS[i][j] = 0;
            BISHOP_RAYS[i][j] = 0;
        }

        int startFile = i % 8;
        int startRank = i / 8;

        for (int d = 0; d < 4; ++d)
        {
            int dx = ROOK_DIRS[d][0];
            int dy = ROOK_DIRS[d][1];

            int file = startFile + dx;
            int rank = startRank + dy;

            while (file >= 0 && file < 8 && rank >= 0 && rank < 8)
            {
                ROOK_RAYS[i][d] |= 1ULL << (rank * 8 + file);

                file += dx;
                rank += dy;
            }
        }

        for (int d = 0; d < 4; ++d)
        {
            int dx = BISHOP_DIRS[d][0];
            int dy = BISHOP_DIRS[d][1];

            int file = startFile + dx;
            int rank = startRank + dy;

            while (file >= 0 && file < 8 && rank >= 0 && rank < 8)
            {
                BISHOP_RAYS[i][d] |= 1ULL << (rank * 8 + file);

                file += dx;
                rank += dy;
            }
        }
    }
}

Bitboard Attacks::getPawnAttacks(Color color, Square square)
{
    return PAWN_ATTACKS[(color == Color::WHITE) ? 0 : 1][square];
}

Bitboard Attacks::getKnightAttacks(Square square)
{
    return KNIGHT_ATTACKS[square];
}

Bitboard Attacks::getKingAttacks(Square square)
{
    return KING_ATTACKS[square];
}

Bitboard Attacks::getRookAttacks(Square square, Bitboard occupied)
{
    occupied &= ROOK_MASKS[square];
    uint64_t index = (occupied * ROOK_MAGICS[square]) >> ROOK_SHIFTS[square];
    uint64_t possible = ROOK_ATTACK_TABLE[square][index];
    return possible;
}

Bitboard Attacks::getBishopAttacks(Square square, Bitboard occupied)
{
    occupied &= BISHOP_MASKS[square];
    uint64_t index = (occupied * BISHOP_MAGICS[square]) >> BISHOP_SHIFTS[square];
    uint64_t possible = BISHOP_ATTACK_TABLE[square][index];
    return possible;
}

Bitboard Attacks::getAttackers(const Position& position, Square square, Color attacker, Bitboard occupancy)
{
    Bitboard attackers = 0;

    Bitboard pawns = position.getBitboard(attacker, PieceType::PAWN) & occupancy;
    Bitboard knights = position.getBitboard(attacker, PieceType::KNIGHT) & occupancy;
    Bitboard kings = position.getBitboard(attacker, PieceType::KING) & occupancy;
    Bitboard bishops = position.getBitboard(attacker, PieceType::BISHOP) & occupancy;
    Bitboard rooks = position.getBitboard(attacker, PieceType::ROOK) & occupancy;
    Bitboard queens = position.getBitboard(attacker, PieceType::QUEEN) & occupancy;

    attackers |= PAWN_ATTACKS[attacker == Color::WHITE ? 1 : 0][square] & pawns;
    attackers |= KNIGHT_ATTACKS[square] & knights;
    attackers |= KING_ATTACKS[square] & kings;

    attackers |= getBishopAttacks(square, occupancy) & (bishops | queens);
    attackers |= getRookAttacks(square, occupancy) & (rooks | queens);

    return attackers;
}

// see, i could just make this function use getAttackers, but then that'd make unnecessary checks for more attackers once a single attacker is found.
// so just repeat the logic up there but return as soon as one is found to be true

bool Attacks::isSquareAttacked(const Position& position, Square square, Color attacker, Bitboard occupied)
{
    Bitboard pawns = position.getBitboard(attacker, PieceType::PAWN) & occupied;
    Bitboard knights = position.getBitboard(attacker, PieceType::KNIGHT) & occupied;
    Bitboard kings = position.getBitboard(attacker, PieceType::KING) & occupied;
    Bitboard bishops = position.getBitboard(attacker, PieceType::BISHOP) & occupied;
    Bitboard rooks = position.getBitboard(attacker, PieceType::ROOK) & occupied;
    Bitboard queens = position.getBitboard(attacker, PieceType::QUEEN) & occupied;

    if (PAWN_ATTACKS[attacker == Color::WHITE ? 1 : 0][square] & pawns)
        return true;

    if (KNIGHT_ATTACKS[square] & knights)
        return true;

    if (KING_ATTACKS[square] & kings)
        return true;

    if (getBishopAttacks(square, occupied) & (bishops | queens))
        return true;

    if (getRookAttacks(square, occupied) & (rooks | queens))
        return true;

    return false;
}