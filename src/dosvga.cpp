/*=========================================================================

      dosvga.cpp  -  Copyright (C) 1993-1996 by Don Cross
      email: dcross@intersrv.com
      WWW:   http://www.intersrv.com/~dcross/

      Contains main() for MS-DOS VGA version of Chenard.

=========================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <conio.h>

#include "chess.h"
#include "uivga.h"

unsigned _stklen = 0x6000;

text_info StartupTextInfo;

void Advertise()
{
   printf ( "\n"
            "Chenard / MS-DOS VGA version (22 June 1997)\n"
            "A chess program by Don Cross <dcross@intersrv.com>\n"
            "http://www.intersrv.com/~dcross\n\n" );
}


void ExitCode()
{
    textmode ( StartupTextInfo.currmode );
    Advertise();
}


#pragma argsused
int main ( int argc, char *argv[] )
{
   gettextinfo ( &StartupTextInfo );
   atexit ( ExitCode );

   extern bool sound_flag;
   sound_flag = false;

   extern int Learn_Output;
   Learn_Output = 0;
   Advertise();

   ChessBoard       theBoard;
   ChessUI_dos_vga  theUserInterface;
   ChessGame theGame ( theBoard, theUserInterface );
   theGame.Play();

   Advertise();

   return 0;
}


void ChessFatal ( const char *message )
{
   fprintf ( stderr, "Chess fatal:\n%s\n", message );

   FILE *errorLog = fopen ( "doscga.err", "wt" );
   if ( errorLog )
   {
      fprintf ( errorLog, "Chess fatal:\n%s\n", message );
      fclose ( errorLog );
   }

   exit(1);
}


/*
    $Log: dosvga.cpp,v $
    Revision 1.2  2006/01/18 19:58:11  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.1  2005/11/25 19:47:27  dcross
    Recovered lots of old chess source files from the dead!
    Found these on CDROM marked "14 September 1999".
    Added cvs log tag and moved old revision history after that.

    
     Revision history:

1996 March 6 [Don Cross]
     Added code to preserve original video mode upon exit.

1996 March 2 [Don Cross]
     Copied from doscga.cpp to make a new VGA version of Chenard.

1996 February 28 [Don Cross]
     Put in my new e-mail address, etc.

1993 August 30 [Don Cross]
     Changing pointers to references in the interfaces where
     appropriate.
    
*/

