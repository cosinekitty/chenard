/*=============================================================================

    undopath.cpp  -  Copyright (C) 1998-2005 by Don Cross

=============================================================================*/

#include "chess.h"
#include "winchess.h"
#include "winguich.h"

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
    int initial = board.InitialPlyNumber();
    current = length = board.GetCurrentPlyNumber() - initial;

    if ( !moveList )
    {
        size = 1024;    // arbitrary but larger than necessary
        if (size < length) {
            size = 2 * length;
        }

        moveList = new Move [size];
    }

    for (int i = 0; i < length; i++)
    {
        moveList[i] = board.GetPastMove (i + initial);
    }
}


void MoveUndoPath::undo ( ChessBoard &board )
{
    // Allow undo only if board has not been edited
    if ( board.HasBeenEdited() )
    {
        if ( Global_UI )
        {
            //TODO: Display error dialog box
        }
        return;
    }

    if ( current < 2 )
    {
        if ( Global_UI )
        {
            //TODO: Display error dialog box
        }
        return;
    }

    board.Init();
    current -= 2;
    UnmoveInfo unmove;
    for ( int i=0; i<current; i++ )
    {
        board.MakeMove ( moveList[i], unmove );
    }
}


void MoveUndoPath::redo ( ChessBoard &board )
{
    // Allow undo only if board has not been edited
    if ( board.HasBeenEdited() )
    {
        if ( Global_UI )
        {
            //TODO: Display error dialog box
        }
        return;
    }

    if ( current + 2 > length )
    {
        return;
    }

    UnmoveInfo unmove;
    board.MakeMove ( moveList[current], unmove );
    board.MakeMove ( moveList[current+1], unmove );
    current += 2;
}


/*
    $Log: undopath.cpp,v $
    Revision 1.4  2008/11/13 15:29:05  Don.Cross
    Fixed bug: user can now Undo/Redo moves after loading a game from a PGN file.

    Revision 1.3  2005/11/23 21:30:32  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:44  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!


        Revision history:
    
    1997 January 30 [Don Cross]
        Started writing.
    
*/

