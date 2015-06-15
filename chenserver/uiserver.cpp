/*
	uiserver.cpp
*/

#include "chenserver.h"

ChessUI_Server::ChessUI_Server()
{
}

ChessUI_Server::~ChessUI_Server()
{
}

ChessPlayer* ChessUI_Server::CreatePlayer(ChessSide)
{
	// This function should never be called because chenserver does not use the ChessGame class.
	ChessFatal("ChessUI_Server::CreatePlayer() should not have been called!");
	return nullptr;
}

void ChessUI_Server::ReportEndOfGame(ChessSide winner)
{
	// Nothing to do here.
}

void ChessUI_Server::Resign(ChessSide, QuitGameReason)
{
	// Do we need this?
}

bool ChessUI_Server::ReadMove(
	ChessBoard  &board,
	int         &source,
	int         &dest,
	SQUARE      &promPieceIndex)
{
	ChessFatal("ChessUI_Server::ReadMove() should not have been called!");
	return false;
}

SQUARE ChessUI_Server::PromotePawn(int /*PawnDest*/, ChessSide)
{
	ChessFatal("ChessUI_Server::PromotePawn() should not have been called!");
	return 0;
}

void ChessUI_Server::DisplayMove(ChessBoard &, Move)
{
	// Nothing to do here.
}

void ChessUI_Server::RecordMove(ChessBoard &, Move, INT32 thinkTime)
{
	ChessFatal("ChessUI_Server::RecordMove() should not have been called!");
}

void ChessUI_Server::ReportComputerStats(
	INT32   thinkTime,
	UINT32  nodesVisited,
	UINT32  nodesEvaluated,
	UINT32  nodesGenerated,
	int     fwSearchDepth,
	UINT32  vis[NODES_ARRAY_SIZE],
	UINT32  gen[NODES_ARRAY_SIZE])
{
	// Nothing to do here.
}

void ChessUI_Server::DrawBoard(const ChessBoard &)
{
	// Nothing to do.
}

void ChessUI_Server::NotifyUser(const char * /*message*/)
{
	// Nothing to do.
}

void ChessUI_Server::DisplayBestMoveSoFar(
	const ChessBoard &,
	Move,
	int /*level*/)
{
}

void ChessUI_Server::DisplayCurrentMove(
	const ChessBoard &,
	Move,
	int /*level*/)
{
}

void ChessUI_Server::PredictMate(int /*numMovesFromNow*/)
{
}
