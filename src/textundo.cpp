/*=============================================================================

    textundo.cpp  -  Copyright (C) 1998-2005 by Don Cross

=============================================================================*/

#include <stdio.h>

#include "chess.h"
#include "uistdio.h"

MoveUndoPath Global_MoveUndoPath;

MoveUndoPath::~MoveUndoPath()
{
    if ( moveList )
    {
        delete[] moveList;
        moveList = 0;
        current = length = size = 0;
    }
}


void MoveUndoPath::resetFromBoard ( const ChessBoard &board )
{
    if ( !moveList )
    {
        size = 1024;    // arbitrary but larger than necessary
        moveList = new Move [size];
        if ( !moveList )
        {
            ChessFatal ( "Out of memory allocating MoveUndoPath" );
            return;
        }
    }

    current = length = board.GetCurrentPlyNumber();
    for ( int i=0; i<length; i++ )
        moveList[i] = board.GetPastMove(i);
}


bool MoveUndoPath::undo ( ChessBoard &board )
{
    // Allow undo only if board has not been edited
    if ( board.HasBeenEdited() )
    {
        printf ( "!!! Cannot undo move because board has been edited!\n" );
        return false;
    }

    if ( current < 2 )
    {
        printf ( "!!! Cannot undo this early in the game!\n" );
        return false;
    }

    board.Init();
    current -= 2;
    UnmoveInfo unmove;
    for ( int i=0; i<current; i++ )
        board.MakeMove ( moveList[i], unmove );

    return true;
}


bool MoveUndoPath::redo ( ChessBoard &board )
{
    // Allow undo only if board has not been edited
    if ( board.HasBeenEdited() )
    {
        printf ( "!!! Cannot redo move because board has been edited!\n" );
        return false;
    }

    if ( current + 2 > length )
    {
        printf ( "!!! At end of redo list!\n" );
        return false;
    }

    UnmoveInfo unmove;
    board.MakeMove ( moveList[current], unmove );
    board.MakeMove ( moveList[current+1], unmove );
    current += 2;

    return true;
}


/*
    $Log: textundo.cpp,v $
    Revision 1.4  2006/01/18 19:58:13  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.3  2005/11/23 21:30:31  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:43  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!


        Revision history:

    1999 January 16 [Don Cross]
        Split off from file 'undopath.cpp' for text-mode version
        of Chenard.

    1997 January 30 [Don Cross]
        Started writing.

*/

