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
            if (iface->ReadLine(command, keepRunning) && keepRunning)
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

bool ParseCommand(const std::string& command, std::string& verb, std::vector<std::string>& args)
{
    using namespace std;

    verb.clear();
    args.clear();

    std::string tok;
    for (auto c : command)
    {
        if (isspace(c))
        {
            if (tok.length() > 0)
            {
                if (verb.length() == 0)
                {
                    verb = tok;
                }
                else
                {
                    args.push_back(tok);
                }
                tok.clear();
            }
        }
        else
        {
            tok.push_back(c);
        }
    }

    if (tok.length() > 0)
    {
        if (verb.length() == 0)
        {
            verb = tok;
        }
        else
        {
            args.push_back(tok);
        }
    }

    return verb.length() > 0;
}

std::string ExecuteCommand(ChessBoard& board, ChessUI& ui, const std::string& command, bool &keepRunning)
{
    using namespace std;

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

        if (verb == "legal")
        {
            return LegalMoveList(board);
        }

        if (verb == "move")
        {
            return MakeMoves(board, args);
        }

        if (command == "new")
        {
            // Start a new game.
            board.Init();
            return "OK";
        }

        if (command == "status")
        {
            return GameStatus(board);
        }

        if (verb == "test")
        {
            // Test a single move for legality.
            if (args.size() != 1)
            {
                return "BAD_ARGS";
            }
            return TestLegality(board, args[0]);
        }

        return "UNKNOWN_COMMAND";
    }

    return "CANNOT_PARSE";
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

std::string TestLegality(ChessBoard& board, const std::string& notation)
{
    Move move;
    if (ParseFancyMove(notation.c_str(), board, move))
    {
        if (board.isLegal(move))
        {
            char pgn[MAX_MOVE_STRLEN + 1];
            FormatChessMove(board, move, pgn);

            char longmove[LONGMOVE_MAX_CHARS];
            if (!FormatLongMove(board.WhiteToMove(), move, longmove))
            {
                return "FORMAT_INTERNAL_ERROR";
            }

            return std::string("OK ") + longmove + " " + pgn;
        }
        else
        {
            // This should never happen because ParseFancyMove() has been updated to
            // return false if the move is illegal, even if it is well-formed notation.
            return "PARSE_INTERNAL_ERROR";
        }
    }
    return "ILLEGAL";
}

std::string MakeMoves(ChessBoard& board, const std::vector<std::string>& moveTokenList)
{
    using namespace std;

    // If there are any illegal moves in the list, we roll back any moves we made before that.
    // Therefore, we need to store a list of all (move, unmove) pairs we do make.
    struct MoveState
    {
        Move move;
        UnmoveInfo unmove;

        MoveState(const Move& _move, const UnmoveInfo& _unmove)
            : move(_move)
            , unmove(_unmove)
            {}
    };
    vector<MoveState> stateList;

    for (const string& mtoken : moveTokenList)
    {
        Move move;
        if (ParseFancyMove(mtoken.c_str(), board, move))
        {
            UnmoveInfo unmove;
            board.MakeMove(move, unmove);
            stateList.push_back(MoveState(move, unmove));
        }
        else
        {            
            // Illegal or malformed move text.
            // Roll back all the moves we made before finding this invalid move text.
            while (stateList.size() > 0)
            {
                MoveState s = stateList.back();
                board.UnmakeMove(s.move, s.unmove);
                stateList.pop_back();
            }

            // Report back the problem.
            return "BAD_MOVE " + mtoken;
        }
    }

    return "OK " + to_string(stateList.size());     // return "OK " followed by number of moves we made
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

std::string LegalMoveList(ChessBoard& board)
{
    using namespace std;

    MoveList ml;
    int numMoves = board.GenMoves(ml);
    string text = to_string(numMoves);
    bool whiteToMove = board.WhiteToMove();
    for (int i = 0; i < numMoves; ++i)
    {
        char notation[6];
        if (!FormatLongMove(whiteToMove, ml.m[i], notation))
        {
            return "INTERNAL_ERROR";    // should never happen!
        }
        text.push_back(' ');
        text += notation;
    }
    return text;
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
