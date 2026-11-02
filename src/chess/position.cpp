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

void Position::clearCastlingRights(uint8_t rights)
{
    zobristKey ^= Zobrist::CASTLING_RIGHTS[castlingRights];
    castlingRights ^= rights;
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

void Position::updateCastlingRights(Square start, Square end, Piece moving, Piece captured = Pieces::EMPTY)
{
    if (Pieces::getType(moving) == PieceType::KING)
    {
        if (sideToMove == Color::WHITE)
            clearCastlingRights(WHITE_KINGSIDE | WHITE_QUEENSIDE);
        else
            clearCastlingRights(BLACK_KINGSIDE | BLACK_QUEENSIDE);
    }

    if (Pieces::getType(captured) == PieceType::ROOK)
    {
        if (end == 0) clearCastlingRights(WHITE_QUEENSIDE);
        if (end == 7) clearCastlingRights(WHITE_KINGSIDE);
        if (end == 56) clearCastlingRights(BLACK_QUEENSIDE);
        if (end == 63) clearCastlingRights(BLACK_KINGSIDE);
    }

    if (Pieces::getType(moving) == PieceType::ROOK)
    {
        if (start == 0) clearCastlingRights(WHITE_QUEENSIDE);
        if (start == 7) clearCastlingRights(WHITE_KINGSIDE);
        if (start == 56) clearCastlingRights(BLACK_QUEENSIDE);
        if (start == 63) clearCastlingRights(BLACK_KINGSIDE);
    }
}

UndoInfo Position::makeMove(Move m)
{
    UndoInfo undo;
    undo.move = m;
    undo.capturedPiece = Pieces::EMPTY;
    undo.castlingRights = castlingRights;
    undo.enPassant = enPassant;
    undo.halfmoveClock = halfmoveClock;
    undo.fullmoveNumber = fullmoveNumber;
    undo.zobristKey = zobristKey;

    setEnPassant(NO_SQUARE);

    Square start = MovePacker::getStart(m);
    Square end = MovePacker::getEnd(m);
    MovePacker::Flag flag = MovePacker::getFlag(m);
    Piece movingPiece = getPiece(start);
    Piece captured = getPiece(end);

    if (captured)
    {
        undo.capturedPiece = captured;
        remove(end);
    }

    move(start, end);

    if (MovePacker::isPromotion(flag))
    {
        remove(end);
        Piece promoPiece = Pieces::makePiece(sideToMove, MovePacker::promotedPiece(flag));
        add(promoPiece, end);
    } else {
        switch (flag)
        {
            case MovePacker::Flag::DOUBLE_PAWN_PUSH:
                setEnPassant(end);
                break;
            case MovePacker::Flag::EN_PASSANT:
                Square capturedSquare = end + (sideToMove == Color::WHITE ? -8 : 8);
                undo.capturedPiece = getPiece(capturedSquare);
                remove(capturedSquare);

                break;
            case MovePacker::Flag::CASTLE_KINGSIDE:
                if (sideToMove == Color::WHITE)
                    move(7, end - 1);
                else
                    move(63, end - 1);

                break;
            case MovePacker::Flag::CASTLE_QUEENSIDE:
                if (sideToMove == Color::WHITE)
                    move(0, end + 1);
                else
                    move(56, end + 1);

                break;
        }
    }

    if (Pieces::getType(movingPiece) == PieceType::PAWN || captured)
        halfmoveClock = 0;
    else
        ++halfmoveClock;

    updateCastlingRights(start, end, captured);
    switchTurn();

    return undo;
}

void Position::undoMove(UndoInfo undo)
{
    Square start = MovePacker::getStart(undo.move);
    Square end = MovePacker::getEnd(undo.move);
    MovePacker::Flag flag = MovePacker::getFlag(undo.move);

    switchTurn();

    // undo special moves first
    switch (flag)
    {
        case MovePacker::Flag::EN_PASSANT:
            add(undo.capturedPiece, end + (sideToMove == Color::WHITE ? -8 : 8));
            break;
        case MovePacker::Flag::CASTLE_KINGSIDE:
            if (sideToMove == Color::WHITE)
                move(end - 1, 7);
            else
                move(end - 1, 63);
            break;
        case MovePacker::Flag::CASTLE_QUEENSIDE:
            if (sideToMove == Color::WHITE)
                move(end + 1, 0);
            else
                move(end + 1, 56);
            break;
    }

    // undo main move
    if (MovePacker::isPromotion(flag))
    {
        remove(end);
        Piece pawn = Pieces::makePiece(sideToMove, PieceType::PAWN);
        add(pawn, start);
    }
    else
    {
        move(end, start);
    }

    // undo captures
    if (undo.capturedPiece != Pieces::EMPTY && flag != MovePacker::Flag::EN_PASSANT)
        add(undo.capturedPiece, end);

    zobristKey = undo.zobristKey; // i mean technically theoretically this would completely restore the old hash, but just as a safeguard
    castlingRights = undo.castlingRights;
    enPassant = undo.enPassant;
    halfmoveClock = undo.halfmoveClock;
    fullmoveNumber = undo.fullmoveNumber;
}