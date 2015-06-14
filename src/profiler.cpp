/*=======================================================================

    profiler.cpp  -  Copyright (C) 1998-2005 by Don Cross

    Contains an execution profiler for Chenard.
    This should help me figure out which parts of the code
    need to be optimized.

=======================================================================*/
#include <windows.h>

#include "chess.h"
#include "winguich.h"
#include "profiler.h"

#define PROFILE_ARRAY_SIZE 64
int ProfilerHitCount [PROFILE_ARRAY_SIZE];
int ProfilerCallCount [PROFILE_ARRAY_SIZE];
int ProfilerIndex = PX_UNDEFINED;
static int TimerEnabledFlag = 0;

#ifdef CHENARD_PROFILER

void StartProfiler()
{
    // reset the table of counters...

    for ( int i=0; i<PROFILE_ARRAY_SIZE; i++ )
        ProfilerHitCount[i] = ProfilerCallCount[i] = 0;

    ProfilerIndex = PX_UNDEFINED;       

    // start the Win32 timer...
    
    TimerEnabledFlag = (SetTimer(HwndMain,1,55,NULL) != NULL);
}


void StopProfiler()
{
    if ( TimerEnabledFlag )
    {
        KillTimer ( HwndMain, 1 );
    }
}

#endif

/*
    $Log: profiler.cpp,v $
    Revision 1.3  2005/11/23 21:30:31  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:42  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!


        Revision history:
    
    1997 June 10 [Don Cross]
        Started writing.
    
*/

