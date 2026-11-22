#include "uci.h"

bool UCI::recieveCommand(const std::string& line)
{
    std::istringstream token(line);
    std::string command;

    token >> command;

    if (command == "uci")
    {
        std::cout << "id name Vertex\n";
        std::cout << "id author Collin Edge\n";
        std::cout << "uciok" << std::endl;
    }
    else if (command == "debug")
    {
        // TODO: add later
    }
    else if (command == "isready")
    {
        // TODO: add actual initialization stuff maybe

        std::cout << "readyok" << std::endl;
    }
    else if (command == "setoption")
    {
        // TODO: add later
    }
    else if (command == "go")
    {
        // TODO: add later
    }
    else if (command == "ponderhit")
    {
        // TODO: add later
    }
    else if (command == "position")
    {
        // TODO: add later
    }
    else if (command == "quit")
    {
        return false;
    }
    else if (command == "stop")
    {
        // TODO: add later
    }

    return true;
}