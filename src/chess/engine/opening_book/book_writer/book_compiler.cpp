#include "../../../position.h"
#include "../../../types.h"
#include "../../../move_generation/movegen.h"
#include "../../../zobrist.h"
#include "polyglot_book.h"
#include "polyglot.h"
#include <unordered_set>
#include <vector>
#include <fstream>
#include <algorithm>
#include <iostream>

// polyglot books don't use the same zobrist hashing system, so we gotta convert them! lovely
// instead of doing it at runtime because that'd be suuuper inefficient, i'm gonna make my own bin file with MY zobrist hashes

struct BookEntry
{
    uint64_t key;
    Move move;
    uint16_t weight;
};

Move findLegalMove(Move decoded, const MoveList& legalMoves)
{
    for (Move move : legalMoves)
    {
        if (MovePacker::getStart(move) == MovePacker::getStart(decoded) &&
            MovePacker::getEnd(move) == MovePacker::getEnd(decoded) &&
            MovePacker::getFlag(move) == MovePacker::getFlag(decoded))
        {
            return move;
        }
    }

    return NO_MOVE;
}

void enumerate(Position& position, const PolyglotBook& source, std::unordered_set<uint64_t>& visited, std::vector<BookEntry>& output)
{
    uint64_t polyKey = Polyglot::hash(position);

    if (!visited.insert(polyKey).second)
        return;

    const auto* entries = source.find(polyKey);

    if (!entries)
        return;

    uint64_t nativeKey = position.getZobristKey();

    MoveGenInfo info = MoveGenerator::calculateInfo(position);

    MoveList legalMoves;
    MoveGenerator::generateLegalMoves(position, info, legalMoves, MoveGenType::ALL);

    for (const PolyglotEntry& entry : *entries)
    {
        Move decoded = Polyglot::decodeMove(entry.move, position);

        Move move = findLegalMove(decoded, legalMoves);

        if (move == NO_MOVE)
            continue;

        output.push_back({nativeKey, move, entry.weight});

        UndoInfo undo = position.makeMove(move);

        enumerate(position, source, visited, output);

        position.undoMove(undo);
    }
}

bool writeBook(const std::string& filename, std::vector<BookEntry>& entries)
{
    std::sort(entries.begin(), entries.end(),
        [](const BookEntry& a, const BookEntry& b)
        {
            return a.key < b.key;
        });

    std::ofstream file(filename, std::ios::binary);

    if (!file.is_open())
        return false;

    for (const BookEntry& entry : entries)
    {
        uint8_t buffer[12];

        for (int i = 0; i < 8; ++i)
            buffer[i] = static_cast<uint8_t>(entry.key >> (i * 8));

        buffer[8] = static_cast<uint8_t>(entry.move);
        buffer[9] = static_cast<uint8_t>(entry.move >> 8);

        buffer[10] = static_cast<uint8_t>(entry.weight);
        buffer[11] = static_cast<uint8_t>(entry.weight >> 8);

        file.write(reinterpret_cast<const char*>(buffer), sizeof(buffer));

        if (!file)
            return false;
    }

    return true;
}

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        std::cout << "Usage: book_compiler <input.bin> <output.bin>\n";
        return 1;
    }

    const std::string inputFile = argv[1];
    const std::string outputFile = argv[2];

    Zobrist::generate_keys();

    PolyglotBook source;

    if (!source.open(inputFile))
    {
        std::cerr << "Failed to open Polyglot book.\n";
        return 1;
    }

    std::cout << "Loaded Polyglot book.\n";

    Position position;

    constexpr std::string_view STARTING_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    if (!position.constructPosition(STARTING_FEN))
    {
        std::cerr << "Failed to construct starting position.\n";
        return 1;
    }

    std::unordered_set<uint64_t> visited;
    std::vector<BookEntry> output;

    std::cout << "Enumerating book positions...\n";

    enumerate(position, source, visited, output);

    std::cout << "Enumeration complete!\n";
    std::cout << "Positions visited: " << visited.size() << '\n';
    std::cout << "Book entries generated: " << output.size() << '\n';

    std::cout << "Writing converted book...\n";

    if (!writeBook(outputFile, output))
    {
        std::cerr << "Failed to write output book.\n";
        return 1;
    }

    std::cout << "Successfully compiled opening book!\n";

    return 0;
}