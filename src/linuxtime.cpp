/*=======================================================================

     linuxtime.cpp  -  Copyright (C) 1993-1999 by Don Cross
     email: dcross@intersrv.com
     WWW:   http://www.intersrv.com/~dcross/

     Version of ChessTime() for Linux version of Chenard.

=======================================================================*/

#include <sys/time.h>
#include "chess.h"


static double GetAbsoluteTime()
{
    timeval now;
    gettimeofday ( &now, 0 );
    return 100.0*double(now.tv_sec) + double(now.tv_usec)/10000.0;
}


INT32 ChessTime()
{
    static double baseTime = 0;     // abs time at first call to ChessTime()
    static bool firstCall = true;   // is this the first call to ChessTime()?

    if ( firstCall )
    {
        firstCall = false;
        baseTime = GetAbsoluteTime();
        return 0;
    }

    return INT32 ( GetAbsoluteTime() - baseTime );
}


/*
    $Log: linuxtime.cpp,v $
    Revision 1.2  2008/11/07 20:31:54  Don.Cross
    Changed int to bool for local variable firstCall.

    Revision 1.1  2005/11/25 19:47:27  dcross
    Recovered lots of old chess source files from the dead!
    Found these on CDROM marked "14 September 1999".
    Added cvs log tag and moved old revision history after that.


Revision history:

1999 January 15 [Don Cross]
     Found out that 'bool' type is still not supported on certain
     Unix C++ compilers (e.g. Sun Solaris).  Replacing 'bool' with
     'int', etc.

1999 January 1 [Don Cross]
     Fixed bug where ChessTime() was "wrapping around" to
     absurdly large values.  Was incorrectly using the clock()
     function, which measures processor time, not real time.
     Doesn't look like constructor for static object was being called,
     perhaps because the object was not being used anywhere (???).

1998 December 25 [Don Cross]
     Started writing.

*/

