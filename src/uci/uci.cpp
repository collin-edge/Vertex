#include "uci.h"

#include <iostream>
#include <sstream>
#include <utility>
#include "../chess/moves.h"
#include "../chess/types.h"
#include "../chess/move_generation/movegen.h"

// make some functions first that can convert any given Move into the coordinate notation UCI uses

std::string squareToString(Square square)
{
    char file = 'a' + (static_cast<int>(square) % 8);
    char rank = '1' + (static_cast<int>(square) / 8);

    return std::string{file, rank};
}

Square stringToSquare(const std::string& str)
{
    int file = str[0] - 'a';
    int rank = str[1] - '1';
    return rank * 8 + file;
}

UCIMove moveToUCI(Move move)
{
    Square start = MovePacker::getStart(move);
    Square end = MovePacker::getEnd(move);

    UCIMove result;

    result += squareToString(start);
    result += squareToString(end);

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

Move uciToMove(const UCIMove& ucimove)
{
    Square start = stringToSquare(ucimove.substr(0, 2));
    Square end = stringToSquare(ucimove.substr(2, 2));

    MovePacker::Flag flag = MovePacker::Flag::NONE;

    if (ucimove.size() == 5)
    {
        switch (ucimove[4])
        {
            case 'n': flag = MovePacker::Flag::KNIGHT; break;
            case 'b': flag = MovePacker::Flag::BISHOP; break;
            case 'r': flag = MovePacker::Flag::ROOK; break;
            case 'q': flag = MovePacker::Flag::QUEEN; break;
        }
    }

    return MovePacker::encodeMove(start, end, flag);
}

UCI::UCI(Position& pos) : position(pos) {}

UCI::~UCI()
{
    search.stop();

    if (searchThread.joinable())
        searchThread.join();
}

bool UCI::receiveCommand(const std::string& line)
{
    std::istringstream token(line);
    std::string command;

    token >> command;

    if (command == "uci")
    {
        std::cout << "id name Vertex\n";
        std::cout << "id author Collin Edge\n";

        std::cout << "option name Hash type spin default 64 min 1 max 4096\n";
        std::cout << "option name QHash type spin default 8 min 1 max 4096\n";
        std::cout << "option name Threads type spin default 1 min 1 max 1\n";

        std::cout << "uciok" << std::endl;
    }
    else if (command == "setoption")
    {
        return parseSetOptionCommand(token);
    }
    else if (command == "debug")
    {
        // TODO: add later
    }
    else if (command == "isready")
    {
        if (searchThread.joinable())
        {
            search.stop();
            searchThread.join();
        }
        
        search.applySettings(settings);

        std::cout << "readyok" << std::endl;
    }
    else if (command == "go")
    {
        return parseGoCommand(token);
    }
    else if (command == "position")
    {
        // this shouldn't happen, but just incase a position command gets sent mid search
        search.stop();

        bool success = parsePositionCommand(token);

        if (!success)
            std::cerr << "ERROR: Failed to parse position command!\n";

        return true; // Don't terminate the engine.
    }
    else if (command == "quit")
    {
        search.stop();

        if (searchThread.joinable())
            searchThread.join();

        return false;
    }
    else if (command == "stop")
    {
        search.stop();

        if (searchThread.joinable())
            searchThread.join();
    }

    return true;
}

bool UCI::parsePositionCommand(std::istringstream& token)
{
    constexpr const char* START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    std::string type;
    if (!(token >> type))
        return false;

    std::string newFEN;

    if (type == "startpos")
    {
        newFEN = START_FEN;
    }
    else if (type == "fen")
    {
        std::string field;

        for (int i = 0; i < 6; i++)
        {
            if (!(token >> field))
                return false;

            if (i > 0)
                newFEN += ' ';

            newFEN += field;
        }
    }
    else
    {
        return false;
    }

    std::vector<Move> newMoves;
    std::string keyword;

    if (token >> keyword)
    {
        if (keyword != "moves")
            return false;

        std::string uciMove;

        while (token >> uciMove)
        {
            newMoves.push_back(uciToMove(uciMove));
        }
    }

    size_t commonPly = 0;

    if (newFEN == initialFEN)
    {
        while (commonPly < gameProgress.size() && commonPly < newMoves.size() && gameProgress[commonPly] == newMoves[commonPly])
        {
            commonPly++;
        }
    }

    Position candidate = position;

    std::vector<Position> newHistory;

    if (newFEN == initialFEN && !positionHistory.empty())
    {
        candidate = positionHistory[commonPly];

        newHistory.assign(positionHistory.begin(), positionHistory.begin() + commonPly + 1);
    }
    else
    {
        candidate.constructPosition(newFEN);
        newHistory.push_back(candidate);
        commonPly = 0;
    }

    for (size_t i = commonPly; i < newMoves.size(); i++)
    {
        Move move = newMoves[i];

        MoveGenInfo info = MoveGenerator::calculateInfo(candidate);
        MoveList legalMoves;
        MoveGenerator::generateLegalMoves(candidate, info, legalMoves, MoveGenType::ALL);

        bool legal = false;

        for (Move m : legalMoves)
        {
            Move fakeMove = MovePacker::encodeMove(
                MovePacker::getStart(m),
                MovePacker::getEnd(m),
                (MovePacker::isPromotion(MovePacker::getFlag(m))) ? MovePacker::getFlag(m) : MovePacker::Flag::NONE
            );

            if (fakeMove == move)
            {
                candidate.makeMove(m);
                legal = true;
                break;
            }
        }

        if (!legal)
            return false;

        newHistory.push_back(candidate);
    }

    position = candidate;
    initialFEN = newFEN;
    gameProgress = std::move(newMoves);
    positionHistory = std::move(newHistory);

    return true;
}

bool UCI::parseGoCommand(std::istringstream& token)
{
    if (searchThread.joinable())
    {
        search.stop();
        searchThread.join();
    }

    SearchLimits limits;

    std::string keyword;

    while (token >> keyword)
    {
        if (keyword == "wtime") token >> limits.wtime;
        else if (keyword == "btime") token >> limits.btime;
        else if (keyword == "winc") token >> limits.winc;
        else if (keyword == "binc") token >> limits.binc;
        else if (keyword == "movestogo") token >> limits.movesToGo;
        else if (keyword == "depth") token >> limits.depth;
        else if (keyword == "nodes") token >> limits.nodes;
        else if (keyword == "mate") token >> limits.mate;
        else if (keyword == "movetime") token >> limits.moveTime;
        else if (keyword == "infinite") limits.infinite = true;
        else if (keyword == "ponder") limits.ponder = true;
        else if (keyword == "searchmoves")
        {
            std::string move;

            while (token >> move)
            {
                limits.searchMoves.push_back(uciToMove(move));
            }
        }
    }

    Position searchPosition = position;

    searchThread = std::thread([this, searchPosition, limits]() mutable
    {
        SearchResult result = search.findBestMove(searchPosition, limits);
        Move bestMove = result.bestMove;

        std::cout << "bestmove " << moveToUCI(bestMove) << std::endl;
    });

    return true;
}

bool UCI::parseSetOptionCommand(std::istringstream& token)
{
    std::string keyword;
    std::string name;
    std::string value;

    if (!(token >> keyword) || keyword != "name")
        return false;

    while (token >> keyword && keyword != "value")
    {
        if (!name.empty())
            name += ' ';

        name += keyword;
    }

    if (keyword != "value")
        return false;

    if (!(token >> value))
        return false;

    try
    {
        int number = std::stoi(value);

        if (name == "Hash")
        {
            if (number < 1 || number > 4096)
                return false;

            settings.hashSize = number;
        }
        else if (name == "QHash")
        {
            if (number < 1 || number > 4096)
                return false;

            settings.qHashSize = number;
        }
        else if (name == "Threads")
        {
            if (number != 1)
                return false;

            settings.threads = number;
        }
        else
        {
            std::cerr << "Unknown option: " << name << '\n';
            return false;
        }
    }
    catch (...)
    {
        return false;
    }

    return true;
}
