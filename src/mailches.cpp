/*=========================================================================

      mailches.cpp  -  Copyright (C) 1993-1996 by Don Cross
      email: dcross@intersrv.com
      WWW:   http://www.intersrv.com/~dcross/

      Contains main() for e-mail based correspondence
      chess game MAILCHES.EXE.

=========================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <string.h>
#include <dos.h>
#include <graphics.h>
#include <ctype.h>
#include <bios.h>

#include "chess.h"
#include "uicga.h"

unsigned _stklen = 0x2000;

const char * const MailChess_Version = "1.01";


const char * const ChessDataStart =
    "----- beginning of chess data (do not edit) -----\n";

const char * const ChessDataEnd =
    "----- end of chess data -----\n";

static char ExitMessage [256];


// The following function outputs a human-readable
// version of the chess game to the given output file.

void ListChessGame ( const ChessBoard &theBoard, FILE *out )
{
    fprintf ( out, "\n      WHITE               BLACK\n\n" );

    ChessBoard temp;
    unsigned n = theBoard.GetCurrentPlyNumber();
    for ( unsigned i=0; i < n; i++ )
    {
        Move move = theBoard.GetPastMove(i);
        char movestr [MAX_MOVE_STRLEN + 1];
        FormatChessMove ( temp, move, movestr );
        if ( temp.WhiteToMove() )
        {
            fprintf ( out, "%3u.  %-20s", i/2 + 1, movestr );
        }
        else
        {
            fprintf ( out, "%s\n", movestr );
        }

        UnmoveInfo unmove;
        temp.MakeMove ( move, unmove );
    }

    if ( n & 1 )
    {
        fprintf ( out, "\n" );
    }

    fprintf ( out, "\n\n" );

    for ( int y=7; y >= 0; y-- )
    {
        fprintf ( out, "      " );

        for ( int x=0; x <= 7; x++ )
        {
            SQUARE p = theBoard.GetSquareContents ( x, y );
            char c = '?';
            switch ( p )
            {
            case EMPTY:      c = '.';   break;
            case WPAWN:      c = 'P';   break;
            case WKNIGHT:    c = 'N';   break;
            case WBISHOP:    c = 'B';   break;
            case WROOK:      c = 'R';   break;
            case WQUEEN:     c = 'Q';   break;
            case WKING:      c = 'K';   break;
            case BPAWN:      c = 'p';   break;
            case BKNIGHT:    c = 'n';   break;
            case BBISHOP:    c = 'b';   break;
            case BROOK:      c = 'r';   break;
            case BQUEEN:     c = 'q';   break;
            case BKING:      c = 'k';   break;
            }

            fprintf ( out, "%c", c );
        }

        fprintf ( out, " %d", y+1 );

        if ( y == 3 )
        {
            const char *side = theBoard.WhiteToMove() ? "White" : "Black";
            fprintf ( out, "    %s to move", side );
        }

        fprintf ( out, "\n" );
    }

    fprintf ( out, "\n      abcdefgh\n\n" );
}


bool ReadEmail ( ChessUI *ui, ChessBoard &board, const char *filename )
{
    board.Init();   // reset the chess board to beginning of game

    FILE *f = fopen ( filename, "rt" );

    if ( !f )
    {
        return false;
    }

    char line [128];

    // Search through file for start-of-data marker...
    while ( fgets ( line, sizeof(line), f ) != NULL )
    {
        if ( strcmp ( line, ChessDataStart ) == 0 )
        {
            // Found it!
            // Now start scanning the moves in...

            int n = 0;
            if ( fscanf ( f, "%d", &n ) != 1 )
            {
                break;
            }

            if ( n & 1 )
            {
                // It's Black's turn

                extern int whites_view;
                whites_view = 0;
            }

            if ( ui && !ui->Activate() )
            {
                return false;
            }

            for ( int i=0; i < n; i++ )
            {
                unsigned source, dest;
                fscanf ( f, "%x %x", &source, &dest );

                Move move;
                move.source = BYTE (source);
                move.dest   = BYTE (dest);
                move.score  = 0;

                if ( ui && i >= n-2 )
                {
                    if ( i == n-1 )
                    {
                        ui->DrawBoard ( board );
                        delay (1000);
                        ui->DisplayMove ( board, move );
                        ui->RecordMove ( board, move, 0 );
                    }
                }

                UnmoveInfo unmove;
                board.MakeMove ( move, unmove );
            }

            fclose ( f );
            return true;
        }
    }

    fclose ( f );

    fprintf ( stderr,
              "Error:  The file '%s' exists but has no valid chess data in it!\n",
              filename );

    exit(1);

    return false;   // Make the compiler happy
}


void WriteEmail ( const ChessBoard &board, const char *filename )
{
    FILE *f = fopen ( filename, "wt" );
    if ( !f )
    {
        sprintf ( ExitMessage, "Error opening file '%s' for write!\n", filename );
        exit(1);
    }

    ListChessGame ( board, f );
    fprintf ( f, "\n\n%s", ChessDataStart );

    unsigned n = board.GetCurrentPlyNumber();

    fprintf ( f, "%u\n", n );

    for ( unsigned i=0; i < n; i++ )
    {
        Move move = board.GetPastMove ( i );
        // Write source, dest codes as ASCII data...

        fprintf ( f, "%02X %02X\n", unsigned(move.source), unsigned(move.dest) );
    }

    fprintf ( f, "%s", ChessDataEnd );
    fclose ( f );
}


static text_info StartupTextInfo;


void exitcode()
{
    closegraph();

    if ( StartupTextInfo.screenheight > 25 )
    {
        textmode ( C4350 );
    }
    else
    {
        textmode ( StartupTextInfo.currmode );
    }
    printf ( "%s\n", ExitMessage );
}


void HelpUser_MakeMove()
{
    int x = 60;
    int y = 1;
    gotoxy ( x, y++ );
    printf ( "MailChess v %s", MailChess_Version );
    gotoxy ( x, y++ );
    printf ( "by Don Cross" );
    gotoxy ( x, y++ );
    printf ( "<dcross@intersrv.com>" );

    y = 10;
    gotoxy ( x, y++ );
    printf ( "Use arrow keys and" );
    gotoxy ( x, y++ );
    printf ( "Enter key to move." );
    gotoxy ( x, y++ );

    y++;
    gotoxy ( x, y++ );
    printf ( "Press Esc to cancel" );
    gotoxy ( x, y++ );
    printf ( "and exit MailChess." );
}


void HelpUser_EndOfGame ( ChessSide winner )
{
    int x = 60;
    int y = 1;
    gotoxy ( x, y++ );
    printf ( "MailChess v %s", MailChess_Version );
    gotoxy ( x, y++ );
    printf ( "by Don Cross" );
    gotoxy ( x, y++ );
    printf ( "<dcross@intersrv.com>" );

    y++;
    gotoxy ( x, y++ );
    switch ( winner )
    {
    case SIDE_WHITE:
        printf ( "White wins." );
        break;

    case SIDE_BLACK:
        printf ( "Black wins." );
        break;

    default:
        printf ( "Draw game." );
    }
    y++;

    gotoxy ( x, y++ );
    printf ( "Press Esc to exit." );
}


#pragma argsused
int main ( int argc, char *argv[] )
{
    gettextinfo ( &StartupTextInfo );
    atexit ( exitcode );

    extern bool sound_flag;
    sound_flag = true;

    printf (
        "\n"
        "MailChess v %s by Don Cross <dcross@intersrv.com>, 1 March 1996.\n"
        "http://www.intersrv.com/~dcross\n"
        "\n",
        MailChess_Version );

    const char * const help =
        "Use:  MAILCHES mailfile\n"
        "\n"
        "This program helps two people play chess by e-mail.  MailChess reads\n"
        "and writes a text file which can be sent back and forth by email.\n"
        "MailChess ignores e-mail headers and other extraneous text.\n"
        "\n"
        "If 'mailfile' exists, it is scanned for chess data and MailChess tries\n"
        "to restore a game in progress so that the next move can be made.\n"
        "\n"
        "If 'mailfile' does not exist, MailChess assumes that you want to\n"
        "start a new game, and that you will play White.\n"
        "In this case, you will make your first move and MailChess will\n"
        "create the file 'mailfile'.\n";

//  "If the optional parameter 'listfile' is specified, MailChess will\n"
//  "read the game encoded in 'mailfile' and create a human-readable\n"
//  "listing of the game in 'listfile', then immediately exit.\n";

    if ( argc != 2 && argc != 3 )
    {
        printf ( "%s", help );
        return 1;
    }

    ChessBoard theBoard;

    if ( argc == 3 )
    {
        if ( !ReadEmail ( 0, theBoard, argv[1] ) )
        {
            fprintf ( stderr, "No valid chess data in file '%s'\n", argv[1] );
            return 1;
        }

        FILE *out = fopen ( argv[2], "wt" );
        if ( !out )
        {
            fprintf ( stderr, "Cannot open file '%s' for write!\n", argv[2] );
            return 1;
        }

        ListChessGame ( theBoard, out );

        fclose ( out );
        printf ( "Game listing written to file '%s'\n", argv[2] );
        return 0;
    }

    ChessUI_dos_cga theUserInterface;

    ChessPlayer *player = new HumanChessPlayer ( theUserInterface );
    if ( !player )
    {
        fprintf ( stderr, "Error:  Not enough memory to run this program!\n" );
        return 1;
    }

    if ( !ReadEmail ( &theUserInterface, theBoard, argv[1] ) )
    {
        fprintf ( stderr, "The file '%s' does not exist.\n", argv[1] );
        fprintf ( stderr, "Do you want to start a new game and create '%s'?\n", argv[1] );
        fprintf ( stderr, "(y/n):  " );
        fflush ( stderr );
        char answer [20];
        answer[0] = '\0';
        fgets ( answer, sizeof(answer), stdin );
        if ( answer[0]!='y' && answer[0]!='Y' )
        {
            delete player;
            return 1;
        }
    }

    MoveList legalMoveList;
    ChessSide winner = SIDE_NEITHER;
    if ( theBoard.WhiteToMove() )
    {
        theBoard.GenWhiteMoves ( legalMoveList );
        if ( legalMoveList.num == 0 )
        {
            winner = theBoard.WhiteInCheck() ? SIDE_BLACK : SIDE_NEITHER;
        }
    }
    else
    {
        theBoard.GenBlackMoves ( legalMoveList );
        if ( legalMoveList.num == 0 )
        {
            winner = theBoard.BlackInCheck() ? SIDE_WHITE : SIDE_NEITHER;
        }
    }

    bool gameIsOver = (legalMoveList.num == 0);
    if ( !gameIsOver )
    {
        // Look for definite draws...

        if ( theBoard.IsDefiniteDraw(3) )
        {
            winner = SIDE_NEITHER;
            gameIsOver = true;
        }
    }

    // Now, 'gameIsOver' definitely has the correct value.

    if ( gameIsOver )
    {
        // Display the ending state of the game and let the
        // user gloat/sulk/sigh.

        HelpUser_EndOfGame ( winner );
        while ( (bioskey(0) & 0xff) != 0x1b );
        sprintf ( ExitMessage,
                  "This game is over.  MailChess has not changed '%s'.\n",
                  argv[1] );
    }
    else
    {
        // Read a single move from the user.
        // Ask the user if he/she is sure afterward.
        // If not, cancel the move and let them do another.
        // Otherwise, commit to the move, write the updated game
        // state to the text file, and exit.

        HelpUser_MakeMove();

        for(;;)
        {
            Move move;
            INT32 timeSpent = 0;

            bool moveWasRead = player->GetMove (
                                   theBoard,
                                   move,
                                   timeSpent );

            if ( moveWasRead )
            {
                theUserInterface.RecordMove ( theBoard, move, timeSpent );
                UnmoveInfo unmove;
                theBoard.MakeMove ( move, unmove );
                theUserInterface.DrawBoard ( theBoard );

                int promptY = theBoard.WhiteToMove() ? 25 : 24;

                gotoxy ( 20, promptY );
                printf ( "Are you sure this is the move you want to make? (Y/N) " );
                unsigned key = 0;
                while ( toupper(key) != 'Y' && toupper(key) != 'N' )
                {
                    key = bioskey(0) & 0xff;
                }

                if ( key == 'Y' || key == 'y' )
                {
                    WriteEmail ( theBoard, argv[1] );
                    sprintf ( ExitMessage, "Your move has been added to the file '%s'.\n", argv[1] );
                    break;
                }
                else
                {
                    gotoxy ( 1, promptY );
                    printf ( "                                                                          " );
                    theBoard.UnmakeMove ( move, unmove );
                    theUserInterface.DrawBoard ( theBoard );
                }
            }
            else
            {
                sprintf ( ExitMessage, "The file '%s' has not been changed.\n", argv[1] );
                break;
            }
        }
    }

    delete player;

    return 0;
}


void ChessFatal ( const char *message )
{
    sprintf ( ExitMessage, "Chess fatal:\n%s\n", message );

    FILE *errorLog = fopen ( "mailches.err", "wt" );
    if ( errorLog )
    {
        fprintf ( errorLog, "Chess fatal:\n%s\n", message );
        fclose ( errorLog );
    }

    exit(1);
}


/*
    $Log: mailches.cpp,v $
    Revision 1.2  2006/01/18 19:58:12  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.1  2005/11/25 19:47:27  dcross
    Recovered lots of old chess source files from the dead!
    Found these on CDROM marked "14 September 1999".
    Added cvs log tag and moved old revision history after that.


     Revision history:

1995 March 25 [Don Cross] Version 1.00
     Added code to always include a human-readable version
     of the game in the e-mail file.

1995 March 23 [Don Cross]
     Made lots of little fixes.
     Improved usability.
     Added code to handle end of game.

1995 March 22 [Don Cross]
     Copied this file from DOSCGA.CPP and hacked on it to
     make MAILCHES.

1993 August 30 [Don Cross]
     Changing pointers to references in the interfaces where
     appropriate.

*/

