/*
	chenserver.h  -  Don Cross  -  http://cosinekitty.com

	Main header file for server version of Chenard chess engine.
*/

#include <cstdio>
#include <cstdlib>
#include <string>
#include <iostream>
#include <WinSock2.h>
#include "chess.h"
#include "uiserver.h"

void PrintUsage();
std::string ExecuteCommand(ChessBoard &board, ChessUI &ui, const std::string& command);

/*
	ChessCommandInterface is an abstract class representing 
	a way of sending and receiving strings of text commands.
*/
class ChessCommandInterface
{
public:
	virtual ~ChessCommandInterface() {}
	virtual bool ReadLine(std::string& line) = 0;
	virtual void WriteLine(const std::string& line) = 0;
};

class ChessCommandInterface_stdio : public ChessCommandInterface
{
public:
	ChessCommandInterface_stdio();
	virtual ~ChessCommandInterface_stdio();
	virtual bool ReadLine(std::string& line);
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
    virtual bool ReadLine(std::string& line);
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
