#pragma once

#include <string>
#include <vector>
#include "../chess/position.h"
#include <thread>
#include "../chess/engine/search.h"

using UCIMove = std::string;

class UCI
{
private:
    Position& position;

    std::string initialFEN;
    std::vector<Move> gameProgress;

    std::vector<Position> positionHistory;

    Search search;
    std::thread searchThread;

public:
    UCI(Position& pos);
    ~UCI();

    bool receiveCommand(const std::string& line);

private:
    bool parsePositionCommand(std::istringstream& token);
    bool parseGoCommand(std::istringstream& token);
    // TODO: add some options probably       bool parseSetOptionCommand(std::istringstream& token);
};