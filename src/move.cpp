/*==========================================================================

      move.cpp  -  Copyright (C) 1993-2005 by Don Cross

      Contains code for making moves on a ChessBoard.

==========================================================================*/

#include "chess.h"
#include "profiler.h"


#define  CHESS_MOVE_DEBUG  0
#define  BOARD_HASH_DEBUG  0

#define HASH_PIECE(piece,ofs)  (PieceMush[SPIECE_INDEX(piece)]*OffsetMush[ofs])
#define LIFT_PIECE(piece,ofs)  (cachedHash -= HASH_PIECE(piece,ofs))
#define DROP_PIECE(piece,ofs)  (cachedHash += HASH_PIECE(piece,ofs))

// The following array helps to "randomize" the offsets to improve
// the ChessBoard::CalcHash() function.  Before, the hash function
// was hash = sum(board[ofs]*ofs), but now I have changed this to
// hash = sum(board[ofs]*OffsetMush[ofs]).  I did this because I
// found a chess game which causes a false draw-by-repetition.
// Hopefully this will make such a thing far less likely.


UINT32 OffsetMush [144] =
{
    0x386C1150, 0xE9BEF540, 0xA958C655, 0xC34BE6D7,
    0xCBCAC60E, 0xDED40541, 0x0B9F8909, 0x08AC5E02,
    0xCAB98467, 0xACBE1918, 0xBF07E0F3, 0x2E001E97,
    0x93A0F2C8, 0xD3AE930D, 0x241AAB1D, 0x53B59E16,

    0x763A2550, 0x683A8C25, 0x6D649356, 0xAA07A163,
    0xECFF7A21, 0x85F21A3D, 0x69910950, 0xFFE520C4,
    0x9E2BADB1, 0x9919CAAB, 0xD1BB5EEC, 0xBE772461,
    0x29E13885, 0xCFBCB1BE, 0xFEADC57B, 0x41F6459D,

    0x8AB32782, 0xA97748E9, 0x47E25326, 0x4D05B06C,
    0x9B39183A, 0xFDE2036F, 0x04EC1537, 0x374604C6,
    0xD86440F8, 0x63786AEA, 0xD939A410, 0x55E05FC6,
    0xB43D817A, 0xAF8198F2, 0x77C2DEDD, 0xC16B3688,

    0xEAB61119, 0xB218F5FA, 0xC536FA28, 0x6CCBF054,
    0xAAF58D01, 0x8DEF71F0, 0xE4D9B485, 0x93F41495,
    0x040087B4, 0x5FB10F6C, 0x708CD5F4, 0xC7814663,
    0xF363C6D6, 0xDF5A4E0E, 0x499EF158, 0x4DD1F995,

    0x504128D1, 0x098C808A, 0xC0001ACC, 0xE3EE0A53,
    0xC153DC84, 0x39A490A6, 0x956202E3, 0xE133DA1F,
    0x30111018, 0x82B89623, 0x8374881E, 0x4F641DA0,
    0xDCC467D9, 0x26375851, 0xB5F95DCA, 0x968F6DBF,

    0x29F37AF2, 0xE08ECB70, 0x27C2A910, 0xB32CF6B6,
    0xF0E66DD3, 0xF9AC566B, 0x3A1790C3, 0xDFABF62D,
    0xF37CCF4F, 0xB9232C39, 0x418018C3, 0x7C38BA30,
    0x12AF6D87, 0x4ED8A572, 0xB1D0C7A5, 0xC14384DC,

    0x1E59E581, 0x1BD9C529, 0x6CB4300D, 0xD6F36867,
    0x35D96756, 0xFDA46536, 0xBF81EF8F, 0x1148F5E3,
    0x370B10E1, 0xE4CC0C0E, 0xE2ADFF2F, 0xD6DD7A05,
    0xB6663C55, 0x7F17F295, 0x07C81689, 0x6555550F,

    0xE3A73DC6, 0x11149F78, 0xB7550DEC, 0x594841DE,
    0xBDF7BB62, 0xD8403385, 0x7285C7D0, 0xDB29EB32,
    0x1FEFC269, 0xCFF586B3, 0x95F87A8F, 0x28A3FD26,
    0x318707DF, 0x789FB468, 0xDD3E95AE, 0x23444A93,

    0xBC0285E8, 0xCD1895BA, 0xA18A3DBF, 0xF144205C,
    0x9AFE5BAF, 0xE86E2888, 0x165A5CBB, 0xB1028749,
    0x2CAEAF16, 0x6E072112, 0xA310776F, 0xF2B62D17,
    0x07936573, 0xB2EB9641, 0x85331564, 0x6D4062C2
};


UINT32 PieceMush [PIECE_ARRAY_SIZE] =
{
    0xCE60179C, 0x5F219D7C, 0x44E30E88, 0x6938280F,
    0x2FF85385, 0x074913B1, 0xD7EA5566, 0x3B29B998,
    0x6C4B79D1, 0x6EE4418D, 0x4325BEED, 0xEF4A5E34,
    0xE62B155F, 0x77B3EE4C, 0x8BC2B49F, 0xB9C1EDE2,
    0xA3D5ACA8, 0xB42762A7, 0xD3DA999F, 0xDCA0C943,
    0x122842D8, 0xD8DB0DD2
    // note intentionally left 0 entries at the end (32 total, 22 used)
};


UINT32 ChessBoard::CalcHash() const
{
    UINT32 h = 0;

    for ( int y=OFFSET(2,2); y <= OFFSET(2,9); y += NORTH )
    {
        for ( int x=0; x<8; x++ )
        {
            int ofs = x+y;
            if ( board[ofs] != EMPTY )
                h += HASH_PIECE(board[ofs],ofs);
        }
    }

    if ( h == 0 )
        h = 0xFFFFFFFF;   // This way we know 0 will never match any hash code

    return h;
}


#if BOARD_HASH_DEBUG

#include <stdio.h>

void DebugDumpBoard ( const SQUARE board [144], const char *message, Move move )
{
    const char *debugFilename = "chenbug.txt";
    FILE *db = fopen ( debugFilename, "at" );
    if ( db )
    {
        fprintf ( db, "[source=%d (%d,%d); dest=%d (%d,%d)]\n%s\n\n",
                  int(move.source), int(XPART(move.source)), int(YPART(move.source)),
                  int(move.dest), int(XPART(move.dest)), int(YPART(move.dest)),
                  message );

        for ( int y=OFFSET(2,2); y <= OFFSET(2,9); y += NORTH )
        {
            for ( int x=0; x < 8; ++x )
            {
                int ofs = x + y;
                char piece = '?';
                switch ( board[ofs] )
                {
                case EMPTY:     piece = '.';    break;

                case WPAWN:     piece = 'P';    break;
                case WKNIGHT:   piece = 'N';    break;
                case WBISHOP:   piece = 'B';    break;
                case WROOK:     piece = 'R';    break;
                case WQUEEN:    piece = 'Q';    break;
                case WKING:     piece = 'K';    break;

                case BPAWN:     piece = 'p';    break;
                case BKNIGHT:   piece = 'n';    break;
                case BBISHOP:   piece = 'b';    break;
                case BROOK:     piece = 'r';    break;
                case BQUEEN:    piece = 'q';    break;
                case BKING:     piece = 'k';    break;
                }

                fprintf ( db, "%c", piece );
            }
            fprintf ( db, "\n" );
        }

        fprintf ( db, "\n---------------------------------\n\n" );

        fclose(db);
    }
}

#endif



void ChessBoard::MakeWhiteMove (
    Move         &move,
    UnmoveInfo   &unmove,
    bool      look_for_self_check,
    bool      look_for_enemy_check )
{
    PROFILER_ENTER(PX_MAKEMOVE);

    int dest   = move.dest;
    int source = (move.source & BOARD_OFFSET_MASK);

#if CHESS_MOVE_DEBUG
    if ( source < OFFSET(2,2) || source > OFFSET(9,9) || (board[source] & OFFBOARD) )
        ChessFatal ( "Invalid source in ChessBoard::MakeWhiteMove" );

    if ( board[source] == EMPTY )
        ChessFatal ( "Source square is empty in ChessBoard::MakeWhiteMove" );
#endif

#if BOARD_HASH_DEBUG
    // Copy an image of the board before the move is made
    SQUARE saveBoardBeforeMove [144];
    for ( int _temp = 0; _temp < 144; ++_temp )
        saveBoardBeforeMove[_temp] = board[_temp];
#endif

    SQUARE capture = EMPTY;
    SQUARE piece = board[source];

#if BOARD_HASH_DEBUG
    if ( !(piece & WHITE_MASK) )
    {
        DebugDumpBoard ( board, "attempt to move non-white piece in MakeWhiteMove", move );
        ChessFatal ( "Attempt to move non-white piece in MakeWhiteMove" );
    }
#endif

    unmove.flags           =   flags;
    unmove.bmaterial       =   bmaterial;
    unmove.wmaterial       =   wmaterial;
    unmove.prev_move       =   prev_move;
    unmove.lastCapOrPawn   =   lastCapOrPawn;
    unmove.cachedHash      =   cachedHash;

    if ( dest > OFFSET(9,9) )
    {
        // If we get here, it means that this is a "special" move.
        // Special moves store a move code in 'dest', instead of the
        // actual board offset of the move's destination.
        // The move's actual destination is calculated based on
        // what kind of special move this is.

        switch ( dest & SPECIAL_MOVE_MASK )      // Get kind of special move from 'dest'
        {
        case SPECIAL_MOVE_PROMOTE_NORM:
            piece = PROM_PIECE ( dest, WHITE_IND );
            --inventory [WP_INDEX];
            ++inventory [SPIECE_INDEX(piece)];
            wmaterial += (RAW_PIECE_VALUE(piece) - PAWN_VAL);
            LIFT_PIECE(board[source],source);
            board [source] = EMPTY;
            board [dest = source + NORTH] = piece;
            DROP_PIECE(piece,dest);
            lastCapOrPawn = ply_number;
            break;

        case SPECIAL_MOVE_PROMOTE_CAP_EAST:
            capture = board [source + NORTHEAST];
            LIFT_PIECE(capture, source + NORTHEAST);
            piece = PROM_PIECE ( dest, WHITE_IND );
            --inventory [WP_INDEX];    // promoted pawn "disappears"
            ++inventory [SPIECE_INDEX(piece)];   // prom piece "created"
            wmaterial += (RAW_PIECE_VALUE(piece) - PAWN_VAL);
            LIFT_PIECE(board[source],source);
            board [source] = EMPTY;
            board [dest = source + NORTHEAST] = piece;
            DROP_PIECE(piece,dest);
            break;

        case SPECIAL_MOVE_PROMOTE_CAP_WEST:
            capture = board [source + NORTHWEST];
            LIFT_PIECE(capture, source + NORTHWEST);
            piece = PROM_PIECE ( dest, WHITE_IND );
            --inventory [WP_INDEX];    // promoted pawn "disappears"
            ++inventory [SPIECE_INDEX(piece)];   // prom piece "created"
            wmaterial += (RAW_PIECE_VALUE(piece) - PAWN_VAL);
            LIFT_PIECE(board[source],source);
            board [source] = EMPTY;
            board [dest = source + NORTHWEST] = piece;
            DROP_PIECE(piece, dest);
            break;

        case SPECIAL_MOVE_KCASTLE:
            dest = wk_offset = OFFSET(8,2);
            LIFT_PIECE(WKING,OFFSET(6,2));
            DROP_PIECE(WKING,OFFSET(8,2));
            board [OFFSET(6,2)] = EMPTY;
            board [OFFSET(8,2)] = WKING;
            LIFT_PIECE(WROOK,OFFSET(9,2));
            DROP_PIECE(WROOK,OFFSET(7,2));
            board [OFFSET(9,2)] = EMPTY;
            board [OFFSET(7,2)] = WROOK;
            flags |= (SF_WKMOVED | SF_WKRMOVED);
            break;

        case SPECIAL_MOVE_QCASTLE:
            dest = wk_offset = OFFSET(4,2);
            LIFT_PIECE(WKING,OFFSET(6,2));
            DROP_PIECE(WKING,OFFSET(4,2));
            board [OFFSET(6,2)] = EMPTY;
            board [OFFSET(4,2)] = WKING;
            LIFT_PIECE(WROOK,OFFSET(2,2));
            DROP_PIECE(WROOK,OFFSET(5,2));
            board [OFFSET(2,2)] = EMPTY;
            board [OFFSET(5,2)] = WROOK;
            flags |= (SF_WKMOVED | SF_WQRMOVED);
            break;

        case SPECIAL_MOVE_EP_EAST:
            LIFT_PIECE(board[source],source);
            board [source] = EMPTY;            // pick up w-pawn
            board [dest = source + NORTHEAST] = piece;   // put w-pawn down
            DROP_PIECE(piece,dest);
            capture = board [source + EAST];   // remove captured b-pawn
            LIFT_PIECE(capture, source + EAST);
            board [source + EAST] = EMPTY;
            break;

        case SPECIAL_MOVE_EP_WEST:
            LIFT_PIECE(board[source],source);
            board [source] = EMPTY;              // pick up w-pawn
            board [dest = source + NORTHWEST] = piece;  // put w-pawn down
            DROP_PIECE(piece,dest);
            capture = board [source + WEST];     // remove captured b-pawn
            LIFT_PIECE(capture, source + WEST);
            board [source + WEST] = EMPTY;
            break;

        default:
            ChessFatal ( "Invalid special move code in ChessBoard::MakeWhiteMove" );
            break;
        }
    }
    else   // This is a "normal" move...
    {
        capture = board[dest];      // Remember what was captured
        if ( capture != EMPTY )
        {
            LIFT_PIECE(capture,dest);

            // look for bugs in move generator...
            if ( capture & (BK_MASK | WHITE_MASK | OFFBOARD) )
            {
                if ( capture & BK_MASK )
                    ChessFatal ( "Attempt to capture black king in ChessBoard::MakeWhiteMove" );
                else if ( capture & OFFBOARD )
                    ChessFatal ( "Attempt to move piece off the board in ChessBoard::MakeWhiteMove" );
                else
                    ChessFatal ( "Attempt to capture white piece in ChessBoard::MakeWhiteMove" );
            }
        }

        LIFT_PIECE(piece,source);   // update hash codes ...
        DROP_PIECE(piece,dest);     // ... due to piece moving

        board[dest]    =  piece;    // Move the piece
        board[source]  =  EMPTY;    // Erase piece from old square

        if ( piece & WK_MASK )
        {
            flags |= SF_WKMOVED;    // forfeit castling
            wk_offset = dest;       // we always remember where kings are
        }
        else
        {
            if ( source == OFFSET(9,2) )
            {
                if ( piece & WR_MASK )
                    flags |= SF_WKRMOVED;   // forfeit O-O
            }
            else if ( source == OFFSET(2,2) )
            {
                if ( piece & WR_MASK )
                    flags |= SF_WQRMOVED;   // forfeit O-O-O
            }
        }
    }

    if ( (unmove.capture = capture) != EMPTY )
    {
        // Update material, inventory, etc.
        --inventory [SPIECE_INDEX(capture)];    // one less of the captured piece
        bmaterial -= RAW_PIECE_VALUE(capture);  // deduct material from black

        lastCapOrPawn = ply_number;

        // See if we captured a black rook which had not yet moved...
        // If so, we set its "moved" flag, so that the special case
        // of black moving the other rook onto the square later does not
        // confuse the legal move generator into thinking it can castle!
        // This is because the legal move generator simply checks for
        // the flag NOT being set and a black rook in the square.
        // This is safe even if the rook being captured isn't the original,
        // unmoved rook.  In this case, we are setting the flag redundantly.

        if ( dest == OFFSET(9,9) )
        {
            if ( capture & BR_MASK )
                flags |= SF_BKRMOVED;
        }
        else if ( dest == OFFSET(2,9) )
        {
            if ( capture & BR_MASK )
                flags |= SF_BQRMOVED;
        }
    }
    else if ( piece & WP_MASK )  // not a capture, but might be pawn advance
        lastCapOrPawn = ply_number;

    if ( look_for_self_check )
    {
        if ( IsAttackedByBlack(wk_offset) )
            flags |= SF_WCHECK;
        else
            flags &= ~SF_WCHECK;

        if ( look_for_enemy_check )
        {
            if ( IsAttackedByWhite(bk_offset) )
            {
                flags |= SF_BCHECK;
                move.source |= CAUSES_CHECK_BIT;
            }
            else
                flags &= ~SF_BCHECK;
        }
    }
    else
    {
        // If we get here, it means that we are re-making the move
        // on the board with the knowledge that it is a legal move.
        // It also means that the high-order bit of move.source tells
        // whether this move causes check to black.

        flags &= ~(SF_WCHECK | SF_BCHECK);
        if ( move.source & CAUSES_CHECK_BIT )
            flags |= SF_BCHECK;
    }

    if ( ply_number < MAX_GAME_HISTORY )
        gameHistory [ply_number] = move;

    ++ply_number;
    prev_move = move;
    white_to_move = false;

    if ( cachedHash == 0 )
        cachedHash = 0xFFFFFFFF;   // This way we know 0 will never match any hash code

    ++blackRepeatHash[cachedHash % REPEAT_HASH_SIZE];

#if BOARD_HASH_DEBUG
    UINT32 actualHash = CalcHash();
    if ( cachedHash != actualHash )
    {
        DebugDumpBoard ( saveBoardBeforeMove, "Hash code out of whack - before white move", move );
        DebugDumpBoard ( board, "Hash code out of whack - after white move", move );
        ChessFatal ( "Hash code out of whack in ChessBoard::MakeWhiteMove()" );
        cachedHash = actualHash;
    }
#endif

    PROFILER_EXIT()
}


//---------------------------------------------------------------------------


void ChessBoard::MakeBlackMove (
    Move         &move,
    UnmoveInfo   &unmove,
    bool      look_for_self_check,
    bool      look_for_enemy_check )
{
    PROFILER_ENTER(PX_MAKEMOVE);

    int dest   = move.dest;
    int source = (move.source & BOARD_OFFSET_MASK);

#if CHESS_MOVE_DEBUG
    if ( source < OFFSET(2,2) || source > OFFSET(9,9) || (board[source] & OFFBOARD) )
        ChessFatal ( "Invalid source in ChessBoard::MakeBlackMove" );

    if ( board[source] == EMPTY )
        ChessFatal ( "Source square is empty in ChessBoard::MakeBlackMove" );
#endif

#if BOARD_HASH_DEBUG
    // Copy an image of the board before the move is made
    SQUARE saveBoardBeforeMove [144];
    for ( int _temp = 0; _temp < 144; ++_temp )
        saveBoardBeforeMove[_temp] = board[_temp];
#endif

    SQUARE capture = EMPTY;
    SQUARE piece = board[source];

#if BOARD_HASH_DEBUG
    if ( !(piece & BLACK_MASK) )
    {
        DebugDumpBoard ( board, "attempt to move non-black piece in MakeBlackMove", move );
        ChessFatal ( "Attempt to move non-black piece in MakeBlackMove" );
    }
#endif

    unmove.flags           =   flags;
    unmove.bmaterial       =   bmaterial;
    unmove.wmaterial       =   wmaterial;
    unmove.prev_move       =   prev_move;
    unmove.lastCapOrPawn   =   lastCapOrPawn;
    unmove.cachedHash      =   cachedHash;

    if ( dest > OFFSET(9,9) )
    {
        // If we get here, it means that this is a "special" move.
        // Special moves store a move code in 'dest', instead of the
        // actual board offset of the move's destination.
        // The move's actual destination is calculated based on
        // what kind of special move this is.

        switch ( dest & SPECIAL_MOVE_MASK )      // Get kind of special move from 'dest'
        {
        case SPECIAL_MOVE_PROMOTE_NORM:
            piece = PROM_PIECE ( dest, BLACK_IND );
            --inventory [BP_INDEX];
            ++inventory [SPIECE_INDEX(piece)];
            bmaterial += (RAW_PIECE_VALUE(piece) - PAWN_VAL);
            LIFT_PIECE(board[source],source);
            board [source] = EMPTY;
            board [dest = source + SOUTH] = piece;
            DROP_PIECE(piece,dest);
            lastCapOrPawn = ply_number;
            break;

        case SPECIAL_MOVE_PROMOTE_CAP_EAST:
            capture = board [source + SOUTHEAST];
            LIFT_PIECE(capture, source + SOUTHEAST);
            piece = PROM_PIECE ( dest, BLACK_IND );
            --inventory [BP_INDEX];    // promoted pawn "disappears"
            ++inventory [SPIECE_INDEX(piece)];   // prom piece "created"
            bmaterial += (RAW_PIECE_VALUE(piece) - PAWN_VAL);
            LIFT_PIECE(board[source],source);
            board [source] = EMPTY;
            board [dest = source + SOUTHEAST] = piece;
            DROP_PIECE(piece,dest);
            break;

        case SPECIAL_MOVE_PROMOTE_CAP_WEST:
            capture = board [source + SOUTHWEST];
            LIFT_PIECE(capture, source + SOUTHWEST);
            piece = PROM_PIECE ( dest, BLACK_IND );
            --inventory [BP_INDEX];    // promoted pawn "disappears"
            ++inventory [SPIECE_INDEX(piece)];   // prom piece "created"
            bmaterial += (RAW_PIECE_VALUE(piece) - PAWN_VAL);
            LIFT_PIECE(board[source], source);
            board [source] = EMPTY;
            board [dest = source + SOUTHWEST] = piece;
            DROP_PIECE(piece,dest);
            break;

        case SPECIAL_MOVE_KCASTLE:
            dest = bk_offset = OFFSET(8,9);
            LIFT_PIECE(BKING,OFFSET(6,9));
            DROP_PIECE(BKING,OFFSET(8,9));
            board [OFFSET(6,9)] = EMPTY;
            board [OFFSET(8,9)] = BKING;
            LIFT_PIECE(BROOK,OFFSET(9,9));
            DROP_PIECE(BROOK,OFFSET(7,9));
            board [OFFSET(9,9)] = EMPTY;
            board [OFFSET(7,9)] = BROOK;
            flags |= (SF_BKMOVED | SF_BKRMOVED);
            break;

        case SPECIAL_MOVE_QCASTLE:
            dest = bk_offset = OFFSET(4,9);
            LIFT_PIECE(BKING,OFFSET(6,9));
            DROP_PIECE(BKING,OFFSET(4,9));
            board [OFFSET(6,9)] = EMPTY;
            board [OFFSET(4,9)] = BKING;
            LIFT_PIECE(BROOK,OFFSET(2,9));
            DROP_PIECE(BROOK,OFFSET(5,9));
            board [OFFSET(2,9)] = EMPTY;
            board [OFFSET(5,9)] = BROOK;
            flags |= (SF_BKMOVED | SF_BQRMOVED);
            break;

        case SPECIAL_MOVE_EP_EAST:
            LIFT_PIECE(piece,source);
            board [source] = EMPTY;
            board [dest = source + SOUTHEAST] = piece;
            DROP_PIECE(piece,dest);
            capture = board [source + EAST];
            board [source + EAST] = EMPTY;
            LIFT_PIECE(capture,source + EAST);
            break;

        case SPECIAL_MOVE_EP_WEST:
            LIFT_PIECE(piece,source);
            board [source] = EMPTY;
            board [dest = source + SOUTHWEST] = piece;
            DROP_PIECE(piece,dest);
            capture = board [source + WEST];
            board [source + WEST] = EMPTY;
            LIFT_PIECE(capture,source + WEST);
            break;

        default:
            ChessFatal ( "Invalid special move code in ChessBoard::MakeBlackMove" );
            break;
        }
    }
    else    // This is a "normal" move...
    {
        capture = board[dest];      // Remember what was captured
        if ( capture != EMPTY )
        {
            LIFT_PIECE(capture,dest);

            // look for bugs in move generator...
            if ( capture & (WK_MASK | BLACK_MASK | OFFBOARD) )
            {
                if ( capture & WK_MASK )
                    ChessFatal ( "Attempt to capture white king in ChessBoard::MakeBlackMove" );
                else if ( capture & OFFBOARD )
                    ChessFatal ( "Attempt to move piece off the board in ChessBoard::MakeBlackMove" );
                else
                    ChessFatal ( "Attempt to capture black piece in ChessBoard::MakeBlackMove" );
            }
        }

        LIFT_PIECE(piece,source);   // update hash codes ...
        DROP_PIECE(piece,dest);     // ... due to piece moving

        board[dest]    =  piece;    // Move the piece
        board[source]  =  EMPTY;    // Erase piece from old square

        if ( piece & BK_MASK )
        {
            flags |= SF_BKMOVED;
            bk_offset = dest;
        }
        else
        {
            if ( source == OFFSET(9,9) )
            {
                if ( piece & BR_MASK )
                    flags |= SF_BKRMOVED;
            }
            else if ( source == OFFSET(2,9) )
            {
                if ( piece & BR_MASK )
                    flags |= SF_BQRMOVED;
            }
        }
    }

    if ( (unmove.capture = capture) != EMPTY )
    {
        // Update material, inventory, etc.
        --inventory [SPIECE_INDEX(capture)];    // one less of the captured piece
        wmaterial -= RAW_PIECE_VALUE(capture);  // deduct material from white

        lastCapOrPawn = ply_number;

        // See if we captured a white rook which had not yet moved...
        // If so, we set its "moved" flag, so that the special case
        // of white moving the other rook onto the square later does not
        // confuse the legal move generator into thinking it can castle!
        // This is because the legal move generator simply checks for
        // the flag NOT being set and a white rook in the square.
        // This is safe even if the rook being captured isn't the original,
        // unmoved rook.  In this case, we are setting the flag redundantly.

        if ( dest == OFFSET(9,2) )
        {
            if ( capture & WR_MASK )
                flags |= SF_WKRMOVED;
        }
        else if ( dest == OFFSET(2,2) )
        {
            if ( capture & WR_MASK )
                flags |= SF_WQRMOVED;
        }
    }
    else if ( piece & BP_MASK )  // not a capture, but might be pawn advance
        lastCapOrPawn = ply_number;

    if ( look_for_self_check )
    {
        if ( IsAttackedByWhite(bk_offset) )
            flags |= SF_BCHECK;
        else
            flags &= ~SF_BCHECK;

        if ( look_for_enemy_check )
        {
            if ( IsAttackedByBlack(wk_offset) )
            {
                flags |= SF_WCHECK;
                move.source |= CAUSES_CHECK_BIT;
            }
            else
                flags &= ~SF_WCHECK;
        }
    }
    else
    {
        // If we get here, it means that we are re-making the move
        // on the board with the knowledge that it is a legal move.
        // It also means that the high-order bit of move.source tells
        // whether this move causes check to white.

        flags &= ~(SF_WCHECK | SF_BCHECK);
        if ( move.source & CAUSES_CHECK_BIT )
            flags |= SF_WCHECK;
    }

    if ( ply_number < MAX_GAME_HISTORY )
        gameHistory [ply_number] = move;

    ++ply_number;
    prev_move = move;
    white_to_move = true;

    if ( cachedHash == 0 )
        cachedHash = 0xFFFFFFFF;   // This way we know 0 will never match any hash code

    ++whiteRepeatHash[cachedHash % REPEAT_HASH_SIZE];

#if BOARD_HASH_DEBUG
    UINT32 actualHash = CalcHash();
    if ( cachedHash != actualHash )
    {
        DebugDumpBoard ( saveBoardBeforeMove, "Hash code out of whack - before black move", move );
        DebugDumpBoard ( board, "Hash code out of whack - after black move", move );
        ChessFatal ( "Hash code out of whack in ChessBoard::MakeBlackMove()" );
        cachedHash = actualHash;  // fix it to avoid cascades
    }
#endif

    PROFILER_EXIT()
}


/*
    $Log: move.cpp,v $
    Revision 1.5  2006/01/18 19:58:13  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.4  2006/01/08 15:11:26  dcross
    1. Getting ready to add support for wp.egm endgame database.  I know I am going to need
       an easy way to detect pawn promotions, so I have modified the venerable Move::actualOffsets
       functions to detect that case for me and return EMPTY if a move is not a pawn promotion,
       or the actual piece value if it is a promotion.
    2. While doing this I noticed and corrected an oddity with the PROM_PIECE macro:
        the parameter 'piece' was unused, so I removed it.

    Revision 1.3  2005/11/23 21:30:30  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:41  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!


          Revision history:

    1993 August 30 [Don Cross]
         Changing pointers to references in the interfaces where
         appropriate.

    1993 October 17 [Don Cross]
         Splitting single 'look_for_check' parameter into
         'look_for_self_check' and 'look_for_enemy_check'.

    1993 October 18 [Don Cross]
         Fixed bug that happened in simultateous pawn promotion
         and capture:  I was destroying the fake 'dest' value
         which contained the promoted piece before I saved it away.

    1994 February 10 [Don Cross]
         Started implementing indexed pieces.

    1999 January 19 [Don Cross]
         Adding board hash caching.  This is important for (finally?)
         detecting draw by repetition.
         Was not updating lastCapOrPawn during non-capture pawn promotion.
         Updated coding style.

    1999 August 4 [Don Cross]
         Adding a little bit more debug code to try to catch some
         weird bugs that happen extremely rarely (e.g., pieces
         vanishing from the board, including the king!).
         The new debug code is very fast (a single bitwise AND per move),
         so I think I will leave it in even after I fix the bug
         that inspired it.

*/

