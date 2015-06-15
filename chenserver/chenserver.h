/*
	chenserver.h  -  Don Cross  -  http://cosinekitty.com

	Main header file for server version of Chenard chess engine.
*/

#include <cstdio>
#include <cstdlib>
#include <string>
#include <iostream>
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
};


