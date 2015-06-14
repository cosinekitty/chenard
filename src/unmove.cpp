/*============================================================================

      unmove.cpp  -  Copyright (C) 1993-2005 by Don Cross

      Contains code for unmaking a move on a ChessBoard.
      When making a move in a ChessBoard, state information
      is saved in an 'UnmoveInfo' struct which allows this
      code to restore the previous state of the board.
      This is a lot faster than saving and restoring the entire board,
      though this is more complicated.

============================================================================*/

#include "chess.h"
#include "profiler.h"

#define DEBUG_UNMOVE 0

void ChessBoard::UnmakeWhiteMove ( Move move, UnmoveInfo &unmove )
{
    PROFILER_ENTER(PX_UNMOVE);

    int      source = move.source & BOARD_OFFSET_MASK;
    int      dest;
    SQUARE   capture = unmove.capture;
    int      prom_piece_index;

#if DEBUG_UNMOVE
    if ( board[source] != EMPTY )
        ChessFatal ( "source not empty in UnmakeWhiteMove" );

    SQUARE destSquare = EMPTY;
#endif

    if ( move.dest > OFFSET(9,9) )
    {
        // This is a "special" move...

        switch ( move.dest & 0xF0 )
        {
            case SPECIAL_MOVE_PROMOTE_NORM:
                prom_piece_index = (move.dest & PIECE_MASK) | WHITE_IND;
                dest = source + NORTH;
                #if DEBUG_UNMOVE
                    destSquare = board[dest];
                #endif
                board [source] = WPAWN;
                board [dest] = EMPTY;
                ++inventory [WP_INDEX];
                --inventory [prom_piece_index];
                break;

            case SPECIAL_MOVE_PROMOTE_CAP_EAST:
                prom_piece_index = (move.dest & PIECE_MASK) | WHITE_IND;
                dest = source + NORTHEAST;
                #if DEBUG_UNMOVE
                    destSquare = board[dest];
                #endif
                board [source] = WPAWN;
                board [dest] = capture;
                ++inventory [WP_INDEX];
                --inventory [prom_piece_index];
                break;

            case SPECIAL_MOVE_PROMOTE_CAP_WEST:
                prom_piece_index = (move.dest & PIECE_MASK) | WHITE_IND;
                dest = source + NORTHWEST;
                #if DEBUG_UNMOVE
                    destSquare = board[dest];
                #endif
                board [source] = WPAWN;
                board [dest] = capture;
                ++inventory [WP_INDEX];
                --inventory [prom_piece_index];
                break;

            case SPECIAL_MOVE_KCASTLE:
                #if DEBUG_UNMOVE
                    destSquare = board[OFFSET(8,2)];
                #endif
                wk_offset = OFFSET(6,2);
                board [ OFFSET(6,2) ] = WKING;
                board [ OFFSET(9,2) ] = WROOK;
                board [ OFFSET(7,2) ] = board [ OFFSET(8,2) ] = EMPTY;
                break;

            case SPECIAL_MOVE_QCASTLE:
                #if DEBUG_UNMOVE
                    destSquare = board[OFFSET(4,2)];
                #endif
                wk_offset = OFFSET(6,2);
                board [ OFFSET(6,2) ] = WKING;
                board [ OFFSET(2,2) ] = WROOK;
                board [ OFFSET(4,2) ] = board [ OFFSET(5,2) ] = EMPTY;
                break;

            case SPECIAL_MOVE_EP_EAST:
                board [source] = board [dest = source + NORTHEAST];
                #if DEBUG_UNMOVE
                    destSquare = board[dest];
                #endif
                board [dest] = EMPTY;
                board [source + EAST] = capture;
                break;

            case SPECIAL_MOVE_EP_WEST:
                board [source] = board [dest = source + NORTHWEST];
                #if DEBUG_UNMOVE
                    destSquare = board[dest];
                #endif
                board [dest] = EMPTY;
                board [source + WEST] = capture;
                break;

            default:
                ChessFatal ( "Invalid special move code in ChessBoard::UnmakeWhiteMove" );
                break;
        }
    }
    else
    {
        // This is a "normal" move...
        dest = move.dest;
        SQUARE move_piece = board[source] = board[dest];
        #if DEBUG_UNMOVE
            destSquare = board[dest];
        #endif
        board[dest] = capture;

        if ( move_piece & WK_MASK )
            wk_offset = source;
    }

#if DEBUG_UNMOVE
    if ( !(destSquare & WHITE_MASK) )
        ChessFatal ( "Attempt to unmove non-white piece in UnmakeWhiteMove" );
#endif

    if ( capture != EMPTY )
        ++inventory [SPIECE_INDEX(capture)];

    flags            =  unmove.flags;
    bmaterial        =  unmove.bmaterial;
    wmaterial        =  unmove.wmaterial;
    prev_move        =  unmove.prev_move;
    lastCapOrPawn    =  unmove.lastCapOrPawn;
    repeatHash [cachedHash % REPEAT_HASH_SIZE] -= 0x10;  // no longer Black to move
    cachedHash       =  unmove.cachedHash;

    --ply_number;
    white_to_move = true;

    PROFILER_EXIT();
}


//------------------------------------------------------------------------


void ChessBoard::UnmakeBlackMove ( Move move, UnmoveInfo &unmove )
{
    PROFILER_ENTER(PX_UNMOVE)

    int      source = move.source & BOARD_OFFSET_MASK;
    int      dest;
    SQUARE   capture = unmove.capture;
    SQUARE   prom_piece_index;

#if DEBUG_UNMOVE
    if ( board[source] != EMPTY )
        ChessFatal ( "source not empty in UnmakeWhiteMove" );

    SQUARE destSquare = EMPTY;
#endif

    if ( move.dest > OFFSET(9,9) )
    {
        // This is a "special" move...

        switch ( move.dest & 0xF0 )
        {
            case SPECIAL_MOVE_PROMOTE_NORM:
                prom_piece_index = (move.dest & PIECE_MASK) | BLACK_IND;
                dest = source + SOUTH;
                #if DEBUG_UNMOVE
                    destSquare = board[dest];
                #endif
                board [source] = BPAWN;
                board [dest] = EMPTY;
                ++inventory [BP_INDEX];
                --inventory [prom_piece_index];
                break;

            case SPECIAL_MOVE_PROMOTE_CAP_EAST:
                prom_piece_index = (move.dest & PIECE_MASK) | BLACK_IND;
                dest = source + SOUTHEAST;
                #if DEBUG_UNMOVE
                    destSquare = board[dest];
                #endif
                board [source] = BPAWN;
                board [dest] = capture;
                ++inventory [BP_INDEX];
                --inventory [prom_piece_index];
                break;

            case SPECIAL_MOVE_PROMOTE_CAP_WEST:
                prom_piece_index = (move.dest & PIECE_MASK) | BLACK_IND;
                dest = source + SOUTHWEST;
                #if DEBUG_UNMOVE
                    destSquare = board[dest];
                #endif
                board [source] = BPAWN;
                board [dest] = capture;
                ++inventory [BP_INDEX];
                --inventory [prom_piece_index];
                break;

            case SPECIAL_MOVE_KCASTLE:
                #if DEBUG_UNMOVE
                    destSquare = board[OFFSET(8,9)];
                #endif
                bk_offset = OFFSET(6,9);
                board [ OFFSET(6,9) ] = BKING;
                board [ OFFSET(9,9) ] = BROOK;
                board [ OFFSET(7,9) ] = board [ OFFSET(8,9) ] = EMPTY;
                break;

            case SPECIAL_MOVE_QCASTLE:
                #if DEBUG_UNMOVE
                    destSquare = board[OFFSET(4,9)];
                #endif
                bk_offset = OFFSET(6,9);
                board [ OFFSET(6,9) ] = BKING;
                board [ OFFSET(2,9) ] = BROOK;
                board [ OFFSET(4,9) ] = board [ OFFSET(5,9) ] = EMPTY;
                break;

            case SPECIAL_MOVE_EP_EAST:
                board [source] = board [dest = source + SOUTHEAST];
                #if DEBUG_UNMOVE
                    destSquare = board[dest];
                #endif
                board [dest] = EMPTY;
                board [source + EAST] = capture;
                break;

            case SPECIAL_MOVE_EP_WEST:
                board [source] = board [dest = source + SOUTHWEST];
                #if DEBUG_UNMOVE
                    destSquare = board[dest];
                #endif
                board [dest] = EMPTY;
                board [source + WEST] = capture;
                break;

            default:
                ChessFatal ( "Invalid special move code in ChessBoard::UnmakeBlackMove" );
                break;
        }
    }
    else
    {
        // This is a "normal" move...
        dest = move.dest;
        #if DEBUG_UNMOVE
            destSquare = board[dest];
        #endif
        SQUARE move_piece = board[source] = board[dest];
        board [dest] = capture;

        if ( move_piece & BK_MASK )
            bk_offset = source;
    }

#if DEBUG_UNMOVE
    if ( !(destSquare & BLACK_MASK) )
        ChessFatal ( "Attempt to unmove non-black piece in UnmakeBlackMove" );
#endif

    if ( capture != EMPTY )
        ++inventory [SPIECE_INDEX(capture)];

    flags            =  unmove.flags;
    bmaterial        =  unmove.bmaterial;
    wmaterial        =  unmove.wmaterial;
    prev_move        =  unmove.prev_move;
    lastCapOrPawn    =  unmove.lastCapOrPawn;

    repeatHash [cachedHash % REPEAT_HASH_SIZE] -= 0x01;  // no longer White to move
    cachedHash       =  unmove.cachedHash;

    --ply_number;
    white_to_move = false;

    PROFILER_EXIT()
}


/*
    $Log: unmove.cpp,v $
    Revision 1.4  2006/01/18 19:58:16  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.3  2005/11/23 21:30:32  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:44  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!

    
      Revision history:

    1999 August 4 [Don Cross]
         Adding more debug code.
    
    1999 January 19 [Don Cross]
         Adding new board hashing for detecting draws by repetition.
    
    1999 January 5 [Don Cross]
         Updating coding style.
    
    1994 February 10 [Don Cross]
         Adding indexed pieces.
    
    1993 October 22 [Don Cross]
         Fixed a bug in pawn promotion code:  when a pawn promotion was
         also a capture, I was forgetting to initialize prom_piece.
         I have changed the code from getting prom_piece from the dest
         square in this case to just using the move.dest itself to
         figure out the promoted piece.
    
    1993 October 19 [Don Cross]
         Removed unnecessary updates of wmaterial and bmaterial.
         They are unnecessary because we just restore them from
         unmove.wmaterial and unmove.bmaterial, respectively.
    
    1993 August 30 [Don Cross]
         Changing pointers to references in the interfaces where
         appropriate.
    
    1993 May 29 [Don Cross]:
         Fixed bug with not updating bk_offset and wk_offset while unmaking
         non-special moves.
    
*/

