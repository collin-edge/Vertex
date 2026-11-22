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

void MoveGenerator::king(MoveList& moves, const Position& position, const MoveGenInfo& info)
{
    Square square = info.kingSquare;
    Color sideToMove = position.getSideToMove();
    Color enemy = (sideToMove == Color::WHITE) ? Color::BLACK : Color::WHITE;

    Bitboard occupied = position.getOccupied();
    Bitboard possible = Attacks::getKingAttacks(info.kingSquare) & ~position.getOccupied(sideToMove);
    possible &= ~info.kingAttackers;

    addToList(moves, possible, square);

    if (info.checkers != 0) return; // if the king is in check, don't care about castling cuz you can't

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

void MoveGenerator::pawn(MoveList& moves, const Position& position, const MoveGenInfo& info)
{
    Color sideToMove = position.getSideToMove();
    Color enemy = (sideToMove == Color::WHITE) ? Color::BLACK : Color::WHITE;

    Bitboard occupied = position.getOccupied();
    Bitboard empty = ~occupied;
    Bitboard pawns = position.getBitboard(Pieces::makePiece(sideToMove, PieceType::PAWN));

    int direction = (sideToMove == Color::WHITE) ? 8 : -8;
    int startingRank = (sideToMove == Color::WHITE) ? 1 : 6;
    int promotionRank = (sideToMove == Color::WHITE) ? 7 : 0;

    while (pawns)
    {
        unsigned long target;
        _BitScanForward64(&target, pawns);

        Square square = static_cast<Square>(target);

        Bitboard possible = 0;

        int forward = square + direction;

        if (forward >= 0 && forward < 64 && (empty & (1ULL << forward)))
        {
            possible |= 1ULL << forward;

            int forwardTwo = square + direction * 2;

            if (square / 8 == startingRank && (empty & (1ULL << forwardTwo)))
            {
                possible |= 1ULL << forwardTwo;
            }
        }

        possible |= Attacks::getPawnAttacks(sideToMove, square) & position.getOccupied(enemy);

        possible &= info.evasionMask;
        possible &= info.pinMasks[square];

        while (possible)
        {
            unsigned long destination;
            _BitScanForward64(&destination, possible);

            Square end = static_cast<Square>(destination);

            if (end / 8 == promotionRank)
            {
                const MovePacker::Flag promotions[4] = {
                    MovePacker::Flag::KNIGHT,
                    MovePacker::Flag::BISHOP,
                    MovePacker::Flag::ROOK,
                    MovePacker::Flag::QUEEN
                };

                for (int i = 0; i < 4; ++i)
                {
                    moves.append(MovePacker::encodeMove(square, end, promotions[i]));
                }
            }
            else
            {
                MovePacker::Flag flag = (abs(end - square) == 16) ? MovePacker::Flag::DOUBLE_PAWN_PUSH : MovePacker::Flag::NONE;

                moves.append(MovePacker::encodeMove(square, end, flag));
            }

            possible &= possible - 1;
        }

        checkEnPassant(moves, position, info, square);

        pawns &= pawns - 1;
    }
}

void MoveGenerator::knight(MoveList& moves, const Position& position, const MoveGenInfo& info)
{
    Color sideToMove = position.getSideToMove();

    Bitboard occupied = position.getOccupied(sideToMove);
    Bitboard knights = position.getBitboard(Pieces::makePiece(sideToMove, PieceType::KNIGHT));

    while (knights)
    {
        unsigned long target;
        _BitScanForward64(&target, knights);

        Square square = static_cast<Square>(target);

        Bitboard possible = Attacks::getKnightAttacks(square) & ~occupied;
        possible &= info.evasionMask;
        possible &= info.pinMasks[square];

        addToList(moves, possible, square);

        knights &= knights - 1;
    }
}

void MoveGenerator::bishop(MoveList& moves, const Position& position, const MoveGenInfo& info)
{
    Color sideToMove = position.getSideToMove();

    Bitboard occupied = position.getOccupied();
    Bitboard bishops = position.getBitboard(Pieces::makePiece(sideToMove, PieceType::BISHOP));

    while (bishops)
    {
        unsigned long target;
        _BitScanForward64(&target, bishops);

        Square square = static_cast<Square>(target);

        Bitboard possible = Attacks::getBishopAttacks(square, occupied);
        possible &= ~position.getOccupied(sideToMove);
        possible &= info.evasionMask;
        possible &= info.pinMasks[square];

        addToList(moves, possible, square);

        bishops &= bishops - 1;
    }
}

void MoveGenerator::rook(MoveList& moves, const Position& position, const MoveGenInfo& info)
{
    Color sideToMove = position.getSideToMove();

    Bitboard occupied = position.getOccupied();
    Bitboard rooks = position.getBitboard(Pieces::makePiece(sideToMove, PieceType::ROOK));

    while (rooks)
    {
        unsigned long target;
        _BitScanForward64(&target, rooks);

        Square square = static_cast<Square>(target);

        Bitboard possible = Attacks::getRookAttacks(square, occupied);
        possible &= ~position.getOccupied(sideToMove);
        possible &= info.evasionMask;
        possible &= info.pinMasks[square];

        addToList(moves, possible, square);

        rooks &= rooks - 1;
    }
}

void MoveGenerator::queen(MoveList& moves, const Position& position, const MoveGenInfo& info)
{
    Color sideToMove = position.getSideToMove();

    Bitboard occupied = position.getOccupied();
    Bitboard queens = position.getBitboard(Pieces::makePiece(sideToMove, PieceType::QUEEN));

    while (queens)
    {
        unsigned long target;
        _BitScanForward64(&target, queens);

        Square square = static_cast<Square>(target);

        Bitboard possible = Attacks::getRookAttacks(square, occupied) | Attacks::getBishopAttacks(square, occupied);
        possible &= ~position.getOccupied(sideToMove);
        possible &= info.evasionMask;
        possible &= info.pinMasks[square];

        addToList(moves, possible, square);

        queens &= queens - 1;
    }
}

// and then the generate everything function

MoveList MoveGenerator::generateLegalMoves(const Position& position)
{
    MoveList moves;

    MoveGenInfo info = calculateInfo(position);

    if (info.checkerCount() >= 2)
    {
        king(moves, position, info);
        return moves;
    }

    king(moves, position, info);
    pawn(moves, position, info);
    knight(moves, position, info);
    bishop(moves, position, info);
    rook(moves, position, info);
    queen(moves, position, info);

    return moves;
}