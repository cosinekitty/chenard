/*
    chenserver.h  -  Don Cross  -  http://cosinekitty.com

    Main header file for server version of Chenard chess engine.
*/

#include <assert.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <iostream>
#include <WinSock2.h>
#include "chess.h"
#include "uiserver.h"

class ChessGameState;

void PrintUsage();
std::string ExecuteCommand(ChessGameState& game, ChessUI_Server& ui, const std::string& command, bool& keepRunning);
std::string GameStatus(ChessGameState& game);      // Forsyth Edwards Notation
std::string MakeMoves(ChessGameState& game, const std::vector<std::string>& moveTokenList);
std::string LegalMoveList(ChessGameState& game);
std::string TestLegality(ChessGameState& game, const std::string& notation);
std::string Think(ChessUI_Server& ui, ChessGameState& game, int thinkTimeMillis);
std::string Undo(ChessGameState& game, int numTurns);

const size_t LONGMOVE_MAX_CHARS = 6;        // max: "e7e8q\0" is 6 characters
bool FormatLongMove(bool whiteToMove, Move move, char notation[LONGMOVE_MAX_CHARS]);
char SquareCharacter(SQUARE);

class ChessGameState
{
private:
    struct MoveState
    {
        Move move;
        UnmoveInfo unmove;

        MoveState(const Move& _move, const UnmoveInfo& _unmove)
            : move(_move)
            , unmove(_unmove)
        {}
    };

    ChessBoard board;
    std::vector<MoveState> moveStack;

public:
    ChessGameState() {}

    void Reset();
    const char *GameResult();
    std::string GetForsythEdwardsNotation();
    std::string FormatLongMove(Move move);
    std::string FormatPgn(Move move);       // caller must pass only verified legal moves
    bool ParseMove(const std::string& notation, Move& move);    // returns true only if notation represents legal move
    int NumTurns() const { return static_cast<int>(moveStack.size()); }
    void PushMove(Move move);   // caller must pass only verified legal moves
    void PopMove();
    int GenMoves(MoveList& ml) { return board.GenMoves(ml); }
    bool Think(ChessUI_Server& ui, int thinkTimeMillis, Move& move);
    bool IsGameOver() { return board.GameIsOver(); }

private:
    ChessGameState(const ChessGameState&);              // disable copy constructor
    ChessGameState& operator=(const ChessGameState&);   // disable assignment operator
};

/*
    ChessCommandInterface is an abstract class representing
    a way of sending and receiving strings of text commands.
*/
class ChessCommandInterface
{
public:
    virtual ~ChessCommandInterface() {}
    virtual bool ReadLine(std::string& line, bool& keepRunning) = 0;
    virtual void WriteLine(const std::string& line) = 0;
};

class ChessCommandInterface_stdio : public ChessCommandInterface
{
public:
    ChessCommandInterface_stdio();
    virtual ~ChessCommandInterface_stdio();
    virtual bool ReadLine(std::string& line, bool& keepRunning);
    virtual void WriteLine(const std::string& line);

private:
    ChessCommandInterface_stdio(const ChessCommandInterface_stdio&);                // disable copy constructor
    ChessCommandInterface_stdio& operator= (const ChessCommandInterface_stdio&);    // disable assignment
};

class ChessCommandInterface_tcp : public ChessCommandInterface
{
public:
    explicit ChessCommandInterface_tcp(int _port);
    virtual ~ChessCommandInterface_tcp();
    virtual bool ReadLine(std::string& line, bool& keepRunning);
    virtual void WriteLine(const std::string& line);

private:
    const int port;
    bool initialized;
    bool ready;
    SOCKET hostSocket;
    SOCKET clientSocket;

private:
    ChessCommandInterface_tcp(const ChessCommandInterface_tcp&);                // disable copy constructor
    ChessCommandInterface_tcp& operator= (const ChessCommandInterface_tcp&);    // disable assignment

    void CloseSocket(SOCKET &sock)
    {
        if (sock != INVALID_SOCKET)
        {
            ::closesocket(sock);
            sock = INVALID_SOCKET;
        }
    }
};
