/*==========================================================================

     canmove.cpp  -  Copyright (C) 1993-2005 by Don Cross

     Part of new chess code base.
     Contains code to determine whether the game is over or not.

==========================================================================*/

#include "chess.h"
#include "profiler.h"


#define CHECK_WHITE_MOVE(move,unmove)  \
   MakeWhiteMove ( move, unmove, true, false );   \
   islegal = !(flags & SF_WCHECK);                  \
   UnmakeWhiteMove ( move, unmove );                \
   if ( islegal ) return true;


#define CHECK_BLACK_MOVE(move,unmove)  \
   MakeBlackMove ( move, unmove, true, false );   \
   islegal = !(flags & SF_BCHECK);                  \
   UnmakeBlackMove ( move, unmove );                \
   if ( islegal ) return true;


bool ChessBoard::WhiteCanMove()
{
    PROFILER_ENTER(PX_CANMOVE)
    // The following 'if' should be a statistical optimization.
    // The idea is that, if white is in check, then the
    // most of the time, moving the king is one way to legally
    // get out of check.  This will be true especially in the
    // super-stupid kinds of positions that will show up deep in the
    // search.
    // However, if white is not in check, then we are more likely
    // to find another piece that can legally move first, especially
    // at the beginning of the game.

    if ( (flags & SF_WCHECK) && WK_CanMove(wk_offset) )
    {
        return true;
    }

    int x, ybase, ofs;

    for ( ybase = OFFSET(2,2); ybase <= OFFSET(2,9); ybase += NORTH )
    {
        for ( x=0; x < 8; x++ )
        {
            SQUARE piece = board [ofs = ybase + x];

            switch ( piece )
            {
            case WPAWN:
                if ( WP_CanMove(ofs,ybase) )
                {
                    PROFILER_EXIT();
                    return true;
                }
                break;

            case WKNIGHT:
                if ( WN_CanMove(ofs) )
                {
                    PROFILER_EXIT();
                    return true;
                }
                break;

            case WBISHOP:
                if ( WB_CanMove(ofs) )
                {
                    PROFILER_EXIT();
                    return true;
                }
                break;

            case WROOK:
                if ( WR_CanMove(ofs) )
                {
                    PROFILER_EXIT();
                    return true;
                }
                break;

            case WQUEEN:
                if ( WQ_CanMove(ofs) )
                {
                    PROFILER_EXIT();
                    return true;
                }
                break;

            case WKING:
                if ( !(flags & SF_WCHECK) )
                {
                    // If white were in check, we would have
                    // already done this.

                    if ( WK_CanMove(ofs) )
                    {
                        PROFILER_EXIT();
                        return true;
                    }
                }
                break;
            }
        }
    }

    PROFILER_EXIT();
    return false;
}


bool ChessBoard::WP_CanMove ( int ofs, int ybase )
{
    Move         move;
    UnmoveInfo   unmove;
    int          islegal;

    move.source = ofs;

    switch ( ybase )
    {
    case OFFSET(2,3):   // The pawn's home square
        if ( board [move.dest = ofs + NORTH] == EMPTY )
        {
            CHECK_WHITE_MOVE ( move, unmove );

            if ( board [move.dest = ofs + 2*NORTH] == EMPTY )
            {
                CHECK_WHITE_MOVE ( move, unmove );
            }
        }

        if ( board [move.dest = ofs + NORTHEAST] & BLACK_MASK )
        {
            CHECK_WHITE_MOVE ( move, unmove );
        }

        if ( board [move.dest = ofs + NORTHWEST] & BLACK_MASK )
        {
            CHECK_WHITE_MOVE ( move, unmove );
        }
        break;

    case OFFSET(2,6):   // Have to look for e.p.
        if ( board [move.dest = ofs + NORTH] == EMPTY )
        {
            CHECK_WHITE_MOVE ( move, unmove );
        }

        if ( board [move.dest = ofs + NORTHEAST] & BLACK_MASK )
        {
            CHECK_WHITE_MOVE ( move, unmove );
        }
        else if ( prev_move.dest == ofs + EAST &&
                  ((prev_move.source & BOARD_OFFSET_MASK) == ofs + OFFSET(1,2)) &&
                  (board [prev_move.dest] & BP_MASK) )
        {
            move.dest = SPECIAL_MOVE_EP_EAST;
            CHECK_WHITE_MOVE ( move, unmove );
        }

        if ( board [move.dest = ofs + NORTHWEST] & BLACK_MASK )
        {
            CHECK_WHITE_MOVE ( move, unmove );
        }
        else if ( prev_move.dest == ofs + WEST &&
                  ((prev_move.source & BOARD_OFFSET_MASK) == ofs + OFFSET(-1,2)) &&
                  (board [prev_move.dest] & BP_MASK) )
        {
            move.dest = SPECIAL_MOVE_EP_WEST;
            CHECK_WHITE_MOVE ( move, unmove );
        }
        break;

    case OFFSET(2,8):   // Have to look for promotion
        if ( board [ofs + NORTH] == EMPTY )
        {
            move.dest = (SPECIAL_MOVE_PROMOTE_NORM | Q_INDEX);
            CHECK_WHITE_MOVE ( move, unmove );
        }

        if ( board [ofs + NORTHEAST] & BLACK_MASK )
        {
            move.dest = (SPECIAL_MOVE_PROMOTE_CAP_EAST | Q_INDEX);
            CHECK_WHITE_MOVE ( move, unmove );
        }

        if ( board [ofs + NORTHWEST] & BLACK_MASK )
        {
            move.dest = (SPECIAL_MOVE_PROMOTE_CAP_WEST | Q_INDEX);
            CHECK_WHITE_MOVE ( move, unmove );
        }
        break;

    default:            // Just an ordinary pawn move or capture
        if ( board [move.dest = ofs + NORTH] == EMPTY )
        {
            CHECK_WHITE_MOVE ( move, unmove );
        }

        if ( board [move.dest = ofs + NORTHEAST] & BLACK_MASK )
        {
            CHECK_WHITE_MOVE ( move, unmove );
        }

        if ( board [move.dest = ofs + NORTHWEST] & BLACK_MASK )
        {
            CHECK_WHITE_MOVE ( move, unmove );
        }
        break;
    }

    return false;
}


bool ChessBoard::WN_CanMove ( int ofs )
{
    Move        move;
    UnmoveInfo  unmove;
    bool        islegal;

    move.source = BYTE(ofs);

    if ( (board[move.dest = ofs + OFFSET(1,2)] & (WHITE_MASK | OFFBOARD)) == 0 )
    {
        CHECK_WHITE_MOVE ( move, unmove );
    }

    if ( (board[move.dest = ofs + OFFSET(-1,2)] & (WHITE_MASK | OFFBOARD)) == 0 )
    {
        CHECK_WHITE_MOVE ( move, unmove );
    }

    if ( (board[move.dest = ofs + OFFSET(1,-2)] & (WHITE_MASK | OFFBOARD)) == 0 )
    {
        CHECK_WHITE_MOVE ( move, unmove );
    }

    if ( (board[move.dest = ofs + OFFSET(-1,-2)] & (WHITE_MASK | OFFBOARD)) == 0 )
    {
        CHECK_WHITE_MOVE ( move, unmove );
    }

    if ( (board[move.dest = ofs + OFFSET(2,1)] & (WHITE_MASK | OFFBOARD)) == 0 )
    {
        CHECK_WHITE_MOVE ( move, unmove );
    }

    if ( (board[move.dest = ofs + OFFSET(-2,1)] & (WHITE_MASK | OFFBOARD)) == 0 )
    {
        CHECK_WHITE_MOVE ( move, unmove );
    }

    if ( (board[move.dest = ofs + OFFSET(2,-1)] & (WHITE_MASK | OFFBOARD)) == 0 )
    {
        CHECK_WHITE_MOVE ( move, unmove );
    }

    if ( (board[move.dest = ofs + OFFSET(-2,-1)] & (WHITE_MASK | OFFBOARD)) == 0 )
    {
        CHECK_WHITE_MOVE ( move, unmove );
    }

    return false;
}


bool ChessBoard::WB_CanMove ( int ofs )
{
    Move        move;
    UnmoveInfo  unmove;
    int         islegal;

    move.source = ofs;

    for ( move.dest = ofs + NORTHEAST;
            (board [move.dest] & (WHITE_MASK | OFFBOARD)) == 0;
            move.dest += NORTHEAST )
    {
        CHECK_WHITE_MOVE ( move, unmove );
        if ( board [move.dest] & BLACK_MASK )   break;   // stop if just captured
    }


    for ( move.dest = ofs + NORTHWEST;
            (board [move.dest] & (WHITE_MASK | OFFBOARD)) == 0;
            move.dest += NORTHWEST )
    {
        CHECK_WHITE_MOVE ( move, unmove );
        if ( board [move.dest] & BLACK_MASK )   break;   // stop if just captured
    }


    for ( move.dest = ofs + SOUTHWEST;
            (board [move.dest] & (WHITE_MASK | OFFBOARD)) == 0;
            move.dest += BYTE(SOUTHWEST) )
    {
        CHECK_WHITE_MOVE ( move, unmove );
        if ( board [move.dest] & BLACK_MASK )   break;   // stop if just captured
    }


    for ( move.dest = ofs + SOUTHEAST;
            (board [move.dest] & (WHITE_MASK | OFFBOARD)) == 0;
            move.dest += BYTE(SOUTHEAST) )
    {
        CHECK_WHITE_MOVE ( move, unmove );
        if ( board [move.dest] & BLACK_MASK )   break;   // stop if just captured
    }

    return false;
}


bool ChessBoard::WR_CanMove ( int ofs )
{
    Move        move;
    UnmoveInfo  unmove;
    int         islegal;

    move.source = ofs;

    for ( move.dest = ofs + NORTH;
            (board [move.dest] & (WHITE_MASK | OFFBOARD)) == 0;
            move.dest += NORTH )
    {
        CHECK_WHITE_MOVE ( move, unmove );
        if ( board [move.dest] & BLACK_MASK )   break;   // stop if just captured
    }


    for ( move.dest = ofs + WEST;
            (board [move.dest] & (WHITE_MASK | OFFBOARD)) == 0;
            move.dest += BYTE(WEST) )
    {
        CHECK_WHITE_MOVE ( move, unmove );
        if ( board [move.dest] & BLACK_MASK )   break;   // stop if just captured
    }


    for ( move.dest = ofs + SOUTH;
            (board [move.dest] & (WHITE_MASK | OFFBOARD)) == 0;
            move.dest += BYTE(SOUTH) )
    {
        CHECK_WHITE_MOVE ( move, unmove );
        if ( board [move.dest] & BLACK_MASK )   break;   // stop if just captured
    }


    for ( move.dest = ofs + EAST;
            (board [move.dest] & (WHITE_MASK | OFFBOARD)) == 0;
            move.dest += EAST)
    {
        CHECK_WHITE_MOVE ( move, unmove );
        if ( board [move.dest] & BLACK_MASK )   break;   // stop if just captured
    }

    return false;
}


bool ChessBoard::WQ_CanMove ( int ofs )
{
    Move        move;
    UnmoveInfo  unmove;
    int         islegal;

    move.source = ofs;

    for ( move.dest = ofs + NORTHEAST;
            (board [move.dest] & (WHITE_MASK | OFFBOARD)) == 0;
            move.dest += NORTHEAST )
    {
        CHECK_WHITE_MOVE ( move, unmove );
        if ( board [move.dest] & BLACK_MASK )   break;   // stop if just captured
    }


    for ( move.dest = ofs + NORTHWEST;
            (board [move.dest] & (WHITE_MASK | OFFBOARD)) == 0;
            move.dest += NORTHWEST )
    {
        CHECK_WHITE_MOVE ( move, unmove );
        if ( board [move.dest] & BLACK_MASK )   break;   // stop if just captured
    }


    for ( move.dest = ofs + SOUTHWEST;
            (board [move.dest] & (WHITE_MASK | OFFBOARD)) == 0;
            move.dest += BYTE(SOUTHWEST) )
    {
        CHECK_WHITE_MOVE ( move, unmove );
        if ( board [move.dest] & BLACK_MASK )   break;   // stop if just captured
    }


    for ( move.dest = ofs + SOUTHEAST;
            (board [move.dest] & (WHITE_MASK | OFFBOARD)) == 0;
            move.dest += BYTE(SOUTHEAST) )
    {
        CHECK_WHITE_MOVE ( move, unmove );
        if ( board [move.dest] & BLACK_MASK )   break;   // stop if just captured
    }

    for ( move.dest = ofs + NORTH;
            (board [move.dest] & (WHITE_MASK | OFFBOARD)) == 0;
            move.dest += NORTH )
    {
        CHECK_WHITE_MOVE ( move, unmove );
        if ( board [move.dest] & BLACK_MASK )   break;   // stop if just captured
    }


    for ( move.dest = ofs + WEST;
            (board [move.dest] & (WHITE_MASK | OFFBOARD)) == 0;
            move.dest += BYTE(WEST) )
    {
        CHECK_WHITE_MOVE ( move, unmove );
        if ( board [move.dest] & BLACK_MASK )   break;   // stop if just captured
    }

    for ( move.dest = ofs + SOUTH;
            (board [move.dest] & (WHITE_MASK | OFFBOARD)) == 0;
            move.dest += BYTE(SOUTH) )
    {
        CHECK_WHITE_MOVE ( move, unmove );
        if ( board [move.dest] & BLACK_MASK )   break;   // stop if just captured
    }

    for ( move.dest = ofs + EAST;
            (board [move.dest] & (WHITE_MASK | OFFBOARD)) == 0;
            move.dest += EAST)
    {
        CHECK_WHITE_MOVE ( move, unmove );
        if ( board [move.dest] & BLACK_MASK )   break;   // stop if just captured
    }

    return false;
}


bool ChessBoard::WK_CanMove ( int ofs )
{
    // NOTE:  We deliberately ignore castling here.
    //        This is because, if castling is legal, then
    //        moving the king one square in the direction of
    //        the castling rook is also legal.  Therefore,
    //        leaving out castling never causes us to think
    //        that we don't have a legal move where we really do,
    //        or vice versa.

    Move        move;
    UnmoveInfo  unmove;
    bool        islegal;

    move.source = BYTE(ofs);

    if ( (board[move.dest = ofs + EAST] & (WHITE_MASK | OFFBOARD)) == 0 )
    {
        CHECK_WHITE_MOVE ( move, unmove );
    }

    if ( (board[move.dest = ofs + WEST] & (WHITE_MASK | OFFBOARD)) == 0 )
    {
        CHECK_WHITE_MOVE ( move, unmove );
    }

    if ( (board[move.dest = ofs + NORTH] & (WHITE_MASK | OFFBOARD)) == 0 )
    {
        CHECK_WHITE_MOVE ( move, unmove );
    }

    if ( (board[move.dest = ofs + NORTHEAST] & (WHITE_MASK | OFFBOARD)) == 0 )
    {
        CHECK_WHITE_MOVE ( move, unmove );
    }

    if ( (board[move.dest = ofs + NORTHWEST] & (WHITE_MASK | OFFBOARD)) == 0 )
    {
        CHECK_WHITE_MOVE ( move, unmove );
    }

    if ( (board[move.dest = ofs + SOUTH] & (WHITE_MASK | OFFBOARD)) == 0 )
    {
        CHECK_WHITE_MOVE ( move, unmove );
    }

    if ( (board[move.dest = ofs + SOUTHEAST] & (WHITE_MASK | OFFBOARD)) == 0 )
    {
        CHECK_WHITE_MOVE ( move, unmove );
    }

    if ( (board[move.dest = ofs + SOUTHWEST] & (WHITE_MASK | OFFBOARD)) == 0 )
    {
        CHECK_WHITE_MOVE ( move, unmove );
    }

    return false;
}


//-------------------------------------------------------------------------




bool ChessBoard::BlackCanMove()
{
    PROFILER_ENTER(PX_CANMOVE)

    // This should be a statistical optimization.
    // See ChessBoard::WhiteCanMove()

    if ( (flags & SF_BCHECK) && BK_CanMove(bk_offset) )
    {
        return true;
    }

    int x, ybase, ofs;

    for ( ybase = OFFSET(2,9); ybase >= OFFSET(2,2); ybase += SOUTH )
    {
        for ( x=0; x < 8; x++ )
        {
            SQUARE piece = board [ofs = ybase + x];

            switch ( piece )
            {
            case BPAWN:
                if ( BP_CanMove(ofs,ybase) )
                {
                    PROFILER_EXIT();
                    return true;
                }
                break;

            case BKNIGHT:
                if ( BN_CanMove(ofs) )
                {
                    PROFILER_EXIT();
                    return true;
                }
                break;

            case BBISHOP:
                if ( BB_CanMove(ofs) )
                {
                    PROFILER_EXIT();
                    return true;
                }
                break;

            case BROOK:
                if ( BR_CanMove(ofs) )
                {
                    PROFILER_EXIT();
                    return true;
                }
                break;

            case BQUEEN:
                if ( BQ_CanMove(ofs) )
                {
                    PROFILER_EXIT();
                    return true;
                }
                break;

            case BKING:
                if ( !(flags & SF_BCHECK) )
                {
                    // If black were in check, we would have already
                    // done this.

                    if ( BK_CanMove(ofs) )
                    {
                        PROFILER_EXIT();
                        return true;
                    }
                }
                break;
            }
        }
    }

    PROFILER_EXIT();
    return false;
}


bool ChessBoard::BP_CanMove ( int ofs, int ybase )
{
    Move         move;
    UnmoveInfo   unmove;
    int          islegal;

    move.source = ofs;

    switch ( ybase )
    {
    case OFFSET(2,8):   // The pawn's home square
        if ( board [move.dest = ofs + SOUTH] == EMPTY )
        {
            CHECK_BLACK_MOVE ( move, unmove );

            if ( board [move.dest = ofs + 2*SOUTH] == EMPTY )
            {
                CHECK_BLACK_MOVE ( move, unmove );
            }
        }

        if ( board [move.dest = ofs + SOUTHEAST] & WHITE_MASK )
        {
            CHECK_BLACK_MOVE ( move, unmove );
        }

        if ( board [move.dest = ofs + SOUTHWEST] & WHITE_MASK )
        {
            CHECK_BLACK_MOVE ( move, unmove );
        }
        break;

    case OFFSET(2,5):   // Have to look for e.p.
        if ( board [move.dest = ofs + SOUTH] == EMPTY )
        {
            CHECK_BLACK_MOVE ( move, unmove );
        }

        if ( board [move.dest = ofs + SOUTHEAST] & WHITE_MASK )
        {
            CHECK_BLACK_MOVE ( move, unmove );
        }
        else if ( prev_move.dest == ofs + EAST &&
                  ((prev_move.source & BOARD_OFFSET_MASK) == ofs + OFFSET(1,-2)) &&
                  (board [prev_move.dest] & WP_MASK) )
        {
            move.dest = SPECIAL_MOVE_EP_EAST;
            CHECK_BLACK_MOVE ( move, unmove );
        }

        if ( board [move.dest = ofs + SOUTHWEST] & WHITE_MASK )
        {
            CHECK_BLACK_MOVE ( move, unmove );
        }
        else if ( prev_move.dest == ofs + WEST &&
                  ((prev_move.source & BOARD_OFFSET_MASK) == ofs + OFFSET(-1,-2)) &&
                  (board [prev_move.dest] & WP_MASK) )
        {
            move.dest = SPECIAL_MOVE_EP_WEST;
            CHECK_BLACK_MOVE ( move, unmove );
        }
        break;

    case OFFSET(2,3):   // Have to look for promotion
        if ( board [ofs + SOUTH] == EMPTY )
        {
            move.dest = (SPECIAL_MOVE_PROMOTE_NORM | Q_INDEX);
            CHECK_BLACK_MOVE ( move, unmove );
        }

        if ( board [ofs + SOUTHEAST] & WHITE_MASK )
        {
            move.dest = (SPECIAL_MOVE_PROMOTE_CAP_EAST | Q_INDEX);
            CHECK_BLACK_MOVE ( move, unmove );
        }

        if ( board [ofs + SOUTHWEST] & WHITE_MASK )
        {
            move.dest = (SPECIAL_MOVE_PROMOTE_CAP_WEST | Q_INDEX);
            CHECK_BLACK_MOVE ( move, unmove );
        }
        break;

    default:            // Just an ordinary pawn move or capture
        if ( board [move.dest = ofs + SOUTH] == EMPTY )
        {
            CHECK_BLACK_MOVE ( move, unmove );
        }

        if ( board [move.dest = ofs + SOUTHEAST] & WHITE_MASK )
        {
            CHECK_BLACK_MOVE ( move, unmove );
        }

        if ( board [move.dest = ofs + SOUTHWEST] & WHITE_MASK )
        {
            CHECK_BLACK_MOVE ( move, unmove );
        }
        break;
    }

    return false;
}


bool ChessBoard::BN_CanMove ( int ofs )
{
    Move        move;
    UnmoveInfo  unmove;
    bool        islegal;

    move.source = BYTE(ofs);

    if ( (board[move.dest = ofs + OFFSET(1,2)] & (BLACK_MASK | OFFBOARD)) == 0 )
    {
        CHECK_BLACK_MOVE ( move, unmove );
    }

    if ( (board[move.dest = ofs + OFFSET(-1,2)] & (BLACK_MASK | OFFBOARD)) == 0 )
    {
        CHECK_BLACK_MOVE ( move, unmove );
    }

    if ( (board[move.dest = ofs + OFFSET(1,-2)] & (BLACK_MASK | OFFBOARD)) == 0 )
    {
        CHECK_BLACK_MOVE ( move, unmove );
    }

    if ( (board[move.dest = ofs + OFFSET(-1,-2)] & (BLACK_MASK | OFFBOARD)) == 0 )
    {
        CHECK_BLACK_MOVE ( move, unmove );
    }

    if ( (board[move.dest = ofs + OFFSET(2,1)] & (BLACK_MASK | OFFBOARD)) == 0 )
    {
        CHECK_BLACK_MOVE ( move, unmove );
    }

    if ( (board[move.dest = ofs + OFFSET(-2,1)] & (BLACK_MASK | OFFBOARD)) == 0 )
    {
        CHECK_BLACK_MOVE ( move, unmove );
    }

    if ( (board[move.dest = ofs + OFFSET(2,-1)] & (BLACK_MASK | OFFBOARD)) == 0 )
    {
        CHECK_BLACK_MOVE ( move, unmove );
    }

    if ( (board[move.dest = ofs + OFFSET(-2,-1)] & (BLACK_MASK | OFFBOARD)) == 0 )
    {
        CHECK_BLACK_MOVE ( move, unmove );
    }

    return false;
}


bool ChessBoard::BB_CanMove ( int ofs )
{
    Move        move;
    UnmoveInfo  unmove;
    int         islegal;

    move.source = ofs;

    for ( move.dest = ofs + NORTHEAST;
            (board [move.dest] & (BLACK_MASK | OFFBOARD)) == 0;
            move.dest += NORTHEAST )
    {
        CHECK_BLACK_MOVE ( move, unmove );
        if ( board [move.dest] & WHITE_MASK )   break;   // stop if just captured
    }


    for ( move.dest = ofs + NORTHWEST;
            (board [move.dest] & (BLACK_MASK | OFFBOARD)) == 0;
            move.dest += NORTHWEST )
    {
        CHECK_BLACK_MOVE ( move, unmove );
        if ( board [move.dest] & WHITE_MASK )   break;   // stop if just captured
    }


    for ( move.dest = ofs + SOUTHWEST;
            (board [move.dest] & (BLACK_MASK | OFFBOARD)) == 0;
            move.dest += BYTE(SOUTHWEST) )
    {
        CHECK_BLACK_MOVE ( move, unmove );
        if ( board [move.dest] & WHITE_MASK )   break;   // stop if just captured
    }


    for ( move.dest = ofs + SOUTHEAST;
            (board [move.dest] & (BLACK_MASK | OFFBOARD)) == 0;
            move.dest += BYTE(SOUTHEAST) )
    {
        CHECK_BLACK_MOVE ( move, unmove );
        if ( board [move.dest] & WHITE_MASK )   break;   // stop if just captured
    }

    return false;
}


bool ChessBoard::BR_CanMove ( int ofs )
{
    Move        move;
    UnmoveInfo  unmove;
    int         islegal;

    move.source = ofs;

    for ( move.dest = ofs + NORTH;
            (board [move.dest] & (BLACK_MASK | OFFBOARD)) == 0;
            move.dest += NORTH )
    {
        CHECK_BLACK_MOVE ( move, unmove );
        if ( board [move.dest] & WHITE_MASK )   break;   // stop if just captured
    }


    for ( move.dest = ofs + WEST;
            (board [move.dest] & (BLACK_MASK | OFFBOARD)) == 0;
            move.dest += BYTE(WEST) )
    {
        CHECK_BLACK_MOVE ( move, unmove );
        if ( board [move.dest] & WHITE_MASK )   break;   // stop if just captured
    }


    for ( move.dest = ofs + SOUTH;
            (board [move.dest] & (BLACK_MASK | OFFBOARD)) == 0;
            move.dest += BYTE(SOUTH) )
    {
        CHECK_BLACK_MOVE ( move, unmove );
        if ( board [move.dest] & WHITE_MASK )   break;   // stop if just captured
    }


    for ( move.dest = ofs + EAST;
            (board [move.dest] & (BLACK_MASK | OFFBOARD)) == 0;
            move.dest += EAST )
    {
        CHECK_BLACK_MOVE ( move, unmove );
        if ( board [move.dest] & WHITE_MASK )   break;   // stop if just captured
    }

    return false;
}


bool ChessBoard::BQ_CanMove ( int ofs )
{
    Move        move;
    UnmoveInfo  unmove;
    int         islegal;

    move.source = ofs;

    for ( move.dest = ofs + NORTH;
            (board [move.dest] & (BLACK_MASK | OFFBOARD)) == 0;
            move.dest += NORTH )
    {
        CHECK_BLACK_MOVE ( move, unmove );
        if ( board [move.dest] & WHITE_MASK )   break;   // stop if just captured
    }


    for ( move.dest = ofs + WEST;
            (board [move.dest] & (BLACK_MASK | OFFBOARD)) == 0;
            move.dest += BYTE(WEST) )
    {
        CHECK_BLACK_MOVE ( move, unmove );
        if ( board [move.dest] & WHITE_MASK )   break;   // stop if just captured
    }


    for ( move.dest = ofs + SOUTH;
            (board [move.dest] & (BLACK_MASK | OFFBOARD)) == 0;
            move.dest += BYTE(SOUTH) )
    {
        CHECK_BLACK_MOVE ( move, unmove );
        if ( board [move.dest] & WHITE_MASK )   break;   // stop if just captured
    }


    for ( move.dest = ofs + EAST;
            (board [move.dest] & (BLACK_MASK | OFFBOARD)) == 0;
            move.dest += BYTE(EAST) )
    {
        CHECK_BLACK_MOVE ( move, unmove );
        if ( board [move.dest] & WHITE_MASK )   break;   // stop if just captured
    }

    for ( move.dest = ofs + NORTHEAST;
            (board [move.dest] & (BLACK_MASK | OFFBOARD)) == 0;
            move.dest += NORTHEAST )
    {
        CHECK_BLACK_MOVE ( move, unmove );
        if ( board [move.dest] & WHITE_MASK )   break;   // stop if just captured
    }


    for ( move.dest = ofs + NORTHWEST;
            (board [move.dest] & (BLACK_MASK | OFFBOARD)) == 0;
            move.dest += BYTE(NORTHWEST) )
    {
        CHECK_BLACK_MOVE ( move, unmove );
        if ( board [move.dest] & WHITE_MASK )   break;   // stop if just captured
    }


    for ( move.dest = ofs + SOUTHWEST;
            (board [move.dest] & (BLACK_MASK | OFFBOARD)) == 0;
            move.dest += BYTE(SOUTHWEST) )
    {
        CHECK_BLACK_MOVE ( move, unmove );
        if ( board [move.dest] & WHITE_MASK )   break;   // stop if just captured
    }


    for ( move.dest = ofs + SOUTHEAST;
            (board [move.dest] & (BLACK_MASK | OFFBOARD)) == 0;
            move.dest += BYTE(SOUTHEAST) )
    {
        CHECK_BLACK_MOVE ( move, unmove );
        if ( board [move.dest] & WHITE_MASK )   break;   // stop if just captured
    }

    return false;
}


bool ChessBoard::BK_CanMove ( int ofs )
{
    // NOTE:  We deliberately ignore castling here.
    //        This is because, if castling is legal, then
    //        moving the king one square in the direction of
    //        the castling rook is also legal.  Therefore,
    //        leaving out castling never causes us to think
    //        that we don't have a legal move where we really do,
    //        or vice versa.

    Move        move;
    UnmoveInfo  unmove;
    bool        islegal;

    move.source = BYTE(ofs);

    if ( (board[move.dest = ofs + WEST] & (BLACK_MASK | OFFBOARD)) == 0 )
    {
        CHECK_BLACK_MOVE ( move, unmove );
    }

    if ( (board[move.dest = ofs + EAST] & (BLACK_MASK | OFFBOARD)) == 0 )
    {
        CHECK_BLACK_MOVE ( move, unmove );
    }

    if ( (board[move.dest = ofs + SOUTH] & (BLACK_MASK | OFFBOARD)) == 0 )
    {
        CHECK_BLACK_MOVE ( move, unmove );
    }

    if ( (board[move.dest = ofs + SOUTHWEST] & (BLACK_MASK | OFFBOARD)) == 0 )
    {
        CHECK_BLACK_MOVE ( move, unmove );
    }

    if ( (board[move.dest = ofs + SOUTHEAST] & (BLACK_MASK | OFFBOARD)) == 0 )
    {
        CHECK_BLACK_MOVE ( move, unmove );
    }

    if ( (board[move.dest = ofs + NORTH] & (BLACK_MASK | OFFBOARD)) == 0 )
    {
        CHECK_BLACK_MOVE ( move, unmove );
    }

    if ( (board[move.dest = ofs + NORTHWEST] & (BLACK_MASK | OFFBOARD)) == 0 )
    {
        CHECK_BLACK_MOVE ( move, unmove );
    }

    if ( (board[move.dest = ofs + NORTHEAST] & (BLACK_MASK | OFFBOARD)) == 0 )
    {
        CHECK_BLACK_MOVE ( move, unmove );
    }

    return false;
}


/*
    $Log: canmove.cpp,v $
    Revision 1.4  2006/01/18 19:58:11  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.3  2005/11/23 21:30:29  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:38  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!


         Revision history:

    1993 October 17 [Don Cross]
         Started writing.

    1994 February 10 [Don Cross]
         Started implementing indexed pieces

*/

