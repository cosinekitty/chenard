/*=========================================================================

       wintime.cpp  -  Copyright (C) 1993-2005 by Don Cross

       Win32 version of ChessTime.  This function returns a
       time value expressed in hundredths of a second.
       The most important use of this function is to enable
       a timed search to know when to stop.

=========================================================================*/

#include <windows.h>

#include "chess.h"

INT32 ChessTime()
{
    static bool firstTime = true;
    static LARGE_INTEGER startTime;
    static LARGE_INTEGER performanceFrequency;

    LARGE_INTEGER pc;
    if (!QueryPerformanceCounter (&pc)) {
        ChessFatal ("Failure in QueryPerformanceCounter()");
    }

    if (firstTime) {
        startTime = pc;
        firstTime = false;

        if (!QueryPerformanceFrequency (&performanceFrequency)) {
            ChessFatal ("Failure in QueryPerformanceFrequency");
        }

        if (performanceFrequency.QuadPart <= 1000) {
            ChessFatal ("QueryPerformanceFrequency returned a value that is too small!");
        }

        performanceFrequency.QuadPart /= 100;       // instead of counts per second, we want counts per centisecond!

        return 0;
    } else {
        // WARNING:  This code can go 248.5513 days before wraping around the signed 32-bit integer.
        // That should be good enough for most uses, but it could cause problems some day.
        // If so, I will need to redesign ChessTime() to return a 64-bit integer.

        __int64 diff = pc.QuadPart - startTime.QuadPart;
        diff /= performanceFrequency.QuadPart;
        return (INT32) diff;
	}
}


/*
    $Log: wintime.cpp,v $
    Revision 1.4  2008/11/07 20:56:29  Don.Cross
    Reworked wintime.cpp so it uses QueryPerformanceCounter instead of GetTickCount.
    The only reason I did this was to increase the time of integer overflow from 49 days to 248 days.

    Revision 1.3  2005/11/23 21:30:33  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:45  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!



    Revision history:

    1999 February 12 [Don Cross]
         Updated to call the newer function 'GetTickCount()'.
         Also protected against clock wraparound by calculating
         all times as differences from the first call.
*/

