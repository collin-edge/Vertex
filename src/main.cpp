#include <iostream>
#include <string>

#include "uci/uci.h"
#include "chess/position.h"
#include "chess/zobrist.h"
#include "chess/move_generation/attacks.h"

Position position;

int main()
{
    std::string line;

    Zobrist::generate_keys();
    Attacks::buildTables();

    position.constructPosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    while (std::getline(std::cin, line))
    {
        if (!UCI::recieveCommand(line))
            break;
    }

    return 0;
}