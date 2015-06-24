/*
    chenserver.cpp  -  Don Cross  -  http://cosinekitty.com

    A version of the Chenard chess engine designed to be used by
    software via a very simple command set over a TCP connection.
*/

#include "chenserver.h"

int main(int argc, const char *argv[])
{
    int rc = 1;
    ChessCommandInterface *iface = nullptr;
    int argindex = 1;
    if ((argc > 1) && (0 == strcmp(argv[1], "-s")))
    {
        ++argindex;
        iface = new ChessCommandInterface_stdio();
    }
    else if ((argc > 2) && (0 == strcmp(argv[1], "-p")))
    {
        argindex += 2;
        int port = 0;
        if (1 == sscanf(argv[2], "%d", &port) && (port > 0) && (port <= 0xffff))
        {
            // FIXFIXFIX - create TCP interface and store pointer in 'iface'.
            iface = new ChessCommandInterface_tcp(port);
        }
        else
        {
            std::cerr << "Invalid port number '" << argv[2] << "': must be integer in the range 0.." << 0xffff << std::endl;
        }
    }
    else
    {
        PrintUsage();
    }

    if (iface != nullptr)
    {
        ChessBoard board;
        ChessUI_Server ui;

        bool keepRunning = true;
        while (keepRunning)
        {
            std::string command;
            if (iface->ReadLine(command))
            {
                std::string response = ExecuteCommand(board, ui, command, keepRunning);
                iface->WriteLine(response);
            }
        }

        delete iface;
        iface = nullptr;
        rc = 0;
    }

    return rc;
}

const char *GetWhitePlayerString()  // used by SavePortableGameNotation()
{
    return "White";
}

const char *GetBlackPlayerString()	// used by SavePortableGameNotation()
{
    return "Black";
}

void ChessFatal(const char *message)
{
    fprintf(stderr, "Fatal chess error: %s\n", message);
    exit(1);
}

void PrintUsage()
{
    std::cerr <<
        "\n"
        "USAGE:\n"
        "\n"
        "chenserver -s\n"
        "    Runs the Chenard server using standard input/output.\n"
        "\n"
        "chenserver -p port\n"
        "    Runs the Chenard server using the specified TCP port.\n"
        "\n";
}

std::string ExecuteCommand(ChessBoard& board, ChessUI& ui, const std::string& command, bool &keepRunning)
{
    keepRunning = true;

    if (command == "exit")
    {
        keepRunning = false;
        return "OK";
    }

    if (command == "status")
    {
        return GameStatus(board);
    }

    if (command == "new")
    {
        // Start a new game.
        board.Init();
        return "OK";
    }

    // Unknown command.
    return "?";
}

std::string GameStatus(ChessBoard& board)
{
    const char *result = "*";       // assume the game is not over yet, unless we find otherwise.
    if (board.GameIsOver())
    {
        if (board.CurrentPlayerInCheck())
        {
            result = board.WhiteToMove() ? "0-1" : "1-0";     // Whoever has the move just lost.
        }
        else
        {
            result = "1/2-1/2";
        }
    }

    std::string text = result;
    text.push_back(' ');

    char buffer[200];
    if (nullptr != board.GetForsythEdwardsNotation(buffer, sizeof(buffer)))
    {
        text += buffer;
    }
    else
    {
        text += "FEN_ERROR";     // this should never happen!
    }
    return text;
}

#if 0
char SquareCharacter(SQUARE square)
{
    switch (square)
    {
    case EMPTY:     return '.';

    case WPAWN:     return 'P';
    case WKNIGHT:   return 'N';
    case WBISHOP:   return 'B';
    case WROOK:     return 'R';
    case WQUEEN:    return 'Q';
    case WKING:     return 'K';

    case BPAWN:     return 'p';
    case BKNIGHT:   return 'n';
    case BBISHOP:   return 'b';
    case BROOK:     return 'r';
    case BQUEEN:    return 'q';
    case BKING:     return 'k';

    default:        return '?';
    }
}
#endif