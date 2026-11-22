#include "position.h"

#include "zobrist.h"
#include "moves.h"
#include "move_generation/movegen.h"
#include <charconv>

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
    castlingRights &= ~rights;
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

void Position::updateCastlingRights(Square start, Square end, Piece moving, Piece captured)
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

std::string_view next_field(std::string_view& input)
{
    input.remove_prefix(std::min(input.find_first_not_of(' '), input.size()));

    size_t end = input.find(' ');

    auto field = input.substr(0, end);

    if (end == std::string_view::npos)
        input = {};
    else
        input.remove_prefix(end + 1);

    return field;
}

bool parseInt(std::string_view str, int& result)
{
    auto[ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), result);

    return ec == std::errc{} && ptr == str.data() + str.size();
}

bool Position::constructPosition(std::string_view fen)
{
    std::string_view pieces = next_field(fen);
    std::string_view side = next_field(fen);
    std::string_view castling = next_field(fen);
    std::string_view ep = next_field(fen);
    std::string_view halfmove = next_field(fen);
    std::string_view fullmove = next_field(fen);

    if (pieces.empty() || side.empty() || castling.empty() || ep.empty() || halfmove.empty() || fullmove.empty() || !fen.empty())
    {
        return false;
    }

    int file = 0;
    int rank = 7;

    int whiteKings = 0;
    int blackKings = 0;

    for (int i = 0; i < pieces.size(); ++i)
    {
        if (pieces[i] == '/')
        {
            if (file != 8)
                return false;

            file = 0;

            if (rank == 0) 
                return false;

            --rank;
        }
        else if (std::isdigit(static_cast<unsigned char>(pieces[i])))
        {
            int dig = pieces[i] - '0';
            if (dig < 1 || dig > 8 || file + dig > 8) return false;
            file += dig;
        }
        else
        {
            Piece toadd;
            switch(pieces[i])
            {
                case 'K': toadd = Pieces::makePiece(Color::WHITE, PieceType::KING); break;
                case 'P': toadd = Pieces::makePiece(Color::WHITE, PieceType::PAWN); break;
                case 'N': toadd = Pieces::makePiece(Color::WHITE, PieceType::KNIGHT); break;
                case 'B': toadd = Pieces::makePiece(Color::WHITE, PieceType::BISHOP); break;
                case 'R': toadd = Pieces::makePiece(Color::WHITE, PieceType::ROOK); break;
                case 'Q': toadd = Pieces::makePiece(Color::WHITE, PieceType::QUEEN); break;
                case 'k': toadd = Pieces::makePiece(Color::BLACK, PieceType::KING); break;
                case 'p': toadd = Pieces::makePiece(Color::BLACK, PieceType::PAWN); break;
                case 'n': toadd = Pieces::makePiece(Color::BLACK, PieceType::KNIGHT); break;
                case 'b': toadd = Pieces::makePiece(Color::BLACK, PieceType::BISHOP); break;
                case 'r': toadd = Pieces::makePiece(Color::BLACK, PieceType::ROOK); break;
                case 'q': toadd = Pieces::makePiece(Color::BLACK, PieceType::QUEEN); break;
                default: return false;
            }

            if (toadd == Pieces::makePiece(Color::WHITE, PieceType::KING)) ++whiteKings;
            if (toadd == Pieces::makePiece(Color::BLACK, PieceType::KING)) ++blackKings;

            add(toadd, rank * 8 + file);
            if (file > 7) return false;
            ++file;
        }
    }

    if (whiteKings != 1 || blackKings != 1) return false;

    if (side == "w" || side == "b")
        sideToMove = (side == "w") ? Color::WHITE : Color::BLACK;
    else 
        return false;

    if (castling != "-")
    {
        int previousPriority = 0;

        for (char c : castling)
        {
            int priority = 0;
            switch (c)
            {
                case 'K': priority = 1; break;
                case 'Q': priority = 2; break;
                case 'k': priority = 3; break;
                case 'q': priority = 4; break;
            }

            if (priority == 0) return false;
            if (priority <= previousPriority) return false;

            previousPriority = priority;
        }

        // now that it'd a valid castling string

        for (char c : castling)
        {
            switch (c)
            {
                case 'K': castlingRights |= WHITE_KINGSIDE; break;
                case 'Q': castlingRights |= WHITE_QUEENSIDE; break;
                case 'k': castlingRights |= BLACK_KINGSIDE; break;
                case 'q': castlingRights |= BLACK_QUEENSIDE; break;
            }
        }
    }

    if (ep != "-")
    {
        if (ep.size() != 2)
            return false;

        int file = ep[0] - 'a';
        int rank = ep[1] - '1';

        if (file >= 0 && file < 8 && (rank == 2 || rank == 5))
        {
            enPassant = rank * 8 + file;
        } else {
            return false;
        }
    }

    if (!parseInt(halfmove, halfmoveClock))
        return false;

    if (!parseInt(fullmove, fullmoveNumber))
        return false;

    if (halfmoveClock < 0 || fullmoveNumber < 1)
        return false;

    return true;
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
            {
                Square capturedSquare = end + (sideToMove == Color::WHITE ? -8 : 8);
                undo.capturedPiece = getPiece(capturedSquare);
                remove(capturedSquare);

                break;
            }
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

    updateCastlingRights(start, end, movingPiece, captured);
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