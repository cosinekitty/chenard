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
        ChessGameState game;
        ChessUI_Server ui;

        bool keepRunning = true;
        while (keepRunning)
        {
            std::string command;
            if (iface->ReadLine(command, keepRunning) && keepRunning)
            {
                std::string response = ExecuteCommand(game, ui, command, keepRunning);
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

inline void EndToken(std::string& tok, std::string& verb, std::vector<std::string>& args)
{
    if (tok.length() > 0)           // Did we find a token (a group of consecutive non-space characters)?
    {
        if (verb.length() == 0)
        {
            verb = tok;             // The first token becomes the verb.
        }
        else
        {
            args.push_back(tok);    // Any tokens after the first become arguments.
        }
        tok.clear();                // Get ready for another token (if there are any more).
    }
}

bool ParseCommand(const std::string& command, std::string& verb, std::vector<std::string>& args)
{
    using namespace std;

    verb.clear();
    args.clear();

    std::string tok;
    for (char c : command)
    {
        if (isspace(c))
        {
            EndToken(tok, verb, args);
        }
        else
        {
            tok.push_back(c);
        }
    }

    EndToken(tok, verb, args);

    return verb.length() > 0;
}

std::string ExecuteCommand(ChessGameState& game, ChessUI_Server& ui, const std::string& command, bool &keepRunning)
{
    using namespace std;

    static const char BAD_ARGS[] = "BAD_ARGS";

    keepRunning = true;

    string verb;
    vector<string> args;
    if (ParseCommand(command, verb, args))
    {
        if (verb == "exit")
        {
            keepRunning = false;
            return "OK";
        }

        if (verb == "history")
        {
            return History(game, args);
        }

        if (verb == "legal")
        {
            return LegalMoveList(game);
        }

        if (verb == "move")
        {
            return MakeMoves(game, args);
        }

        if (verb == "new")
        {
            // Start a new game.
            game.Reset();
            return "OK";
        }

        if (verb == "status")
        {
            return GameStatus(game);
        }

        if (verb == "test")
        {
            // Test a single move for legality.
            if (args.size() != 1)
            {
                return BAD_ARGS;
            }
            return TestLegality(game, args[0]);
        }

        if (verb == "think")
        {
            if (args.size() != 1)
            {
                return BAD_ARGS;
            }
            return Think(ui, game, atoi(args[0].c_str()));
        }

        if (verb == "undo")
        {
            if (args.size() != 1)
            {
                return BAD_ARGS;
            }
            return Undo(game, atoi(args[0].c_str()));
        }

        return "UNKNOWN_COMMAND";
    }

    return "CANNOT_PARSE";
}

std::string GameStatus(ChessGameState& game)
{
    std::string text = game.GameResult();
    text.push_back(' ');
    text += game.GetForsythEdwardsNotation();
    return text;
}

std::string TestLegality(ChessGameState& game, const std::string& notation)
{
    Move move;
    if (game.ParseMove(notation, move))
    {
        return std::string("OK ") + game.FormatAlg(move) + " " + game.FormatPgn(move);
    }
    return "ILLEGAL";
}

std::string MakeMoves(ChessGameState& game, const std::vector<std::string>& moveTokenList)
{
    using namespace std;

    int numPushedMoves = 0;

    for (const string& mtoken : moveTokenList)
    {
        Move move;
        if (game.ParseMove(mtoken, move))
        {
            game.PushMove(move);
            ++numPushedMoves;
        }
        else
        {            
            // Illegal or malformed move text.
            // Roll back all the moves we made before finding this invalid move text.
            while (numPushedMoves > 0)
            {
                game.PopMove();
                --numPushedMoves;
            }

            // Report back the problem.
            return "BAD_MOVE " + mtoken;
        }
    }

    return "OK " + to_string(numPushedMoves);     // return "OK " followed by number of moves we made
}

std::string Think(ChessUI_Server& ui, ChessGameState& game, int thinkTimeMillis)
{
    using namespace std;

    if (game.IsGameOver())
    {
        return "GAME_OVER";
    }

    const int MAX_THINK_MINUTES = 5;        // adjust as needed
    const int MAX_THINK_MILLIS = 1000 * 60 * MAX_THINK_MINUTES;
    if ((thinkTimeMillis > 0) && (thinkTimeMillis <= MAX_THINK_MILLIS))
    {
        Move move;
        if (game.Think(ui, thinkTimeMillis, move))
        {
            // Must format the response BEFORE making the move because board must be in pre-move state.
            string response = string("OK ") + game.FormatAlg(move) + " " + game.FormatPgn(move);
            game.PushMove(move);
            return response;
        }
        else
        {
            return "THINK_ERROR";
        }
    }
    return "BAD_THINK_TIME";
}

bool FormatLongMove(bool whiteToMove, Move move, char notation[LONGMOVE_MAX_CHARS])
{
    int source;
    int dest;
    SQUARE prom = move.actualOffsets(whiteToMove, source, dest);
    char pchar = (prom == EMPTY) ? '\0' : tolower(SquareCharacter(prom));
    if (pchar == '?' || source < OFFSET(2, 2) || source > OFFSET(9, 9) || dest < OFFSET(2, 2) || dest > OFFSET(9, 9))
    {
        // Something is wrong with this move!
        notation[0] = '\0';
        return false;
    }
    else
    {
        notation[0] = 'a' + (XPART(source) - 2);
        notation[1] = '1' + (YPART(source) - 2);
        notation[2] = 'a' + (XPART(dest) - 2);
        notation[3] = '1' + (YPART(dest) - 2);
        notation[4] = pchar;
        notation[5] = '\0';
        return true;
    }
}

std::string LegalMoveList(ChessGameState& game)
{
    MoveList ml;
    int numMoves = game.GenMoves(ml);
    std::string text = std::to_string(numMoves);
    for (int i = 0; i < numMoves; ++i)
    {
        text.push_back(' ');
        text += game.FormatAlg(ml.m[i]);
    }
    return text;
}

std::string Undo(ChessGameState& game, int numTurns)
{
    if (numTurns < 1 || numTurns > game.NumTurns())
    {
        return "BAD_NUM_TURNS";
    }

    for (int i = 0; i < numTurns; ++i)
    {
        game.PopMove();
    }

    return "OK";
}

MoveFormatKind ParseMoveFormat(const std::string& text)
{
    if (text == "pgn")
    {
        return MOVE_FORMAT_PGN;
    }

    if (text == "alg")
    {
        return MOVE_FORMAT_ALGEBRAIC;
    }

    return MOVE_FORMAT_INVALID;
}

std::string History(ChessGameState& game, const std::vector<std::string>& args)
{
    using namespace std;

    MoveFormatKind format = MOVE_FORMAT_ALGEBRAIC;
    if (args.size() > 0)
    {
        format = ParseMoveFormat(args[0]);
    }

    if (format == MOVE_FORMAT_INVALID)
    {
        return "BAD_FORMAT";
    }

    vector<string> history = game.History(format);
    string listing = "OK ";
    listing += to_string(history.size());
    for (string movetext : history)
    {
        listing += " " + movetext;
    }
    return listing;
}

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
