#include "move_ordering.h"

int MoveOrdering::scoreTactical(Move move, const Position& position)
{
    // using MVV-LVA, a pawn capturing a queen is better than a queen capturing a pawn
    // formula is victimScore * 10 - attackerScore

    Square from = MovePacker::getStart(move);
    Square to = MovePacker::getEnd(move);
    auto flag = MovePacker::getFlag(move);

    Piece attacker = position.getPiece(from);
    Piece victim = position.getPiece(to);

    int captureScore = 0;

    if (flag == MovePacker::Flag::EN_PASSANT)
    {
        int capturedSquare = to + (position.getSideToMove() == Color::WHITE ? -8 : 8);
        victim = position.getPiece(static_cast<Square>(capturedSquare));
    }

    if (victim != Pieces::EMPTY)
    {
        int attackerScore = pieceValue(Pieces::getType(attacker));
        int victimScore = pieceValue(Pieces::getType(victim));

        captureScore = victimScore * 10 - attackerScore;
    }

    if (MovePacker::isPromotion(flag)) return 90000 + captureScore;
    return captureScore;
}

int MoveOrdering::see(Move move, const Position& position)
{
    Square from = MovePacker::getStart(move);
    Square to = MovePacker::getEnd(move);

    Piece attacker = position.getPiece(from);
    Piece victim = position.getPiece(to);

    Color side = position.getSideToMove();

    auto flag = MovePacker::getFlag(move);

    Bitboard occupied = position.getOccupied();

    if (flag == MovePacker::Flag::EN_PASSANT)
    {
        Square capturedSquare = static_cast<Square>(to + (side == Color::WHITE ? -8 : 8));

        victim = position.getPiece(capturedSquare);
        occupied &= ~(1ULL << capturedSquare);
    }

    occupied &= ~(1ULL << from);
    occupied |= (1ULL << to);

    int gain[32]{};
    int depth = 0;

    gain[0] = pieceValue(Pieces::getType(victim));

    if (MovePacker::isPromotion(flag))
    {
        PieceType promotionType;

        switch (flag)
        {
            case MovePacker::Flag::KNIGHT: promotionType = PieceType::KNIGHT; break;
            case MovePacker::Flag::BISHOP: promotionType = PieceType::BISHOP; break;
            case MovePacker::Flag::ROOK: promotionType = PieceType::ROOK; break;
            default: promotionType = PieceType::QUEEN; break;
        }

        gain[0] += pieceValue(promotionType) - pieceValue(PieceType::PAWN);
    }

    PieceType currentVictim = Pieces::getType(attacker);

    Color sideToMove = (side == Color::WHITE) ? Color::BLACK : Color::WHITE;

    while (depth < 30)
    {
        Bitboard attackers = Attacks::getAttackers(position, to, sideToMove, occupied);

        attackers &= occupied;

        if (!attackers)
            break;

        Square attackerSquare = NO_SQUARE;
        PieceType attackerType = PieceType::KING;

        for (int type = 0; type < 6; ++type)
        {
            PieceType pieceType = static_cast<PieceType>(type);

            Bitboard candidates = attackers & position.getBitboard(Pieces::makePiece(sideToMove, pieceType)) & occupied;

            if (candidates)
            {
                unsigned long square;
                _BitScanForward64(&square, candidates);

                attackerSquare = static_cast<Square>(square);
                attackerType = pieceType;
                break;
            }
        }

        if (attackerSquare == NO_SQUARE)
            break;

        ++depth;

        gain[depth] = pieceValue(currentVictim) - gain[depth - 1];

        currentVictim = attackerType;

        occupied &= ~(1ULL << attackerSquare);

        sideToMove = (sideToMove == Color::WHITE) ? Color::BLACK : Color::WHITE;
    }

    while (depth > 0)
    {
        gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);
        --depth;
    }

    return gain[0];
}

int MoveOrdering::capturedPieceValue(Move move, const Position& position)
{
    Square to = MovePacker::getEnd(move);

    auto flag = MovePacker::getFlag(move);

    if (flag == MovePacker::Flag::EN_PASSANT)
    {
        Square capturedSquare = static_cast<Square>(to + (position.getSideToMove() == Color::WHITE ? -8 : 8));

        return pieceValue(Pieces::getType(position.getPiece(capturedSquare)));
    }

    Piece victim = position.getPiece(to);

    if (victim == Pieces::EMPTY)
        return 0;

    return pieceValue(Pieces::getType(victim));
}
