#include "board.h"

void Board::reset()
{
    mailbox = {};
    bitboards = {};
    occupied = {};
}

void Board::add(Piece piece, Square square)
{
    if (mailbox[square] != Pieces::EMPTY)
        remove(square);

    mailbox[square] = piece;

    int idx = Pieces::toBitboardIndex(piece);

    bitboards[idx] |= (1ULL << square);
    occupied[idx / 6] |= (1ULL << square);
}

void Board::remove(Square square)
{
    Piece piece = mailbox[square];

    if (piece == Pieces::EMPTY)
        return;

    mailbox[square] = Pieces::EMPTY;

    int idx = Pieces::toBitboardIndex(piece);

    bitboards[idx] &= ~(1ULL << square);
    occupied[idx / 6] &= ~(1ULL << square);
}