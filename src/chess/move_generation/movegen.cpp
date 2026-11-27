#include "movegen.h"

#include "../position.h"
#include "attacks.h"
#include "../board.h"

int rookRayIndex(int direction)
{
    switch (direction)
    {
        case -1: return 0;
        case 1: return 1;
        case -8: return 2;
        case 8: return 3;
    }

    return -1;
}

int bishopRayIndex(int direction)
{
    switch (direction)
    {
        case -9: return 0;
        case 7: return 1;
        case -7: return 2;
        case 9: return 3;
    }

    return -1;
}

void calculatePins(MoveGenInfo& info, const Position& position)
{
    Color sideToMove = position.getSideToMove();
    Bitboard occupied = position.getOccupied();

    Bitboard firstBlockers = (Attacks::getRookAttacks(info.kingSquare, occupied) | Attacks::getBishopAttacks(info.kingSquare, occupied)) & occupied;

    int kingFile = info.kingSquare % 8;
    int kingRank = info.kingSquare / 8;

    while (firstBlockers)
    {
        unsigned long target;
        _BitScanForward64(&target, firstBlockers);

        // every bit is the square a blocker is on

        Piece blocker = position.getPiece(target);
        bool friendlyBlocker = Pieces::getColor(blocker) == sideToMove;

        if (friendlyBlocker)
        {
            // we need to figure out what direction we actually came from
            int blockerFile = target % 8;
            int blockerRank = target / 8;

            int fileStep = ((blockerFile - kingFile) > 0) - ((blockerFile - kingFile) < 0);
            int rankStep = ((blockerRank - kingRank) > 0) - ((blockerRank - kingRank) < 0);

            int direction = rankStep * 8 + fileStep;
            int absDirection = abs(direction);
            Bitboard rayMask;
            Bitboard nextBlocker;

            if (absDirection == 1 || absDirection == 8) 
            {
                rayMask = Attacks::ROOK_RAYS[target][rookRayIndex(direction)];
                nextBlocker = Attacks::getRookAttacks(target, occupied) & rayMask & occupied;
            }
            else
            {
                rayMask = Attacks::BISHOP_RAYS[target][bishopRayIndex(direction)];
                nextBlocker = Attacks::getBishopAttacks(target, occupied) & rayMask & occupied;
            }

            if (nextBlocker != 0)
            {
                unsigned long newTarget;

                if (direction > 0)
                    _BitScanForward64(&newTarget, nextBlocker);
                else
                    _BitScanReverse64(&newTarget, nextBlocker);

                Piece piece = position.getPiece(newTarget);
                PieceType pieceType = Pieces::getType(piece);
                bool isSlider;

                if (absDirection == 1 || absDirection == 8)
                    isSlider = (pieceType == PieceType::ROOK || pieceType == PieceType::QUEEN);
                else
                    isSlider = (pieceType == PieceType::BISHOP || pieceType == PieceType::QUEEN);

                bool enemyPinner = Pieces::getColor(piece) != sideToMove;

                if (enemyPinner && isSlider)
                {
                    // we need to include the square the pin is on, because we could take the piece that's pinning us
                    uint64_t pinRay = Attacks::BETWEEN[info.kingSquare][newTarget] | (1ULL << newTarget);
                    info.pinMasks[target] = pinRay;
                }
            }
        }
        
        firstBlockers &= firstBlockers - 1;
    }
}

void calculateKingAttackers(MoveGenInfo& info, const Position& position)
{
    Color sideToMove = position.getSideToMove();
    Color enemy = (sideToMove == Color::WHITE) ? Color::BLACK : Color::WHITE;

    Bitboard occupied = position.getOccupied();

    Bitboard kingMoves = Attacks::getKingAttacks(info.kingSquare) & ~position.getOccupied(sideToMove);

    while (kingMoves)
    {
        unsigned long target;
        _BitScanForward64(&target, kingMoves);

        Square square = static_cast<Square>(target);
        Bitboard targetBit = 1ULL << square;

        Bitboard fakeOccupancy = occupied & ~(1ULL << info.kingSquare);

        fakeOccupancy &= ~targetBit;

        if (Attacks::isSquareAttacked(position, square, enemy, fakeOccupancy))
        {
            info.kingAttackers |= targetBit;
        }

        kingMoves &= kingMoves - 1;
    }
}

MoveGenInfo MoveGenerator::calculateInfo(const Position& position)
{
    Color sideToMove = position.getSideToMove();
    Color enemy = (sideToMove == Color::WHITE) ? Color::BLACK : Color::WHITE;

    MoveGenInfo info;

    for (int i = 0; i < 64; ++i)
        info.pinMasks[i] = ~0ULL;

    unsigned long target;
    _BitScanForward64(&target, position.getBitboard(sideToMove, PieceType::KING));

    info.kingSquare = static_cast<int>(target);

    // ok now start this

    info.checkers = Attacks::getAttackers(position, info.kingSquare, enemy, position.getOccupied());
    int checkersAmount = __popcnt64(info.checkers);

    if (checkersAmount >= 2) info.evasionMask = 0;
    else if (checkersAmount == 1)
    {
        unsigned long target;
        _BitScanForward64(&target, info.checkers);
        int checkerSquare = static_cast<int>(target);

        info.evasionMask = 1ULL << checkerSquare;

        // now check if it's a slider

        PieceType checker = Pieces::getType(position.getPiece(checkerSquare));

        if (checker == PieceType::BISHOP || checker == PieceType::ROOK || checker == PieceType::QUEEN)
        {
            info.evasionMask |= Attacks::BETWEEN[info.kingSquare][checkerSquare];
        }
    }

    calculatePins(info, position);
    calculateKingAttackers(info, position);

    return info;
}

// ok now uh we do actual move gen stuff

void addToList(MoveList& moves, uint64_t possible, Square square)
{
    while (possible)
    {
        unsigned long target;
        _BitScanForward64(&target, possible);

        moves.append(MovePacker::encodeMove(square, target));
        
        possible &= possible - 1;
    }
}

void MoveGenerator::king(MoveList& moves, const Position& position, const MoveGenInfo& info, MoveGenType type)
{
    Square square = info.kingSquare;

    Color sideToMove = position.getSideToMove();
    Color enemy = (sideToMove == Color::WHITE) ? Color::BLACK : Color::WHITE;

    Bitboard occupied = position.getOccupied();
    Bitboard possible = Attacks::getKingAttacks(info.kingSquare) & ~position.getOccupied(sideToMove);
    possible &= ~info.kingAttackers;

    switch (type)
    {
        case MoveGenType::ALL: break;
        case MoveGenType::TACTICAL: possible &= position.getOccupied(enemy); break;
        case MoveGenType::QUIETS: possible &= ~occupied; break;
    }

    addToList(moves, possible, square);

    // if the king is in check, don't care about castling cuz you can't
    // castling is also classified as a quiet move, so if the movegen wants tactical we can ignore castling
    if (info.checkers != 0 || type == MoveGenType::TACTICAL) return;

    Square kingHome = (sideToMove == Color::WHITE) ? 4 : 60;
    Square rookKingSide = (sideToMove == Color::WHITE) ? 7 : 63;
    Square rookQueenSide = (sideToMove == Color::WHITE) ? 0 : 56;

    Bitboard kingSideSquares = (sideToMove == Color::WHITE) ? ((1ULL << 5) | (1ULL << 6)) : ((1ULL << 61) | (1ULL << 62));
    Bitboard queenSideSquares = (sideToMove == Color::WHITE) ? ((1ULL << 1) | (1ULL << 2) | (1ULL << 3)) : ((1ULL << 57) | (1ULL << 58) | (1ULL << 59));

    uint8_t kingSideRight = (sideToMove == Color::WHITE) ? WHITE_KINGSIDE : BLACK_KINGSIDE;
    uint8_t queenSideRight = (sideToMove == Color::WHITE) ? WHITE_QUEENSIDE : BLACK_QUEENSIDE;

    if (square != kingHome) return;

    if (position.canCastle(kingSideRight))
    {
        if ((occupied & kingSideSquares) == 0)
        {
            bool attacked = Attacks::isSquareAttacked(position, kingHome + 1, enemy, occupied) || Attacks::isSquareAttacked(position, kingHome + 2, enemy, occupied);

            Piece rook = position.getPiece(rookKingSide);

            if (!attacked && rook == Pieces::makePiece(sideToMove, PieceType::ROOK))
            {
                moves.append(MovePacker::encodeMove(
                    square,
                    square + 2,
                    MovePacker::Flag::CASTLE_KINGSIDE
                ));
            }
        }
    }

    if (position.canCastle(queenSideRight))
    {
        if ((occupied & queenSideSquares) == 0)
        {
            bool attacked = Attacks::isSquareAttacked(position, kingHome - 1, enemy, occupied) || Attacks::isSquareAttacked(position, kingHome - 2, enemy, occupied);

            Piece rook = position.getPiece(rookQueenSide);

            if (!attacked && rook == Pieces::makePiece(sideToMove, PieceType::ROOK))
            {
                moves.append(MovePacker::encodeMove(
                    square,
                    square - 2,
                    MovePacker::Flag::CASTLE_QUEENSIDE
                ));
            }
        }
    }
}

constexpr Bitboard RANK_2 = 0x000000000000FF00ULL;
constexpr Bitboard RANK_2_TO_7 = 0x00FFFFFFFFFFFF00ULL;
constexpr Bitboard RANK_7 = 0x00FF000000000000ULL;

void checkEnPassant(MoveList& moves, const Position& position, const MoveGenInfo& info, Square from)
{
    Color sideToMove = position.getSideToMove();
    Color enemy = (sideToMove == Color::WHITE) ? Color::BLACK : Color::WHITE;

    Square epSquare = position.getEnPassant();

    if (epSquare == NO_SQUARE) return;
    if (abs((from % 8) - (epSquare % 8)) != 1) return;
    if (from / 8 != epSquare / 8) return;

    Square target = static_cast<Square>(epSquare + (sideToMove == Color::WHITE ? 8 : -8));

    Bitboard occupancy = position.getOccupied();

    occupancy &= ~(1ULL << from); // remove moving pawn
    occupancy &= ~(1ULL << epSquare); // remove captured pawn
    occupancy |= (1ULL << target); // place pawn on destination

    if (!Attacks::isSquareAttacked(position, info.kingSquare, enemy, occupancy))
    {
        moves.append(MovePacker::encodeMove(from, target, MovePacker::Flag::EN_PASSANT));
    }
}

// ugh this one is so complicated
void MoveGenerator::pawn(MoveList& moves, const Position& position, const MoveGenInfo& info, MoveGenType type)
{
    Color sideToMove = position.getSideToMove();

    Bitboard enemy = position.getOccupied(sideToMove == Color::WHITE ? Color::BLACK : Color::WHITE);
    Bitboard occupied = position.getOccupied();
    Bitboard empty = ~occupied;

    Bitboard pawns = position.getBitboard(Pieces::makePiece(sideToMove, PieceType::PAWN));

    // exclude pawns that can promote for now. we operate on those separately
    Bitboard normalPawns = pawns & RANK_2_TO_7;

    int direction = (sideToMove == Color::WHITE) ? 8 : -8;
    int startingRank = (sideToMove == Color::WHITE) ? 1 : 6;

    while (normalPawns)
    {
        unsigned long target;
        _BitScanForward64(&target, normalPawns);

        Square square = static_cast<Square>(target);

        // quiet moves
        if (type != MoveGenType::TACTICAL)
        {
            int forward = square + direction;

            if (forward >= 0 && forward < 64 && (empty & (1ULL << forward)))
            {
                Bitboard possible = 1ULL << forward;

                int forwardTwo = square + direction * 2;

                // single push
                possible &= RANK_2_TO_7;
                possible &= info.evasionMask;
                possible &= info.pinMasks[square];

                if (possible)
                {
                    moves.append(MovePacker::encodeMove(square, forward));
                }

                // double push
                if (square / 8 == startingRank && (empty & (1ULL << forwardTwo)))
                {
                    Bitboard doublePossible = 1ULL << forwardTwo;

                    doublePossible &= RANK_2_TO_7;
                    doublePossible &= info.evasionMask;
                    doublePossible &= info.pinMasks[square];

                    if (doublePossible)
                    {
                        moves.append(MovePacker::encodeMove(square, forwardTwo, MovePacker::Flag::DOUBLE_PAWN_PUSH));
                    }
                }
            }
        }

        // tactical moves
        if (type != MoveGenType::QUIETS)
        {
            Bitboard possible = Attacks::getPawnAttacks(sideToMove, square) & enemy;

            possible &= RANK_2_TO_7;
            possible &= info.evasionMask;
            possible &= info.pinMasks[square];

            addToList(moves, possible, square);

            // en passant
            checkEnPassant(moves, position, info, square);
        }

        normalPawns &= normalPawns - 1;
    }

    if (type == MoveGenType::QUIETS) return;

    Bitboard promotionPawns = pawns & (sideToMove == Color::WHITE ? RANK_7 : RANK_2);

    const MovePacker::Flag promotions[4] = {
        MovePacker::Flag::KNIGHT,
        MovePacker::Flag::BISHOP,
        MovePacker::Flag::ROOK,
        MovePacker::Flag::QUEEN
    };

    while (promotionPawns)
    {
        unsigned long target;
        _BitScanForward64(&target, promotionPawns);

        Square square = static_cast<Square>(target);

        int forward = square + direction;

        Bitboard possible = 0;

        // quiet promotion
        if (forward >= 0 && forward < 64 && (empty & (1ULL << forward)))
        {
            possible |= 1ULL << forward;
        }
        // capture promotion
        possible |= Attacks::getPawnAttacks(sideToMove, square) & enemy;

        possible &= info.evasionMask;
        possible &= info.pinMasks[square];

        while (possible)
        {
            unsigned long destination;
            _BitScanForward64(&destination, possible);

            for (auto promotion : promotions)
            {
                moves.append(MovePacker::encodeMove(square, static_cast<Square>(destination), promotion));
            }

            possible &= possible - 1;
        }

        promotionPawns &= promotionPawns - 1;
    }
}

void MoveGenerator::knight(MoveList& moves, const Position& position, const MoveGenInfo& info, MoveGenType type)
{
    Color sideToMove = position.getSideToMove();

    Bitboard friendly = position.getOccupied(sideToMove);
    Bitboard enemy = position.getOccupied(sideToMove == Color::WHITE ? Color::BLACK : Color::WHITE);
    Bitboard knights = position.getBitboard(Pieces::makePiece(sideToMove, PieceType::KNIGHT));

    while (knights)
    {
        unsigned long target;
        _BitScanForward64(&target, knights);

        Square square = static_cast<Square>(target);

        Bitboard possible = Attacks::getKnightAttacks(square);

        switch (type)
        {
            case MoveGenType::TACTICAL: possible &= enemy; break;
            case MoveGenType::QUIETS: possible &= ~position.getOccupied(); break;
            case MoveGenType::ALL: possible &= ~friendly; break;
        }

        possible &= info.evasionMask;
        possible &= info.pinMasks[square];

        addToList(moves, possible, square);

        knights &= knights - 1;
    }
}

void MoveGenerator::bishop(MoveList& moves, const Position& position, const MoveGenInfo& info, MoveGenType type)
{
    Color sideToMove = position.getSideToMove();

    Bitboard friendly = position.getOccupied(sideToMove);
    Bitboard enemy = position.getOccupied(sideToMove == Color::WHITE ? Color::BLACK : Color::WHITE);
    Bitboard bishops = position.getBitboard(Pieces::makePiece(sideToMove, PieceType::BISHOP));

    while (bishops)
    {
        unsigned long target;
        _BitScanForward64(&target, bishops);

        Square square = static_cast<Square>(target);

        Bitboard possible = Attacks::getBishopAttacks(square, friendly | enemy);

        switch (type)
        {
            case MoveGenType::TACTICAL: possible &= enemy; break;
            case MoveGenType::QUIETS: possible &= ~position.getOccupied(); break;
            case MoveGenType::ALL: possible &= ~friendly; break;
        }

        possible &= info.evasionMask;
        possible &= info.pinMasks[square];

        addToList(moves, possible, square);

        bishops &= bishops - 1;
    }
}

void MoveGenerator::rook(MoveList& moves, const Position& position, const MoveGenInfo& info, MoveGenType type)
{
    Color sideToMove = position.getSideToMove();

    Bitboard friendly = position.getOccupied(sideToMove);
    Bitboard enemy = position.getOccupied(sideToMove == Color::WHITE ? Color::BLACK : Color::WHITE);
    Bitboard rooks = position.getBitboard(Pieces::makePiece(sideToMove, PieceType::ROOK));

    while (rooks)
    {
        unsigned long target;
        _BitScanForward64(&target, rooks);

        Square square = static_cast<Square>(target);

        Bitboard possible = Attacks::getRookAttacks(square, friendly | enemy);

        switch (type)
        {
            case MoveGenType::TACTICAL: possible &= enemy; break;
            case MoveGenType::QUIETS: possible &= ~position.getOccupied(); break;
            case MoveGenType::ALL: possible &= ~friendly; break;
        }

        possible &= info.evasionMask;
        possible &= info.pinMasks[square];

        addToList(moves, possible, square);

        rooks &= rooks - 1;
    }
}

void MoveGenerator::queen(MoveList& moves, const Position& position, const MoveGenInfo& info, MoveGenType type)
{
    Color sideToMove = position.getSideToMove();

    Bitboard friendly = position.getOccupied(sideToMove);
    Bitboard enemy = position.getOccupied(sideToMove == Color::WHITE ? Color::BLACK : Color::WHITE);
    Bitboard queens = position.getBitboard(Pieces::makePiece(sideToMove, PieceType::QUEEN));

    while (queens)
    {
        unsigned long target;
        _BitScanForward64(&target, queens);

        Square square = static_cast<Square>(target);

        Bitboard possible = Attacks::getRookAttacks(square, friendly | enemy) | Attacks::getBishopAttacks(square, friendly | enemy);

        switch (type)
        {
            case MoveGenType::TACTICAL: possible &= enemy; break;
            case MoveGenType::QUIETS: possible &= ~position.getOccupied(); break;
            case MoveGenType::ALL: possible &= ~friendly; break;
        }

        possible &= info.evasionMask;
        possible &= info.pinMasks[square];

        addToList(moves, possible, square);

        queens &= queens - 1;
    }
}

// and then the generate everything function

void MoveGenerator::generateLegalMoves(const Position& position, MoveGenInfo info, MoveList& moves, MoveGenType type)
{
    moves.count = 0;

    if (info.checkerCount() >= 2)
    {
        king(moves, position, info, type);
        return;
    }

    king(moves, position, info, type);
    pawn(moves, position, info, type);
    knight(moves, position, info, type);
    bishop(moves, position, info, type);
    rook(moves, position, info, type);
    queen(moves, position, info, type);
}

// this function is really big
bool MoveGenerator::isLegal(const Position& position, MoveGenInfo info, Move move)
{
    Color sideToMove = position.getSideToMove();
    Color enemy = sideToMove == Color::WHITE ? Color::BLACK : Color::WHITE;

    Square start = MovePacker::getStart(move);
    Square end = MovePacker::getEnd(move);

    auto flag = MovePacker::getFlag(move);

    if (start >= 64 || end >= 64 || start == end)
        return false;

    Piece movingPiece = position.getPiece(start);
    Piece targetPiece = position.getPiece(end);

    if (movingPiece == Pieces::EMPTY) return false;
    if (Pieces::getColor(movingPiece) != sideToMove) return false;
    if (targetPiece != Pieces::EMPTY && Pieces::getColor(targetPiece) == sideToMove) return false;
    if (targetPiece != Pieces::EMPTY && Pieces::getType(targetPiece) == PieceType::KING) return false;

    PieceType pieceType = Pieces::getType(movingPiece);

    Bitboard destination = 1ULL << end;
    Bitboard occupied = position.getOccupied();

    if (flag == MovePacker::Flag::EN_PASSANT)
    {
        if (pieceType != PieceType::PAWN)
            return false;

        Square epSquare = position.getEnPassant();

        if (epSquare == NO_SQUARE) return false;
        if (abs(static_cast<int>(start % 8) - static_cast<int>(epSquare % 8)) != 1) return false;
        if (start / 8 != epSquare / 8) return false;

        Square expectedTarget = static_cast<Square>(epSquare + (sideToMove == Color::WHITE ? 8 : -8));

        if (end != expectedTarget) return false;

        if (position.getPiece(end) != Pieces::EMPTY) return false;
        if (position.getPiece(epSquare) != Pieces::makePiece(enemy, PieceType::PAWN)) return false;

        Bitboard newOccupancy = occupied;

        newOccupancy &= ~(1ULL << start);
        newOccupancy &= ~(1ULL << epSquare);
        newOccupancy |= (1ULL << end);

        return !Attacks::isSquareAttacked( position, info.kingSquare, enemy, newOccupancy);
    }

    if (flag == MovePacker::Flag::CASTLE_KINGSIDE || flag == MovePacker::Flag::CASTLE_QUEENSIDE)
    {
        if (pieceType != PieceType::KING)
            return false;

        if (info.checkerCount() > 0)
            return false;

        Square kingHome = sideToMove == Color::WHITE ? 4 : 60;

        if (start != kingHome)
            return false;

        bool kingSide = flag == MovePacker::Flag::CASTLE_KINGSIDE;

        Square expectedEnd = static_cast<Square>(kingHome + (kingSide ? 2 : -2));

        if (end != expectedEnd)
            return false;

        uint8_t right;

        if (sideToMove == Color::WHITE)
        {
            right = kingSide ? WHITE_KINGSIDE : WHITE_QUEENSIDE;
        }
        else
        {
            right = kingSide ? BLACK_KINGSIDE : BLACK_QUEENSIDE;
        }

        if (!position.canCastle(right))
            return false;

        Square rookSquare;

        Bitboard emptySquares;
        Square transitSquare;
        Square destinationSquare;

        if (kingSide)
        {
            rookSquare = static_cast<Square>(kingHome + 3);

            emptySquares = (1ULL << (kingHome + 1)) | (1ULL << (kingHome + 2));

            transitSquare = static_cast<Square>(kingHome + 1);
            destinationSquare = static_cast<Square>(kingHome + 2);
        }
        else
        {
            rookSquare = static_cast<Square>(kingHome - 4);

            emptySquares = (1ULL << (kingHome - 1)) | (1ULL << (kingHome - 2)) | (1ULL << (kingHome - 3));

            transitSquare = static_cast<Square>(kingHome - 1);
            destinationSquare = static_cast<Square>(kingHome - 2);
        }

        if (occupied & emptySquares) return false;
        if (position.getPiece(rookSquare) != Pieces::makePiece(sideToMove, PieceType::ROOK)) return false;

        // king can't cross or land on an attacked square.
        if (Attacks::isSquareAttacked(position, transitSquare, enemy, occupied)) return false;
        if (Attacks::isSquareAttacked(position, destinationSquare, enemy, occupied)) return false;

        return true;
    }

    if (flag == MovePacker::Flag::DOUBLE_PAWN_PUSH && pieceType != PieceType::PAWN)
        return false;

    if (MovePacker::isPromotion(flag) && pieceType != PieceType::PAWN)
        return false;

    if (pieceType == PieceType::PAWN)
    {
        int direction = sideToMove == Color::WHITE ? 8 : -8;
        int startingRank = sideToMove == Color::WHITE ? 1 : 6;
        int promotionRank = sideToMove == Color::WHITE ? 7 : 0;

        int delta = static_cast<int>(end) - static_cast<int>(start);

        int fileDifference = abs(static_cast<int>(end % 8) - static_cast<int>(start % 8));

        bool capture = targetPiece != Pieces::EMPTY;
        bool promotion = MovePacker::isPromotion(flag);

        if (promotion)
        {
            if (flag != MovePacker::Flag::KNIGHT &&
                flag != MovePacker::Flag::BISHOP &&
                flag != MovePacker::Flag::ROOK &&
                flag != MovePacker::Flag::QUEEN)
                return false;

            if (end / 8 != promotionRank)
                return false;
        }
        else if (end / 8 == promotionRank)
        {
            return false;
        }

        if (flag == MovePacker::Flag::DOUBLE_PAWN_PUSH)
        {
            if (promotion) return false;
            if (start / 8 != startingRank) return false;
            if (delta != direction * 2) return false;

            Square middle = static_cast<Square>(static_cast<int>(start) + direction);

            if (position.getPiece(middle) != Pieces::EMPTY || targetPiece != Pieces::EMPTY) return false;
        }
        else if (capture)
        {
            if (fileDifference != 1) return false;
            if (delta != direction - 1 && delta != direction + 1) return false;
        }
        else
        {
            if (fileDifference != 0) return false;
            if (delta != direction) return false;
            if (targetPiece != Pieces::EMPTY) return false;
        }
    }
    else
    {
        Bitboard possible = 0;

        switch (pieceType)
        {
            case PieceType::KNIGHT: possible = Attacks::getKnightAttacks(start); break;
            case PieceType::BISHOP: possible = Attacks::getBishopAttacks(start, occupied); break;
            case PieceType::ROOK: possible = Attacks::getRookAttacks(start, occupied); break;
            case PieceType::QUEEN: possible = Attacks::getRookAttacks(start, occupied) | Attacks::getBishopAttacks(start, occupied); break;
            case PieceType::KING:
                possible = Attacks::getKingAttacks(start);
                if (info.kingAttackers & destination) return false;
                break;
            default:
                return false;
        }

        if (!(possible & destination))
            return false;
    }

    // en passant / pin validation

    if (pieceType != PieceType::KING)
    {
        if (!(info.evasionMask & destination)) return false;
        if (!(info.pinMasks[start] & destination)) return false;
    }

    return true;
}