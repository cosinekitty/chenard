/*============================================================================

      gencaps.cpp  -  Copyright (C) 1993-2005 by Don Cross

      Contains code for generating a list of capture moves for
      a given ChessBoard.

============================================================================*/

#include "chess.h"
#include "profiler.h"

int ChessBoard::GenWhiteCaptures ( 
    MoveList &ml,
    ComputerChessPlayer *player )
{
    PROFILER_ENTER(PX_GENCAPS)
    int x, ybase, ofs;
    SQUARE piece;

    ml.num = 0;    // Make the MoveList empty.

    for ( ybase = OFFSET(2,2); ybase < OFFSET(2,10); ybase += NORTH )
    {
        for ( x=0; x < 8; x++ )
        {
            piece = board [ofs = ybase + x];

            if ( piece & WHITE_MASK )
            {
                switch ( UPIECE_INDEX(piece) )
                {
                    case P_INDEX:
                        GenCaps_WP ( ml, ofs, ybase );
                        break;

                    case N_INDEX:
                        GenCaps_WN ( ml, ofs );
                        break;

                    case B_INDEX:
                        GenCaps_WB ( ml, ofs );
                        break;

                    case R_INDEX:
                        GenCaps_WR ( ml, ofs );
                        break;

                    case Q_INDEX:
                        GenCaps_WQ ( ml, ofs );
                        break;

                    case K_INDEX:
                        GenCaps_WK ( ml, ofs );
                        break;

                    default:
                        ChessFatal ( "Invalid piece in ChessBoard::GenWhiteCaptures" );
                        break;
                }
            }
        }
    }

    RemoveIllegalWhite ( ml, player );
    PROFILER_EXIT();
    return ml.num;
}


void ChessBoard::GenCaps_WP ( MoveList &ml, int ofs, int ybase )
{
    if ( ybase == OFFSET(2,6) )
    {
        if ( board [ofs + NORTHEAST] & BLACK_MASK )
            ml.AddMove ( ofs, ofs + NORTHEAST );
        else if ( (prev_move.source & BOARD_OFFSET_MASK) == ofs + OFFSET(1,2) &&
                prev_move.dest == ofs + OFFSET(1,0) &&
                (board [prev_move.dest] & BP_MASK) )
            ml.AddMove ( ofs, SPECIAL_MOVE_EP_EAST );

        if ( board [ofs + NORTHWEST] & BLACK_MASK )
            ml.AddMove ( ofs, ofs + NORTHWEST );
        else if ( (prev_move.source & BOARD_OFFSET_MASK) == ofs + OFFSET(-1,2) &&
                prev_move.dest == ofs + OFFSET(-1,0) &&
                (board [prev_move.dest] & BP_MASK) )
            ml.AddMove ( ofs, SPECIAL_MOVE_EP_WEST );
    }
    else if ( ybase == OFFSET(2,8) )
    {
        // Count ANY pawn promotion as a capture, even if it is not
        // capturing an enemy piece.

        if ( board [ofs + NORTH] == EMPTY )
        {
            ml.AddMove ( ofs, SPECIAL_MOVE_PROMOTE_NORM | Q_INDEX );
            ml.AddMove ( ofs, SPECIAL_MOVE_PROMOTE_NORM | R_INDEX );
            ml.AddMove ( ofs, SPECIAL_MOVE_PROMOTE_NORM | B_INDEX );
            ml.AddMove ( ofs, SPECIAL_MOVE_PROMOTE_NORM | N_INDEX );
        }

        if ( board [ofs + NORTHEAST] & BLACK_MASK )
        {
            ml.AddMove ( ofs, SPECIAL_MOVE_PROMOTE_CAP_EAST | Q_INDEX );
            ml.AddMove ( ofs, SPECIAL_MOVE_PROMOTE_CAP_EAST | R_INDEX );
            ml.AddMove ( ofs, SPECIAL_MOVE_PROMOTE_CAP_EAST | B_INDEX );
            ml.AddMove ( ofs, SPECIAL_MOVE_PROMOTE_CAP_EAST | N_INDEX );
        }

        if ( board [ofs + NORTHWEST] & BLACK_MASK )
        {
            ml.AddMove ( ofs, SPECIAL_MOVE_PROMOTE_CAP_WEST | Q_INDEX );
            ml.AddMove ( ofs, SPECIAL_MOVE_PROMOTE_CAP_WEST | R_INDEX );
            ml.AddMove ( ofs, SPECIAL_MOVE_PROMOTE_CAP_WEST | B_INDEX );
            ml.AddMove ( ofs, SPECIAL_MOVE_PROMOTE_CAP_WEST | N_INDEX );
        }
    }
    else  // Look for normal captures
    {
        if ( board [ofs + NORTHEAST] & BLACK_MASK )
            ml.AddMove ( ofs, ofs + NORTHEAST );

        if ( board [ofs + NORTHWEST] & BLACK_MASK )
            ml.AddMove ( ofs, ofs + NORTHWEST );
    }
}


void ChessBoard::GenCaps_WN ( MoveList &ml, int source )
{
    if ( board [source + OFFSET(1,2)] & BLACK_MASK )
        ml.AddMove ( source, source + OFFSET(1,2) );

    if ( board [source + OFFSET(-1,2)] & BLACK_MASK )
        ml.AddMove ( source, source + OFFSET(-1,2) );

    if ( board [source + OFFSET(1,-2)] & BLACK_MASK )
        ml.AddMove ( source, source + OFFSET(1,-2) );

    if ( board [source + OFFSET(-1,-2)] & BLACK_MASK )
        ml.AddMove ( source, source + OFFSET(-1,-2) );

    if ( board [source + OFFSET(2,1)] & BLACK_MASK )
        ml.AddMove ( source, source + OFFSET(2,1) );

    if ( board [source + OFFSET(2,-1)] & BLACK_MASK )
        ml.AddMove ( source, source + OFFSET(2,-1) );

    if ( board [source + OFFSET(-2,1)] & BLACK_MASK )
        ml.AddMove ( source, source + OFFSET(-2,1) );

    if ( board [source + OFFSET(-2,-1)] & BLACK_MASK )
        ml.AddMove ( source, source + OFFSET(-2,-1) );
}


void ChessBoard::GenCaps_WB ( MoveList &ml, int source )
{
    int ofs;
    for ( ofs = source + NORTHEAST; board[ofs] == EMPTY; ofs += NORTHEAST );
    if ( board[ofs] & BLACK_MASK )
        ml.AddMove ( source, ofs );

    for ( ofs = source + NORTHWEST; board[ofs] == EMPTY; ofs += NORTHWEST );
    if ( board[ofs] & BLACK_MASK )
        ml.AddMove ( source, ofs );

    for ( ofs = source + SOUTHWEST; board[ofs] == EMPTY; ofs += SOUTHWEST );
    if ( board[ofs] & BLACK_MASK )
        ml.AddMove ( source, ofs );

    for ( ofs = source + SOUTHEAST; board[ofs] == EMPTY; ofs += SOUTHEAST );
    if ( board[ofs] & BLACK_MASK )
        ml.AddMove ( source, ofs );
}


void ChessBoard::GenCaps_WR ( MoveList &ml, int source )
{
    int ofs;
    for ( ofs = source + NORTH; board[ofs] == EMPTY; ofs += NORTH );
    if ( board[ofs] & BLACK_MASK )
        ml.AddMove ( source, ofs );

    for ( ofs = source + WEST; board[ofs] == EMPTY; ofs += WEST );
    if ( board[ofs] & BLACK_MASK )
        ml.AddMove ( source, ofs );

    for ( ofs = source + SOUTH; board[ofs] == EMPTY; ofs += SOUTH );
    if ( board[ofs] & BLACK_MASK )
        ml.AddMove ( source, ofs );

    for ( ofs = source + EAST; board[ofs] == EMPTY; ofs += EAST );
    if ( board[ofs] & BLACK_MASK )
        ml.AddMove ( source, ofs );
}


void ChessBoard::GenCaps_WQ ( MoveList &ml, int source )
{
    int ofs;
    for ( ofs = source + NORTHEAST; board[ofs] == EMPTY; ofs += NORTHEAST );
    if ( board[ofs] & BLACK_MASK )
        ml.AddMove ( source, ofs );

    for ( ofs = source + NORTHWEST; board[ofs] == EMPTY; ofs += NORTHWEST );
    if ( board[ofs] & BLACK_MASK )
        ml.AddMove ( source, ofs );

    for ( ofs = source + SOUTHWEST; board[ofs] == EMPTY; ofs += SOUTHWEST );
    if ( board[ofs] & BLACK_MASK )
        ml.AddMove ( source, ofs );

    for ( ofs = source + SOUTHEAST; board[ofs] == EMPTY; ofs += SOUTHEAST );
    if ( board[ofs] & BLACK_MASK )
        ml.AddMove ( source, ofs );

    for ( ofs = source + NORTH; board[ofs] == EMPTY; ofs += NORTH );
    if ( board[ofs] & BLACK_MASK )
        ml.AddMove ( source, ofs );

    for ( ofs = source + WEST; board[ofs] == EMPTY; ofs += WEST );
    if ( board[ofs] & BLACK_MASK )
        ml.AddMove ( source, ofs );

    for ( ofs = source + SOUTH; board[ofs] == EMPTY; ofs += SOUTH );
    if ( board[ofs] & BLACK_MASK )
        ml.AddMove ( source, ofs );

    for ( ofs = source + EAST; board[ofs] == EMPTY; ofs += EAST );
    if ( board[ofs] & BLACK_MASK )
        ml.AddMove ( source, ofs );
}


void ChessBoard::GenCaps_WK ( MoveList &ml, int source )
{
    if ( board [source + NORTH] & BLACK_MASK )
        ml.AddMove ( source, source + NORTH );

    if ( board [source + SOUTH] & BLACK_MASK )
        ml.AddMove ( source, source + SOUTH );

    if ( board [source + EAST] & BLACK_MASK )
        ml.AddMove ( source, source + EAST );

    if ( board [source + WEST] & BLACK_MASK )
        ml.AddMove ( source, source + WEST );

    if ( board [source + NORTHEAST] & BLACK_MASK )
        ml.AddMove ( source, source + NORTHEAST );

    if ( board [source + NORTHWEST] & BLACK_MASK )
        ml.AddMove ( source, source + NORTHWEST );

    if ( board [source + SOUTHEAST] & BLACK_MASK )
        ml.AddMove ( source, source + SOUTHEAST );

    if ( board [source + SOUTHWEST] & BLACK_MASK )
        ml.AddMove ( source, source + SOUTHWEST );
}


//---------------------------------------------------------------------------

int ChessBoard::GenBlackCaptures ( MoveList &ml, ComputerChessPlayer *player )
{
    PROFILER_ENTER(PX_GENCAPS);
    int x, ybase, ofs;
    SQUARE piece;

    ml.num = 0;    // Make the MoveList empty.

    for ( ybase = OFFSET(2,2); ybase < OFFSET(2,10); ybase += NORTH )
    {
        for ( x=0; x < 8; x++ )
        {
            piece = board [ofs = ybase + x];

            if ( piece & BLACK_MASK )
            {
                switch ( UPIECE_INDEX(piece) )
                {
                    case P_INDEX:
                        GenCaps_BP ( ml, ofs, ybase );
                        break;

                    case N_INDEX:
                        GenCaps_BN ( ml, ofs );
                        break;

                    case B_INDEX:
                        GenCaps_BB ( ml, ofs );
                        break;

                    case R_INDEX:
                        GenCaps_BR ( ml, ofs );
                        break;

                    case Q_INDEX:
                        GenCaps_BQ ( ml, ofs );
                        break;

                    case K_INDEX:
                        GenCaps_BK ( ml, ofs );
                        break;

                    default:
                        ChessFatal ( "Invalid piece in ChessBoard::GenBlackCaptures" );
                        break;
                }
            }
        }
    }

    RemoveIllegalBlack ( ml, player );
    PROFILER_EXIT();
    return ml.num;
}


void ChessBoard::GenCaps_BP ( MoveList &ml, int ofs, int ybase )
{
    if ( ybase == OFFSET(2,5) )   // check for en passant and normal captures
    {
        if ( board [ofs + SOUTHEAST] & WHITE_MASK )
            ml.AddMove ( ofs, ofs + SOUTHEAST );
        else if ( (prev_move.source & BOARD_OFFSET_MASK) == ofs + OFFSET(1,-2) &&
                prev_move.dest == ofs + OFFSET(1,0) &&
                (board [prev_move.dest] & WP_MASK) )
            ml.AddMove ( ofs, SPECIAL_MOVE_EP_EAST );

        if ( board [ofs + SOUTHWEST] & WHITE_MASK )
            ml.AddMove ( ofs, ofs + SOUTHWEST );
        else if ( (prev_move.source & BOARD_OFFSET_MASK) == ofs + OFFSET(-1,-2) &&
                prev_move.dest == ofs + OFFSET(-1,0) &&
                (board [prev_move.dest] & WP_MASK) )
            ml.AddMove ( ofs, SPECIAL_MOVE_EP_WEST );
    }
    else if ( ybase == OFFSET(2,3) )   // check for pawn promotion
    {
        // Count ANY pawn promotion as a capture, even if it is not
        // capturing an enemy piece.

        if ( board [ofs + SOUTH] == EMPTY )
        {
            ml.AddMove ( ofs, SPECIAL_MOVE_PROMOTE_NORM | Q_INDEX );
            ml.AddMove ( ofs, SPECIAL_MOVE_PROMOTE_NORM | R_INDEX );
            ml.AddMove ( ofs, SPECIAL_MOVE_PROMOTE_NORM | B_INDEX );
            ml.AddMove ( ofs, SPECIAL_MOVE_PROMOTE_NORM | N_INDEX );
        }

        if ( board [ofs + SOUTHEAST] & WHITE_MASK )
        {
            ml.AddMove ( ofs, SPECIAL_MOVE_PROMOTE_CAP_EAST | Q_INDEX );
            ml.AddMove ( ofs, SPECIAL_MOVE_PROMOTE_CAP_EAST | R_INDEX );
            ml.AddMove ( ofs, SPECIAL_MOVE_PROMOTE_CAP_EAST | B_INDEX );
            ml.AddMove ( ofs, SPECIAL_MOVE_PROMOTE_CAP_EAST | N_INDEX );
        }

        if ( board [ofs + SOUTHWEST] & WHITE_MASK )
        {
            ml.AddMove ( ofs, SPECIAL_MOVE_PROMOTE_CAP_WEST | Q_INDEX );
            ml.AddMove ( ofs, SPECIAL_MOVE_PROMOTE_CAP_WEST | R_INDEX );
            ml.AddMove ( ofs, SPECIAL_MOVE_PROMOTE_CAP_WEST | B_INDEX );
            ml.AddMove ( ofs, SPECIAL_MOVE_PROMOTE_CAP_WEST | N_INDEX );
        }
    }
    else  // Look for normal captures
    {
        if ( board [ofs + SOUTHEAST] & WHITE_MASK )
            ml.AddMove ( ofs, ofs + SOUTHEAST );

        if ( board [ofs + SOUTHWEST] & WHITE_MASK )
            ml.AddMove ( ofs, ofs + SOUTHWEST );
    }
}


void ChessBoard::GenCaps_BN ( MoveList &ml, int source )
{
    if ( board [source + OFFSET(1,2)] & WHITE_MASK )
        ml.AddMove ( source, source + OFFSET(1,2) );

    if ( board [source + OFFSET(-1,2)] & WHITE_MASK )
        ml.AddMove ( source, source + OFFSET(-1,2) );

    if ( board [source + OFFSET(1,-2)] & WHITE_MASK )
        ml.AddMove ( source, source + OFFSET(1,-2) );

    if ( board [source + OFFSET(-1,-2)] & WHITE_MASK )
        ml.AddMove ( source, source + OFFSET(-1,-2) );

    if ( board [source + OFFSET(2,1)] & WHITE_MASK )
        ml.AddMove ( source, source + OFFSET(2,1) );

    if ( board [source + OFFSET(2,-1)] & WHITE_MASK )
        ml.AddMove ( source, source + OFFSET(2,-1) );

    if ( board [source + OFFSET(-2,1)] & WHITE_MASK )
        ml.AddMove ( source, source + OFFSET(-2,1) );

    if ( board [source + OFFSET(-2,-1)] & WHITE_MASK )
        ml.AddMove ( source, source + OFFSET(-2,-1) );
}


void ChessBoard::GenCaps_BB ( MoveList &ml, int source )
{
    int ofs;
    for ( ofs = source + NORTHEAST; board[ofs] == EMPTY; ofs += NORTHEAST );
    if ( board[ofs] & WHITE_MASK )
        ml.AddMove ( source, ofs );

    for ( ofs = source + NORTHWEST; board[ofs] == EMPTY; ofs += NORTHWEST );
    if ( board[ofs] & WHITE_MASK )
        ml.AddMove ( source, ofs );

    for ( ofs = source + SOUTHWEST; board[ofs] == EMPTY; ofs += SOUTHWEST );
    if ( board[ofs] & WHITE_MASK )
        ml.AddMove ( source, ofs );

    for ( ofs = source + SOUTHEAST; board[ofs] == EMPTY; ofs += SOUTHEAST );
    if ( board[ofs] & WHITE_MASK )
        ml.AddMove ( source, ofs );
}


void ChessBoard::GenCaps_BR ( MoveList &ml, int source )
{
    int ofs;
    for ( ofs = source + NORTH; board[ofs] == EMPTY; ofs += NORTH );
    if ( board[ofs] & WHITE_MASK )
        ml.AddMove ( source, ofs );

    for ( ofs = source + WEST; board[ofs] == EMPTY; ofs += WEST );
    if ( board[ofs] & WHITE_MASK )
        ml.AddMove ( source, ofs );

    for ( ofs = source + SOUTH; board[ofs] == EMPTY; ofs += SOUTH );
    if ( board[ofs] & WHITE_MASK )
        ml.AddMove ( source, ofs );

    for ( ofs = source + EAST; board[ofs] == EMPTY; ofs += EAST );
    if ( board[ofs] & WHITE_MASK )
        ml.AddMove ( source, ofs );
}


void ChessBoard::GenCaps_BQ ( MoveList &ml, int source )
{
    int ofs;
    for ( ofs = source + NORTHEAST; board[ofs] == EMPTY; ofs += NORTHEAST );
    if ( board[ofs] & WHITE_MASK )
        ml.AddMove ( source, ofs );

    for ( ofs = source + NORTHWEST; board[ofs] == EMPTY; ofs += NORTHWEST );
    if ( board[ofs] & WHITE_MASK )
        ml.AddMove ( source, ofs );

    for ( ofs = source + SOUTHWEST; board[ofs] == EMPTY; ofs += SOUTHWEST );
    if ( board[ofs] & WHITE_MASK )
        ml.AddMove ( source, ofs );

    for ( ofs = source + SOUTHEAST; board[ofs] == EMPTY; ofs += SOUTHEAST );
    if ( board[ofs] & WHITE_MASK )
        ml.AddMove ( source, ofs );

    for ( ofs = source + NORTH; board[ofs] == EMPTY; ofs += NORTH );
    if ( board[ofs] & WHITE_MASK )
        ml.AddMove ( source, ofs );

    for ( ofs = source + WEST; board[ofs] == EMPTY; ofs += WEST );
    if ( board[ofs] & WHITE_MASK )
        ml.AddMove ( source, ofs );

    for ( ofs = source + SOUTH; board[ofs] == EMPTY; ofs += SOUTH );
    if ( board[ofs] & WHITE_MASK )
        ml.AddMove ( source, ofs );

    for ( ofs = source + EAST; board[ofs] == EMPTY; ofs += EAST );
    if ( board[ofs] & WHITE_MASK )
        ml.AddMove ( source, ofs );
}


void ChessBoard::GenCaps_BK ( MoveList &ml, int source )
{
    if ( board [source + NORTH] & WHITE_MASK )
        ml.AddMove ( source, source + NORTH );

    if ( board [source + SOUTH] & WHITE_MASK )
        ml.AddMove ( source, source + SOUTH );

    if ( board [source + EAST] & WHITE_MASK )
        ml.AddMove ( source, source + EAST );

    if ( board [source + WEST] & WHITE_MASK )
        ml.AddMove ( source, source + WEST );

    if ( board [source + NORTHEAST] & WHITE_MASK )
        ml.AddMove ( source, source + NORTHEAST );

    if ( board [source + NORTHWEST] & WHITE_MASK )
        ml.AddMove ( source, source + NORTHWEST );

    if ( board [source + SOUTHEAST] & WHITE_MASK )
        ml.AddMove ( source, source + SOUTHEAST );

    if ( board [source + SOUTHWEST] & WHITE_MASK )
        ml.AddMove ( source, source + SOUTHWEST );
}


/*
    $Log: gencaps.cpp,v $
    Revision 1.3  2005/11/23 21:30:30  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:40  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!



          Revision history:
    
    1999 August 4 [Don Cross]
         Adding debug code.
    
    1999 January 4 [Don Cross]
         Updated coding style.
    
    1994 February 10 [Don Cross]
         Adding piece indexing.
    
    1993 August 30 [Don Cross]
         Changing pointers to references in the interfaces where
         appropriate.
*/

