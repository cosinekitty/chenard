/*
    chenserver.h  -  Don Cross  -  http://cosinekitty.com

    Main header file for server version of Chenard chess engine.
*/

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <iostream>
#include <WinSock2.h>
#include "chess.h"
#include "uiserver.h"

void PrintUsage();
std::string ExecuteCommand(ChessBoard& board, ChessUI& ui, const std::string& command, bool& keepRunning);
std::string GameStatus(ChessBoard& board);      // Forsyth Edwards Notation
std::string MakeMoves(ChessBoard& board, const std::vector<std::string>& moveTokenList);
std::string LegalMoveList(ChessBoard& board);
std::string TestLegality(ChessBoard& board, const std::string& notation);
char SquareCharacter(SQUARE);

const size_t LONGMOVE_MAX_CHARS = 6;        // max: "e7e8q\0" is 6 characters
bool FormatLongMove(bool whiteToMove, Move move, char notation[LONGMOVE_MAX_CHARS]);


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
