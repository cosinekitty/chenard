/*===========================================================================

      dispgame.cpp  -  Copyright (C) 1993-1998 by Don Cross
      email: dcross@intersrv.com
      WWW:   http://www.intersrv.com/~dcross/

      Utility to dump a NewChess CHESS.GAM file to stdout.

===========================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include "chess.h"

int main ( int argc, char *argv[] )
{
    if ( argc < 2 || argc > 3 )
    {
        fprintf ( stderr, "Use:  DISPGAME game_in_file [text_out_file]\n" );
        return 1;
    }

    const char *infilename = argv[1];

    FILE *infile = fopen ( argv[1], "rb" );

    if ( !infile )
    {
        fprintf ( stderr, "Error:  Cannot open input file '%s'\n", infilename );
        return 1;
    }

    FILE *outfile = stdout;

    if ( argc == 3 )
    {
        outfile = fopen ( argv[2], "wt" );
        if ( !outfile )
        {
            fprintf ( stderr, "Error: Cannot open output file '%s'\n", argv[2] );
            fclose(infile);
            return 1;
        }
    }

    ChessBoard  board;
    Move        move;
    UnmoveInfo  unmove;
    int         movenum = 1;
    char        moveString [MAX_MOVE_STRLEN + 1];

    fprintf ( outfile, "      %-15s %-15s\n\n", "White", "Black" );

    while ( fread ( &move, sizeof(Move), 1, infile ) == 1 )
    {
        FormatChessMove ( board, move, moveString );

        if ( board.WhiteToMove() )
        {
            fprintf ( outfile, "%3d.  %-15s", movenum, moveString );
            board.MakeWhiteMove ( move, unmove, true, true );
        }
        else
        {
            fprintf ( outfile, " %-15s\n", moveString );
            board.MakeBlackMove ( move, unmove, true, true );
            ++movenum;
        }
    }

    fprintf ( outfile, "\n" );

    fclose ( infile );

    if ( argc != 3 )
    {
        fclose ( outfile );
    }

    return 0;
}


void ChessFatal ( const char *message )
{
    fprintf ( stderr, "Fatal chess error: %s\n", message );
    exit(1);
}


/*
    $Log: dispgame.cpp,v $
    Revision 1.2  2006/01/18 19:58:11  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.1  2005/11/25 19:47:26  dcross
    Recovered lots of old chess source files from the dead!
    Found these on CDROM marked "14 September 1999".
    Added cvs log tag and moved old revision history after that.

*/

