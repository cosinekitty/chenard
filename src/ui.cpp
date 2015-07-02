/*==========================================================================

      ui.cpp   -   Copyright (C) 1993-2005 by Don Cross

      Contains abstract class ChessUI.

==========================================================================*/

#include "chess.h"


ChessUI::ChessUI()
{
}


ChessUI::~ChessUI()
{
}


void ChessUI::DisplayBestMoveSoFar ( const ChessBoard &, Move, int )
{
}


void ChessUI::DisplayCurrentMove ( const ChessBoard &, Move, int )
{
}


void ChessUI::PredictMate ( int )
{
}


void ChessUI::ReportComputerStats (
    INT32, UINT32, UINT32, UINT32,
    int, UINT32[NODES_ARRAY_SIZE],
    UINT32[NODES_ARRAY_SIZE] )
{
}


/*
    $Log: ui.cpp,v $
    Revision 1.3  2005/11/23 21:30:31  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:43  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!


        Revision history:

    1999 January 5 [Don Cross]
        Updating coding style.

*/
