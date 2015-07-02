/*=======================================================================

    profiler.h  -  Copyright (C) 1998-2005 by Don Cross

    Contains an execution profiler for Chenard.
    This should help me figure out which parts of the code
    need to be optimized.

=======================================================================*/
#ifndef __ddc_chenard_profiler_h
#define __ddc_chenard_profiler_h

#define  PX_UNDEFINED    0
#define  PX_SEARCH       1
#define  PX_EVAL         2
#define  PX_GENMOVES     3
#define  PX_GENCAPS      4
#define  PX_MAKEMOVE     5
#define  PX_UNMOVE       6
#define  PX_CANMOVE      7
#define  PX_MOVEORDER    8
#define  PX_ATTACK       9
#define  PX_XPOS        10

#ifdef CHENARD_PROFILER
    void StartProfiler();
    void StopProfiler();

    // all of the PX_... symbols are indices into the profiler array.

    extern int ProfilerHitCount[];
    extern int ProfilerCallCount[];
    extern int ProfilerIndex;

    #define PROFILER_ENTER(newIndex)  \
        int ___oldIndex=ProfilerIndex; ProfilerIndex=(newIndex); ++ProfilerCallCount[newIndex];

    #define PROFILER_EXIT()  \
        ProfilerIndex = ___oldIndex;

#else   // CHENARD_PROFILER is not defined
    inline void StartProfiler() {}
    inline void StopProfiler() {}

    #define PROFILER_ENTER(newIndex) 
    #define PROFILER_EXIT() 
#endif  // CHENARD_PROFILER


#endif // __ddc_chenard_profiler_h

/*
    $Log: profiler.h,v $
    Revision 1.3  2005/11/23 21:30:31  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:43  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!


        Revision history:

    1998 December 5 [Don Cross]
        Updated comments to be more understandable.

    1997 June 10 [Don Cross]
        Started writing.
*/

