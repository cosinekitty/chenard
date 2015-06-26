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

std::string ChessGameState::FormatAlg(Move move)
{
    char algebraic[LONGMOVE_MAX_CHARS];
    if (!::FormatLongMove(board.WhiteToMove(), move, algebraic))
    {
        assert(false);
        return "BADALG";
    }
    return std::string(algebraic);
}

std::string ChessGameState::FormatPgn(Move move)
{
    if (board.isLegal(move))
    {
        char pgn[MAX_MOVE_STRLEN + 1];
        ::FormatChessMove(board, move, pgn);
        return std::string(pgn);
    }
    assert(false);      // caller should not have passed an illegal move
    return "BADPGN";
}

std::string ChessGameState::Format(Move move, MoveFormatKind format)
{
    switch (format)
    {
    case MOVE_FORMAT_ALGEBRAIC:
        return FormatAlg(move);

    case MOVE_FORMAT_PGN:
        return FormatPgn(move);

    default:
        assert(false);      // caller should verify format
        return "BAD_FORMAT";
    }
}

std::vector<std::string> ChessGameState::History(MoveFormatKind format)
{
    using namespace std;

    // Every time we format a move, we need the board to be in the state it was
    // just before that move was made.
    // Pop all the moves from the move stack one by one and push onto 'revStack'.
    // This has the effect of reversing the order of the moves as we rewind 
    // the board back to its initial state.
    vector<MoveState> revStack;
    while (moveStack.size() > 0)
    {
        MoveState m = moveStack.back();
        moveStack.pop_back();
        revStack.push_back(m);
        board.UnmakeMove(m.move, m.unmove);
    }

    // When we get here, 'revStack' has the earliest moves at the back
    // and the most recent moves to the front.
    // Now we make the moves in forward order so that we can format each move.
    vector<string> history;
    while (revStack.size() > 0)
    {
        MoveState m = revStack.back();
        revStack.pop_back();
        moveStack.push_back(m);
        history.push_back(Format(m.move, format));
        board.MakeMove(m.move, m.unmove);
    }

    return history;
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
        MoveState tail = moveStack.back();
        board.UnmakeMove(tail.move, tail.unmove);
        moveStack.pop_back();
    }
}

bool ChessGameState::Think(ChessUI_Server& ui, int thinkTimeMillis, Move& move)
{
    int centis = (thinkTimeMillis + 9) / 10;    // convert milliseconds to centiseconds and round up
    ComputerChessPlayer thinker(ui);
    thinker.setResignFlag(false);       // do not allow computer to resign
    thinker.SetTimeLimit(centis);       // set upper limit on how long computer is allowed to think
    INT32 timeSpent = 0;
    return thinker.GetMove(board, move, timeSpent);
}