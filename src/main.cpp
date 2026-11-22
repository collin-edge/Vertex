#include <iostream>
#include <string>

#include "uci/uci.h"
#include "chess/position.h"
#include "chess/zobrist.h"
#include "chess/move_generation/attacks.h"

Position position;
UCI uci(position);

int main()
{
    std::string line;

    Zobrist::generate_keys();
    Attacks::buildTables();

    while (std::getline(std::cin, line))
    {
        if (!uci.receiveCommand(line))
            break;
    }

    return 0;
}