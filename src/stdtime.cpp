/*=======================================================================

       stdtime.cpp  -  Copyright (C) 1993-1996 by Don Cross
       email: dcross@intersrv.com
       WWW:   http://www.intersrv.com/~dcross/

       Portable (but horrible granularity) ChessTime() implementation.

=======================================================================*/

#include <time.h>
#include "chess.h"

INT32 ProgStartTime = 0;

INT32 ChessTime()
{
   return (INT32(time(0)) - ProgStartTime) * INT32(100);
}


class StdChessTimeInit
{
public:
    StdChessTimeInit()
    {
       ProgStartTime = time(0);
    }
};


static StdChessTimeInit KrankyKitty;

/*
    $Log: stdtime.cpp,v $
    Revision 1.1  2005/11/25 19:47:27  dcross
    Recovered lots of old chess source files from the dead!
    Found these on CDROM marked "14 September 1999".
    Added cvs log tag and moved old revision history after that.

*/

