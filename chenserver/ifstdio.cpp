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


bool ChessCommandInterface_stdio::ReadLine(std::string& line, bool& keepRunning)
{
    line.clear();
    keepRunning = !!std::getline(std::cin, line);       // stop running when we hit EOF
    return keepRunning;
}


void ChessCommandInterface_stdio::WriteLine(const std::string& line)
{
    std::cout << line << std::endl;
}
