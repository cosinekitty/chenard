/*
	ifstdio.cpp  -  Don Cross  -  http://cosinekitty.com
*/

#include "chenserver.h"

ChessCommandInterface_stdio::ChessCommandInterface_stdio()
{
}


ChessCommandInterface_stdio::~ChessCommandInterface_stdio()
{
}


bool ChessCommandInterface_stdio::ReadLine(std::string& line)
{
	line.clear();
	return !!std::getline(std::cin, line);
}


void ChessCommandInterface_stdio::WriteLine(const std::string& line)
{
	std::cout << line << std::endl;
}
