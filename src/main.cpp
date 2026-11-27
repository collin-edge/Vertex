#include <iostream>
#include <string>

#include "uci/uci.h"
#include "perft.h"
#include "chess/position.h"
#include "chess/zobrist.h"
#include "chess/move_generation/attacks.h"
#include "chess/engine/objects/search_stack.h"
#include <string>

Position position;
UCI uci(position);

std::string sts(Square square)
{
    char file = 'a' + (static_cast<int>(square) % 8);
    char rank = '1' + (static_cast<int>(square) / 8);

    return std::string{file, rank};
}

std::string mtu(Move move)
{
    Square start = MovePacker::getStart(move);
    Square end = MovePacker::getEnd(move);

    std::string result;

    result += sts(start);
    result += sts(end);

    auto flag = MovePacker::getFlag(move);

    if (MovePacker::isPromotion(flag))
    {
        switch (MovePacker::promotedPiece(flag))
        {
            case PieceType::KNIGHT: result += 'n'; break;
            case PieceType::BISHOP: result += 'b'; break;
            case PieceType::ROOK: result += 'r'; break;
            case PieceType::QUEEN: result += 'q'; break;
            default: break;
        }
    }

    return result;
}


int main()
{
    std::string line;

    Zobrist::generate_keys();
    Attacks::buildTables();

    // position startpos moves e2e4 g8f6 b1c3 f6e4 c3e4 g7g6 d2d3

    while (std::getline(std::cin, line))
    {
        if (!uci.receiveCommand(line))
            break;
    }

    return 0;
}