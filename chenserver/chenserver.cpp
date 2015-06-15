/*
	chenserver.cpp  -  Don Cross  -  http://cosinekitty.com

	A version of the Chenard chess engine designed to be used by
	software via a very simple command set over a TCP connection.
*/

#include <cstdio>
#include <cstdlib>
#include "chess.h"
#include "uiserver.h"

int main(int argc, const char *argv[])
{
	ChessBoard board;
	ChessUI_Server ui;
	return 1;
}

const char *GetWhitePlayerString()  // used by SavePortableGameNotation()
{
	return "White";
}

const char *GetBlackPlayerString() // used by SavePortableGameNotation()
{
	return "Black";
}

void ChessFatal(const char *message)
{
	fprintf(stderr, "Fatal chess error: %s\n", message);
	exit(1);
}
