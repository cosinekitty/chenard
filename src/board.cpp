/*=========================================================================

     board.cpp  -  Copyright (C) 1993-2005 by Don Cross
     
=========================================================================*/

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

#include "chess.h"


ChessBoard::ChessBoard():
    gameHistory ( new Move [MAX_GAME_HISTORY] ),
    initialFen (0)
{
    repeatHash = new BYTE [REPEAT_HASH_SIZE];

    if ( !gameHistory || !repeatHash )
        ChessFatal ( "Out of memory in ChessBoard::ChessBoard()" );

    for ( int i=0; i < MAX_GAME_HISTORY; i++ )
    {
        gameHistory[i].source = 0;
        gameHistory[i].dest   = 0;
    }

    Init();
}


ChessBoard::ChessBoard ( const ChessBoard &other ):
    gameHistory ( new Move [MAX_GAME_HISTORY] ),
    initialFen (0)
{
    repeatHash = new BYTE [REPEAT_HASH_SIZE];

    if ( !gameHistory || !repeatHash )
        ChessFatal ( "Out of memory in ChessBoard copy constructor" );

    *this = other;
}


ChessBoard & ChessBoard::operator= ( const ChessBoard &other )
{
    if ( this != &other )
    {
        ply_number      =   other.ply_number;

        for ( int i=0; i < ply_number; i++ )
            gameHistory[i] = other.gameHistory[i];

        for ( unsigned i=0; i < REPEAT_HASH_SIZE; ++i )
            repeatHash[i] = other.repeatHash[i];

        for ( int i=0; i<144; i++ )
            board[i] = other.board[i];

        flags           =   other.flags;
        wmaterial       =   other.wmaterial;
        bmaterial       =   other.bmaterial;
        wk_offset       =   other.wk_offset;
        bk_offset       =   other.bk_offset;
        white_to_move   =   other.white_to_move;

        for ( int i=0; i < PIECE_ARRAY_SIZE; i++ )
            inventory[i] = other.inventory[i];

        prev_move       =   other.prev_move;

        lastCapOrPawn   =   other.lastCapOrPawn;
        cachedHash      =   other.cachedHash;

        ReplaceString (initialFen, other.initialFen);
    }

    return *this;
}


ChessBoard::~ChessBoard()
{
    if ( gameHistory )
    {
        delete[] gameHistory;
        gameHistory = 0;
    }

    if ( repeatHash )
    {
        delete[] repeatHash;
        repeatHash = 0;
    }

    FreeString (initialFen);
}


void ChessBoard::Init()
{
    int x;
    for ( x=0; x < OFFSET(0,2); ++x )
        board[x] = board[OFFSET(11,11)-x] = OFFBOARD;

    for ( x=OFFSET(0,2); x < OFFSET(0,10); x += NORTH )
        board[x] = board[x+1] = board[x+10] = board[x+11] = OFFBOARD;

    // Now go back and make the interior squares EMPTY...
    // put the pawns on the board at the same time...

    for ( x=2; x <= 9; x++ )
    {
        board [ OFFSET(x,3) ] = WPAWN;
        board [ OFFSET(x,8) ] = BPAWN;

        for ( int y=4; y <= 7; y++ )
            board [ OFFSET(x,y) ] = EMPTY;
    }

    // Put the white pieces on the board...
    board [ OFFSET(2,2) ] = board [ OFFSET(9,2) ] = WROOK;
    board [ OFFSET(3,2) ] = board [ OFFSET(8,2) ] = WKNIGHT;
    board [ OFFSET(4,2) ] = board [ OFFSET(7,2) ] = WBISHOP;
    board [ OFFSET(5,2) ] = WQUEEN;
    board [ wk_offset = OFFSET(6,2) ] = WKING;

    // Put the black pieces on the board...
    board [ OFFSET(2,9) ] = board [ OFFSET(9,9) ] = BROOK;
    board [ OFFSET(3,9) ] = board [ OFFSET(8,9) ] = BKNIGHT;
    board [ OFFSET(4,9) ] = board [ OFFSET(7,9) ] = BBISHOP;
    board [ OFFSET(5,9) ] = BQUEEN;
    board [ bk_offset = OFFSET(6,9) ] = BKING;

    // Now we are done with the board squares.
    // We just need to initialize all the other things in the ChessBoard.

    flags = 0;

    white_to_move = true;

    prev_move.dest   = SPECIAL_MOVE_NULL;
    prev_move.source = 0;
    prev_move.score  = 0;

    for ( x=0; x < PIECE_ARRAY_SIZE; x++ )
        inventory[x] = 0;

    inventory [WP_INDEX] = inventory [BP_INDEX] = 8;
    inventory [WN_INDEX] = inventory [BN_INDEX] = 2;
    inventory [WB_INDEX] = inventory [BB_INDEX] = 2;
    inventory [WR_INDEX] = inventory [BR_INDEX] = 2;
    inventory [WQ_INDEX] = inventory [BQ_INDEX] = 1;
    inventory [WK_INDEX] = inventory [BK_INDEX] = 1;

    wmaterial = bmaterial = 
        8*PAWN_VAL + 2*KNIGHT_VAL + 2*BISHOP_VAL + 
        2*ROOK_VAL + QUEEN_VAL + KING_VAL;

    ply_number = 0;
    initialPlyNumber = 0;
    FreeString (initialFen);    // side effect: causes us to interpret this ChessBoard object as unedited.
    lastCapOrPawn = -1;
    memset ( repeatHash, 0, REPEAT_HASH_SIZE * sizeof(repeatHash[0]) );
    cachedHash = CalcHash();
    repeatHash [cachedHash % REPEAT_HASH_SIZE] = 0x01;
}


bool ChessBoard::GameIsOver()
{
    MoveList ml;
    GenMoves(ml);
    if ( ml.num == 0 )
        return true;

    return IsDefiniteDraw();
}


bool ChessBoard::IsDefiniteDraw ( int *numReps )
{
    if ( numReps )
        *numReps = 0;

    // First, determine whether there is a draw based on material...

    if ( inventory[WR_INDEX] == 0 &&
         inventory[BR_INDEX] == 0 &&
         inventory[WQ_INDEX] == 0 &&
         inventory[BQ_INDEX] == 0 )
    {
        if ( inventory[WP_INDEX] == 0 &&
           inventory[BP_INDEX] == 0 )
        {
            // Declare it a draw if both sides have knights and/or bishops.
            // If either side has just a king, then see if the other side
            // has enough force for a theoretical win.

            if ( inventory[WN_INDEX] == 0 && inventory[WB_INDEX] == 0 )
            {
                // See if black has enough for a theoretical win.
                if ( inventory[BB_INDEX]>=1 && inventory[BN_INDEX]>=1 )
                    return false;

                return (inventory[BB_INDEX] < 2) ? true : false;
            }
            else if ( inventory[BN_INDEX] == 0 && inventory[BB_INDEX] == 0 )
            {
                // See if white has enough for a theoretical win.
                if ( inventory[WB_INDEX]>=1 && inventory[WN_INDEX]>=1 )
                    return false;

                return (inventory[WB_INDEX] < 2) ? true : false;
            }
            else
                return true;
        }
    }

    // Look for draw based on 50-move rule...

    if ( ply_number - lastCapOrPawn >= 100 )
        return true;

    // Now look for draw based upon position repeated 3 times...

    unsigned r = repeatHash[cachedHash % REPEAT_HASH_SIZE];
    if ( white_to_move )
        r &= 0x0f;
    else
        r >>= 4;

    if ( r >= 3 )
        r = NumberOfRepetitions();

    if ( numReps )
        *numReps = r;

    return r >= 3;
}


int ChessBoard::NumberOfRepetitions()
{
    if ( initialFen != 0 )   // FIXFIXFIX:  This prevents us from detecting draws by repetition in edited boards!
        return 0;

    // Strategy: make the moves found in the history of 'this'
    // into 'x', and look for positions that exactly match

    static ChessBoard x;   // make static so memory isn't allocated on each call
    x.Init();

    UnmoveInfo unmove;
    int numRepetitions = 0;     // we compare against every position including the current position
    int maxPly = (ply_number < MAX_GAME_HISTORY) ? ply_number : MAX_GAME_HISTORY;
    for ( int ply=0; ply < maxPly; ++ply )
    {
        Move m = gameHistory[ply];
        x.MakeMove ( m, unmove );
        if ( *this == x )
            ++numRepetitions;
    }

    return numRepetitions;
}


bool ChessBoard::WhiteToMove() const
{
    return white_to_move;
}


bool ChessBoard::BlackToMove() const
{
    return !white_to_move;
}


bool ChessBoard::WhiteInCheck() const
{
    return (flags & SF_WCHECK) ? true : false;
}


bool ChessBoard::BlackInCheck() const
{
    return (flags & SF_BCHECK) ? true : false;
}


SCORE RawPieceValues[] = 
{ PAWN_VAL, KNIGHT_VAL, BISHOP_VAL, 
  ROOK_VAL, QUEEN_VAL,  KING_VAL };


PieceLookup::PieceLookup()
{
    for ( int i=0; i < PIECE_ARRAY_SIZE; i++ )
        sval[i] = 0;

    sval [WP_INDEX] = WPAWN;
    sval [WN_INDEX] = WKNIGHT;
    sval [WB_INDEX] = WBISHOP;
    sval [WR_INDEX] = WROOK;
    sval [WQ_INDEX] = WQUEEN;
    sval [WK_INDEX] = WKING;

    sval [BP_INDEX] = BPAWN;
    sval [BN_INDEX] = BKNIGHT;
    sval [BB_INDEX] = BBISHOP;
    sval [BR_INDEX] = BROOK;
    sval [BQ_INDEX] = BQUEEN;
    sval [BK_INDEX] = BKING;
}


SQUARE ChessBoard::GetSquareContents ( int x, int y ) const
{
    if ( x < 0 || x > 7 || y < 0 || y > 7 )
        return OFFBOARD;
    else
        return board [ OFFSET ( x+2, y+2 ) ];
}


SQUARE ChessBoard::GetSquareContents ( int offset ) const
{
    if ( offset < 0 || offset > 143 )
        return OFFBOARD;
    else
        return board [ offset ];
}


void ChessBoard::SetSquareContents ( SQUARE s, int x, int y )
{
    if ( x >= 0 && x <= 7 && y >= 0 && y <= 7 )
    {
        if ( SetOffsetContents ( s, OFFSET(x+2,y+2) ) )
        {
            Update();
        }
    }
}


bool ChessBoard::SetOffsetContents ( SQUARE s, int offset, bool force )
{
    if ( offset >= OFFSET(2,2) && offset <= OFFSET(9,9) )
    {
        if ( (board[offset] & OFFBOARD) == 0 )
        {
            if ( board[offset] & (BK_MASK | WK_MASK) )
            {
                if ( force && s != board[offset] )
                {
                    // Find an empty square to put the king in...
                    int kludge = 0;
                    for ( kludge=0; kludge<144; ++kludge )
                    {
                        if ( board[kludge] == EMPTY )
                        {
                            board[kludge] = board[offset];
                            if ( board[kludge] & BK_MASK )
                                bk_offset = kludge;
                            else
                                wk_offset = kludge;
                            break;
                        }
                    }
                }
                else
                {
                    return false;  // Can't mess with king!
                }
            }

            if ( (s & (WP_MASK | BP_MASK)) &&
                 (YPART(offset) == 2 || YPART(offset) == 9) )
                return false;  // Can't put pawns on first or last ranks.

            if ( s == WKING && offset != wk_offset )
            {
                board [wk_offset] = EMPTY;
                wk_offset = offset;
                flags |= SF_WKMOVED;
            }
            else if ( s == BKING && offset != bk_offset )
            {
                board [bk_offset] = EMPTY;
                bk_offset = offset;
                flags |= SF_BKMOVED;
            }
            else if ( board [offset] == WROOK && s != WROOK )
            {
                // See if we need to set the rook-moved flag...
                if ( offset == OFFSET(2,2) )
                    flags |= SF_WQRMOVED;
                else if ( offset == OFFSET(9,2) )
                    flags |= SF_WKRMOVED;
            }
            else if ( board [offset] == BROOK && s != BROOK )
            {
                // See if we need to set the rook-moved flag...
                if ( offset == OFFSET(2,9) )
                    flags |= SF_BQRMOVED;
                else if ( offset == OFFSET(9,9) )
                    flags |= SF_BKRMOVED;
            }

            board [offset] = s;
            
            return true;
        }
    }

    return false;   // move was invalid, so indicate that it was ignored
}


void ChessBoard::ClearEverythingButKings()
{
    for ( int x=0; x < 144; x++ )
    {
        SQUARE s = GetSquareContents(x);

        if ( (s & (OFFBOARD | WK_MASK | BK_MASK)) == 0 )
        {
            SetOffsetContents ( EMPTY, x );
        }
    }

    Update();
}


void ChessBoard::EditCommand ( Move edit )
{
    int offset = edit.source;
    if ( offset == 0 )
    {
        ClearEverythingButKings();   // This is a special case
    }
    else
    {
        // The offset given is the square to be changed.
        // We need to figure out what to put in it...

        int pindex = edit.dest & 0x0F;
        SQUARE s = ConvertNybbleToSquare ( pindex );
        if ( SetOffsetContents ( s, offset ) )
        {
            Update();
        }
    }
}


void ChessBoard::Update()
{
    int i;
	for (i=0; i < PIECE_ARRAY_SIZE; i++) {
        inventory[i] = 0;
	}

    lastCapOrPawn = -1;
    wmaterial = 0;
    bmaterial = 0;

    for (i=OFFSET(2,2); i <= OFFSET(9,9); i++) {
        SQUARE s = board[i];
        if (s & (WHITE_MASK | BLACK_MASK)) {
            ++inventory [ SPIECE_INDEX(s) ];

            switch (s) {
                case WKING:    wmaterial += KING_VAL;    wk_offset = i;		break;
                case WQUEEN:   wmaterial += QUEEN_VAL;   break;
                case WROOK:    wmaterial += ROOK_VAL;    break;
                case WBISHOP:  wmaterial += BISHOP_VAL;  break;
                case WKNIGHT:  wmaterial += KNIGHT_VAL;  break;
                case WPAWN:    wmaterial += PAWN_VAL;    break;

                case BKING:    bmaterial += KING_VAL;    bk_offset = i;		break;
                case BQUEEN:   bmaterial += QUEEN_VAL;   break;
                case BROOK:    bmaterial += ROOK_VAL;    break;
                case BBISHOP:  bmaterial += BISHOP_VAL;  break;
                case BKNIGHT:  bmaterial += KNIGHT_VAL;  break;
                case BPAWN:    bmaterial += PAWN_VAL;    break;

				default:
					ChessFatal ("ChessBoard::Update():  Corrupt board contents!");
            }
        }
    }

	if ( IsAttackedByBlack(wk_offset) ) {
        flags |= SF_WCHECK;
	} else {
        flags &= ~SF_WCHECK;
	}

	if ( IsAttackedByWhite(bk_offset) ) {
        flags |= SF_BCHECK;
	} else {
        flags &= ~SF_BCHECK;
	}

    cachedHash = CalcHash();
}


char PieceRepresentation ( SQUARE square )
{
    switch ( square )
    {
        case OFFBOARD:     return '*';

        case EMPTY:        return '.';

        case WPAWN:        return 'P';
        case WKNIGHT:      return 'N';
        case WBISHOP:      return 'B';
        case WROOK:        return 'R';
        case WQUEEN:       return 'Q';
        case WKING:        return 'K';

        case BPAWN:        return 'p';
        case BKNIGHT:      return 'n';
        case BBISHOP:      return 'b';
        case BROOK:        return 'r';
        case BQUEEN:       return 'q';
        case BKING:        return 'k';
    }

    return '?';
}


char PromPieceRepresentation ( int prom_piece_index )
{
    switch ( prom_piece_index )
    {
        case N_INDEX:   return 'N';
        case B_INDEX:   return 'B';
        case R_INDEX:   return 'R';
        case Q_INDEX:   return 'Q';
    }

    return '?';
}


char GetPieceSymbol (SQUARE piece)
{
    char pieceSymbol;
    switch (piece & PIECE_MASK) {
        case P_INDEX:   pieceSymbol = 'P';  break;
        case N_INDEX:   pieceSymbol = 'N';  break;
        case B_INDEX:   pieceSymbol = 'B';  break;
        case R_INDEX:   pieceSymbol = 'R';  break;
        case Q_INDEX:   pieceSymbol = 'Q';  break;
        case K_INDEX:   pieceSymbol = 'K';  break;
        default:        pieceSymbol = '?';  assert(false);  break;
    }
    return pieceSymbol;
}

SQUARE GetPhonyWhitePiece (const ChessBoard &board, int ofs)
{
    // HACK:  We pretend like 'piece' belongs to White, even when it is Black's,
    // because otherwise we can't tell P_INDEX (0) apart from EMPTY (0).
    
    SQUARE piece = board.GetSquareContents(ofs);
    assert (!(piece & OFFBOARD));
    if (piece != EMPTY) {
        piece = WHITE_IND | UPIECE_INDEX(piece);
    }
    return piece;
}

#define fencopy(c)                  \
    {                                \
        if (length < buffersize) {    \
            buffer[length++] = (c);    \
        } else {                        \
            return NULL;                 \
        }                                 \
    }
    

char *ChessBoard::GetForsythEdwardsNotation (char *buffer, int buffersize) const
{
    // This function generates FEN string for the current board position.
    // It returns NULL if an error occurs: buffer == NULL, or buffersize is too small.

    int     piece, ofs, i;
    char    tempstr [32];

    if (buffer == NULL) {
        return NULL;
    }

    int length = 0;     // used by fencopy macro (see above)

    /*
        http://www.very-best.de/pgn-spec.htm

        16.1.4 Piece placement data
        The first field represents the placement of the pieces on the board. 
        The board contents are specified starting with the eighth rank and 
        ending with the first rank. For each rank, the squares are specified 
        from file a to file h. White pieces are identified by uppercase SAN 
        piece letters ("PNBRQK") and black pieces are identified by lowercase 
        SAN piece letters ("pnbrqk"). Empty squares are represented by the 
        digits one through eight; the digit used represents the count of 
        contiguous empty squares along a rank. A solidus character "/" 
        is used to separate data of adjacent ranks.
    */
    
    for (int rank='8'; rank >= '1'; --rank) {
        int empty_count = 0;
        for (char file='a'; file <= 'h'; ++file) {
            ofs = OFFSET (file - 'a' + 2, rank - '1' + 2);
            assert (XPART(ofs) - 2 + 'a' == file);
            assert (YPART(ofs) - 2 + '1' == rank);
            piece = GetSquareContents (ofs);
            if (piece == EMPTY) {
                ++empty_count;
            } else {
                if (empty_count > 0) {
                    fencopy (empty_count + '0');
                    empty_count = 0;
                }
                fencopy (PieceRepresentation(piece));
            }
        }
        if (empty_count > 0) {
            fencopy (empty_count + '0');
        }
        if (rank > '1') {
            fencopy ('/');
        }
    }

    /*
        16.1.5 Active color
        The second field represents the active color. 
        A lower case "w" is used if White is to move; 
        a lower case "b" is used if Black is the active player.
    */

    fencopy (' ');
    fencopy (WhiteToMove() ? 'w' : 'b');
    fencopy (' ');
    
    /*
        16.1.6 Castling availability
        The third field represents castling availability. 
        This indicates potential future castling that may of may not be 
        possible at the moment due to blocking pieces or enemy attacks. 
        If there is no castling availability for either side, the single 
        character symbol "-" is used. Otherwise, a combination of from 
        one to four characters are present. If White has kingside castling 
        availability, the uppercase letter "K" appears. If White has 
        queenside castling availability, the uppercase letter "Q" appears. 
        If Black has kingside castling availability, the lowercase letter 
        "k" appears. If Black has queenside castling availability, 
        then the lowercase letter "q" appears. Those letters which appear 
        will be ordered first uppercase before lowercase and second kingside 
        before queenside. There is no white space between the letters.
    */
    int castling = 0;
    if (0 == (flags & (SF_WKMOVED | SF_WKRMOVED))) {
        assert (GetSquareContents(OFFSET(6,2)) == WKING);
        assert (GetSquareContents(OFFSET(9,2)) == WROOK);
        fencopy('K');
        ++castling;
    }

    if (0 == (flags & (SF_WKMOVED | SF_WQRMOVED))) {
        assert (GetSquareContents(OFFSET(6,2)) == WKING);
        assert (GetSquareContents(OFFSET(2,2)) == WROOK);
        fencopy('Q');
        ++castling;
    }

    if (0 == (flags & (SF_BKMOVED | SF_BKRMOVED))) {
        assert (GetSquareContents(OFFSET(6,9)) == BKING);
        assert (GetSquareContents(OFFSET(9,9)) == BROOK);
        fencopy('k');
        ++castling;
    }

    if (0 == (flags & (SF_BKMOVED | SF_BQRMOVED))) {
        assert (GetSquareContents(OFFSET(6,9)) == BKING);
        assert (GetSquareContents(OFFSET(2,9)) == BROOK);
        fencopy('q');
        ++castling;
    }

    if (castling == 0) {
        fencopy('-');
    }

    fencopy(' ');

    /*
        16.1.7 En passant target square
        The fourth field is the en passant target square. If there is no en 
        passant target square then the single character symbol "-" appears. 
        If there is an en passant target square then is represented by a 
        lowercase file character immediately followed by a rank digit. 
        Obviously, the rank digit will be "3" following a white pawn double 
        advance (Black is the active color) or else be the digit "6" after 
        a black pawn double advance (White being the active color).

        An en passant target square is given if and only if the last move 
        was a pawn advance of two squares. Therefore, an en passant target 
        square field may have a square name even if there is no pawn of 
        the opposing side that may immediately execute the en passant capture.
    */

    bool found_ep_target = false;
    int prev_source, prev_dest;
    prev_move.actualOffsets (*this, prev_source, prev_dest);

    piece = GetSquareContents(prev_dest);
    if ((piece == WPAWN) || (piece == BPAWN)) {
        int     dir = prev_dest - prev_source;
        char    tfile = XPART(prev_dest) - 2 + 'a';
        switch (dir) {
            case 2*NORTH:
                assert (BlackToMove());
                assert (piece == WPAWN);
                assert (YPART(prev_dest)-2+'1' == '4');
                fencopy(tfile);
                fencopy('3');   // Hail Dale!
                found_ep_target = true;
                break;

            case 2*SOUTH:
                assert (WhiteToMove());
                assert (piece == BPAWN);
                assert (YPART(prev_dest)-2+'1' == '5');
                fencopy(tfile);
                fencopy('6');
                found_ep_target = true;
                break;
        }
    }

    if (!found_ep_target) {
        fencopy('-');
    }

    /*
        16.1.8 Halfmove clock
        The fifth field is a nonnegative integer representing the halfmove clock. 
        This number is the count of halfmoves (or ply) since the last pawn 
        advance or capturing move. This value is used for the fifty move draw rule.
    */
    int quietPlies = ply_number - lastCapOrPawn - 1;
    sprintf (tempstr, " %d ", quietPlies);
    for (i=0; tempstr[i]; ++i) {
        fencopy (tempstr[i]);
    }

    /*
        16.1.9 Fullmove number
        The sixth and last field is a positive integer that gives the fullmove number. 
        This will have the value "1" for the first move of a game for both White and Black. 
        It is incremented by one immediately after each move by Black.
    */
    sprintf (tempstr, "%d", (1 + ply_number/2));
    for (i=0; tempstr[i]; ++i) {
        fencopy (tempstr[i]);
    }

    /*
        16.1.10 Examples
        Here's the FEN for the starting position:

        rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1

        And after the move 1. e4:

        rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1

        And then after 1. ... c5:

        rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2

        And then after 2. Nf3:

        rnbqkbnr/pp1ppppp/8/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2

        For two kings on their home squares and a white pawn on e2 
        (White to move) with thirty eight full moves played with five 
        halfmoves since the last pawn move or capture:

        4k3/8/8/8/8/8/4P3/4K3 w - - 5 39
    */
    
    fencopy('\0');      // must null-terminate the string!
    return buffer;
}

#undef fencopy



static SQUARE PieceFromFenChar (char fenchar, int &count)
{
    count = 1;      // assume a single piece, unless proven otherwise
    
    switch (fenchar) {
        case 'P':   return WPAWN;
        case 'N':   return WKNIGHT;
        case 'B':   return WBISHOP;
        case 'R':   return WROOK;
        case 'Q':   return WQUEEN;
        case 'K':   return WKING;

        case 'p':   return BPAWN;
        case 'n':   return BKNIGHT;
        case 'b':   return BBISHOP;
        case 'r':   return BROOK;
        case 'q':   return BQUEEN;
        case 'k':   return BKING;
        
        default:
            if (fenchar >= '1' && fenchar <= '8') {
                count = fenchar - '0';
            } else {
                count = 0;  // indicate an invalid character!
            }
            return EMPTY;
    }
}


bool ChessBoard::SetForsythEdwardsNotation (const char *fen)
{
    // Create a temporary ChessBoard, in which we try to build from given FEN.
    // If we fail for any reason, we discard the temporary chess board and return false.
    // If we succeed, we copy from the temporary chess board to this chess board and return true.
    
    if (fen == NULL) {
        return false;   // invalid parameter!
    }

    while (*fen && isspace(*fen)) {     // skip any leading white space characters
        ++fen;
    }

    bool success = false;
    int i = 0;  // index into 'fen' string
    int x;
    int y;
    int count;
    SQUARE square;
    
    ChessBoard temp;
    
    // Empty out the entire temp board's contents...
    
    // After 1. e4 c5 2. Nf3
    // "rnbqkbnr/pp1ppppp/8/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2"
    for (y=9; y >= 2; --y) {
        x = 2;
        while (x <= 9) {
            square = PieceFromFenChar (fen[i++], count);
            if (count == 0) {
                return false;   // PieceFromFenChar detected an invalid character in the row.
            } else {
                while (count > 0) {
                    if (x > 9) {
                        return false;   // Too many pieces in this row!
                    }
                    temp.board[OFFSET(x,y)] = square;
                    --count;
                    ++x;
                }
            }            
        }
        
        // We should be exactly at the end of this rank...
        if (y > 2) {
            if (fen[i++] != '/') {
                return false;   // syntax error in fen: wrong separator between ranks
            }
        } else {
            if (fen[i++] != ' ') {
                return false;   // syntax error in fen: expected space to terminate the board contents
            }
        }
    }
    
    // Expect either 'w' or 'b' to indicate whose turn it is...
    switch (fen[i++]) {
        case 'w':
            temp.white_to_move = true;
            break;
            
        case 'b':
            temp.white_to_move = false;
            break;
            
        default:
            return false;   // invalid indicator of whose turn it is!
    }
    
    if (fen[i++] != ' ') {
        return false;   // must be a space after move indicator
    }
    
    // Look at castling availability flags: either '-' or one or more of 'KQkq' in that order.
    // Assume all kings and rooks have moved unless proven otherwise by castling availability flags...
    temp.flags = SF_WKMOVED | SF_WKRMOVED | SF_WQRMOVED | SF_BKMOVED | SF_BKRMOVED | SF_BQRMOVED;
    count = 0;
    if (fen[i] == 'K') {    // is caller claiming White O-O still possible?
        ++i;
        ++count;
        if (temp.board[OFFSET(6,2)] != WKING || temp.board[OFFSET(9,2)] != WROOK) {
            return false;   // king and kingside rook must be in original position to still do O-O
        }
        temp.flags &= ~(SF_WKMOVED | SF_WKRMOVED);  // turn off kingside castling disable flags for White
    }
    if (fen[i] == 'Q') {    // is caller claiming White O-O-O still possible?
        ++i;
        ++count;
        if (temp.board[OFFSET(6,2)] != WKING || temp.board[OFFSET(2,2)] != WROOK) {
            return false;   // king and queenside rook must be in original position to still do O-O-O
        }
        temp.flags &= ~(SF_WKMOVED | SF_WQRMOVED);  // turn off queenside castling disable flags for White
    }
    if (fen[i] == 'k') {    // is caller claiming Black O-O still possible?
        ++i;
        ++count;
        if (temp.board[OFFSET(6,9)] != BKING || temp.board[OFFSET(9,9)] != BROOK) {
            return false;   // king and kingside rook must be in original position to still do O-O
        }
        temp.flags &= ~(SF_BKMOVED | SF_BKRMOVED);  // turn off kingside castling disable flags for Black
    }
    if (fen[i] == 'q') {    // is caller claiming Black O-O-O still possible?
        ++i;
        ++count;
        if (temp.board[OFFSET(6,9)] != BKING || temp.board[OFFSET(2,9)] != BROOK) {
            return false;   // king and queenside rook must be in original position to still do O-O-O
        }
        temp.flags &= ~(SF_BKMOVED | SF_BQRMOVED);  // turn off queenside castling disable flags for Black
    }
    if (count == 0) {   // if this is the case, it means we did not see 'K', 'Q', 'k', or 'q' for castling flags
        if (fen[i++] != '-') {
            return false;   // If K, Q, k, q not present, only choice left is '-'.
        }
    }
    if (fen[i++] != ' ') {
        return false;   // must be exactly one space after castling availability
    }

    // Look for en passant target indication of previous move, using it to set temp.prev_move accordingly.
    temp.prev_move.score = 0;
    if (fen[i] == '-') {
        ++i;    // no en passant target
        temp.prev_move.dest = SPECIAL_MOVE_NULL;    // we have no idea what the previous move was, if any.
        temp.prev_move.source = 0;
    } else {
        x = fen[i++] - 'a' + 2;
        if (x < 2 || x > 9) {
            return false;   // invalid file
        }
        y = fen[i++] - '1' + 2;
        if (temp.white_to_move) {
            // Black just moved, so e.p. target rank must be 7 (in the 2..9 range Chenard uses for ranks)
            if (y != 7) {
                return false;   // invalid e.p. target rank
            }
            // Construct the distinct previous move Black made that would yield this e.p. target.
            temp.prev_move.source = OFFSET(x,8);
            temp.prev_move.dest   = OFFSET(x,6);
            if (temp.board[temp.prev_move.source] != EMPTY || 
                temp.board[OFFSET(x,7)] != EMPTY || 
                temp.board[temp.prev_move.dest] != BPAWN) {
                return false;   // the board is not consistent with the inferred previous move
            }
        } else {
            // White just moved, so e.p. target rank must be 7 (in the 2..9 range Chenard uses for ranks)
            if (y != 4) {
                return false;   // invalid e.p. target rank
            }
            // Construct the distinct previous move White made that would yield this e.p. target.
            temp.prev_move.source = OFFSET(x,3);
            temp.prev_move.dest   = OFFSET(x,5);
            if (temp.board[temp.prev_move.source] != EMPTY || 
                temp.board[OFFSET(x,4)] != EMPTY || 
                temp.board[temp.prev_move.dest] != WPAWN) {
                return false;   // the board is not consistent with the inferred previous move
            }
        }
    }
    
    // Look for the halfmove clock: how many plies have gone by since the last pawn advance or capture.
    int quietPlies     = 0;        // a.k.a. half-move clock
    int fullMoveNumber = 0;
    if (2 != sscanf(&fen[i], " %d %d", &quietPlies, &fullMoveNumber)) {
        return false;   // missing the numeric indicators at the end
    }

    // We can't really do anything with calculating ply_number from fullMoveNumber, because
    // doing so would mess up places we use it as an array index.
    
    // A little algebra:  quietPlies = ply_number - lastCapOrPawn - 1, so...
    temp.lastCapOrPawn = temp.ply_number - quietPlies - 1;      // it's perfectly OK if this is negative    
    
    temp.Update();                            // needed to tally inventory and calculate hash values, etc.

    ReplaceString (temp.initialFen, fen);     // this serves the side effect of remembering that the board has been edited.
    
    if (temp.PositionIsPossible()) {
        success = true;
        *this = temp;
    }    
    
    return success;
}


char *ChessBoard::GetInitialForsythEdwardsNotation() const
{
    return CopyString (initialFen);
}


void FormatChessMove (
    const ChessBoard &board,
    Move              move,
    char              movestr [MAX_MOVE_STRLEN + 1] )
{
    MoveList movelist;
    ChessBoard &temp = (ChessBoard &) board;
    temp.GenMoves (movelist);
    FormatChessMove (board, movelist, move, movestr);
}


// Must be called before move is made on the given board!!!
void FormatChessMove ( 
    const ChessBoard &board,
    const MoveList   &LegalMoves,
    Move              move,
    char              movestr [MAX_MOVE_STRLEN + 1] )
{
    int         source, dest, i, k, msource, mdest;
    UnmoveInfo  unmove;
    SQUARE      promotion = EMPTY;
    SQUARE      mpiece, mprom;
    int         length = 0;     // when done, this is strlen(movestr)

    MoveList    movelist = LegalMoves;  // make a local copy that we can modify

    move.actualOffsets (board, source, dest);
    char file1 = XPART(source) - 2 + 'a';
    char rank1 = YPART(source) - 2 + '1';
    char file2 = XPART(dest)   - 2 + 'a';
    char rank2 = YPART(dest)   - 2 + '1';

    ChessBoard &temp = (ChessBoard &) board;    // cheating, but it's OK because we will put the board back into the same state
    temp.MakeMove (move, unmove);
    bool check    = temp.CurrentPlayerInCheck();
    bool immobile = !temp.CurrentPlayerCanMove();
    temp.UnmakeMove (move, unmove);

    int dir = dest - source;
    SQUARE piece = GetPhonyWhitePiece (board, source);    // WP_INDEX .. WK_INDEX
    char pieceSymbol = GetPieceSymbol (piece);

    if ((piece == WK_INDEX) && (dir == 2*EAST)) {
        strcpy (movestr, "O-O");
        length = (int) strlen(movestr);
    } else if ((piece == WK_INDEX) && (dir == 2*WEST)) {
        strcpy (movestr, "O-O-O");
        length = (int) strlen(movestr);
    } else {
        // This move is not castling.
        SQUARE capture = GetPhonyWhitePiece (board, dest);
        if ((piece == WP_INDEX) && (file1 != file2) && (capture == EMPTY)) {
            // Weirdness alert:  adjust for en passant capture...
            capture = WP_INDEX;
        }

        if (piece == WP_INDEX) {
            // We need to know whether this is pawn promotion for 2 reasons:
            // (1) So we can filter out the 4 pawn promotions for N,B,R,Q as not being ambiguous.
            // (2) So we can put "=Q", "=R", ... in the move notation.
            if ((rank2 == '1') || (rank2 == '8')) {
                // This is pawn promotion, so figure out which piece is being promoted to.
                switch (move.dest & 0xf0) {
                    case SPECIAL_MOVE_PROMOTE_NORM:
                    case SPECIAL_MOVE_PROMOTE_CAP_EAST:
                    case SPECIAL_MOVE_PROMOTE_CAP_WEST:
                        promotion = move.dest & PIECE_MASK;
                        break;

                    default:
                        assert (false);
                        promotion = WP_INDEX;    // pick an impossible piece to promote to!
                        break;
                }
            } else {
                // This is a pawn move, but we know for sure this is NOT pawn promotion.
                // Leave promotion == EMPTY.
            }
        } else {
            // we know for sure this is NOT pawn promotion; it is not even a pawn move.
            // Leave promotion == EMPTY.
        }
        
        // Central to PGN is the concept of "ambiguous" notation.
        // We want to figure out the minimum number of characters needed
        // to unambiguously encode the chess move.
        // So let's generate the list of legal moves.

        // Compact the list so that only moves with the same destination and moved piece are listed.
        // Include only pawn promotions to the same promoted piece.
        for (i=k=0; i < movelist.num; ++i) {
            movelist.m[i].actualOffsets (board, msource, mdest);
            if (mdest == dest) {
                mpiece = GetPhonyWhitePiece (board, msource);
                if (mpiece == piece) {
                    // Filter out identical pawn (source,dest) that have different promotions.
                    // We don't need to disambiguate those because the "=" notation will do that later.
                    switch (movelist.m[i].dest & 0xf0) {
                        case SPECIAL_MOVE_PROMOTE_NORM:
                        case SPECIAL_MOVE_PROMOTE_CAP_EAST:
                        case SPECIAL_MOVE_PROMOTE_CAP_WEST:
                            mprom = movelist.m[i].dest & PIECE_MASK;
                            break;

                        default:
                            mprom = EMPTY;
                            break;
                    }

                    if ((promotion == EMPTY) || (promotion == mprom)) {
                        if (k < i) {
                            movelist.m[k] = movelist.m[i];
                        }
                        ++k;
                    }
                }
            }
        }

        assert (k > 0);       // otherwise the supplied move is not legal!
        movelist.num = k;   // truncate the list: now it contains only moves to same dest by same piece

        bool need_source_file = false;
        bool need_source_rank = false;

        if (movelist.num > 1) {
            /*
                [The following is quoted from http://www.very-best.de/pgn-spec.htm, section 8.2.3.]

                In the case of ambiguities (multiple pieces of the same type moving to the same square), 
                the first appropriate disambiguating step of the three following steps is taken:

                First, if the moving pieces can be distinguished by their originating files, 
                the originating file letter of the moving piece is inserted immediately after 
                the moving piece letter.

                Second (when the first step fails), if the moving pieces can be distinguished by 
                their originating ranks, the originating rank digit of the moving piece is inserted 
                immediately after the moving piece letter.

                Third (when both the first and the second steps fail), the two character square 
                coordinate of the originating square of the moving piece is inserted immediately 
                after the moving piece letter.                     
            */

            // Check for distinct files and ranks for other moves that end up at 'dest'.
            int file_count = 0;
            int rank_count = 0;
            for (i=0; i < movelist.num; ++i) {
                movelist.m[i].actualOffsets (board, msource, mdest);
                if ((XPART(msource) - 2 + 'a') == file1) {
                    ++file_count;
                }
                if (YPART(msource) - 2 + '1' == rank1) {
                    ++rank_count;
                }
            }

            assert (rank_count > 0);
            assert (file_count > 0);

            if (file_count == 1) {
                need_source_file = true;
            } else {
                need_source_rank = true;
                if (rank_count > 1) {
                    need_source_file = true;
                }
            }
        }

        if (piece == WP_INDEX) {
            if (capture != EMPTY) {     // NOTE:  capture was set to PAWN above if this move is en passant.
                need_source_file = true;
            }
        } else {
            movestr[length++] = pieceSymbol;
        }

        if (need_source_file) {
            movestr[length++] = file1;
        }

        if (need_source_rank) {
            movestr[length++] = rank1;
        }

        if (capture != EMPTY) {
            movestr[length++] = 'x';
        }

        movestr[length++] = file2;
        movestr[length++] = rank2;

        if (promotion != EMPTY) {
            movestr[length++] = '=';
            movestr[length++] = GetPieceSymbol(promotion);
        }
    }

    if (check) {
        if (immobile) {
            movestr[length++] = '#';
        } else {
            movestr[length++] = '+';
        }
    }

    movestr[length] = '\0';
    assert (length >= 2);
    assert (length <= 7);
}


void GetGameListing (
    const ChessBoard &board,
    char *buffer,
    unsigned bufSize )
{
    sprintf ( buffer, "     %-7s %-7s\r\n", "White", "Black" );
    unsigned len = (unsigned) strlen(buffer);
    char *p = buffer + len;
    ChessBoard temp;
    int numPlies = board.GetCurrentPlyNumber();
    for ( int i=0; i<numPlies && len<bufSize; i++ )
    {
        char moveString [64];
        char s [64];
        int thisLen = 0;
        Move move = board.GetPastMove(i);
        FormatChessMove ( temp, move, moveString );
        if ( temp.WhiteToMove() )
            thisLen = sprintf ( s, "%3d. %-7s", 1+(i/2), moveString );
        else
            thisLen = sprintf ( s, " %s\r\n", moveString );

        if ( len + thisLen + 1 > bufSize )
            break;

        strcat ( p, s );
        p += thisLen;
        len += thisLen;
        UnmoveInfo unmove;
        temp.MakeMove ( move, unmove );
    }
}


int ChessBoard::GetCurrentPlyNumber() const
{
    return int(ply_number);
}


Move ChessBoard::GetPastMove ( int p ) const
{
    Move move;
    if ( p < 0 || p >= ply_number || p >= MAX_GAME_HISTORY || !gameHistory )
    {
        move.source = move.dest = BYTE(0);
        move.score = SCORE(0);
    }
    else
        move = gameHistory[p];

    return move;
}


void ChessBoard::SaveSpecialMove ( Move special )
{
    if ( ply_number < MAX_GAME_HISTORY )
        gameHistory [ply_number] = special;

    initialPlyNumber = ++ply_number;        // Remember the spot right after the last pseudo-move was saved

    char fen [200];
    if (GetForsythEdwardsNotation (fen, sizeof(fen))) {
        ReplaceString (initialFen, fen);
    } else {
        ChessFatal ("ChessBoard::SaveSpecialMove: Unable to save initial FEN");
    }
}


bool ChessBoard::operator== ( const ChessBoard &other ) const
{
    // The vast majority of the time, comparing the hash values will 
    // determine whether the boards are unequal...

    if ( cachedHash != other.cachedHash )
        return false;

    if ( flags != other.flags )
        return false;

    if ( white_to_move != other.white_to_move )
        return false;

    // ... but just in case the hash values do match, make absolutely sure
    // the boards are equivalent.

    for ( int y=OFFSET(2,2); y <= OFFSET(2,9); y += NORTH )
    {
        for ( int x=0; x<8; x++ )
        {
            int ofs = x+y;
            if ( board[ofs] != other.board[ofs] )
                return false;
        }
    }

    return true;
}


bool ChessBoard::isLegal ( Move move )
{
    MoveList ml;
    GenMoves (ml);
    return ml.IsLegal(move);
}


bool ChessBoard::PositionIsPossible() const
{
    bool possible = true;
    int x;
    
    // Caller must ensure that ChessBoard::Update() was called if the internals of the ChessBoard
    // were monkeyed with, or the inventory[] will be out of date.
    // This is an inherent requirement of all ChessBoard internal methods.
    // It should not be possible for an outside user of class ChessBoard to avoid Update being called.
    
    if (inventory[WK_INDEX] == 1 && inventory[BK_INDEX] == 1) {
        // Make sure there are no pawns on the first or eighth ranks...
        for (x=2; x <= 9; ++x) {
            if (board[OFFSET(x,2)] & (WP_MASK | BP_MASK)) {
                return false;
            }
            
            if (board[OFFSET(x,9)] & (WP_MASK | BP_MASK)) {
                return false;
            }
        }
        
        int numberOfWhitePieces = inventory[WK_INDEX];
        numberOfWhitePieces    += inventory[WQ_INDEX];
        numberOfWhitePieces    += inventory[WR_INDEX];
        numberOfWhitePieces    += inventory[WB_INDEX];
        numberOfWhitePieces    += inventory[WN_INDEX];
        numberOfWhitePieces    += inventory[WP_INDEX];
        if (numberOfWhitePieces > 16) {
            return false;   // White cannot have more pieces on the board than he started with!
        }
        
        if (inventory[WQ_INDEX] > 9 || inventory[WR_INDEX] > 10 || inventory[WB_INDEX] > 10 || inventory[WN_INDEX] > 10) {
            return false;   // White cannot promote more than 8 pawns
        }

        int numberOfBlackPieces = inventory[BK_INDEX];
        numberOfBlackPieces    += inventory[BQ_INDEX];
        numberOfBlackPieces    += inventory[BR_INDEX];
        numberOfBlackPieces    += inventory[BB_INDEX];
        numberOfBlackPieces    += inventory[BN_INDEX];
        numberOfBlackPieces    += inventory[BP_INDEX];
        if (numberOfBlackPieces > 16) {
            return false;   // Black cannot have more pieces on the board than he started with!
        }

        if (inventory[BQ_INDEX] > 9 || inventory[BR_INDEX] > 10 || inventory[BB_INDEX] > 10 || inventory[BN_INDEX] > 10) {
            return false;   // Black cannot promote more than 8 pawns
        }

        if (white_to_move) {
            // Make sure Black is not in check.  Otherwise, how can it be White's turn?
            if (IsAttackedByWhite (bk_offset)) {
                possible = false;
            }
        } else {
            // Make sure White is not in check.  Otherwise, how can it be Black's turn?
            if (IsAttackedByBlack (wk_offset)) {
                possible = false;
            }
        }
    } else {
        possible = false;   // wrong number of kings!!!
    }
    
    return possible;
}


bool ChessBoard::ScanMove (const char *pgn, Move &move)
{
    int         i;
    MoveList    ml;
    char        movestr [1 + MAX_MOVE_STRLEN];

    move.source = move.dest = 0;

    if (pgn != NULL) {
        GenMoves (ml);
        for (i=0; i < ml.num; ++i) {
            FormatChessMove (*this, ml, ml.m[i], movestr);
            if (0 == strcmp(movestr,pgn)) {
                move = ml.m[i];
                return true;
            }
        }

        // OK, we did not find an exact match, so we have to fall back on best guess...
        int match = -1;
        for (i=0; i < ml.num; ++i) {
            if (pgnCloseMatch (pgn, ml.m[i])) {
                if (match >= 0) {
                    // ambiguous... we already found something that matched!
                    return false;
                } else {
                    match = i;
                }
            }
        }

        if (match >= 0) {
            move = ml.m[match];
            return true;
        }
    }

    return false;
}


SQUARE ConvertPieceToWhite (SQUARE piece)
{
    switch (piece) {
        case BPAWN:     return WPAWN;
        case BKNIGHT:   return WKNIGHT;
        case BBISHOP:   return WBISHOP;
        case BROOK:     return WROOK;
        case BQUEEN:    return WQUEEN;
        case BKING:     return WKING;
        default:        return piece;
    }
}


SQUARE ConvertCharToWhitePiece (char c)
{
    switch (c) {
        case 'N':   return WKNIGHT;
        case 'B':   return WBISHOP;
        case 'R':   return WROOK;
        case 'Q':   return WQUEEN;
        case 'K':   return WKING;

        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
        case 'g':
        case 'h':   return WPAWN;

        default:    return EMPTY;
    }
}


bool ChessBoard::pgnCloseMatch (const char *pgn, Move move) const
{
    int     msource, mdest;
    move.actualOffsets (*this, msource, mdest);

    // convert piece to White, to make comparisons easier...
    SQUARE piece = ConvertPieceToWhite (GetSquareContents(msource));

    int length = (int) strlen(pgn);
    if ((pgn[length-1] == '+') || (pgn[length-1] == '#')) {
        --length;
    }

    const char *prom = strchr (pgn, '=');
    if (prom) {
        if (strchr("QRBN",prom[1])) {
            length = (int) (prom - pgn);
            if (piece == WPAWN) {
                // ok so far
            } else {
                // promotion does not match non-pawn move
                return false;
            }
        } else {
            // illegal promotion format
            return false;
        }
    }

    if (length >= 2) {
        char    file2 = pgn[length-2];
        char    rank2 = pgn[length-1];
        if ((file2 >= 'a') && (file2 <= 'h')) {
            if ((rank2 >= '1') && (rank2 <= '8')) {
                int dest = OFFSET(file2-'a'+2, rank2-'1'+2);
                if (dest == mdest) {
                    length -= 2;    // chop off destination characters

                    SQUARE capture = ConvertPieceToWhite (GetSquareContents (dest));
                    if ((piece == WPAWN) && (XPART(msource) != XPART(mdest)) && (capture == EMPTY)) {
                        // fake it for en passant
                        capture = WPAWN;
                    }
                    const char *capx = strchr(pgn,'x');
                    if (capx ? (capture != EMPTY) : (capture == EMPTY)) {
                        SQUARE pgnpiece = ConvertCharToWhitePiece (pgn[0]);
                        if (pgnpiece == piece) {
                            if (capx) {
                                length = (int) (capx - pgn);    // truncate at capture symbol 'x'
                            }

                            if (length > 1) {
                                // There must be at least one disambiguating character
                                if ((pgn[1] >= 'a') && (pgn[1] <= 'h')) {
                                    if (length > 2) {
                                        if ((pgn[2] >= '1') && (pgn[2] <= '8')) {
                                            int source = OFFSET(pgn[1]-'a'+2, pgn[2]-'1'+2);
                                            return msource == source;
                                        } else {
                                            return false;
                                        }
                                    }
                                    return XPART(msource)-2+'a' == pgn[1];
                                } else if ((pgn[1] >= '1') && (pgn[1] <= '8')) {
                                    return YPART(msource)-2+'1' == pgn[1];
                                }
                            } else {
                                return true;    // unambiguous and compatible
                            }
                        }
                    }
                }
            }
        }
    }

    return false;
}


const char *GetPgnFileStateString (PGN_FILE_STATE state)
{
    switch (state) {
        case PGN_FILE_STATE_UNDEFINED:          return "undefined";
        case PGN_FILE_STATE_NEWGAME:            return "new game";
        case PGN_FILE_STATE_SAMEGAME:           return "same game";
        case PGN_FILE_STATE_EOF:                return "unexpected end of file";
        case PGN_FILE_STATE_SYNTAX_ERROR:       return "syntax error";
        case PGN_FILE_STATE_INVALID_PARAMETER:  return "invalid parameter";
        default:                                return "unknown state";
    }
}


//--------------------------------------------------------------------------------



PieceLookup PieceLookupTable;   // Initialized by constructor before main()


//--------------------------------------------------------------------------------


PackedChessBoard::PackedChessBoard()
{
    hash = 0;
    flags = 0;
    for ( int i=0; i<64; ++i )
        square[i] = 0;

    prevMove.source = prevMove.dest = 0;
    prevMove.score = 0;
}


PackedChessBoard::PackedChessBoard ( const ChessBoard &board )
{
    *this = board;
}


PackedChessBoard &PackedChessBoard::operator= ( const ChessBoard &board )
{
    hash = board.cachedHash;
    flags = 0;
    if ( board.white_to_move )
        flags |= PACKEDFLAG_WHITE_TO_MOVE;

    if ( board.flags & SF_WKMOVED )
        flags |= PACKEDFLAG_WKMOVED;

    if ( board.flags & SF_WKRMOVED )
        flags |= PACKEDFLAG_WKRMOVED;

    if ( board.flags & SF_WQRMOVED )
        flags |= PACKEDFLAG_WQRMOVED;

    if ( board.flags & SF_BKMOVED )
        flags |= PACKEDFLAG_BKMOVED;

    if ( board.flags & SF_BKRMOVED )
        flags |= PACKEDFLAG_BKRMOVED;

    if ( board.flags & SF_BQRMOVED )
        flags |= PACKEDFLAG_BQRMOVED;

    if ( board.ply_number > 0 )
        prevMove = board.prev_move;
    else
    {
        prevMove.source = prevMove.dest = 0;
        prevMove.score = 0;
    }

    int index=0;
    for ( int y=OFFSET(2,2); y <= OFFSET(2,9); y += NORTH )
    {
        for ( int x=0; x<8; ++x )
        {
            int ofs = x + y;
            square[index++] = SPIECE_INDEX(board.board[ofs]);
        }
    }

    return *this;
}


bool PackedChessBoard::operator== ( const ChessBoard &board ) const
{
    PackedChessBoard other(board);
    return *this == other;
}


bool PackedChessBoard::operator== ( const PackedChessBoard &other ) const
{
    if ( hash != other.hash )
        return false;

    if ( flags != other.flags )
        return false;

    if ( prevMove != other.prevMove )
        return false;

    for ( int i=0; i<64; ++i )
        if ( square[i] != other.square[i] )
            return false;

    return true;
}

//---------------------------------------------------------------------------------------------------------------------------------



/*
    $Log: board.cpp,v $
    Revision 1.22  2009/05/02 20:25:48  Don.Cross
    Fixed bug reported by Olivier Deville, as evident in the PGN listing below.
    ChessBoard::NumberOfRepetitions() was returning 3 when it should have returned 2.
    I am surprised I never noticed this problem before!
    I need to do more regression testing and look for other places where I used return values from this function,
    to make sure there are no additional errors elsewhere.

    [Event "ChessWar XIV Promotion 40m/20'"]
    [Site "DELL-E6600"]
    [Date "2009.04.29"]
    [Round "1.8"]
    [White "ALChess 1.5b"]
    [Black "Chenard 20081206"]
    [Result "1/2-1/2"]
    [Annotator "1. -0.02   5... -0.06"]
    [PlyCount "116"]
    [EventDate "2009.??.??"]

    1. e3 {-0.02/9 30} 1... e5 2. d4 {-0.06/9 30} 2... exd4 3. exd4 {-0.08/9 30}
    3... Nf6 4. Nf3 {+0.00/8 30} 4... Be7 5. Bd3 {+0.03/9 30} 5... d5 {-0.06/7 44}
    6. Be3 {+0.01/9 30} 6... Nc6 {+0.04/7 43} 7. Bb5 {-0.03/9 30} 7... Qd6 {
    +0.12/7 43} 8. O-O {-0.05/8 30} 8... O-O {+0.21/7 42} 9. Re1 {-0.10/8 30} 9...
    Qb4 {+0.57/7 42} 10. Bxc6 {+0.00/10 30} 10... Qxb2 11. Bxb7 {-0.02/10 30} 11...
    Qxb7 12. Nbd2 {-0.02/8 30} 12... Be6 {+0.15/7 43} 13. Ne5 {-0.07/8 30} 13...
    Qb2 {+0.18/7 43} 14. c4 {+0.04/8 30} 14... dxc4 {+0.20/7 42} 15. Ndxc4 {
    +0.08/8 30} 15... Qb7 16. Rc1 {+0.02/8 30} 16... a5 {+0.04/6 43} 17. Re2 {
    +0.08/8 30} 17... Ng4 {+0.11/7 42} 18. Rb2 {+0.19/9 30} 18... Nxe3 {+0.27/7 41}
    19. fxe3 {+0.19/7 30} 19... Qe4 {+0.40/6 41} 20. Qf3 {+0.23/8 30} 20... Qxf3
    21. gxf3 {-0.54/9 30} 21... a4 {+0.54/7 42} 22. e4 {+0.15/9 30} 22... Bg5 {
    +0.93/7 41} 23. Rcc2 {+0.16/9 30} 23... f6 {+0.93/7 40} 24. Nc6 {+0.13/9 30}
    24... Bf4 {+0.93/7 10} 25. Rb1 {+0.15/9 30} 25... Rfe8 {+1.02/7 41} 26. d5 {
    +0.10/10 30} 26... Bd7 {+0.80/7 40} 27. Nd4 {+0.10/9 30} 27... Re7 {+0.85/7 39}
    28. Ne2 {+0.16/9 30} 28... Bg5 {+0.83/7 38} 29. Kg2 {+0.14/9 30} 29... Rd8 {
    +0.87/6 37} 30. d6 {-0.36/9 30} 30... cxd6 {+1.23/7 36} 31. Nxd6 {-0.34/10 35}
    31... Re6 32. Rd1 {-0.34/10 29} 32... Bb5 {+0.64/8 10} 33. Nc3 {-0.36/11 29}
    33... Ba6 34. Nc4 {-0.33/11 29} 34... Rc8 {+0.75/8 34} 35. Nd6 {-0.36/11 29}
    35... Rc7 36. Rb2 {-0.21/10 29} 36... h6 37. Nxa4 {+0.46/9 29} 37... Bf4 38.
    Nf5 {+0.54/9 29} 38... Re8 {+0.08/7 25} 39. Nb6 {+0.64/8 30} 39... Be5 40. Nd5
    {+0.69/5 0.3} 40... Ra7 {+0.15/7 1:25} 41. Rc2 {+0.67/8 31} 41... Bb5 42. Rdd2
    {+0.72/9 31} 42... Bd7 {-0.06/7 41} 43. Nxh6+ {+0.74/9 31} 43... gxh6 {
    -0.59/8 41} 44. f4 {+0.81/10 31} 44... Ba4 45. Rc4 {+0.85/10 31} 45... Bb5 {
    -0.78/8 11} 46. Rb4 {+0.92/10 31} 46... Rg7+ 47. Kf2 {+1.00/9 31} 47... Bxf4
    48. Nxf4 {+1.00/10 31} 48... Bc6 49. Nd5 {+0.99/10 31} 49... Re6 {-0.70/7 45}
    50. a4 {+0.95/10 31} 50... Rf7 {-0.68/7 44} 51. Rc4 {+1.03/9 31} 51... Bb7 {
    -0.44/7 44} 52. Rb2 {+1.05/9 31} 52... Ba6 53. Rcb4 {+1.08/10 31} 53... f5 {
    -0.40/9 44} 54. Ke3 {+1.09/9 31} 54... Kh7 {-0.42/8 13} 55. Rb1 {+1.04/10 31}
    55... Rg7 {-0.23/7 44} 56. R1b2 {+1.05/10 31} 56... Rg4 {-0.15/8 44} 57. Nc3 {
    +1.00/10 31} 57... Rc6 {+0.00/8 43} 58. Nd5 {+1.03/10 31} 58... Re6 {
    +0.00/10 12  Draw} 1/2-1/2

    Revision 1.21  2008/12/01 23:08:53  Don.Cross
    Norm Pollock announced on the WinBoard forum that he has updated his PGN database:
        http://www.open-aurec.com/wbforum/viewtopic.php?t=49522
        http://www.hoflink.com/~npollock/chess.html
    This is just what I was looking for, so that I could beef up chenard.trx (training file).
    I modified the PGN scanner to recognize WhiteElo, BlackElo, and Result tags.
    I am using WhiteElo and BlackElo to decide whether a game is trustworthy enough to pretend like Chenard thought
    of the move itself, so that it will play that move in actual games, instead of just as a reference point for opponent moves.
    I am currently in the process of slurping an enormous amount of openings into chenard.trx.
    I decided to move GetNextPgnMove from board.cpp to gamefile.cpp, where it really belongs.

    Revision 1.20  2008/11/29 03:09:06  Don.Cross
    Some PGN input files have string tokens inside [] tags that disobey the spec.
    For example, I have seen stuff like [Black ""program name" "].
    So from now on, my PGN parser will be very lenient with tags it does not care about.
    As soon as I see "[", followed by a tag name I don't care about, I will just keep eating characters
    until I hit either '\n' or EOF.
    Currently the only tag I care about is "FEN".

    Revision 1.19  2008/11/24 11:21:58  Don.Cross
    Fixed a bug in ChessBoard copy constructor where initialFen was not initialized to NULL,
    causing ReplaceString to free invalid memory!
    Now tree trainer can absorb multiple games from one or more PGN files (not just GAM files like before).
    Made a fix in GetNextPgnMove: now we have a new state PGN_FILE_STATE_GAMEOVER, so that loading
    multiple games from a single PGN file is possible: before, we could not tell the difference between end of file
    and end of game.
    Overloaded ParseFancyMove so that it can directly return struct Move as an output parameter.
    Found a case where other PGN generators can include unnecessary disambiguators like "Nac3" or "N1c3"
    when "Nc3" was already unambiguous.  This caused ParseFancyMove to reject the move as invalid.
    Added recursion in ParseFancyMove to handle this case.
    If ParseFancyMove has to return false (could not parse the move), we now set all output parameters to zero,
    in case the caller forgets to check the return value.

    Revision 1.18  2008/11/17 18:47:07  Don.Cross
    WinChen.exe now has an Edit/Enter FEN function for setting a board position based on a FEN string.
    Tweaked ChessBoard::SetForsythEdwardsNotation to skip over any leading white space.
    Fixed incorrect Chenard web site URL in the about box.
    Now that computers are so much faster, reduced the

    Revision 1.17  2008/11/12 23:06:07  Don.Cross
    Massive refactoring of the concept of an 'editedFlag' in ChessBoard.
    Now instead of just saying the board was edited, we will always remember the most recent position
    after which there are no more pseudo-moves representing edit actions.
    This allows us to save/load PGN files based on edited positions.
    More work needs to be done:
    - Need to allow move undo/redo to work in edited positions.
    - Because the game history is stored now inside the ChessBoard, it seems silly to have an external copy also!
    - Move numbers should be based on FEN when PGN is saved.
    In general, this is a scary change, so it needs more careful testing before being published.

    Revision 1.16  2008/11/12 20:01:34  Don.Cross
    Completely rewrote the PGN file loader to properly scan tokens.
    This allows it to better handle finding the end-of-game markers, and will allow
    adding more advanced features in the future if desired.
    When saving to a PGN file, the code was failing to write an end-of-game result marker,
    but this is required by the PGN spec.

    Revision 1.15  2008/11/02 21:25:54  Don.Cross
    Fixed bug:  SetForsythEdwardsNotation can change the position of the kings,
    but nothing was updating wk_offset and bk_offset.  I have decided that
    ChessBoard::Update should be responsible for this, since any function that
    updates the innards of a ChessBoard must call this method.
    While I was in there, I updated to my current coding style.
    I also added a ChessFatal call in case ChessBoard::Update sees something
    inside the chess board that it doesn't understand.

    Revision 1.14  2008/11/02 21:04:27  Don.Cross
    Building xchenard for Win32 console mode now, so I can debug some stuff.
    Tweaked code to make Microsoft compiler warnings go away: deprecated functions and implicit type conversions.
    Checking in with version set to 1.51 for WinChen.

    Revision 1.13  2008/11/02 20:04:13  Don.Cross
    xchenard: Implemented xboard command 'setboard'.
    This required a new function ChessBoard::SetForsythEdwardsNotation.
    Enhanced ChessBoard::PositionIsPossible to check for more things that are not possible.
    Fixed anachronisms in ChessBoard::operator== where I was returning 0 or 1 instead of false or true.

    Revision 1.12  2008/05/03 15:26:19  Don.Cross
    Updated code for Visual Studio 2005.

    Revision 1.11  2006/01/18 19:58:10  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.10  2006/01/10 22:08:40  dcross
    Fixed a bug that caused Chenard to bomb out with a ChessFatal:
    Go into edit mode with White to move, and place a White piece where it
    attacks Black's king.  Chenard would immediately put a dialog box saying
    that a Black king had been captured, and exit.  Now you just get a dialog
    box saying "doing that would create an impossible board position", but
    Chenard keeps running.

    Revision 1.9  2006/01/03 02:08:22  dcross
    1. Got rid of non-PGN move generator code... I'm committed to the standard now!
    2. Made *.pgn the default filename pattern for File/Open.
    3. GetGameListing now formats more tightly, knowing that maximum possible PGN move is 7 characters.

    Revision 1.8  2006/01/02 19:40:29  dcross
    Added ability to use File/Open dialog to load PGN files into Win32 version of Chenard.

    Revision 1.7  2006/01/01 19:19:53  dcross
    Starting to add support for loading PGN data files into Chenard.
    First I am starting with the ability to read in an opening library into the training tree.
    Not finished yet.

    Revision 1.6  2005/12/31 21:37:02  dcross
    1. Added the ability to generate Forsyth-Edwards Notation (FEN) string, representing the state of the game.
    2. In stdio version of chess UI, display FEN string every time the board is printed.

    Revision 1.5  2005/12/31 17:30:40  dcross
    Ported PGN notation generator from Chess.java to Chenard.
    I left in the goofy old code just in case I ever need to use it again (see #define PGN_FORMAT).

    Revision 1.4  2005/11/23 21:30:29  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.3  2005/11/23 21:14:37  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!

    Revision 1.2  2005/11/23 20:46:53  dcross
    In Linux build, gcc found use of trigraphs that I had not noticed before.
    If the move notation generator had ever had an internal error, it would not
    have generated the string I thought.  Replaced question marks with pound signs.

         Revision history:
    
    1993 July 14 [Don Cross]
         Fixed a bug in FormatChessMove().  It was using 'dest'
         expecting it to contain a pawn promotion piece in a situation
         after it had assigned the destination offset of the promotion
         to 'dest'.
    
    1993 August 30 [Don Cross]
         Changing pointers to references in the interfaces where
         appropriate.
    
    1993 October 19 [Don Cross]
         Adding ChessBoard::IsDefiniteDraw()
    
    1993 October 27 [Don Cross]
         Adding ChessBoard::GetPastMove() and
         ChessBoard::GetCurrentPlyNumber().
    
    1993 December 31 [Don Cross]
         Making the following changes to FormatChessMove:
         (1) If non-pawn is being moved, include piece name in source.
         (2) If capture of non-pawn, include piece name in dest.
    
    1994 January 30 [Don Cross]
         Added ChessBoard::ClearEverythingButKings().
         Added ChessBoard::EditCommand(Move).
    
    1994 March 1 [Don Cross]
         Added hash function and code to help find repeated board positions.
    
    1995 December 25 [Don Cross]
         Fixed a bug: ChessBoard needed copy constructor and
         assignment operator.
    
    1997 January 25 [Don Cross]
         A user reported to me that it really is possible to force a
         checkmate with B+N+K versus a lone K.  Therefore, I have
         modified ChessBoard::IsDefiniteDraw to reflect this.
    
    1999 January 19 [Don Cross]
         Updated coding style.
    
    1999 January 25 [Don Cross]
         Bit the bullet and finally added ChessBoard::IsDrawByRepetition()
         function, so that when repeatHash[] array determines there is a
         draw by repetition, this function is called to verify it for sure.
         This should eliminate false draw-by-repetition rulings once and
         for all.
    
    1999 January 28 [Don Cross]
         Fixed a bug in ChessBoard::IsDrawByRepetition().
         Was unnecessarily checking that the same side was to move
         (ChessBoard::operator== does this anyway), plus I was doing
         it wrong to boot.
    
    1999 February 5 [Don Cross]
         Changing IsDrawByRepetition() to NumberOfRepetitions(),
         so that IsDefiniteDraw() can optionally pass back the
         repetition count to its caller.  This way, I hope to prevent
         the transposition table code from messing up detecting
         forced draws by repetition.
    
    1999 August 4 [Don Cross]
         Adding ChessBoard::isLegal(Move) to stomp out a bug!
    
    1999 August 5 [Don Cross]
         Added utility function ChessBoard::GameIsOver().
    
    2001 January 14 [Don Cross]
         Adding class PackedBoard.
    
    2001 January 20 [Don Cross]
         Performance improvements for functions that can be called
         a lot (e.g. from endgame database generator and Windows GUI).
    
         1. In ChessBoard copy constructor, no longer copy the entire contents
            of the gameHistory array, but just the part needed as determined
            by ply_number.
    
         2. The function ChessBoard::SetOffsetContents is called only
            by the endgame database generator and the ChessBoard class itself.
            I am going to make it more efficient by not always updating
            everything in the ChessBoard.
            Have to be careful not to screw up how the endgame database
            algorithms work.
*/

