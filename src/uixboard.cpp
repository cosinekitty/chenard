/*
    uixboard.cpp  -  Don Cross  -  http://cosinekitty.com/chenard

    An adaptation of Chenard for xboard version 2.
    See:   http://www.tim-mann.org/xboard.html

    This source file contains a stubbed out implementation of ChessUI_xboard.
    This class exists just to get the code to compile.
    Because xboard has a very specific way of interacting with a chess engine,
    I do not use the Chenard ChessGame class at all.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "chess.h"
#include "uixboard.h"
#include "lrntree.h"

const int CHECK_INPUT_INTERVAL = 5000;

ChessUI_xboard::ChessUI_xboard():
    checkInputCounter (0),
    adHocTextEnabled (false),
    thinkingDisplayEnabled (false),
    levelCache (0),
    player (0),
    pondering (false)
{
    oppMoveWhilePondering[0] = '\0';
}


ChessUI_xboard::~ChessUI_xboard()
{
}


void ChessUI_xboard::SetAdHocText (int /*index*/, const char *format, ...)
{
    if (adHocTextEnabled)
    {
        va_list argptr;
        va_start (argptr, format);
        vprintf (format, argptr);
        va_end (argptr);

        printf ("\n");
    }
}


void ChessUI_xboard::OnStartSearch()
{
    searchStartTime = ChessTime();
    myPonderClock = -1;
    pendingVerb[0] = '\0';
    pendingRest[0] = '\0';
    oppMoveWhilePondering[0] = '\0';                    // caller of pondering needs to be able to tell if search ended without opponent move arriving
}

void ChessUI_xboard::StartPondering (const char *_predictedAlgebraic, const char *_predictedPgn)
{
    pondering = true;
    strcpy (predictedPgn, _predictedPgn);               // we need in PGN format for displaying best path info to xboard
    strcpy (predictedAlgebraic, _predictedAlgebraic);   // we need in algebraic for simple comparison against actual move, when it arrives
    OnStartSearch();
}


void ChessUI_xboard::FinishPondering (char opponentMoveWhilePonderingAlgebraic [6])
{
    pondering = false;
    strcpy (opponentMoveWhilePonderingAlgebraic, oppMoveWhilePondering);
    oppMoveWhilePondering[0] = '\0';
}


void ChessUI_xboard::OnIncomingUserMove (const char *actualAlgebraic)
{
    strcpy (oppMoveWhilePondering, actualAlgebraic);
    if (0 == strcmp (actualAlgebraic, predictedAlgebraic))
    {
        // We predicted correctly!  Finish up with a reasonable amount of time.
        const int now = ChessTime();
        int timeAlreadySpent = now - searchStartTime;
        if (myPonderClock > 0)
        {
            int budget = TimeBudgetForNextMove (myPonderClock, false);  // not computer's turn yet - we are still pondering from it being opponent's turn
            int excess = budget - timeAlreadySpent;
            if (excess > 25)       // avoid fatal error for search times less than 0.01 seconds
            {
                dprintf ("Pondering an extra %0.2lf seconds.\n", 0.01 * (double)excess);
                player->SetTimeLimit (excess);
            }
            else
            {
                dprintf ("Under time budget - aborting pondering now.\n");
                player->AbortSearch();
            }
        }
        else
        {
            // WEIRD EMERGENCY - We don't know how much time is on our clock: end thinking NOW!
            dprintf ("EMERGENCY - No idea how much time is on clock!\n");
            player->AbortSearch();
        }
    }
    else
    {
        // We were wrong!  GIVE UP NOW!
        player->AbortSearch();
    }
}


void ChessUI_xboard::AbortSearch (const char *verb, const char *rest)
{
    player->AbortSearch();
    strcpy (pendingVerb, verb);
    strcpy (pendingRest, rest);
}


void ChessUI_xboard::DebugPly (int /*depth*/, ChessBoard &, Move)
{
    // We use this method override as a hack for checking for xboard input while thinking.
    // This function gets called a LOT during the search, so we have to be careful about performance.
    // For efficiency, we check for input (by calling IsInputReady) only every now and then.

    if (++checkInputCounter >= CHECK_INPUT_INTERVAL)
    {
        checkInputCounter = 0;

        // If we receive an xboard command that we are not ready to execute now,
        // we save the command away in pendingVerb and pendingRest and abort the search.
        // Once this happens, we still could get more calls here to DebugPly.
        // Therefore we must check to make sure the search has not yet been aborted
        // before looking for any xboard commands, lest we skip over a command,
        // causing it to be ignored.
        if (!GlobalAbortFlag && !player->IsSearchAborted())
        {
            if (IsInputReady())
            {
                // xboard is trying to talk to us, even though we are thinking!
                // See if it is the "?" command, meaning "move now", or something else important like "quit"...
                char verb [MAX_XBOARD_LINE];
                const char *rest = ReadCommandFromXboard (verb);
                if (rest)
                {
                    if (CommandMayBeIgnored(verb))       // There are some commands we can safely ignore, so as to avoid disturbing an active search.
                    {
                        dprintf ("Ignoring command while thinking: %s %s\n", verb, rest);
                    }
                    else
                    {
                        if (0 == strcmp(verb,"?"))
                        {
                            player->AbortSearch();
                        }
                        else if (0 == strcmp(verb,"force"))
                        {
                            ReceiveForceCommand();
                            player->AbortSearch();
                        }
                        else if (0 == strcmp(verb,"quit"))
                        {
                            GlobalAbortFlag = true;
                            ReceiveForceCommand();
                            player->AbortSearch();
                        }
                        else if (0 == strcmp(verb,"post"))
                        {
                            EnableThinkingDisplay (true);
                        }
                        else if (0 == strcmp(verb,"nopost"))
                        {
                            EnableThinkingDisplay (false);
                        }
                        else
                        {
                            // If we are pondering, we may get commands like "time", "otim", "usermove".
                            // Make note of "time" and "otim", and respond conditionally to "usermove",
                            // based on whether we predicted the opponent correctly.
                            if (pondering)
                            {
                                if (0 == strcmp(verb,"time"))
                                {
                                    myPonderClock = atoi(rest);
                                }
                                else if (0 == strcmp(verb,"usermove"))
                                {
                                    if (LooksLikeMove(rest))
                                    {
                                        OnIncomingUserMove (rest);
                                    }
                                }
                                else if (0 == strcmp(verb,"hint"))
                                {
                                    printf ("Hint: %s\n", predictedAlgebraic);
                                }
                                else if (0 == strcmp(verb,"ping"))
                                {
                                    // The xboard spec says we must reply to ping commands immediately while pondering.
                                    printf ("pong %s\n", rest);
                                }
                                else if (LooksLikeMove(verb))               // needed in case usermove feature was not available
                                {
                                    OnIncomingUserMove (verb);
                                }
                                else
                                {
                                    AbortSearch (verb, rest);
                                }
                            }
                            else
                            {
                                AbortSearch (verb, rest);
                            }
                        }
                    }
                }
            }
        }
    }

    if (GlobalAbortFlag)
    {
        player->AbortSearch();        // GlobalAbortFlag can be set by itself if an error occurs reading from stdin
    }
}


ChessPlayer *ChessUI_xboard::CreatePlayer ( ChessSide )
{
    ChessFatal ("Should not have been called: ChessUI_xboard::CreatePlayer()");
    return NULL;
}


bool ChessUI_xboard::ReadMove ( ChessBoard &, int & /*source*/, int & /*dest*/, SQUARE & /*promIndex*/ )
{
    ChessFatal ("Should not have been called: ChessUI_xboard::ReadMove()");
    return false;
}


SQUARE ChessUI_xboard::PromotePawn ( int /*PawnSource*/, int /*PawnDest*/, ChessSide )
{
    ChessFatal ("Should not have been called: ChessUI_xboard::PromotePawn()");
    return EMPTY;
}

void ChessUI_xboard::DisplayMove ( ChessBoard &, Move )
{
}

void ChessUI_xboard::RecordMove ( ChessBoard &, Move, INT32 /*thinkTime*/ )
{
}


void ChessUI_xboard::DrawBoard ( const ChessBoard & )
{
}


void ChessUI_xboard::ReportEndOfGame ( ChessSide /*winner*/ )
{
}


void ChessUI_xboard::DisplayBestMoveSoFar ( const ChessBoard &, Move, int level )
{
    levelCache = level;
}


void ChessUI_xboard::DisplayCurrentMove ( const ChessBoard &, Move, int /*level*/ )
{
}


void ChessUI_xboard::DisplayBestPath (const ChessBoard &_board, const BestPath &path)
{

    if (thinkingDisplayEnabled)
    {
        static const int MAX_PATH = 20;
        UnmoveInfo unmove [MAX_PATH];
        char string [MAX_MOVE_STRLEN + 1];

        int i;
        int n = path.depth;
        if (n > MAX_PATH)
        {
            n = MAX_PATH;   // truncate thinking display to a reasonable number
        }

        // ply score time nodes pv
        unsigned nodesEvaluated = player ? player->queryNodesEvaluated() : 0;
        INT32 elapsedTime = ChessTime() - searchStartTime;
        int relativeScore = path.m[0].score;
        if (_board.BlackToMove())
        {
            // Chenard scores are always expressed in terms of positive values
            // being good for White, and negative values being good for Black.
            // WinBoard/xboard expect positive values being good for the computer,
            // regardless of what side it is playing.
            // Therefore, because the computer is playing Black, we negate the score here.
            relativeScore *= -1;
        }
        int col = printf ("%d %d %d %u", levelCache, relativeScore, elapsedTime, nodesEvaluated);

        // Weird special case: if we are pondering, the xboard spec says we must
        // print the move we assume the opponent is going to make...
        if (pondering)
        {
            if (oppMoveWhilePondering[0] == '\0')       // if we received the opponent move and we are still pondering, the prediction is no longer hypothetical.
            {
                col += printf (" %s", predictedPgn);
            }
        }

        // We change the board here, but we put it back the way we find it!
        ChessBoard &board = (ChessBoard &) _board;
        for (i=0; i < path.depth; ++i)
        {
            Move move = path.m[i];
            FormatChessMove (board, move, string);
            if (col >= 70)
            {
                // xboard allows us to break up long lines in the principal variation (best path),
                // so long as there are at least 4 spaces at the beginning of the continuation line.
                col = 4;
                printf ("\n    ");
            }
            else
            {
                col += printf (" ");
            }
            col += printf ("%s", string);

            board.MakeMove (move, unmove[i]);
        }

        // Undo the changes we made to the board...
        while (i-- > 0)
        {
            board.UnmakeMove (path.m[i], unmove[i]);
        }

        printf ("\n");
    }
}


void ChessUI_xboard::PredictMate ( int /*numMoves*/ )
{
}

void ChessUI_xboard::NotifyUser ( const char * /*message*/ )
{
}

void ChessUI_xboard::ReportComputerStats (
    INT32   /*thinkTime*/,
    UINT32  /*nodesVisited*/,
    UINT32  /*nodesEvaluated*/,
    UINT32  /*nodesGenerated*/,
    int     /*fwSearchDepth*/,
    UINT32  /*vis*/ [NODES_ARRAY_SIZE],
    UINT32  /*gen*/ [NODES_ARRAY_SIZE] )
{
}

void ChessUI_xboard::ReportSpecial ( const char * /*message*/ )
{
}


/*
    $Log: uixboard.cpp,v $
    Revision 1.11  2009/12/08 21:59:22  Don.Cross
    Fixed embarrassing bugs in XChenard's timing code:
    I was using IsComputersTurn() to figure out whether to adjust the remaining moves in the current time period,
    but this was wrong because it in turn uses TheChessBoard to figure out whose turn it is; but TheChessBoard
    can be in a random state due to being called from the search!  Now the caller figures out whether or not the
    opponent's actual move has been received and recorded.
    Another problem was that I was finding out whose turn it was when I received the "time" command, which is received
    before the opponent's move has arrived!  A similar problem occurs at the beginning of the game, when the
    computer is playing neither White nor Black (yet).
    Also needed to reset the ply counter when the "new" command is received.

    Revision 1.10  2008/11/23 19:41:55  Don.Cross
    When we are pondering, and are about to display best path, check to see if we have already received
    the opponent's move that we were predicting.  If so, we should NOT include that move in the best path displayed,
    because it is no longer a hypothetical move.  In other words, from xboard's point of view, we are no longer pondering,
    but thinking about an actual position.
    Fixed bugs that caused MyRemainingMoves to not be in sync with the correct value.
    Worked on TimeBudgetForNextMove so that it does not let the time fall dangerously low.
    I have seen WinBoard display (!) after the time when it falls below one second, which just looks scary!

    Revision 1.9  2008/11/22 16:27:32  Don.Cross
    Massive overhaul to xchenard: now supports pondering.
    This required very complex changes, and will need a lot of testing before release to the public.
    Pondering support made it trivial to implement the "hint" command, so I added that too.
    Pondering required careful attention to every possible command we could receive from xboard,
    so now I have started explicitly sending back errors for any command we don't understand.
    I finally figured out how to get rid of the annoying /WP64 warning from the Microsoft C++ compiler.

    Revision 1.8  2008/11/21 03:16:45  Don.Cross
    I realized I could just call ComputerChessPlayer::getPredictedOpponentMove() instead of having to override a virtual method.
    It returns a move where dest==0 if there is no prediction.  This is much simpler!

    Revision 1.7  2008/11/21 03:05:16  Don.Cross
    First baby step toward pondering in xchenard: used a hack in the Chenard engine that supports multithreaded pondering in WinChen.
    We are not doing pondering the same way, but the hack allows us to share the part of WinChen that predicts the opponent's move.
    Did some other refactoring of code that may help finish pondering, but I'm still trying to figure out what I'm really going to do.

    Revision 1.6  2008/11/18 21:48:36  Don.Cross
    XChenard now supports the xboard commands post and nopost, to enable/disable display of thinking.

    Revision 1.5  2008/11/15 03:04:05  Don.Cross
    I think I have figured out how to make the Windows version of XChenard handle the "?" command
    for making a move immediately.  Needs more testing.  Basically I have conditional code for Linux or Windows.
    In Linux, I use the select() command as before, but now in Windows I start up a dedicated thread for
    processing lines of input from stdin and enqueuing them.  The main chess engine thread can enter a critical
    section and inspect the queue at any time to see if one or more lines of input is waiting.
    Needs more testing on Windows, and I also need to see if this will still compile and run correctly on Linux.
    Checking in tonight to back up my work before I go to bed.

    Revision 1.4  2008/11/06 22:25:06  Don.Cross
    Now ParseFancyMove accepts either PGN or longmove (e7e8q) style notation only.
    We tolerate of check '+' or checkmate '#' characters being absent or extraneous in the input.
    The former is especially important because I often see PGN notation where '+' is missing on checking moves.
    To allow for PGN input, I had to allow avoidance of the PromotePawn callback, so other code was changed as well.
    Version numbers are now always constently yyyy.mm.dd across all builds of Chenard!

    Revision 1.3  2008/11/02 22:23:02  Don.Cross
    Made tweaks so I can use xchenard to generate endgame databases.
    No longer disable SIGTERM and SIGINT: this had the effect of making it so I could not Ctrl+C to kill xchenard!
    These are no longer needed because I send back the feature commands to request xboard not doing that.

    Revision 1.2  2008/11/01 21:55:32  Don.Cross
    Xboard force move command '?' is working.
    Chenard can now play White or Black.
    Crude ability to obey the 'level' command for think time.

    Revision 1.1  2008/11/01 19:01:11  Don.Cross
    The Linux/xboard version of Chenard is starting to work!
    Right now it only knows how to play Black.
    There are a lot more xboard commands it needs to understand in order to be fully functional.

*/
