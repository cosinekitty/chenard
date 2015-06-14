/*===========================================================================

     misc.cpp  -  Copyright (C) 1993-2005 by Don Cross

     Miscellaneous stuff for chess.

===========================================================================*/

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#include "chess.h"


bool MoveList::IsLegal ( Move move ) const
{
    for ( int i=0; i < num; i++ )
        if ( m[i] == move )
            return true;

    return false;
}


void MoveList::AddMove ( int source, int dest )
{
#if 0
    if ( num >= MAX_MOVES )
        ChessFatal ( "MoveList overflow in MoveList::AddMove(int,int)" );
#endif

    m[num].source  =  BYTE(source);
    m[num++].dest  =  BYTE(dest);
}


void MoveList::AddMove ( Move other )
{
#if 0
    if ( num >= MAX_MOVES )
        ChessFatal ( "MoveList overflow in MoveList::AddMove(Move)" );
#endif

    m[num++] = other;
}


void MoveList::SendToFront ( Move x )
{
    for ( int i=1; i<num; i++ )
    {
        if ( m[i] == x )
        {
            while ( i > 0 )
            {
                m[i] = m[i-1];
                --i;
            }

            m[0] = x;

            break;
        }
    }
}


static int MoveHashTable[] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 0, 0,
    0, 0, 8, 9,10,11,12,13,14,15, 0, 0,
    0, 0,16,17,18,19,20,21,22,23, 0, 0,
    0, 0,24,25,26,27,28,29,30,31, 0, 0,
    0, 0,32,33,34,35,36,37,38,39, 0, 0,
    0, 0,40,41,42,43,44,45,46,47, 0, 0,
    0, 0,48,49,50,51,52,53,54,55, 0, 0,
    0, 0,56,57,58,59,60,61,62,63, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


int Move::whiteHash() const
{
    int ofs1 = source & BOARD_OFFSET_MASK;
    int ofs2;

    if ( dest > OFFSET(9,9) )
    {
        switch ( dest & SPECIAL_MOVE_MASK )
        {
            case SPECIAL_MOVE_PROMOTE_NORM:
                ofs2 = ofs1 + NORTH;
                break;

            case SPECIAL_MOVE_PROMOTE_CAP_EAST:
            case SPECIAL_MOVE_EP_EAST:
                ofs2 = ofs1 + NORTHEAST;
                break;

            case SPECIAL_MOVE_PROMOTE_CAP_WEST:
            case SPECIAL_MOVE_EP_WEST:
                ofs2 = ofs1 + NORTHWEST;
                break;

            case SPECIAL_MOVE_KCASTLE:
                ofs2 = ofs1 + 2*EAST;
                break;

            default:
                ofs1 = ofs2 = 0;
        }
    }
    else
    {
        ofs2 = dest;
    }

    return (MoveHashTable[ofs1] << 6) | MoveHashTable[ofs2];
}


int Move::blackHash() const
{
    int ofs1 = source & BOARD_OFFSET_MASK;
    int ofs2;

    if ( dest > OFFSET(9,9) )
    {
        switch ( dest & SPECIAL_MOVE_MASK )
        {
            case SPECIAL_MOVE_PROMOTE_NORM:
                ofs2 = ofs1 + SOUTH;
                break;

            case SPECIAL_MOVE_PROMOTE_CAP_EAST:
            case SPECIAL_MOVE_EP_EAST:
                ofs2 = ofs1 + SOUTHEAST;
                break;

            case SPECIAL_MOVE_PROMOTE_CAP_WEST:
            case SPECIAL_MOVE_EP_WEST:
                ofs2 = ofs1 + SOUTHWEST;
                break;

            case SPECIAL_MOVE_KCASTLE:
                ofs2 = ofs1 + 2*EAST;
                break;

            default:
                ofs1 = ofs2 = 0;
        }
    }
    else
    {
        ofs2 = dest;
    }

    return (MoveHashTable[ofs1] << 6) | MoveHashTable[ofs2];
}


void Move::whiteOffsets ( int &ofs1, int &ofs2 ) const
{
    ofs1 = source & BOARD_OFFSET_MASK;

    if ( dest > OFFSET(9,9) )
    {
        switch ( dest & SPECIAL_MOVE_MASK )
        {
            case SPECIAL_MOVE_PROMOTE_NORM:
                ofs2 = ofs1 + NORTH;
                break;

            case SPECIAL_MOVE_PROMOTE_CAP_EAST:
            case SPECIAL_MOVE_EP_EAST:
                ofs2 = ofs1 + NORTHEAST;
                break;

            case SPECIAL_MOVE_PROMOTE_CAP_WEST:
            case SPECIAL_MOVE_EP_WEST:
                ofs2 = ofs1 + NORTHWEST;
                break;

            case SPECIAL_MOVE_KCASTLE:
                ofs2 = ofs1 + 2*EAST;
                break;

            default:
                ofs1 = ofs2 = 0;
        }
    }
    else
    {
        ofs2 = dest;
    }

    ofs1 = MoveHashTable [ofs1];
    ofs2 = MoveHashTable [ofs2];
}


void Move::blackOffsets ( int &ofs1, int &ofs2 ) const
{
    ofs1 = source & BOARD_OFFSET_MASK;

    if ( dest > OFFSET(9,9) )
    {
        switch ( dest & SPECIAL_MOVE_MASK )
        {
            case SPECIAL_MOVE_PROMOTE_NORM:
                ofs2 = ofs1 + SOUTH;
                break;

            case SPECIAL_MOVE_PROMOTE_CAP_EAST:
            case SPECIAL_MOVE_EP_EAST:
                ofs2 = ofs1 + SOUTHEAST;
                break;

            case SPECIAL_MOVE_PROMOTE_CAP_WEST:
            case SPECIAL_MOVE_EP_WEST:
                ofs2 = ofs1 + SOUTHWEST;
                break;

            case SPECIAL_MOVE_KCASTLE:
                ofs2 = ofs1 + 2*EAST;
                break;

            default:
                ofs1 = ofs2 = 0;
        }
    }
    else
    {
        ofs2 = dest;
    }

    ofs1 = MoveHashTable [ofs1];
    ofs2 = MoveHashTable [ofs2];
}


SQUARE Move::actualOffsets ( const ChessBoard &board, int &ofs1, int &ofs2 ) const
{
    return actualOffsets (board.WhiteToMove(), ofs1, ofs2);
}



SQUARE Move::actualOffsets ( 
    bool     white_to_move,
    int     &ofs1,
    int     &ofs2 ) const
{
    SQUARE promotion = EMPTY;   // assume move is not promotion unless proven otherwise
    ofs1 = source & BOARD_OFFSET_MASK;

    if ( dest > OFFSET(9,9) )
    {
        int pawndir = white_to_move ? NORTH : SOUTH;
        int side    = white_to_move ? WHITE_IND : BLACK_IND;

        switch ( dest & SPECIAL_MOVE_MASK )
        {
            case SPECIAL_MOVE_PROMOTE_NORM:
                ofs2 = ofs1 + pawndir;
                promotion = PROM_PIECE (dest, side);
                break;

            case SPECIAL_MOVE_PROMOTE_CAP_EAST:
                ofs2 = ofs1 + pawndir + EAST;
                promotion = PROM_PIECE (dest, side);
                break;

            case SPECIAL_MOVE_PROMOTE_CAP_WEST:
                ofs2 = ofs1 + pawndir + WEST;
                promotion = PROM_PIECE (dest, side);
                break;

            case SPECIAL_MOVE_EP_EAST:
                ofs2 = ofs1 + pawndir + EAST;
                break;

            case SPECIAL_MOVE_EP_WEST:
                ofs2 = ofs1 + pawndir + WEST;
                break;

            case SPECIAL_MOVE_KCASTLE:
                ofs2 = ofs1 + 2*EAST;
                break;

            case SPECIAL_MOVE_QCASTLE:
                ofs2 = ofs1 + 2*WEST;
                break;

            default:
                ofs1 = ofs2 = 0;
        }
    }
    else
    {
        ofs2 = dest;
    }

    return promotion;
}


bool Move::Fix ( 
    const ChessBoard    &Board,
    int                  Source, 
    int                  Dest,
    SQUARE               OptionalProm,
    ChessUI             &Ui )
{
    // New code added 13 March 1999 to support Game | Suggest Move in Win32 version...

    if ( (Source & 0x80) || (Dest & 0x80) )
    {
        source = Source;
        dest = Dest;
        return true;
    }

    // --- end of Suggest Move code.

    if ( Source<OFFSET(2,2) || Source>OFFSET(9,9) ||
         Dest<OFFSET(2,2) || Dest>OFFSET(9,9) ||
         Board.GetSquareContents(Source) == OFFBOARD ||
         Board.GetSquareContents(Dest) == OFFBOARD )
    {
        return false;
    }

    source = BYTE ( Source );
    dest   = BYTE ( Dest );
    score  = 0;

    // Check for special moves...

    int pawn_dir = Board.WhiteToMove() ? NORTH : SOUTH;
    SQUARE enemyMask = (pawn_dir==NORTH) ? BLACK_MASK : WHITE_MASK;

    ChessSide side = Board.WhiteToMove() ? SIDE_WHITE : SIDE_BLACK;

    SQUARE move_piece = Board.GetSquareContents ( Source );
    SQUARE prom;
    int    moveVector = Dest - Source;

    if ( Source == OFFSET(6,2) && Dest == OFFSET(8,2) &&
        (move_piece & WK_MASK) )
    {
        dest = SPECIAL_MOVE_KCASTLE;
    }
    else if ( Source == OFFSET(6,2) && Dest == OFFSET(4,2) &&
             (move_piece & WK_MASK) )
    {
        dest = SPECIAL_MOVE_QCASTLE;
    }
    else if ( Source == OFFSET(6,9) && Dest == OFFSET(8,9) &&
             (move_piece & BK_MASK) )
    {
        dest = SPECIAL_MOVE_KCASTLE;
    }
    else if ( Source == OFFSET(6,9) && Dest == OFFSET(4,9) &&
             (move_piece & BK_MASK) )
    {
        dest = SPECIAL_MOVE_QCASTLE;
    }
    else if ( move_piece & (WP_MASK | BP_MASK) )  // if moving a pawn
    {
        if ( moveVector == pawn_dir + EAST || moveVector == pawn_dir + WEST )
        {
            // If we get here, a pawn is capturing something
            if ( Board.GetSquareContents(Dest) == EMPTY )
            {
                // If we get here, this move looks like an en passant.
                if ( moveVector == pawn_dir + EAST )
                    dest = SPECIAL_MOVE_EP_EAST;
                else
                    dest = SPECIAL_MOVE_EP_WEST;
            }
            else if ( (pawn_dir==SOUTH && YPART(Dest)==2) ||
                   (pawn_dir==NORTH && YPART(Dest)==9) )
            {
                if (OptionalProm == 0) 
                {
                    if ( (Board.GetSquareContents(Dest) & enemyMask) == 0 )
                    {
                        // We do this because we already know it's not legal.
                        // This avoids a spurious call to the UI.
                        prom = Q_INDEX;
                    }
                    else
                    {
                        prom = Ui.PromotePawn ( Dest, side );
                    }
                }
                else 
                {
                    prom = OptionalProm;
                }

                if ( moveVector == pawn_dir + EAST )
                    dest = SPECIAL_MOVE_PROMOTE_CAP_EAST | prom;
                else
                    dest = SPECIAL_MOVE_PROMOTE_CAP_WEST | prom;
            }
        }
        else
        {
            // This is a pawn move, not a pawn capture.
            // Figure out whether it is a promotion...
            if ( (pawn_dir==SOUTH && YPART(Dest)==2) ||
                 (pawn_dir==NORTH && YPART(Dest)==9) )
            {
                if (OptionalProm == 0) {
                    if ( Board.GetSquareContents(Dest) != EMPTY )
                    {
                        // We know this isn't a legal move already, because
                        // there's something blocking the pawn promotion.
                        // Go ahead and make the move an illegal queen promotion.
                        // This avoids a spurious call to the UI to ask the
                        // user what piece to promote it to.
                        prom = Q_INDEX;
                    }
                    else
                        prom = Ui.PromotePawn ( Dest, side );
                } else {
                    prom = OptionalProm;
                }

                dest = SPECIAL_MOVE_PROMOTE_NORM | prom;
            }
        }
    }

    return true;
}


SQUARE ConvertNybbleToSquare ( int nybble )
{
    SQUARE s = OFFBOARD;

    switch ( nybble )
    {
        case EMPTY_NYBBLE:  s = EMPTY;     break;
        case WP_NYBBLE:     s = WPAWN;     break;
        case WN_NYBBLE:     s = WKNIGHT;   break;
        case WB_NYBBLE:     s = WBISHOP;   break;
        case WR_NYBBLE:     s = WROOK;     break;
        case WQ_NYBBLE:     s = WQUEEN;    break;
        case WK_NYBBLE:     s = WKING;     break;
        case BP_NYBBLE:     s = BPAWN;     break;
        case BN_NYBBLE:     s = BKNIGHT;   break;
        case BB_NYBBLE:     s = BBISHOP;   break;
        case BR_NYBBLE:     s = BROOK;     break;
        case BQ_NYBBLE:     s = BQUEEN;    break;
        case BK_NYBBLE:     s = BKING;     break;
    }

    return s;
}


int ConvertSquareToNybble ( SQUARE s )
{
    int n = 0;

    switch ( s )
    {
        case EMPTY:     n = EMPTY_NYBBLE;  break;
        case WPAWN:     n = WP_NYBBLE;     break;
        case WKNIGHT:   n = WN_NYBBLE;     break;
        case WBISHOP:   n = WB_NYBBLE;     break;
        case WROOK:     n = WR_NYBBLE;     break;
        case WQUEEN:    n = WQ_NYBBLE;     break;
        case WKING:     n = WK_NYBBLE;     break;
        case BPAWN:     n = BP_NYBBLE;     break;
        case BKNIGHT:   n = BN_NYBBLE;     break;
        case BBISHOP:   n = BB_NYBBLE;     break;
        case BROOK:     n = BR_NYBBLE;     break;
        case BQUEEN:    n = BQ_NYBBLE;     break;
        case BKING:     n = BK_NYBBLE;     break;
    }

    return n;
}


const char *ConvertDateToVersion (const char *compileDateString)
{
    // "Nov  1 2008"
    //  012345678901

    static char buffer[40] = "????.??.??";
    static const char * const Month[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    
    if (buffer[0] == '?') {
        // This is either the first time the function has been called, 
        // or compileDateString was not what we expected the last time(s).
        
        if (strlen(compileDateString) == 11) {
            int y = atoi(compileDateString + 7);
            int d = isspace(compileDateString[4]) ? atoi(compileDateString + 5) : atoi(compileDateString + 4);
            if (y >= 2008 && d >= 1) {
                for (int m=0; m < 12; ++m) {
                    if (memcmp (Month[m], compileDateString, 3) == 0) {
                        sprintf (buffer, "%04d.%02d.%02d", y, 1+m, d);
                        break;
                    }
                }
            }        
        }
    }
    
    return buffer;
}


char *CopyString (const char *string)
{
    char *copy;
    if (string) {
        copy = new char [1 + strlen(string)];
        strcpy (copy, string);
    } else {
        copy = 0;
    }
    return copy;
}


void FreeString (char *&string)
{
    if (string) {
        delete[] string;
        string = 0;
    }
}


void ReplaceString (char *&target, const char *source)
{
    FreeString (target);
    target = CopyString (source);
}


/*
    $Log: misc.cpp,v $
    Revision 1.10  2008/11/21 03:05:16  Don.Cross
    First baby step toward pondering in xchenard: used a hack in the Chenard engine that supports multithreaded pondering in WinChen.
    We are not doing pondering the same way, but the hack allows us to share the part of WinChen that predicts the opponent's move.
    Did some other refactoring of code that may help finish pondering, but I'm still trying to figure out what I'm really going to do.

    Revision 1.9  2008/11/12 23:06:07  Don.Cross
    Massive refactoring of the concept of an 'editedFlag' in ChessBoard.
    Now instead of just saying the board was edited, we will always remember the most recent position
    after which there are no more pseudo-moves representing edit actions.
    This allows us to save/load PGN files based on edited positions.
    More work needs to be done:
    - Need to allow move undo/redo to work in edited positions.
    - Because the game history is stored now inside the ChessBoard, it seems silly to have an external copy also!
    - Move numbers should be based on FEN when PGN is saved.
    In general, this is a scary change, so it needs more careful testing before being published.

    Revision 1.8  2008/11/06 22:25:06  Don.Cross
    Now ParseFancyMove accepts either PGN or longmove (e7e8q) style notation only.
    We tolerate of check '+' or checkmate '#' characters being absent or extraneous in the input.
    The former is especially important because I often see PGN notation where '+' is missing on checking moves.
    To allow for PGN input, I had to allow avoidance of the PromotePawn callback, so other code was changed as well.
    Version numbers are now always constently yyyy.mm.dd across all builds of Chenard!

    Revision 1.7  2006/01/18 19:58:12  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.6  2006/01/08 17:21:02  dcross
    Oops!  I really should test stuff before checking it in...
    I was passing the wrong parameter to PROM_PIECE macro... need the
    fake dest, not actual dest, because it contains the encoded promotion piece.

    Revision 1.5  2006/01/08 15:11:26  dcross
    1. Getting ready to add support for wp.egm endgame database.  I know I am going to need
       an easy way to detect pawn promotions, so I have modified the venerable Move::actualOffsets
       functions to detect that case for me and return EMPTY if a move is not a pawn promotion,
       or the actual piece value if it is a promotion.
    2. While doing this I noticed and corrected an oddity with the PROM_PIECE macro:
        the parameter 'piece' was unused, so I removed it.

    Revision 1.4  2006/01/08 02:11:08  dcross
    Initial tests of wq.egm are very promising!

    1. I was having a fit because I needed to get a move's actual offsets without having the correct board position.
       When I looked at the code for Move::actualOffsets, I suddenly realized it only needs to know whether
       it is White's or Black's turn.  So I overloaded the function to send in either a ChessBoard & (for compatibility)
       or a boolean telling whether it is White's move.

    2. Coded generic ConsultDatabase, WhiteDbEndgame, BlackDbEndgame.
       Still needs work for pawns to have any possibility of working right.

    Revision 1.3  2005/11/23 21:30:30  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:41  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!

    
    
    Revision history:

    1999 March 13 [Don Cross]
         The new Game | Suggest Move command in the Win32 version of 
         Chenard caused a bug to occur because the Move::Fix() function
         was incorrectly rejecting the source and dest.  This could 
         happen whenever a move caused check or was a "special" move
         like castling, en passant, or pawn promotion.
         This has been fixed by adding code to Move::Fix() to leave
         the move alone if the high bit is set in either the Source
         or Dest parameters, and just to trust that they already 
         represent a valid Move structure.
    
    1999 January 5 [Don Cross]
         Updating coding style.
    
    1994 February 9 [Don Cross]
         Adding Move::actualOffsets().
         This is useful for UIs that want to display moves graphically.
    
    1994 January 30 [Don Cross]
         Added ConvertNybbleToSquare, ConvertSquareToNybble.
    
    1993 August 30 [Don Cross]
         Changing pointers to references in the interfaces where
         appropriate.
    
*/

