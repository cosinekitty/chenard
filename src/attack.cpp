/*==========================================================================

     attack.cpp  -  Copyright (C) 1993-2005 by Don Cross

     Contains code to see if a given square on a ChessBoard
     is attacked by a given side.

==========================================================================*/

#include "chess.h"
#include "profiler.h"


bool ChessBoard::IsAttackedByWhite ( int offset ) const
{
    PROFILER_ENTER (PX_ATTACK);
    const SQUARE * const base = board + offset;
    const SQUARE *disp;

    // Look for knight attacks...

    if ( inventory[WN_INDEX] > 0 )
    {
        if (    (base [ OFFSET( 1, 2) ] & WN_MASK) 
             || (base [ OFFSET( 1,-2) ] & WN_MASK) 
             || (base [ OFFSET(-1, 2) ] & WN_MASK) 
             || (base [ OFFSET(-1,-2) ] & WN_MASK) 
             || (base [ OFFSET( 2, 1) ] & WN_MASK) 
             || (base [ OFFSET( 2,-1) ] & WN_MASK) 
             || (base [ OFFSET(-2, 1) ] & WN_MASK) 
             || (base [ OFFSET(-2,-1) ] & WN_MASK) )
        {
            PROFILER_EXIT();
            return true;
        }
    }

    const int queensAndBishops = inventory[WQ_INDEX] + inventory[WB_INDEX];

    //-------------------------------------- SOUTHEAST
    if ( base[SOUTHEAST] == EMPTY )
    {
        if ( queensAndBishops > 0 )
        {
            for ( disp = base + 2*SOUTHEAST; *disp == EMPTY; disp += SOUTHEAST );
            if ( *disp & (WQ_MASK | WB_MASK) )
            {
                PROFILER_EXIT();
                return true;
            }
        }
    }
    else if ( base[SOUTHEAST] & (WP_MASK | WB_MASK | WQ_MASK | WK_MASK) )
    {
        PROFILER_EXIT();
        return true;
    }

    //-------------------------------------- SOUTHWEST
    if ( base[SOUTHWEST] == EMPTY )
    {
        if ( queensAndBishops > 0 )
        {
            for ( disp = base + 2*SOUTHWEST; *disp == EMPTY; disp += SOUTHWEST );
            if ( *disp & (WQ_MASK | WB_MASK) )
            {
                PROFILER_EXIT();
                return true;
            }
        }
    }
    else if ( base[SOUTHWEST] & (WP_MASK | WB_MASK | WQ_MASK | WK_MASK) )
    {
        PROFILER_EXIT();
        return true;
    }

    //-------------------------------------- SOUTH
    const int queensAndRooks = inventory[WQ_INDEX] + inventory[WR_INDEX];

    if ( base[SOUTH] == EMPTY )
    {
        if ( queensAndRooks > 0 )
        {
            for ( disp = base + 2*SOUTH; *disp == EMPTY; disp += SOUTH );
            if ( *disp & (WQ_MASK | WR_MASK) )
            {
                PROFILER_EXIT();
                return true;
            }
        }
    }
    else if ( base[SOUTH] & (WQ_MASK | WK_MASK | WR_MASK) )
    {
        PROFILER_EXIT();
        return true;
    }

    //-------------------------------------- EAST
    if ( base[EAST] == EMPTY )
    {
        if ( queensAndRooks > 0 )
        {
            for ( disp = base + 2*EAST; *disp == EMPTY; disp += EAST );
            if ( *disp & (WQ_MASK | WR_MASK) )
            {
                PROFILER_EXIT();
                return true;
            }
        }
    }
    else if ( base[EAST] & (WQ_MASK | WK_MASK | WR_MASK) )
    {
        PROFILER_EXIT();
        return true;
    }

    //-------------------------------------- WEST
    if ( base[WEST] == EMPTY )
    {
        if ( queensAndRooks > 0 )
        {
            for ( disp = base + 2*WEST; *disp == EMPTY; disp += WEST );
            if ( *disp & (WQ_MASK | WR_MASK) )
            {
                PROFILER_EXIT();
                return true;
            }
        }
    }
    else if ( base[WEST] & (WQ_MASK | WK_MASK | WR_MASK) )
    {
        PROFILER_EXIT();
        return true;
    }

    //-------------------------------------- NORTHEAST
    if ( base[NORTHEAST] == EMPTY )
    {
        if ( queensAndBishops > 0 )
        {
            for ( disp = base + 2*NORTHEAST; *disp == EMPTY; disp += NORTHEAST );
            if ( *disp & (WQ_MASK | WB_MASK) )
            {
                PROFILER_EXIT();
                return true;
            }
        }
    }
    else if ( base[NORTHEAST] & (WQ_MASK | WK_MASK | WB_MASK) )
    {
        PROFILER_EXIT();
        return true;
    }

    //-------------------------------------- NORTHWEST
    if ( base[NORTHWEST] == EMPTY )
    {
        if ( queensAndBishops > 0 )
        {
            for ( disp = base + 2*NORTHWEST; *disp == EMPTY; disp += NORTHWEST );
            if ( *disp & (WQ_MASK | WB_MASK) )
            {
                PROFILER_EXIT();
                return true;
            }
        }
    }
    else if ( base[NORTHWEST] & (WQ_MASK | WK_MASK | WB_MASK) )
    {
        PROFILER_EXIT();
        return true;
    }

    //-------------------------------------- NORTH
    if ( base[NORTH] == EMPTY )
    {
        if ( queensAndRooks > 0 )
        {
            for ( disp = base + 2*NORTH; *disp == EMPTY; disp += NORTH );
            if ( *disp & (WQ_MASK | WR_MASK) )
            {
                PROFILER_EXIT();
                return true;
            }
        }
    }
    else if ( base[NORTH] & (WQ_MASK | WK_MASK | WR_MASK) )
    {
        PROFILER_EXIT();
        return true;
    }

    PROFILER_EXIT();
    return false;
}


//--------------------------------------------------------------------------


bool ChessBoard::IsAttackedByBlack ( int offset ) const
{
    PROFILER_ENTER (PX_ATTACK);
    const SQUARE * const base = board + offset;
    const SQUARE *disp;

    // Look for knight attacks...

    if ( inventory[BN_INDEX] > 0 )
    {
        if (    (base [ OFFSET( 1, 2) ] & BN_MASK) 
             || (base [ OFFSET( 1,-2) ] & BN_MASK) 
             || (base [ OFFSET(-1, 2) ] & BN_MASK) 
             || (base [ OFFSET(-1,-2) ] & BN_MASK) 
             || (base [ OFFSET( 2, 1) ] & BN_MASK) 
             || (base [ OFFSET( 2,-1) ] & BN_MASK) 
             || (base [ OFFSET(-2, 1) ] & BN_MASK) 
             || (base [ OFFSET(-2,-1) ] & BN_MASK) )
        {
            PROFILER_EXIT();
            return true;
        }
    }

    const int queensAndBishops = inventory[BQ_INDEX] + inventory[BB_INDEX];

    //-------------------------------------- NORTHEAST
    if ( base[NORTHEAST] == EMPTY )
    {
        if ( queensAndBishops > 0 )
        {
            for ( disp = base + 2*NORTHEAST; *disp == EMPTY; disp += NORTHEAST );
            if ( *disp & (BQ_MASK | BB_MASK) )
            {
                PROFILER_EXIT();
                return true;
            }
        }
    }
    else if ( base[NORTHEAST] & (BQ_MASK | BK_MASK | BB_MASK | BP_MASK) )
    {
        PROFILER_EXIT();
        return true;
    }

    //-------------------------------------- NORTHWEST
    if ( base[NORTHWEST] == EMPTY )
    {
        if ( queensAndBishops > 0 )
        {
            for ( disp = base + 2*NORTHWEST; *disp == EMPTY; disp += NORTHWEST );
            if ( *disp & (BQ_MASK | BB_MASK) )
            {
                PROFILER_EXIT();
                return true;
            }
        }
    }
    else if ( base[NORTHWEST] & (BQ_MASK | BK_MASK | BB_MASK | BP_MASK) )
    {
        PROFILER_EXIT();
        return true;
    }

    //-------------------------------------- NORTH
    const int queensAndRooks = inventory[BQ_INDEX] + inventory[BR_INDEX];

    if ( base[NORTH] == EMPTY )
    {
        if ( queensAndRooks > 0 )
        {
            for ( disp = base + 2*NORTH; *disp == EMPTY; disp += NORTH );
            if ( *disp & (BQ_MASK | BR_MASK) )
            {
                PROFILER_EXIT();
                return true;
            }
        }
    }
    else if ( base[NORTH] & (BQ_MASK | BK_MASK | BR_MASK) )
    {
        PROFILER_EXIT();
        return true;
    }

    //-------------------------------------- EAST
    if ( base[EAST] == EMPTY )
    {
        if ( queensAndRooks > 0 )
        {
            for ( disp = base + 2*EAST; *disp == EMPTY; disp += EAST );
            if ( *disp & (BQ_MASK | BR_MASK) )
            {
                PROFILER_EXIT();
                return true;
            }
        }
    }
    else if ( base[EAST] & (BQ_MASK | BK_MASK | BR_MASK) )
    {
        PROFILER_EXIT();
        return true;
    }

    //-------------------------------------- WEST
    if ( base[WEST] == EMPTY )
    {
        if ( queensAndRooks > 0 )
        {
            for ( disp = base + 2*WEST; *disp == EMPTY; disp += WEST );
            if ( *disp & (BQ_MASK | BR_MASK) )
            {
                PROFILER_EXIT();
                return true;
            }
        }
    }
    else if ( base[WEST] & (BQ_MASK | BK_MASK | BR_MASK) )
    {
        PROFILER_EXIT();
        return true;
    }

    //-------------------------------------- SOUTHEAST
    if ( base[SOUTHEAST] == EMPTY )
    {
        if ( queensAndBishops > 0 )
        {
            for ( disp = base + 2*SOUTHEAST; *disp == EMPTY; disp += SOUTHEAST );
            if ( *disp & (BQ_MASK | BB_MASK) )
            {
                PROFILER_EXIT();
                return true;
            }
        }
    }
    else if ( base[SOUTHEAST] & (BB_MASK | BQ_MASK | BK_MASK) )
    {
        PROFILER_EXIT();
        return true;
    }

    //-------------------------------------- SOUTHWEST
    if ( base[SOUTHWEST] == EMPTY )
    {
        if ( queensAndBishops > 0 )
        {
            for ( disp = base + 2*SOUTHWEST; *disp == EMPTY; disp += SOUTHWEST );
            if ( *disp & (BQ_MASK | BB_MASK) )
            {
                PROFILER_EXIT();
                return true;
            }
        }
    }
    else if ( base[SOUTHWEST] & (BB_MASK | BQ_MASK | BK_MASK) )
    {
        PROFILER_EXIT();
        return true;
    }

    //-------------------------------------- SOUTH
    if ( base[SOUTH] == EMPTY )
    {
        if ( queensAndRooks > 0 )
        {
            for ( disp = base + 2*SOUTH; *disp == EMPTY; disp += SOUTH );
            if ( *disp & (BQ_MASK | BR_MASK) )
            {
                PROFILER_EXIT();
                return true;
            }
        }
    }
    else if ( base[SOUTH] & (BQ_MASK | BK_MASK | BR_MASK) )
    {
        PROFILER_EXIT();
        return true;
    }

    PROFILER_EXIT();
    return false;
}


/*
    $Log: attack.cpp,v $
    Revision 1.4  2006/01/18 19:58:10  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.3  2005/11/23 21:30:29  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:37  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!



         Revision history:

    1993 October 15 [Don Cross]
         Fixed bug where I had a 'SOUTHEAST' where I meant to have
         a 'SOUTHWEST'.

    1994 February 10 [Don Cros]
         Modified to support piece indexes.

    1996 July 28 [Don Cross]
         Adding optimizations based on piece inventory.
         Should help out a lot in endgame positions.

    1999 January 19 [Don Cross]
         Updating coding style.
*/

