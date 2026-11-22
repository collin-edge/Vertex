#pragma once

#include "types.h"
#include "board.h"
#include <sstream>
#include <string_view>

struct UndoInfo
{
    Move move;
    Piece capturedPiece;

    uint8_t castlingRights;
    Square enPassant;

    int halfmoveClock;
    int fullmoveNumber;

    uint64_t zobristKey;
};

struct Position
{
private:
    Board board;

    // metadata
    Color sideToMove = Color::WHITE;
    uint8_t castlingRights = 0;
    Square enPassant = NO_SQUARE;

    int halfmoveClock = 0;
    int fullmoveNumber = 1;

    uint64_t zobristKey = 0;

    void add(Piece piece, Square square);
    void remove(Square square);
    void move(Square start, Square end);

    void switchTurn();
    void clearCastlingRights(uint8_t rights);

    void setEnPassant(Square square);
    void clearEnPassant();

    void incrementHalfmoves()
    {
        ++halfmoveClock;
    }

    void incrementFullmoves()
    {
        ++fullmoveNumber;
    }

    void updateCastlingRights(Square start, Square end, Piece moving, Piece captured = Pieces::EMPTY);

public:
    bool constructPosition(std::string_view fen);

    UndoInfo makeMove(Move m);
    void undoMove(UndoInfo undo);

    // a whole bunch of getters i'm sorry

    // board
    Piece getPiece(Square square) const
    {
        return board.get(square);
    }

    Bitboard getBitboard(Piece piece) const
    {
        return board.getBitboard(piece);
    }

    Bitboard getBitboard(Color color, PieceType type) const
    {
        return board.getBitboard(color, type);
    }

    Bitboard getOccupied(Color color) const
    {
        return board.getOccupied(color);
    }

    Bitboard getOccupied() const
    {
        return board.getOccupied();
    }

    Bitboard getEmpty() const
    {
        return board.getEmpty();
    }

    // position metadata
    Color getSideToMove() const
    {
        return sideToMove;
    }

    uint8_t getCastlingRights() const
    {
        return castlingRights;
    }

    bool canCastle(uint8_t right) const
    {
        return (castlingRights & right) != 0;
    }

    Square getEnPassant() const
    {
        return enPassant;
    }

    int getHalfmoveClock() const
    {
        return halfmoveClock;
    }

    int getFullmoveNumber() const
    {
        return fullmoveNumber;
    }

    uint64_t getZobristKey() const
    {
        return zobristKey;
    }
};