/*=========================================================================

     human.cpp  -  Copyright (C) 1993-2005 by Don Cross

     Contains the class HumanChessPlayer.

=========================================================================*/

#include "chess.h"


HumanChessPlayer::HumanChessPlayer ( ChessUI &ui ):
    ChessPlayer(ui),
    automaticSingularMove (false)
{
}


HumanChessPlayer::~HumanChessPlayer()
{
}


bool HumanChessPlayer::GetMove ( 
    ChessBoard &board,
    Move       &move,
    INT32      &timeSpent )
{
    MoveList legalMoves;
    int      i;
    int      source, dest;
    INT32    startTime = ChessTime();

    board.GenMoves ( legalMoves );
    if ( legalMoves.num==1 && automaticSingularMove )
    {
        userInterface.NotifyUser ( 
            "You have only one legal move - it will be made for you." );

        userInterface.DisplayMove ( board, move = legalMoves.m[0] );
        timeSpent = 0;
        return true;
    }

    for(;;)     // keep looping until UI returns a legal move
    {
        SQUARE promIndex = 0;
        if ( !userInterface.ReadMove ( board, source, dest, promIndex ) )
        {
            timeSpent = ChessTime() - startTime;
            return false;
        }

        // Regenerate legal moves in case they edited the board.
        board.GenMoves ( legalMoves );

        // The following check allows certain UI tricks to work...
        if ( dest == SPECIAL_MOVE_NULL ||
           (dest & SPECIAL_MOVE_MASK) == SPECIAL_MOVE_EDIT )
        {
            move.source = source;
            move.dest = dest;
            move.score = 0;
            timeSpent = ChessTime() - startTime;
            return true;
        }

        // This checks for pawn promotion and stuff like that.
        move.Fix ( board, source, dest, promIndex, userInterface );

        // See if the move we just read is legal!
        for ( i=0; i < legalMoves.num; i++ )
        {
            if ( move == legalMoves.m[i] )
            {
                // We found the move in the list of legal moves.
                // Therefore, the move must be legal.
                timeSpent = ChessTime() - startTime;
                return true;
            }
        }
    }
}


/*
    $Log: human.cpp,v $
    Revision 1.5  2008/11/06 22:25:06  Don.Cross
    Now ParseFancyMove accepts either PGN or longmove (e7e8q) style notation only.
    We tolerate of check '+' or checkmate '#' characters being absent or extraneous in the input.
    The former is especially important because I often see PGN notation where '+' is missing on checking moves.
    To allow for PGN input, I had to allow avoidance of the PromotePawn callback, so other code was changed as well.
    Version numbers are now always constently yyyy.mm.dd across all builds of Chenard!

    Revision 1.4  2006/01/18 19:58:11  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.3  2005/11/23 21:30:30  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:40  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!



         Revision history:
    
    1993 August 30 [Don Cross]
         Changing pointers to references in the interfaces where
         appropriate.
    
    1994 January 15 [Don Cross]
         Added check for SPECIAL_MOVE_NULL in HumanChessPlayer::GetMove().
    
    1994 January 30 [Don Cross]
         Added check for SPECIAL_MOVE_EDIT.
    
    1999 January 15 [Don Cross]
         Updated coding style.
    
    1999 January 16 [Don Cross]
         Adding "automatic singular move" feature.
         This causes the computer to automatically make a move for
         a human player when there is only one legal move.
         This is intended to be of use when I'm playing Chenard
         in timed games, so that the computer can immediately 
         start thinking about its next move.
*/

