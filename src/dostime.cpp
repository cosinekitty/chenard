/*=========================================================================

       dostime.cpp  -  Copyright (C) 1993-1996 by Don Cross
       email: dcross@intersrv.com
       WWW:   http://www.intersrv.com/~dcross/

       MS-DOS version of chess time stuff.

=========================================================================*/

#include <dos.h>
#include "chess.h"

static unsigned long far *timer = (unsigned long far *) MK_FP(0,0x46C);
static unsigned long progStartTime;

INT32 ChessTime()
{
   INT32 t = ((*timer - progStartTime) * 1000) / 182;

   return t;
}


class DosTimeInitializer
{
public:
   DosTimeInitializer()
   {
      progStartTime = *timer;
   }
};


static DosTimeInitializer Kudabuda;


/*
    $Log: dostime.cpp,v $
    Revision 1.1  2005/11/25 19:47:27  dcross
    Recovered lots of old chess source files from the dead!
    Found these on CDROM marked "14 September 1999".
    Added cvs log tag and moved old revision history after that.

*/

