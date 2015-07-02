/*------------------------------------------------------------------

    uivga.cpp  -  Copyright (C) 1993-1996 by Don Cross
    email: dcross@intersrv.com
    WWW:   http://www.intersrv.com/~dcross/

    Mutated from:
    uicga.cpp  -  Don Cross, May 1993.

        Mutated from:
        display.cpp  -  Donald Cross, September 1989.

    (Mutated from Chessola UI code by Don Cross, September 1989.)
    Code for displaying a chess board with CGA graphics.

-------------------------------------------------------------------*/

#include <graphics.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <bios.h>
#include <ctype.h>
#include <mem.h>
#include <dos.h>
#include <conio.h>
#include "kbd.h"
#include "vga.c1"

#include "chess.h"
#include "uivga.h"

#define   BDX1    10
#define   BDY1     1
#define   WIDTH   50
#define   HEIGHT  50
#define   FLASHCOUNT   3
#define   FLASHDELAY   120

#define   SQRWIDTH    (WIDTH)
#define   SQRHEIGHT   (HEIGHT-1)


static int cursor_x, cursor_y;
static int source_x, source_y;
static int save_bx, save_by;


#define sqrofs(x,y)    ((x) + 12*(y) + 26)

#define B(b,x,y)       ( (b) [sqrofs(x,y)] )
#define S(p,imwhite)   ((imwhite) ? (p) : -(p))


void sqrborder ( int x, int y, int color );
void drawsqr ( SQUARE p, int x, int y );


/**********************************************************************/
/*******  If computer plays white, then opponent is black...    *******/
/*******  always display screen relative to opponent.           *******/
/**********************************************************************/
#define scrx(x)   ((!whites_view?(7-(x)):(x)) * SQRWIDTH + BDX1)
#define scry(y)   ((!whites_view?(y):(7-(y))) * (SQRHEIGHT+1) + BDY1)
int whites_view = 1;

static void getxy ( int source, int dest,
                    int *x1, int *y1,
                    int *x2, int *y2 )
{
    *x1 = XPART(source) - 2;
    *y1 = YPART(source) - 2;
    *x2 = XPART(dest) - 2;
    *y2 = YPART(dest) - 2;
}

void sqrborder ( int x, int y, int grcolor )
{
    int sx=scrx(x), sy=scry(y);

    setcolor(grcolor);
    line ( sx+1, sy+1, sx+WIDTH-2, sy+1 );
    line ( sx+1, sy+HEIGHT-2, sx+WIDTH-2, sy+HEIGHT-2 );
    line ( sx+1, sy+1, sx+1, sy+HEIGHT-2 );
    line ( sx+WIDTH-2, sy+1, sx+WIDTH-2, sy+HEIGHT-2 );
    setcolor(1);
}


#define it(P)      b = (w ? P##w : P##b);  break;
#define map(N,P)   case (N):  it(P)
void drawsqr ( SQUARE p, int x, int y )
{
    int sx=scrx(x), sy=scry(y), w=(x+y)&1;  /*White square?*/
    unsigned char *b;  /* bitmap pointer */

    switch ( p )
    {
        map (EMPTY, ew)
        map (WPAWN, pw)
        map (WKNIGHT, nw)
        map (WBISHOP, bw)
        map (WROOK, rw)
        map (WQUEEN, qw)
        map (WKING, kw)
        map (BPAWN, pb)
        map (BKNIGHT, nb)
        map (BBISHOP, bb)
        map (BROOK, rb)
        map (BQUEEN, qb)
        map (BKING, kb)
    default:  it(gw)
    }
    putimage ( sx, sy, b, 0 );
}

void initscreen (void)
{
    int mode=VGAHI, driver=VGA;

    registerbgidriver ( EGAVGA_driver );
    initgraph ( &driver, &mode, "" );
}


void drawgrid (void)
{
    int i, sx, sy;
    char cx[2],cy[2];

    cx[1] = cy[1] = 0;
    if ( whites_view )
    {
        for ( i=0,cx[0]='a',cy[0]='8',sx=BDX1+SQRWIDTH/2 - 3,sy=BDY1+SQRHEIGHT/2 - 4;
              i<8;
              i++,cx[0]++,cy[0]--,sx+=SQRWIDTH,sy+=SQRHEIGHT )
        {
            outtextxy ( sx, BDY1 + 8*(SQRHEIGHT+1) + 4, cx );
            outtextxy ( BDX1 + 8*SQRWIDTH + 14, sy, cy );
        }
    }
    else
    {
        for ( i=0,cx[0]='h',cy[0]='1',sx=BDX1+SQRWIDTH/2 - 3,sy=BDY1+SQRHEIGHT/2 - 4;
                i<8;
                i++,cx[0]--,cy[0]++,sx+=SQRWIDTH,sy+=SQRHEIGHT )
        {
            outtextxy ( sx, BDY1 + 8*(SQRHEIGHT+1) + 4, cx );
            outtextxy ( BDX1 + 8*SQRWIDTH + 14, sy, cy );
        }
    }
}


void drawboard ( const SQUARE board[144] )
{
    cleardevice();
    drawgrid();

    for ( int y=0; y<8; y++ )
    {
        for ( int x=0; x<8; x++ )
        {
            drawsqr ( B(board,x,y), x, y );
        }
    }
}


void refreshboard ( const SQUARE board[144] )
{
    for ( int y=0; y<8; y++ )
    {
        for ( int x=0; x<8; x++ )
        {
            drawsqr ( B(board,x,y), x, y );
        }
    }
}


void refreshboard ( const ChessBoard &board )
{
    for ( int y=0; y<8; y++ )
    {
        for ( int x=0; x<8; x++ )
        {
            drawsqr ( board.GetSquareContents(x,y), x, y );
        }
    }
}


void flashsqr ( int x, int y,
                SQUARE oldpiece, SQUARE newpiece, int keywait )
{
    int t;

    for ( t=0; t<FLASHCOUNT; keywait ? 0 : t++ )
    {
        drawsqr ( oldpiece, x, y );
        if ( !(keywait && KeyPressed()) )
        {
            delay ( FLASHDELAY );
        }
        drawsqr ( newpiece, x, y );
        if ( !(keywait && KeyPressed()) )
        {
            delay ( FLASHDELAY );
        }
        else
        {
            break;
        }
    }
}


int sound_flag = true;   // should program make sounds?

/*  ofs,disp are word offsets  */
void drawmove ( SQUARE b[144], int source, int dest )
{
    int x, y, x2, y2;

    if ( sound_flag )
    {
        sound(1200);
        delay(100);
        nosound();
    }
    getxy ( source, dest, &x, &y, &x2, &y2 );
    flashsqr ( x, y, B(b,x,y), EMPTY, 0 );
    flashsqr ( x2, y2, B(b,x2,y2), B(b,x,y), 0 );
}


int getkey (void)
{
    int key = bioskey(0);
    if ( key & 0xff ) key &= 0xff;
    return key;
}


int select ( char *options )
{
    int key, i;

    for(;;)
    {
        key = getkey();
        if ( key & 0xff ) key = toupper(key);
        for ( i=0; options[i]; i++ )
            if ( key == options[i] )
                return key;
    }
}


static char *SecondsString ( unsigned long timerTicks )
{
    static char buffer [20];

    sprintf ( buffer, "%0.2lf", double(timerTicks) / 18.204444 );
    return buffer;
}


void OutText ( int x, int y, int color, const char *format, ... )
{
    char s[128];

    va_list argptr;
    va_start ( argptr, format );
    vsprintf ( s, format, argptr );
    va_end ( argptr );

    int w = textwidth(s);
    int h = textheight(s);
    setfillstyle ( SOLID_FILL, BLACK );
    bar ( x, y, x+w, y+h );
    setcolor ( color );
    outtextxy ( x, y, s );
}

const int MoveLine_White = 440;
const int MoveLine_Black = 450;


void displaynotation ( char *notation, double thinkTime, int side )
{
    int y;
    char *desc;

    if ( side == WHITE )
    {
        desc = "White";
        y = MoveLine_White;
    }
    else
    {
        desc = "Black";
        y = MoveLine_Black;
    }

    OutText (
        0,
        y,
        LIGHTGRAY,
        "%s: %-20s (%9.2lf sec)",
        desc, notation, thinkTime );
}


static void AnnoyUser()
{
    delay(1);
    sound ( 400 );
    delay ( 150 );
    nosound();
}


//-------------------------------------------------------------------------


static bool UndoMove ( ChessBoard &board )
{
    if ( board.HasBeenEdited() )
        return false;

    int n = board.GetCurrentPlyNumber();
    if ( n >= 2 )
    {
        ChessBoard temp;
        for ( int i=0; i < n-2; i++ )
        {
            Move move = board.GetPastMove(i);
            UnmoveInfo unmove;
            temp.MakeMove ( move, unmove );
        }
        board = temp;
        return true;
    }

    return false;
}


//-------------------------------------------------------------------------


static SQUARE PromotePawn ( int bx, int by, SQUARE side )
{
    unsigned key;
    int bad_choice;
    SQUARE piece = (side & WHITE_MASK) ? WPAWN : BPAWN;

    do
    {
        flashsqr ( bx, by, -1, piece, true );
        key = getkey();
        if ( key & 0xff ) key = toupper(key & 0xff);
        bad_choice = false;

        if ( side & WHITE_MASK )
        {
            switch ( key )
            {
            case 'Q':   piece = WQUEEN;    break;
            case 'R':   piece = WROOK;     break;
            case 'B':   piece = WBISHOP;   break;
            case 'N':   piece = WKNIGHT;   break;

            default:    bad_choice = true;
            }
        }
        else
        {
            switch ( key )
            {
            case 'Q':   piece = BQUEEN;    break;
            case 'R':   piece = BROOK;     break;
            case 'B':   piece = BBISHOP;   break;
            case 'N':   piece = BKNIGHT;   break;

            default:    bad_choice = true;
            }
        }

        if ( bad_choice )
        {
            sound(1000);
            delay(300);
            nosound();
        }
    }
    while ( bad_choice );

    drawsqr ( piece, bx, by );

    return piece;
}


static bool ReadMoveFromConsole ( ChessBoard  &board,
                                  int         &source,
                                  int         &dest,
                                  bool    &debugSearchFlag )
{
    unsigned key;
    int x1, y1;
    int bx, by;                     // Board coords
    SQUARE piece, picked_up=EMPTY;
    SQUARE underneath;
    const char *debugFilename = "dosvga.gam";

    int my_side = board.WhiteToMove() ? WHITE_MASK : BLACK_MASK;

    for(;;)
    {
        bx = whites_view ? cursor_x : (7 - cursor_x);
        by = whites_view ? cursor_y : (7 - cursor_y);
        piece = board.GetSquareContents(bx,by);

        if ( picked_up == EMPTY )
        {
            drawsqr ( piece, bx, by );
            sqrborder ( bx, by, YELLOW );
        }
        else
        {
            underneath = (bx==x1 && by==y1) ? EMPTY : piece;
            flashsqr ( bx, by, picked_up, underneath, true );
        }

        key = getkey();
        sqrborder ( bx, by, ((bx+by)&1) ? CYAN : LIGHTBLUE );

        switch ( key )
        {
        case UPAR:    ++cursor_y;              break;
        case DOWNAR:  --cursor_y;              break;
        case LAR:     --cursor_x;              break;
        case RAR:     ++cursor_x;              break;
        case PGUP:    ++cursor_x; ++cursor_y;  break;
        case PGDN:    ++cursor_x; --cursor_y;  break;
        case END:     --cursor_x; --cursor_y;  break;
        case HOME:    --cursor_x; ++cursor_y;  break;

        case C_J:
            key = ENTER;    // For a breakpoint that user can control

        case ENTER:
            if ( picked_up == EMPTY )
            {
                if ( piece!=EMPTY && (piece & my_side) )
                {
                    picked_up = piece;
                    x1 = bx;
                    y1 = by;
                    source_x = cursor_x;
                    source_y = cursor_y;
                }
            }
            else
            {
                source = OFFSET ( x1+2, y1+2 );
                dest = OFFSET ( bx+2, by+2 );
                save_bx = bx;
                save_by = by;
                return true;
            }
            break;

        case ESCAPE:
            source = dest = 0;
            return false;

        case F10:
            debugSearchFlag = !debugSearchFlag;

        case F2:
            if ( !SaveGame(board, debugFilename) )
                AnnoyUser();
            break;

        case F3:
            if ( !LoadGame(board, debugFilename) )
                AnnoyUser();
            else
                refreshboard ( board );
            break;

        case BACKSPACE:
            UndoMove ( board );
            refreshboard ( board );
            break;
        }

        // Wrap moves off the board around the other side
        cursor_x &= 7;
        cursor_y &= 7;
    }
}


//-------------------------------------------------------------------------


ChessUI_dos_vga::ChessUI_dos_vga():
    graphics_initialized ( false ),
    debugSearchFlag ( false )
{
}


ChessUI_dos_vga::~ChessUI_dos_vga()
{
    if ( graphics_initialized )
    {
        closegraph();
        graphics_initialized = false;
    }
}


ChessPlayer *ChessUI_dos_vga::CreatePlayer ( ChessSide side )
{
    char *sideName;

    static int isHumanSide[2];
    int *thisSide;
    static int callCount = 0;

    ++callCount;

    switch ( side )
    {
    case SIDE_WHITE:
        sideName = "White";
        thisSide = &isHumanSide[0];
        break;

    case SIDE_BLACK:
        sideName = "Black";
        thisSide = &isHumanSide[1];
        break;

    default:
        fprintf ( stderr, "Invalid call to ChessUI_stdio::CreatePlayer()\n" );
        exit(1);
    }

    printf ( "Should %s be played by Human or Computer (h/c)? ", sideName );

    ChessPlayer *NewPlayer = 0;
    char UserString[80];

    while ( fgets ( UserString, sizeof(UserString), stdin ) )
    {
        if ( UserString[0] == 'h' || UserString[0] == 'H' )
        {
            NewPlayer = new HumanChessPlayer ( *(ChessUI *)this );
            *thisSide = 1;
            break;
        }
        else if ( UserString[0] == 'c' || UserString[0] == 'C' )
        {
            ComputerChessPlayer *puter = new ComputerChessPlayer ( *this );
            NewPlayer = puter;
            if ( puter )
            {
                printf ( "Enter max search time in seconds (or p<n> plies): " );

                double thinkTime;

                if ( !fgets ( UserString, sizeof(UserString), stdin ) )
                {
                    exit(1);
                }

                int plies;
                if ( sscanf ( UserString, "p%d", &plies ) == 1 &&
                        plies > 0 && plies < 100 )
                {
                    puter->SetSearchDepth ( plies );
                }
                else
                {
                    if ( sscanf ( UserString, "%lf", &thinkTime ) != 1 ||
                            thinkTime < 1.0 ||
                            thinkTime > 1.0e06 )
                    {
                        exit(1);
                    }

                    puter->SetTimeLimit ( INT32(thinkTime * 100.0 + 0.5) );
                }
            }
            break;
        }
        else
        {
            printf ( "\n??? Please enter 'h' or 'c': " );
        }
    }

    if ( !NewPlayer )
    {
        fprintf ( stderr, "\nFATAL: Failure to create a ChessPlayer object!\n" );
        exit(1);
    }

    if ( callCount == 2 && !isHumanSide[0] && isHumanSide[1] )
    {
        whites_view = 0;
    }

    return NewPlayer;
}


bool ChessUI_dos_vga::ReadMove ( ChessBoard &board,
                                 int &source, int &dest )
{
    DrawBoard ( board );
    return ReadMoveFromConsole ( board, source, dest, debugSearchFlag );
}


void ChessUI_dos_vga::NotifyUser ( const char *message )
{
    gotoxy ( 1, 23 );
    printf ( "%s", message );
    bioskey(0);
}


void ChessUI_dos_vga::RecordMove ( ChessBoard &board,
                                   Move move,
                                   INT32 thinkTime )
{
    char moveString [MAX_MOVE_STRLEN + 1];

    FormatChessMove ( board, move, moveString );
    double timeInSeconds = double(thinkTime) / 100.0;
    char *side;

#if 1
    displaynotation (
        moveString,
        timeInSeconds,
        board.WhiteToMove() ? WHITE : BLACK );
#else
    if ( board.WhiteToMove() )
    {
        gotoxy ( 1, 24 );
        side = "White";
    }
    else
    {
        gotoxy ( 1, 25 );
        side = "Black";
    }

    printf ( "%s: %-12s (%0.2lf)        ", side, moveString, timeInSeconds );
#endif
}


void ChessUI_dos_vga::DisplayMove ( ChessBoard &board, Move move )
{
    int source = move.source & BOARD_OFFSET_MASK;
    int dest = move.dest;
    int pawn_dir = board.WhiteToMove() ? NORTH : SOUTH;

    if ( dest > OFFSET(9,9) )
    {
        switch ( dest & 0xf0 )
        {
        case SPECIAL_MOVE_PROMOTE_NORM:
            dest = dest + pawn_dir;
            break;

        case SPECIAL_MOVE_PROMOTE_CAP_EAST:
            dest = dest + pawn_dir + EAST;
            break;

        case SPECIAL_MOVE_PROMOTE_CAP_WEST:
            dest = dest + pawn_dir + WEST;
            break;

        case SPECIAL_MOVE_KCASTLE:
            dest = source + 2*EAST;
            break;

        case SPECIAL_MOVE_QCASTLE:
            dest = source + 2*WEST;
            break;

        case SPECIAL_MOVE_EP_EAST:
            dest = source + pawn_dir + EAST;
            break;

        case SPECIAL_MOVE_EP_WEST:
            dest = dest + pawn_dir + WEST;
            break;
        }
    }

    drawmove ( board.board, source, dest );
}


void ChessUI_dos_vga::DrawBoard ( const ChessBoard &board )
{
    if ( !graphics_initialized )
    {
        initscreen();
        graphics_initialized = true;
        drawboard ( board.board );
    }
    else
    {
        refreshboard ( board.board );
    }
}


void ChessUI_dos_vga::PredictMate ( int numMoves )
{
    OutText ( 56*8, 7*10, YELLOW, "Mate in %-2d  ", numMoves );
}


void ChessUI_dos_vga::ReportEndOfGame ( ChessSide winner )
{
    const char *result = "";

    switch ( winner )
    {
    case SIDE_WHITE:
        result = "White wins      ";
        break;

    case SIDE_BLACK:
        result = "Black wins      ";
        break;

    default:
        result = "Draw            ";
        break;
    }

    OutText ( 56*8, 7*10, YELLOW, result );
    bioskey(0);
}


SQUARE ChessUI_dos_vga::PromotePawn ( int PawnDest, ChessSide side )
{
    SQUARE _side = (side == SIDE_WHITE) ? WHITE_MASK : BLACK_MASK;
    SQUARE piece = ::PromotePawn ( XPART(PawnDest)-2, YPART(PawnDest)-2, _side );
    return UPIECE_INDEX(piece);
}


void ChessUI_dos_vga::DisplayBestMoveSoFar ( const ChessBoard &board,
        Move              bestSoFar,
        int               /*level*/ )
{
    char moveString [MAX_MOVE_STRLEN + 1];

    FormatChessMove ( board, bestSoFar, moveString );

    OutText (
        56*8, 2*10,
        LIGHTGRAY,
        "%6d: %-12s",
        bestSoFar.score, moveString );
}


void ChessUI_dos_vga::DisplayCurrentMove (
    const ChessBoard &board,
    Move              move,
    int               level )
{
    char moveString [MAX_MOVE_STRLEN + 1];

    FormatChessMove ( board, move, moveString );
    OutText (
        56*8, 1*10,
        LIGHTGRAY,
        "[%2d] %-14s",
        level, moveString );
}


void ChessUI_dos_vga::ReportSpecial ( const char *message )
{
    OutText ( 56*8, 2*10, LIGHTGRAY, "%s", message );
}


void ChessUI_dos_vga::ReportComputerStats (
    INT32    /*thinkTime*/,
    UINT32   nodesVisited,
    UINT32   nodesEvaluated,
    UINT32   nodesGenerated,
    int      /*fwSearchDepth*/,
    UINT32   /*vis*/ [NODES_ARRAY_SIZE],
    UINT32   /*gen*/ [NODES_ARRAY_SIZE] )
{
    int y = 3*10;

    OutText ( 56*8, y, LIGHTGRAY, "generated = %9lu", long(nodesGenerated) );
    y += 10;

    OutText ( 56*8, y, LIGHTGRAY, "visited   = %9lu", long(nodesVisited) );
    y += 10;

    OutText ( 56*8, y, LIGHTGRAY, "evaluated = %9lu", long(nodesEvaluated) );
}



void ChessUI_dos_vga::DebugPly (
    int depth,
    ChessBoard &board,
    Move move )
{
    if ( debugSearchFlag && depth < 38 )
    {
        char moveString [MAX_MOVE_STRLEN + 1];

        FormatChessMove ( board, move, moveString );
        OutText (
            56*8, (depth+10)*10,
            LIGHTGRAY,
            "%-14s",
            moveString );
    }
}


void ChessUI_dos_vga::DebugExit (
    int depth,
    ChessBoard &,
    SCORE )
{
    static unsigned counter = 0;

    if ( debugSearchFlag && depth < 38 )
    {
        OutText (
            56*8, (depth+10)*10,
            LIGHTGRAY,
            "%-14s",
            "" );
    }

    if ( debugSearchFlag || ++counter > 100 )
    {
        counter = 0;
        if ( bioskey(1) )
        {
            unsigned key = getkey();
            switch ( key )
            {
            case F10:    debugSearchFlag = !debugSearchFlag;  break;
            }
        }
    }
}


/*
    $Log: uivga.cpp,v $
    Revision 1.2  2006/01/18 19:58:16  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.1  2005/11/25 19:47:28  dcross
    Recovered lots of old chess source files from the dead!
    Found these on CDROM marked "14 September 1999".
    Added cvs log tag and moved old revision history after that.


    Revision history:

1993 August 30 [Don Cross]
     Changing pointers to references in the interfaces where
     appropriate.

1994 February 23 [Don Cross]
     Made up to date with OS/2 version of Dr. Chenard.

1996 March 2 [Don Cross]
     Copied from uicga.cpp to make a new VGA version of Chenard.

1996 March 9 [Don Cross]
     Fixed bug: pawn promotion was not working for Black.
     I was converting key to upper case but comparing it with
     lower case 'q', 'r', 'b', 'n'.

1996 July 28 [Don Cross]
     Adding ChessUI_dos_vga::DebugPly

1997 March 27 [Don Cross]
     Adding support for UndoMove.

1999 February 14 [Don Cross]
     Bringing UIVGA up to date with new member functions
     needed to inherit non-abstractly from ChessUI.

*/

