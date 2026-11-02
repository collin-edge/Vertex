#include "position.h"

void Position::add(Piece piece, Square square)
{
    board.add(piece, square);

    zobristKey ^= Zobrist::PIECE_KEYS[Pieces::toBitboardIndex(piece)][square];
}

void Position::remove(Square square)
{
    Piece piece = board.get(square);

    if (piece == Pieces::EMPTY)
        return;

    board.remove(square);

    zobristKey ^= Zobrist::PIECE_KEYS[Pieces::toBitboardIndex(piece)][square];
}

void Position::move(Square start, Square end)
{
    Piece piece = board.get(start);

    if (piece == Pieces::EMPTY || start == end)
        return;

    remove(start);
    add(piece, end);
}

void Position::switchTurn()
{
    zobristKey ^= Zobrist::SIDE_TO_MOVE;
    sideToMove = (sideToMove == Color::WHITE) ? Color::BLACK : Color::WHITE;
}

void Position::clearCastlingRights(uint8_t right)
{
    zobristKey ^= Zobrist::CASTLING_RIGHTS[castlingRights];
    castlingRights ^= right;
    zobristKey ^= Zobrist::CASTLING_RIGHTS[castlingRights];
}

void Position::setEnPassant(Square square)
{
    if (enPassant != NO_SQUARE) zobristKey ^= Zobrist::EN_PASSANT_FILE[enPassant % 8];
    enPassant = square;
    if (enPassant != NO_SQUARE) zobristKey ^= Zobrist::EN_PASSANT_FILE[enPassant % 8];
}

void Position::clearEnPassant()
{
    if (enPassant != NO_SQUARE) zobristKey ^= Zobrist::EN_PASSANT_FILE[enPassant % 8];
    enPassant = NO_SQUARE;
}