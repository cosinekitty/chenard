/*=========================================================================

    speak.cpp  -  Copyright (C) 1998-2005 by Don Cross

    This file contains a C++ function for speaking my chess moves,
    using a set of stock wave files with known filenames.
    This code works only in the Win32 environment.

=========================================================================*/

#include <assert.h>
#include <string.h>
#include <windows.h>
#include <mmsystem.h>
#include "chess.h"


static void PlayWav ( const char *prefix )
{
    char filename [256];
    strcpy ( filename, prefix );
    strcat ( filename, ".wav" );
    BOOL playResult = PlaySound ( filename, NULL, SND_FILENAME|SND_SYNC );
}


static void PlayWav ( char c )
{
    char prefix[2];
    prefix[0] = c;
    prefix[1] = '\0';
    PlayWav ( prefix );
}


void Speak (const char *pgn)
{
    int index = 0;

    if (pgn[0] == 'O') {
        // must be castling... either "O-O" or "O-O-O"
        if (pgn[1]=='-' && pgn[2]=='O') {
            if (pgn[3]=='-' && pgn[4]=='O') {
                PlayWav ("ooo");
                index = 5;	// skip over "O-O-O", in case there is a "+", etc.
            } else {
                PlayWav ("oo");
                index = 3;	// skip over "O-O", in case there is a "+", etc.
            }
        } else {
            assert (false);     // not valid PGN notation!
        }
    }

    char c;
    while ((c = pgn[index++]) != '\0') {
        switch (c) {
            case 'N':   PlayWav("knight");      break;
            case 'B':   PlayWav("bishop");      break;
            case 'R':   PlayWav("rook");        break;
            case 'Q':   PlayWav("queen");       break;
            case 'K':   PlayWav("king");        break;
            case 'x':   PlayWav("takes");       break;
            case '+':   PlayWav("check");       break;
            case '#':   PlayWav("mate");        break;
            case '=':   PlayWav("prom");        break;

            default:
                if ((c >= 'a' && c <= 'h') || (c >= '1' && c <= '8')) {
                    PlayWav (c);
                } else {
                    assert (false);     // unexpected character
                }
                break;
        }
    }
}


/*
    // This is the old code that worked with goofy move notation.
    // It no longer works because Chenard generates PGN notation now.

void Speak (const char *moveString)
{
    // Piece being moved...
    const char *p = moveString;
    char c = *p++;
    int castled = 0;
    switch ( c )
    {
        case 'N':   PlayWav("knight");      break;
        case 'B':   PlayWav("bishop");      break;
        case 'R':   PlayWav("rook");        break;
        case 'Q':   PlayWav("queen");       break;
        case 'K':   PlayWav("king");        break;
        case 'O':
        {
            castled = 1;
            // p now points to either "-O-O???" or "-O???"
            //                         01234
            if ( p[2] == '-' && p[3] == 'O' )
            {
                PlayWav ( "ooo" );
                p += 4;
            }
            else
            {
                PlayWav ( "oo" );
                p += 2;
            }
        }
        default:
        {
            if ( c >= 'a' && c <= 'h' )
            {
                PlayWav ( c );
                PlayWav ( *p++ );
            }
        }
    }

    if ( !castled )
    {
        if ( *p && *p!='-' && *p!='x' )
        {
            PlayWav ( "at" );
            while ( *p && *p!='-' && *p!='x' )
            {
                PlayWav ( *p++ );
            }
        }

        if ( *p == '-' )
        {
            PlayWav ( "movesto" );
            ++p;
        }
        else if ( *p == 'x' )
        {
            PlayWav ( "takes" );
            ++p;
        }

        switch ( c = *p++ )
        {
            case 'N':   PlayWav("knight");      break;
            case 'B':   PlayWav("bishop");      break;
            case 'R':   PlayWav("rook");        break;
            case 'Q':   PlayWav("queen");       break;
            case 'K':   PlayWav("king");        break;
            default:
            {
                if ( c >= 'a' && c <= 'h' )
                {
                    PlayWav ( c );
                    PlayWav ( *p++ );
                }
            }
        }
    }

    while ( *p )
    {
        if ( strcmp ( p, " mate" ) == 0 )
        {
            PlayWav ( "mate" );
            break;
        }
        else if ( strcmp ( p, " stale" ) == 0 )
        {
            PlayWav ( "stale" );
            break;
        }
        else if ( strcmp ( p, " e.p." ) == 0 )
        {
            PlayWav ( "ep" );
            break;
        }
        else switch ( c = *p++ )
        {
            case '/':   PlayWav ("prom");   break;
            case '+':   PlayWav ("check");  break;
            case 'N':   PlayWav ("knight"); break;
            case 'B':   PlayWav ("bishop"); break;
            case 'R':   PlayWav ("rook");   break;
            case 'Q':   PlayWav ("queen");  break;
        }
    }
}

*/


/*
    $Log: speak.cpp,v $
    Revision 1.4  2006/01/18 20:50:08  dcross
    The ability to speak move notation through the sound card was broken when
    I went from goofy move notation to PGN.  This is a quickie hack to make it
    work, though it's not perfect because it should say "moves to", and "at", but it doesn't.
    Also removed the board and move parameters to Speak(), because they were never used.

    Revision 1.3  2005/11/23 21:30:31  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:43  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!


        Revision history:
    
    1997 February 1 [Don Cross]
        Started writing.
    
*/

