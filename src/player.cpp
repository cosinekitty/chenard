/*=======================================================================

     player.cpp  -  Copyright (C) 1993-2005 by Don Cross

=======================================================================*/

#include "chess.h"

ChessPlayer::ChessPlayer ( ChessUI &ui )
    : userInterface(ui)
    , quitReason(qgr_resign)
{
}


ChessPlayer::~ChessPlayer()
{
}

/*
    $Log: player.cpp,v $
    Revision 1.3  2005/11/23 21:30:30  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:42  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!


         Revision history:
    
    1993 August 30 [Don Cross]
         Changing pointers to references in the interfaces where
         appropriate.
    
*/

