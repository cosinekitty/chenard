/*
    gamestate.cpp  -  Don Cross  -  http://cosinekitty.com
*/

#include "chenserver.h"

void ChessGameState::Reset()
{
    board.Init();
    moveStack.clear();
}

const char * ChessGameState::GameResult()
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
    return result;
}

std::string ChessGameState::GetForsythEdwardsNotation()
{
    char buffer[200];
    if (nullptr == board.GetForsythEdwardsNotation(buffer, sizeof(buffer)))
    {
        assert(false);       // this should never happen!
        return "BADFEN";
    }
    return std::string(buffer);
}

std::string ChessGameState::FormatLongMove(Move move)
{
    char longmove[LONGMOVE_MAX_CHARS];
    if (!::FormatLongMove(board.WhiteToMove(), move, longmove))
    {
        assert(false);
        return "BADLM";     // cryptic, but fits in 5 characters as required by a longmove
    }
    return std::string(longmove);
}

bool ChessGameState::ParseMove(const std::string& notation, Move& move)
{
    if (::ParseFancyMove(notation.c_str(), board, move))
    {
        if (board.isLegal(move))
        {
            return true;
        }
        assert(false);      // internal error: ParseFancyMove should not have returned true for an illegal move!
    }
    return false;
}

std::string ChessGameState::FormatPgn(Move move)
{
    if (board.isLegal(move))
    {
        char pgn[MAX_MOVE_STRLEN + 1];
        FormatChessMove(board, move, pgn);
        return std::string(pgn);
    }
    assert(false);      // caller should not have passed an illegal move
    return "BADPGN";
}

void ChessGameState::PushMove(Move move) 
{
    if (board.isLegal(move))
    {
        UnmoveInfo unmove;
        board.MakeMove(move, unmove);
        moveStack.push_back(MoveState(move, unmove));
    }
    else
    {
        assert(false);    // caller must pass only verified legal moves
    }
}

void ChessGameState::PopMove()
{
    assert(moveStack.size() > 0);
    if (moveStack.size() > 0)
    {
        moveStack.pop_back();
    }
}
