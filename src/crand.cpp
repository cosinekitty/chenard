/*==========================================================================

      crand.cpp  -  Copyright (C) 1993-2005 by Don Cross

      Contains random number generator for chess.

==========================================================================*/

#include <stdlib.h>
#include <time.h>
#include "chess.h"


#define MULTIPLIER      0x015a4e35L
#define INCREMENT       1

static  INT32    Seed = 1;


static INT16 _ChessRandom ()
{
    Seed = MULTIPLIER * Seed + INCREMENT;
	INT16 r = INT16(Seed >> 16) & 0x7fff;
	if (r < 0) {
		ChessFatal ("Negative value returned by _ChessRandom");
	}
    return r;
}


int ChessRandom ( int n )
{
    static int firstTime = 1;
    if ( firstTime )
    {
        firstTime = 0;
        Seed = INT32 ( time(NULL) );
    }
    
    if ( n <= 0 )
    {
        ChessFatal ( "Non-positive argument to ChessRandom()!" );
        return 0;
    }

    int r = int(_ChessRandom()) % n;
	if (r<0 || r>=n) {
		ChessFatal ("ChessRandom is broken!");
	}
	return r;
}


/*
    $Log: crand.cpp,v $
    Revision 1.4  2006/03/19 20:27:12  dcross
    Still trying to find bug that causes Chenard to occasionally delay a long time when first using experience tree.
    All I have found so far is what looked like a negative number being returned by ChessRandom,
    but I cannot reproduce it.  So I just stuck in ChessFatal calls to see if it ever happens again.
    Also found a case where we open the experience tree twice by creating a redundant LearnTree object.
    I removed the unnecessary redundancy, thinking maybe trying to open the file twice is causing some
    kind of delay.  So far this is just shooting in the dark!

    Revision 1.3  2005/11/23 21:30:29  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:38  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!


    Revision history:
    
    1999 February 14 [Don Cross]
         Added code to call ChessFatal() if argument to ChessRandom()
         is non-positive.
         Changing time-based seed initializer from global variable's
         constructor to first time flag.
    
*/

