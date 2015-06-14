/*============================================================================

     genmove.cpp  -  Copyright (C) 1993-2005 by Don Cross


     Contains code for generating legal moves for a given ChessBoard.

============================================================================*/

#include "chess.h"
#include "profiler.h"


int ChessBoard::GenWhiteMoves ( 
    MoveList            &ml,
    ComputerChessPlayer *player )
{
    PROFILER_ENTER(PX_GENMOVES);

    int x, ybase, ofs;
    SQUARE  piece;

    ml.num = 0;   // make the MoveList empty.
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
                        GenMoves_WP ( ml, ofs, ybase );
                        break;

                    case N_INDEX:
                        GenMoves_WN ( ml, ofs );
                        break;

                    case B_INDEX:
                        GenMoves_WB ( ml, ofs );
                        break;

                    case R_INDEX:
                        GenMoves_WR ( ml, ofs );
                        break;

                    case Q_INDEX:
                        GenMoves_WQ ( ml, ofs );
                        break;

                    case K_INDEX:
                        GenMoves_WK ( ml, ofs );
                        break;

                    default:
                        ChessFatal ( "Undefined white piece in ChessBoard::GenWhiteMoves" );
                        break;
                }
            }
        }
    }

    RemoveIllegalWhite ( ml, player );
    PROFILER_EXIT();
    return ml.num;
}


void ChessBoard::GenMoves_WP ( MoveList &ml, int ofs, int ybase )
{
    if ( ybase == OFFSET(2,3) )
    {
        // Pawn is on the home row.

        // Check for non-capture moves...
        if ( board [ofs + NORTH] == EMPTY )
        {
            // It can move one square forward...
            ml.AddMove ( ofs, ofs + NORTH );

            // See if we can go two squares!
            if ( board [ofs + 2*NORTH] == EMPTY )
                ml.AddMove ( ofs, ofs + 2*NORTH );
        }

        // Check for capture moves...
        if ( board [ofs + NORTHEAST] & BLACK_MASK )
            ml.AddMove ( ofs, ofs + NORTHEAST );

        if ( board [ofs + NORTHWEST] & BLACK_MASK )
            ml.AddMove ( ofs, ofs + NORTHWEST );
    }
    else if ( ybase == OFFSET(2,6) )
    {
        // Need to look for en passant captures on this rank.

        if ( board [ofs + NORTH] == EMPTY )
            ml.AddMove ( ofs, ofs + NORTH );

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
        // Pawn is one square away from promoting.  See if it can promote...
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
    else
    {
        // Normal pawn move away from home square...

        if ( board [ofs + NORTH] == EMPTY )
            ml.AddMove ( ofs, ofs + NORTH );

        // Check for capture moves...
        if ( board [ofs + NORTHEAST] & BLACK_MASK )
            ml.AddMove ( ofs, ofs + NORTHEAST );

        if ( board [ofs + NORTHWEST] & BLACK_MASK )
            ml.AddMove ( ofs, ofs + NORTHWEST );
    }
}


void ChessBoard::GenMoves_WN ( MoveList &ml, int source )
{
    if ( (board [source + OFFSET(1,2)] & (WHITE_MASK | OFFBOARD)) == 0 )
        ml.AddMove ( source, source + OFFSET(1,2) );

    if ( (board [source + OFFSET(1,-2)] & (WHITE_MASK | OFFBOARD)) == 0 )
        ml.AddMove ( source, source + OFFSET(1,-2) );

    if ( (board [source + OFFSET(-1,2)] & (WHITE_MASK | OFFBOARD)) == 0 )
        ml.AddMove ( source, source + OFFSET(-1,2) );

    if ( (board [source + OFFSET(-1,-2)] & (WHITE_MASK | OFFBOARD)) == 0 )
        ml.AddMove ( source, source + OFFSET(-1,-2) );

    if ( (board [source + OFFSET(2,1)] & (WHITE_MASK | OFFBOARD)) == 0 )
        ml.AddMove ( source, source + OFFSET(2,1) );

    if ( (board [source + OFFSET(2,-1)] & (WHITE_MASK | OFFBOARD)) == 0 )
        ml.AddMove ( source, source + OFFSET(2,-1) );

    if ( (board [source + OFFSET(-2,1)] & (WHITE_MASK | OFFBOARD)) == 0 )
        ml.AddMove ( source, source + OFFSET(-2,1) );

    if ( (board [source + OFFSET(-2,-1)] & (WHITE_MASK | OFFBOARD)) == 0 )
        ml.AddMove ( source, source + OFFSET(-2,-1) );
}


void ChessBoard::GenMoves_WB ( MoveList &ml, int source )
{
    int dest;
    for ( dest = source + NORTHEAST; board[dest] == EMPTY; dest += NORTHEAST )
        ml.AddMove ( source, dest );

    if ( board[dest] & BLACK_MASK )
        ml.AddMove ( source, dest );

    for ( dest = source + NORTHWEST; board[dest] == EMPTY; dest += NORTHWEST )
        ml.AddMove ( source, dest );

    if ( board[dest] & BLACK_MASK )
        ml.AddMove ( source, dest );

    for ( dest = source + SOUTHEAST; board[dest] == EMPTY; dest += SOUTHEAST )
        ml.AddMove ( source, dest );

    if ( board[dest] & BLACK_MASK )
        ml.AddMove ( source, dest );

    for ( dest = source + SOUTHWEST; board[dest] == EMPTY; dest += SOUTHWEST )
        ml.AddMove ( source, dest );

    if ( board[dest] & BLACK_MASK )
        ml.AddMove ( source, dest );
}


void ChessBoard::GenMoves_WR ( MoveList &ml, int source )
{
    int dest;
    for ( dest = source + NORTH; board[dest] == EMPTY; dest += NORTH )
        ml.AddMove ( source, dest );

    if ( board[dest] & BLACK_MASK )
        ml.AddMove ( source, dest );

    for ( dest = source + WEST; board[dest] == EMPTY; dest += WEST )
        ml.AddMove ( source, dest );

    if ( board[dest] & BLACK_MASK )
        ml.AddMove ( source, dest );

    for ( dest = source + EAST; board[dest] == EMPTY; dest += EAST )
        ml.AddMove ( source, dest );

    if ( board[dest] & BLACK_MASK )
        ml.AddMove ( source, dest );

    for ( dest = source + SOUTH; board[dest] == EMPTY; dest += SOUTH )
        ml.AddMove ( source, dest );

    if ( board[dest] & BLACK_MASK )
        ml.AddMove ( source, dest );
}


void ChessBoard::GenMoves_WQ ( MoveList &ml, int source )
{
    int dest;
    for ( dest = source + NORTHEAST; board[dest] == EMPTY; dest += NORTHEAST )
        ml.AddMove ( source, dest );

    if ( board[dest] & BLACK_MASK )
        ml.AddMove ( source, dest );

    for ( dest = source + NORTHWEST; board[dest] == EMPTY; dest += NORTHWEST )
        ml.AddMove ( source, dest );

    if ( board[dest] & BLACK_MASK )
        ml.AddMove ( source, dest );

    for ( dest = source + SOUTHEAST; board[dest] == EMPTY; dest += SOUTHEAST )
        ml.AddMove ( source, dest );

    if ( board[dest] & BLACK_MASK )
        ml.AddMove ( source, dest );

    for ( dest = source + SOUTHWEST; board[dest] == EMPTY; dest += SOUTHWEST )
        ml.AddMove ( source, dest );

    if ( board[dest] & BLACK_MASK )
        ml.AddMove ( source, dest );

    for ( dest = source + NORTH; board[dest] == EMPTY; dest += NORTH )
        ml.AddMove ( source, dest );

    if ( board[dest] & BLACK_MASK )
        ml.AddMove ( source, dest );

    for ( dest = source + WEST; board[dest] == EMPTY; dest += WEST )
        ml.AddMove ( source, dest );

    if ( board[dest] & BLACK_MASK )
        ml.AddMove ( source, dest );

    for ( dest = source + EAST; board[dest] == EMPTY; dest += EAST )
        ml.AddMove ( source, dest );

    if ( board[dest] & BLACK_MASK )
        ml.AddMove ( source, dest );

    for ( dest = source + SOUTH; board[dest] == EMPTY; dest += SOUTH )
        ml.AddMove ( source, dest );

    if ( board[dest] & BLACK_MASK )
        ml.AddMove ( source, dest );
}


void ChessBoard::GenMoves_WK ( MoveList &ml, int source )
{
    if ( (board [source + NORTH] & (WHITE_MASK | OFFBOARD)) == 0 )
        ml.AddMove ( source, source + NORTH );

    if ( (board [source + NORTHEAST] & (WHITE_MASK | OFFBOARD)) == 0 )
        ml.AddMove ( source, source + NORTHEAST );

    if ( (board [source + NORTHWEST] & (WHITE_MASK | OFFBOARD)) == 0 )
        ml.AddMove ( source, source + NORTHWEST );

    if ( (board [source + EAST] & (WHITE_MASK | OFFBOARD)) == 0 )
        ml.AddMove ( source, source + EAST );

    if ( (board [source + WEST] & (WHITE_MASK | OFFBOARD)) == 0 )
        ml.AddMove ( source, source + WEST );

    if ( (board [source + SOUTHEAST] & (WHITE_MASK | OFFBOARD)) == 0 )
        ml.AddMove ( source, source + SOUTHEAST);

    if ( (board [source + SOUTHWEST] & (WHITE_MASK | OFFBOARD)) == 0 )
        ml.AddMove ( source, source + SOUTHWEST);

    if ( (board [source + SOUTH] & (WHITE_MASK | OFFBOARD)) == 0 )
        ml.AddMove ( source, source + SOUTH);

    // Check for castling...
    if ( (flags & (SF_WKMOVED|SF_WCHECK)) == 0 )
    {
        // check for O-O
        if (    (flags & SF_WKRMOVED) == 0  &&
                (board [OFFSET(9,2)] & WR_MASK)  &&
                board [OFFSET(8,2)] == EMPTY     &&
                board [OFFSET(7,2)] == EMPTY     &&
                !IsAttackedByBlack ( OFFSET(7,2) ) )
            ml.AddMove ( source, SPECIAL_MOVE_KCASTLE );

        // check for O-O-O
        if (    (flags & SF_WQRMOVED) == 0  &&
                (board [OFFSET(2,2)] & WR_MASK)  &&
                board [OFFSET(3,2)] == EMPTY     &&
                board [OFFSET(4,2)] == EMPTY     &&
                board [OFFSET(5,2)] == EMPTY     &&
                !IsAttackedByBlack ( OFFSET(5,2) ) )
            ml.AddMove ( source, SPECIAL_MOVE_QCASTLE );
    }
}



//--------------------------------------------------------------------------

int ChessBoard::GenBlackMoves ( 
    MoveList             &ml,
    ComputerChessPlayer  *player )
{
    PROFILER_ENTER(PX_GENMOVES)
    int x, ybase, ofs;
    SQUARE  piece;

    ml.num = 0;   // make the MoveList empty.

    for ( ybase = OFFSET(2,9); ybase > OFFSET(2,1); ybase += SOUTH )
    {
        for ( x=0; x < 8; x++ )
        {
            piece = board [ofs = ybase + x];

            if ( piece & BLACK_MASK )
            {
                switch ( UPIECE_INDEX(piece) )
                {
                    case P_INDEX:
                        GenMoves_BP ( ml, ofs, ybase );
                        break;

                    case N_INDEX:
                        GenMoves_BN ( ml, ofs );
                        break;

                    case B_INDEX:
                        GenMoves_BB ( ml, ofs );
                        break;

                    case R_INDEX:
                        GenMoves_BR ( ml, ofs );
                        break;

                    case Q_INDEX:
                        GenMoves_BQ ( ml, ofs );
                        break;

                    case K_INDEX:
                        GenMoves_BK ( ml, ofs );
                        break;

                    default:
                        ChessFatal ( "Undefined black piece in ChessBoard::GenBlackMoves" );
                        break;
                }
            }
        }
    }

    RemoveIllegalBlack ( ml, player );
    PROFILER_EXIT()
    return ml.num;
}


void ChessBoard::GenMoves_BP ( MoveList &ml, int ofs, int ybase )
{
    if ( ybase == OFFSET(2,8) )
    {
        // Pawn is on the home row.

        // Check for non-capture moves...
        if ( board [ofs + SOUTH] == EMPTY )
        {
            // It can move one square forward...
            ml.AddMove ( ofs, ofs + SOUTH );

            // See if we can go two squares!
            if ( board [ofs + 2*SOUTH] == EMPTY )
                ml.AddMove ( ofs, ofs + 2*SOUTH );
        }

        // Check for capture moves...
        if ( board [ofs + SOUTHEAST] & WHITE_MASK )
            ml.AddMove ( ofs, ofs + SOUTHEAST );

        if ( board [ofs + SOUTHWEST] & WHITE_MASK )
            ml.AddMove ( ofs, ofs + SOUTHWEST );
    }
    else if ( ybase == OFFSET(2,5) )
    {
        // Need to look for en passant captures on this rank.

        if ( board [ofs + SOUTH] == EMPTY )
            ml.AddMove ( ofs, ofs + SOUTH );

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
    else if ( ybase == OFFSET(2,3) )
    {
        // Pawn is one square away from promoting.  See if it can promote...
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
    else
    {
        // Normal pawn move away from home square...

        if ( board [ofs + SOUTH] == EMPTY )
            ml.AddMove ( ofs, ofs + SOUTH );

        // Check for capture moves...
        if ( board [ofs + SOUTHEAST] & WHITE_MASK )
            ml.AddMove ( ofs, ofs + SOUTHEAST );

        if ( board [ofs + SOUTHWEST] & WHITE_MASK )
            ml.AddMove ( ofs, ofs + SOUTHWEST );
    }
}


void ChessBoard::GenMoves_BN ( MoveList &ml, int source )
{
    if ( (board [source + OFFSET(1,2)] & (BLACK_MASK | OFFBOARD)) == 0 )
        ml.AddMove ( source, source + OFFSET(1,2) );

    if ( (board [source + OFFSET(1,-2)] & (BLACK_MASK | OFFBOARD)) == 0 )
        ml.AddMove ( source, source + OFFSET(1,-2) );

    if ( (board [source + OFFSET(-1,2)] & (BLACK_MASK | OFFBOARD)) == 0 )
        ml.AddMove ( source, source + OFFSET(-1,2) );

    if ( (board [source + OFFSET(-1,-2)] & (BLACK_MASK | OFFBOARD)) == 0 )
        ml.AddMove ( source, source + OFFSET(-1,-2) );

    if ( (board [source + OFFSET(2,1)] & (BLACK_MASK | OFFBOARD)) == 0 )
        ml.AddMove ( source, source + OFFSET(2,1) );

    if ( (board [source + OFFSET(2,-1)] & (BLACK_MASK | OFFBOARD)) == 0 )
        ml.AddMove ( source, source + OFFSET(2,-1) );

    if ( (board [source + OFFSET(-2,1)] & (BLACK_MASK | OFFBOARD)) == 0 )
        ml.AddMove ( source, source + OFFSET(-2,1) );

    if ( (board [source + OFFSET(-2,-1)] & (BLACK_MASK | OFFBOARD)) == 0 )
        ml.AddMove ( source, source + OFFSET(-2,-1) );
}


void ChessBoard::GenMoves_BB ( MoveList &ml, int source )
{
    int dest;
    for ( dest = source + NORTHEAST; board[dest] == EMPTY; dest += NORTHEAST )
        ml.AddMove ( source, dest );

    if ( board[dest] & WHITE_MASK )
        ml.AddMove ( source, dest );

    for ( dest = source + NORTHWEST; board[dest] == EMPTY; dest += NORTHWEST )
        ml.AddMove ( source, dest );

    if ( board[dest] & WHITE_MASK )
        ml.AddMove ( source, dest );

    for ( dest = source + SOUTHEAST; board[dest] == EMPTY; dest += SOUTHEAST )
        ml.AddMove ( source, dest );

    if ( board[dest] & WHITE_MASK )
        ml.AddMove ( source, dest );

    for ( dest = source + SOUTHWEST; board[dest] == EMPTY; dest += SOUTHWEST )
        ml.AddMove ( source, dest );

    if ( board[dest] & WHITE_MASK )
        ml.AddMove ( source, dest );
}


void ChessBoard::GenMoves_BR ( MoveList &ml, int source )
{
    int dest;
    for ( dest = source + NORTH; board[dest] == EMPTY; dest += NORTH )
        ml.AddMove ( source, dest );

    if ( board[dest] & WHITE_MASK )
        ml.AddMove ( source, dest );

    for ( dest = source + WEST; board[dest] == EMPTY; dest += WEST )
        ml.AddMove ( source, dest );

    if ( board[dest] & WHITE_MASK )
        ml.AddMove ( source, dest );

    for ( dest = source + EAST; board[dest] == EMPTY; dest += EAST )
        ml.AddMove ( source, dest );

    if ( board[dest] & WHITE_MASK )
        ml.AddMove ( source, dest );

    for ( dest = source + SOUTH; board[dest] == EMPTY; dest += SOUTH )
        ml.AddMove ( source, dest );

    if ( board[dest] & WHITE_MASK )
        ml.AddMove ( source, dest );
}


void ChessBoard::GenMoves_BQ ( MoveList &ml, int source )
{
    int dest;
    for ( dest = source + NORTHEAST; board[dest] == EMPTY; dest += NORTHEAST )
        ml.AddMove ( source, dest );

    if ( board[dest] & WHITE_MASK )
        ml.AddMove ( source, dest );

    for ( dest = source + NORTHWEST; board[dest] == EMPTY; dest += NORTHWEST )
        ml.AddMove ( source, dest );

    if ( board[dest] & WHITE_MASK )
        ml.AddMove ( source, dest );

    for ( dest = source + SOUTHEAST; board[dest] == EMPTY; dest += SOUTHEAST )
        ml.AddMove ( source, dest );

    if ( board[dest] & WHITE_MASK )
        ml.AddMove ( source, dest );

    for ( dest = source + SOUTHWEST; board[dest] == EMPTY; dest += SOUTHWEST )
        ml.AddMove ( source, dest );

    if ( board[dest] & WHITE_MASK )
        ml.AddMove ( source, dest );

    for ( dest = source + NORTH; board[dest] == EMPTY; dest += NORTH )
        ml.AddMove ( source, dest );

    if ( board[dest] & WHITE_MASK )
        ml.AddMove ( source, dest );

    for ( dest = source + WEST; board[dest] == EMPTY; dest += WEST )
        ml.AddMove ( source, dest );

    if ( board[dest] & WHITE_MASK )
        ml.AddMove ( source, dest );

    for ( dest = source + EAST; board[dest] == EMPTY; dest += EAST )
        ml.AddMove ( source, dest );

    if ( board[dest] & WHITE_MASK )
        ml.AddMove ( source, dest );

    for ( dest = source + SOUTH; board[dest] == EMPTY; dest += SOUTH )
        ml.AddMove ( source, dest );

    if ( board[dest] & WHITE_MASK )
        ml.AddMove ( source, dest );
}


void ChessBoard::GenMoves_BK ( MoveList &ml, int source )
{
    if ( (board [source + NORTH] & (BLACK_MASK | OFFBOARD)) == 0 )
        ml.AddMove ( source, source + NORTH );

    if ( (board [source + NORTHEAST] & (BLACK_MASK | OFFBOARD)) == 0 )
        ml.AddMove ( source, source + NORTHEAST );

    if ( (board [source + NORTHWEST] & (BLACK_MASK | OFFBOARD)) == 0 )
        ml.AddMove ( source, source + NORTHWEST );

    if ( (board [source + EAST] & (BLACK_MASK | OFFBOARD)) == 0 )
        ml.AddMove ( source, source + EAST );

    if ( (board [source + WEST] & (BLACK_MASK | OFFBOARD)) == 0 )
        ml.AddMove ( source, source + WEST );

    if ( (board [source + SOUTHEAST] & (BLACK_MASK | OFFBOARD)) == 0 )
        ml.AddMove ( source, source + SOUTHEAST);

    if ( (board [source + SOUTHWEST] & (BLACK_MASK | OFFBOARD)) == 0 )
        ml.AddMove ( source, source + SOUTHWEST);

    if ( (board [source + SOUTH] & (BLACK_MASK | OFFBOARD)) == 0 )
        ml.AddMove ( source, source + SOUTH);

    // Check for castling...
    if ( (flags & (SF_BKMOVED|SF_BCHECK)) == 0 )
    {
        // check for O-O
        if (    (flags & SF_BKRMOVED) == 0       &&
                (board [OFFSET(9,9)] & BR_MASK)  &&
                board [OFFSET(8,9)] == EMPTY     &&
                board [OFFSET(7,9)] == EMPTY     &&
                !IsAttackedByWhite ( OFFSET(7,9) ) )
            ml.AddMove ( source, SPECIAL_MOVE_KCASTLE );

        // check for O-O-O
        if (    (flags & SF_BQRMOVED) == 0       &&
                (board [OFFSET(2,9)] & BR_MASK)  &&
                board [OFFSET(3,9)] == EMPTY     &&
                board [OFFSET(4,9)] == EMPTY     &&
                board [OFFSET(5,9)] == EMPTY     &&
                !IsAttackedByWhite ( OFFSET(5,9) ) )
            ml.AddMove ( source, SPECIAL_MOVE_QCASTLE );
    }
}


void ChessBoard::RemoveIllegalWhite ( 
    MoveList &ml,
    ComputerChessPlayer *player )
{
    UnmoveInfo    unmove;
    Move         *move;
    int           i;
    bool      is_illegal;

    if ( player )
    {
        for ( i=0, move = &ml.m[0]; i < ml.num; )
        {
            MakeWhiteMove ( *move, unmove, true, true );
            is_illegal = (flags & SF_WCHECK) ? true : false;
            if ( !is_illegal )
            {
                player->WhiteMoveOrdering ( 
                    *this, *move, unmove,
                    player->moveOrder_depth,
                    player->moveOrder_bestPathFlag );
            }
            UnmakeWhiteMove ( *move, unmove );

            if ( is_illegal )
            {
                if ( i < --(ml.num) )                   
                    *move = ml.m [ml.num];      // Overwrite this move with the last move...
            }
            else
            {
                ++i;
                ++move;
            }
        }
        if (player->IsSearchRandomized()) {
            ml.Shuffle();
        }
        ml.WhiteSort();
    }
    else
    {
        for ( i=0, move = &ml.m[0]; i < ml.num; )
        {
            MakeWhiteMove ( *move, unmove, true, true );
            is_illegal = (flags & SF_WCHECK) ? true : false;
            UnmakeWhiteMove ( *move, unmove );

            if ( is_illegal )
            {
                if ( i < --(ml.num) )
                   *move = ml.m [ml.num];       // Overwrite this move with the last move...
            }
            else
            {
                move->score = 0;
                ++i;
                ++move;
            }
        }
    }
}


void ChessBoard::RemoveIllegalBlack ( 
    MoveList            &ml,
    ComputerChessPlayer *player )
{
    UnmoveInfo   unmove;
    Move        *move;
    int          i;
    bool     is_illegal;

    if ( player )
    {
        for ( i=0, move = &ml.m[0]; i < ml.num; )
        {
            MakeBlackMove ( *move, unmove, true, true );
            is_illegal = (flags & SF_BCHECK) ? true : false;
            if ( !is_illegal )
            {
                player->BlackMoveOrdering ( 
                    *this, *move, unmove,
                    player->moveOrder_depth,
                    player->moveOrder_bestPathFlag );
            }
            UnmakeBlackMove ( *move, unmove );

            if ( is_illegal )
            {
                if ( i < --(ml.num) )
                   *move = ml.m [ml.num];   // Overwrite this move with the last move...
            }
            else
            {
                ++i;
                ++move;
            }
        }
        if (player->IsSearchRandomized()) {
            ml.Shuffle();
        }
        ml.BlackSort();
    }
    else
    {
        for ( i=0, move = &ml.m[0]; i < ml.num; )
        {
            MakeBlackMove ( *move, unmove, true, true );
            is_illegal = (flags & SF_BCHECK) ? true : false;
            UnmakeBlackMove ( *move, unmove );

            if ( is_illegal )
            {
                if ( i < --(ml.num) )
                    *move = ml.m [ml.num];      // Overwrite this move with the last move...
            }
            else
            {
                move->score = 0;
                ++i;
                ++move;
            }
        }
    }
}


/*
    $Log: genmove.cpp,v $
    Revision 1.5  2006/01/25 19:09:12  dcross
    Today I noticed a case where Chenard correctly predicted the opponent's next move,
    but ran out of time while doing the deep search.  It then proceeded to pick a horrible
    move randomly from the prior list.  Because of this, and the fact that I knew the
    search would be faster, I have adopted the approach I took in my Java chess applet:
    Instead of using a pruning searchBias of 1, and randomly picking from the set of moves
    with maximum value after the fact, just shuffle the movelist right before doing move
    ordering at the top level of the search.  This also has the advantage of always making
    the move shown at the top of the best path.  So now "searchBias" is really a boolean
    flag that specifies whether to pre-shuffle the move list or not.  It has been removed
    from the pruning logic, so the search is slightly more efficient now.

    Revision 1.4  2006/01/18 19:58:11  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

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
         Adding some debug code.
    
    1999 January 4 [Don Cross]
         Updating coding style.
    
    1994 February 10 [Don Cross]
         Adding piece indexing.
    
    1994 February 3 [Don Cross]
         Added support for BestPath.
    
    1993 October 21 [Don Cross]
         Made GenWhiteMoves and GenBlackMoves initialize each move.score
         to 0 if this is not being done for a ComputerChessPlayer.
    
    1993 August 30 [Don Cross]
         Changing pointers to references in the interfaces where
         appropriate.
    
    1993 April ?? [Don Cross]
         Started writing.
    
*/

