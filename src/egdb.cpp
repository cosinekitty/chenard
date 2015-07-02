#error This source file is obsolete - use egdbase.cpp instead.

/*===========================================================================

     egdb.cpp  -  Copyright (C) 2001-2005 by Don Cross

===========================================================================*/

#ifdef _WIN32
// needed for setting process priority
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chess.h"

const SCORE FORCED_WIN = 10000;

const unsigned MOVE_TABLE_SIZE    = 64 * 64 * 64;
const unsigned MOVE_TABLE_SIZE_BB = 64 * 64 * 32 * 32;
const unsigned MOVE_TABLE_SIZE_BN = 64 * 64 * 64 * 32;

const char *EDB_ROOK_FILENAME   = "rook.edb";
const char *EDB_QUEEN_FILENAME  = "queen.edb";
const char *EDB_PAWN_FILENAME   = "pawn.edb";
const char *EDB_BB_FILENAME     = "bb.edb";
const char *EDB_BN_FILENAME     = "bn.edb";

int Global_EndgameDatabaseMode = 0;

//--------------------------------------------------------------------------

bool EGDB_IsLegal ( ChessBoard & );


SCORE EGDB_WhiteSearch (
    ChessBoard  &board,
    int         depth,
    Move        *whiteTable,
    Move        *blackTable,
    ChessUI     &ui,
    SQUARE      whitePiece );


SCORE EGDB_BlackSearch (
    ChessBoard  &board,
    int         depth,
    Move        *whiteTable,
    Move        *blackTable,
    ChessUI     &ui,
    SQUARE      whitePiece );

//-------------------------------------------------------------------------

SCORE EGDB_WhiteSearchBB (
    ChessBoard  &board,
    int         depth,
    Move        *whiteTable,
    Move        *blackTable,
    ChessUI     &ui,
    int          required_mate_plies
);


SCORE EGDB_BlackSearchBB (
    ChessBoard  &board,
    int         depth,
    Move        *whiteTable,
    Move        *blackTable,
    ChessUI     &ui,
    int          required_mate_plies
);

//-------------------------------------------------------------------------

SCORE EGDB_WhiteSearchBN (
    ChessBoard  &board,
    int         depth,
    int         required_mate_plies,
    Move        *whiteTable,
    Move        *blackTable,
    ChessUI     &ui,
    unsigned    wbOffset,
    unsigned    wnOffset
);

SCORE EGDB_BlackSearchBN (
    ChessBoard  &board,
    int         depth,
    int         required_mate_plies,
    Move        *whiteTable,
    Move        *blackTable,
    ChessUI     &ui,
    unsigned    wbOffset,
    unsigned    wnOffset
);

//-------------------------------------------------------------------------


void GenerateEndgameDatabase (
    const char  *outFilename,
    SQUARE      whitePieceOtherThanKing,
    ChessBoard  &board,
    ChessUI     &ui );

void GenerateEndgameDatabaseBB (        // K+B+B vs K
    const char  *outFilename,
    ChessBoard  &board,
    ChessUI     &ui );

void GenerateEndgameDatabaseBN (        // K+B+N vs K
    const char  *outFilename,
    ChessBoard  &board,
    ChessUI     &ui );

bool ConsultEndgameDatabase (
    ChessBoard  &board,
    Move        &move,
    const char  *databaseFilename );

typedef unsigned (* TABLE_INDEX_FUNC)  ( ChessBoard & );

void CalcEndgameBestPath (
    ChessBoard          &board,
    BestPath            &bp,
    const Move          *whiteTable,
    const Move          *blackTable,
    TABLE_INDEX_FUNC    tableIndexFunc );

bool FetchMoveFromDatabase (
    const char  *databaseFilename,
    unsigned    tableIndex,
    Move        &move );


void DumpEndgameDatabaseBN (
    const char          *inDatabaseFileName,
    const char          *outTextFileName
);


//--------------------------------------------------------------------------


inline unsigned CalcPieceOffset (unsigned pieceIndex)
{
    return (pieceIndex%8 + 2) + 12*(pieceIndex/8 + 2);
}


inline unsigned CalcBishopBlackOffset (
    unsigned bishopIndex )
{
    unsigned pieceIndex = 2 * bishopIndex;
    if ( bishopIndex & 4 )
        ++pieceIndex;

    return CalcPieceOffset (pieceIndex);
}


inline unsigned CalcBishopWhiteOffset (
    unsigned bishopIndex )
{
    unsigned pieceIndex = 2 * bishopIndex;
    if ( !(bishopIndex & 4) )
        ++pieceIndex;

    return CalcPieceOffset (pieceIndex);
}


//--------------------------------------------------------------------------


unsigned LocateNonKingPiece (
    ChessBoard &board )
{
    const SQUARE *bptr = board.queryBoardPointer();
    const SQUARE nonking =
        WP_MASK | WN_MASK | WB_MASK | WR_MASK | WQ_MASK |
        BP_MASK | BN_MASK | BB_MASK | BR_MASK | BQ_MASK;

    for ( int y=OFFSET(2,2); y <= OFFSET(2,9); y += NORTH )
    {
        for ( int x=0; x<8; ++x )
        {
            int ofs = x + y;
            if ( bptr[ofs] & nonking )
                return ofs;
        }
    }

    return 0;
}


//--------------------------------------------------------------------------


bool SquareIsWhite ( unsigned offset )
{
    int x = XPART(offset);
    int y = YPART(offset);
    return (x+y) & 1;
}


bool SquareIsBlack ( unsigned offset )
{
    return !SquareIsWhite(offset);
}


bool LocateBishops (
    ChessBoard  &board,
    unsigned    &wbwOffset,
    unsigned    &wbbOffset )
{
    const SQUARE *bptr = board.queryBoardPointer();
    unsigned wbwCounter = 0;
    unsigned wbbCounter = 0;

    for ( int y=OFFSET(2,2); y <= OFFSET(2,9); y += NORTH )
    {
        for ( int x=0; x<8; ++x )
        {
            int ofs = x + y;
            if ( bptr[ofs] & (WB_MASK|BB_MASK) )
            {
                if ( SquareIsWhite(ofs) )
                {
                    ++wbwCounter;
                    wbwOffset = ofs;
                }
                else
                {
                    ++wbbCounter;
                    wbbOffset = ofs;
                }
            }
        }
    }

    if ( wbwCounter>1 )
        ChessFatal("More than one bishop on white");

    if ( wbbCounter>1 )
        ChessFatal("More than one bishop on black");

    return wbwCounter==1 && wbbCounter==1;
}


void ValidateBB (
    ChessBoard  &board,
    unsigned    wbwOffset,
    unsigned    wbbOffset )
{
    unsigned wbwVerify=0, wbbVerify=0;
    if ( !LocateBishops(board,wbwVerify,wbbVerify) )
        ChessFatal("could not locate bishops in ValidateBB");
    if ( wbwOffset != wbwVerify )
        ChessFatal("mislocated bishop on white in ValidateBB");
    if ( wbbOffset != wbbVerify )
        ChessFatal("mislocated bishop on black in ValidateBB");
}


//--------------------------------------------------------------------------


inline unsigned CalcPieceIndex (unsigned pieceOffset)
{
    return (pieceOffset%12 - 2) + 8*(pieceOffset/12 - 2);
}


unsigned CalcTableIndex (
    unsigned whiteKingOffset,
    unsigned whitePieceOffset,
    unsigned blackKingOffset )
{
    unsigned ti = CalcPieceIndex(whiteKingOffset) +
                  64 * (CalcPieceIndex(whitePieceOffset) +
                        64 * CalcPieceIndex(blackKingOffset));

    if ( ti >= MOVE_TABLE_SIZE )
    {
        ChessFatal ( "Table index too big in CalcTableIndex" );
        return 0;
    }

    return ti;
}


unsigned CalcTableIndex ( ChessBoard &board )
{
    unsigned whiteKingOffset    = board.GetWhiteKingOffset();
    unsigned whitePieceOffset   = LocateNonKingPiece ( board );
    unsigned blackKingOffset    = board.GetBlackKingOffset();
    return CalcTableIndex (whiteKingOffset, whitePieceOffset, blackKingOffset);
}


unsigned CalcTableIndexBB (
    unsigned whiteKingOffset,
    unsigned blackKingOffset,
    unsigned wbwOffset,
    unsigned wbbOffset )
{
    unsigned ti = (CalcPieceIndex(wbbOffset)/2) +
                  32 * ((CalcPieceIndex(wbwOffset)/2) +
                        32 * (CalcPieceIndex(blackKingOffset) +
                              64 * CalcPieceIndex(whiteKingOffset)));

    if ( ti >= MOVE_TABLE_SIZE_BB )
    {
        ChessFatal ( "Table index too big in CalcTableIndexBB" );
        return 0;
    }

    return ti;
}


unsigned CalcTableIndexBN (
    unsigned whiteKingOffset,
    unsigned blackKingOffset,
    unsigned whiteBishopOffset,
    unsigned whiteKnightOffset )
{
    unsigned ti  =  CalcPieceIndex (whiteKingOffset);
    ti = (64*ti) +  CalcPieceIndex (blackKingOffset);
    ti = (64*ti) +  CalcPieceIndex (whiteKnightOffset);
    ti = (32*ti) + (CalcPieceIndex (whiteBishopOffset) / 2);

    if ( ti >= MOVE_TABLE_SIZE_BN )
    {
        ChessFatal ( "Table index too big in CalcTableIndexBN" );
        return 0;
    }

    return ti;
}


void CalcPiecePositionsBN (     // inverse function of CalcTableIndexBN
    unsigned     ti,
    unsigned    &whiteKingOffset,
    unsigned    &blackKingOffset,
    unsigned    &whiteBishopOffset,
    unsigned    &whiteKnightOffset )
{
    if (ti >= MOVE_TABLE_SIZE_BN)
    {
        ChessFatal ( "Table index too big in CalcTableIndexBN" );
        whiteKingOffset = blackKingOffset = whiteBishopOffset = whiteKnightOffset = 0;
    }
    else
    {
        whiteBishopOffset = CalcPieceOffset (2 * (ti%32));  // always lands on files a,c,e,g
        if (SquareIsBlack (whiteBishopOffset))
        {
            ++whiteBishopOffset;    // correct for odd/even ranks
        }

        ti /= 32;
        whiteKnightOffset = CalcPieceOffset (ti % 64);

        ti /= 64;
        blackKingOffset = CalcPieceOffset (ti % 64);

        ti /= 64;
        whiteKingOffset = CalcPieceOffset (ti);
    }
}



bool LocateBishopAndKnight (ChessBoard &board, unsigned &wbOffset, unsigned &wnOffset);

unsigned CalcTableIndexBN (ChessBoard &board)
{
    unsigned wnOffset, wbOffset;
    if (!LocateBishopAndKnight (board, wbOffset, wnOffset))
    {
        ChessFatal ("Cannot find Bishop and Knight in CalcTableIndexBN");
        return 0;
    }

    return CalcTableIndexBN (
               board.GetWhiteKingOffset(),
               board.GetBlackKingOffset(),
               wbOffset,
               wnOffset
           );
}


inline unsigned CalcTableIndexBB (
    ChessBoard  &board,
    unsigned    wbwOffset,
    unsigned    wbbOffset )
{
    return CalcTableIndexBB (
               board.GetWhiteKingOffset(),
               board.GetBlackKingOffset(),
               wbwOffset,
               wbbOffset );
}


unsigned CalcTableIndexBB (
    ChessBoard  &board )
{
    unsigned wbwOffset=0, wbbOffset=0;
    if ( !LocateBishops(board,wbwOffset,wbbOffset) )
    {
        ChessFatal ( "Bishops messed up in CalcTableIndexBB" );
        return 0;
    }

    return CalcTableIndexBB ( board, wbwOffset, wbbOffset );
}


//--------------------------------------------------------------------------

int Symmetry (int n, int ofs)
{
    // The value of n is treated as 0..7.  Only the 3 lowest bits are used.
    // All other bits in n are ignored.
    // The lowest  bit (1) selects whether x is replaced with 11-x.
    // The middle  bit (2) selects whether y is replaced with 11-y.
    // The highest bit (4) selects whether the coordinates <x,y> are swapped to <y,x>.

    int x = XPART(ofs);
    int y = YPART(ofs);

    if (n & 1)
    {
        x = 11 - x;
    }

    if (n & 2)
    {
        y = 11 - y;
    }

    if (n & 4)
    {
        int t = x;
        x = y;
        y = t;
    }

    return OFFSET(x,y);
}


int AntiSymmetry (int n, int ofs)
{
    int x = XPART(ofs);
    int y = YPART(ofs);

    if (n & 4)
    {
        int t = x;
        x = y;
        y = t;
    }

    if (n & 2)
    {
        y = 11 - y;
    }

    if (n & 1)
    {
        x = 11 - x;
    }

    return OFFSET(x,y);
}


bool SymmetryPreservesSquareColor (int n)
{
    /*
        n           SymmetryPreservesSquareColor(n)
        000 (0)     true
        001 (1)     false
        010 (2)     false
        011 (3)     true
        100 (4)     true
        101 (5)     false
        110 (6)     false
        111 (7)     true
    */

    return ((n ^ (n >> 1)) & 1) == 0;
}

// The following are symbolic names for values of 'n'
// that you can pass to Symmetry/AntiSymmetry:

#define SYMMETRY_IDENTITY           0
#define SYMMETRY_LEFT_RIGHT_MIRROR  1
#define SYMMETRY_TOP_BOTTOM_MIRROR  2
#define SYMMETRY_ROTATE_180         3
#define SYMMETRY_FLIP_SLASH         4
#define SYMMETRY_CLOCKWISE          5
#define SYMMETRY_COUNTERCLOCKWISE   6
#define SYMMETRY_FLIP_BACKSLASH     7

#define LeftRightMirror(ofs)    Symmetry(SYMMETRY_LEFT_RIGHT_MIRROR,(ofs))

const int PRESERVE_SQUARE_COLOR [4] =
{
    SYMMETRY_IDENTITY,
    SYMMETRY_ROTATE_180,
    SYMMETRY_FLIP_SLASH,
    SYMMETRY_FLIP_BACKSLASH
};

//--------------------------------------------------------------------------

unsigned SymmetryBN (unsigned wkOffset, unsigned bkOffset, unsigned wbOffset, unsigned wnOffset, int sym)
{
    wkOffset = Symmetry (sym, wkOffset);
    bkOffset = Symmetry (sym, bkOffset);
    wbOffset = Symmetry (sym, wbOffset);
    wnOffset = Symmetry (sym, wnOffset);

    return CalcTableIndexBN (wkOffset, bkOffset, wbOffset, wnOffset);
}


unsigned SymmetryBN (unsigned ti, int sym)
{
    unsigned    wkOffset, bkOffset, wbOffset, wnOffset;
    CalcPiecePositionsBN (ti, wkOffset, bkOffset, wbOffset, wnOffset);
    return SymmetryBN (wkOffset, bkOffset, wbOffset, wnOffset, sym);
}


unsigned FindEquivalentPositionBN (unsigned ti, int &sym)
{
    unsigned mi = ti;   // minimum equivalent index
    sym = PRESERVE_SQUARE_COLOR[0];

    unsigned    wkOffset, bkOffset, wbOffset, wnOffset;
    CalcPiecePositionsBN (ti, wkOffset, bkOffset, wbOffset, wnOffset);

    for (int n=1; n<=3; ++n)
    {
        unsigned index = SymmetryBN (wkOffset, bkOffset, wbOffset, wnOffset, PRESERVE_SQUARE_COLOR[n]);
        if (index < mi)
        {
            mi  = index;
            sym = PRESERVE_SQUARE_COLOR[n];
        }
    }

    return mi;
}


void AnalyzeSymmetriesBN (
    FILE        *outfile,
    const Move  *table )
{
    int identical = 0;
    int sym;

    for (unsigned ti=0; ti < MOVE_TABLE_SIZE_BN; ++ti)
    {
        unsigned mi = FindEquivalentPositionBN (ti, sym);
        if (mi == ti)
        {
            ++identical;
        }
        else
        {
            if (mi >= MOVE_TABLE_SIZE_BN)
            {
                fprintf (outfile, "!!! ti=%d, mi=%d, MOVE_TABLE_SIZE_BN=%d\n", ti, mi, MOVE_TABLE_SIZE_BN);
                break;
            }

            if (table[ti].score != table[mi].score)
            {
                fprintf (outfile, "!!! ti=%d, mi=%d, sym=%d, score[ti]=%d, score[mi]=%d\n", ti, mi, sym, table[ti].score, table[mi].score);
                unsigned    wkOffset, bkOffset, wbOffset, wnOffset;
                CalcPiecePositionsBN (ti, wkOffset, bkOffset, wbOffset, wnOffset);
                fprintf (outfile, "raw coords:  wk=%3d, bk=%3d, wb=%3d, wn=%3d\n", wkOffset, bkOffset, wbOffset, wnOffset);

                CalcPiecePositionsBN (mi, wkOffset, bkOffset, wbOffset, wnOffset);
                fprintf (outfile, "sym coords:  wk=%3d, bk=%3d, wb=%3d, wn=%3d\n", wkOffset, bkOffset, wbOffset, wnOffset);
                break;
            }
        }
    }

    fprintf (outfile, "%d out of %d indexes are already minimal.\n", identical, MOVE_TABLE_SIZE_BN);
}

//--------------------------------------------------------------------------

void AnalyzeSymmetriesBN (
    const char *inDatabaseFileName,
    const char *outTextFileName )
{
    FILE *outfile = fopen (outTextFileName, "wt");
    if (outfile)
    {
        FILE *infile = fopen (inDatabaseFileName, "rb");
        if (infile)
        {
            Move *table = new Move [MOVE_TABLE_SIZE_BN];
            if (table)
            {
                if (1 == fread (table, sizeof(Move)*MOVE_TABLE_SIZE_BN, 1, infile))
                {
                    AnalyzeSymmetriesBN (outfile, table);
                }
                else
                {
                    fprintf (outfile, "Error reading table from file '%s'\n", inDatabaseFileName);
                }

                delete[] table;
                table = NULL;
            }
            else
            {
                fprintf (outfile, "Out of memory!\n");
            }

            fclose (infile);
            infile = NULL;
        }
        else
        {
            fprintf (outfile, "Cannot open database file '%s'\n", inDatabaseFileName);
        }

        fclose (outfile);
        outfile = NULL;
    }
}

//--------------------------------------------------------------------------
// 'GenerateEndgameDatabases' is the main entry point for this module.

void GenerateEndgameDatabases (
    ChessBoard  &board,
    ChessUI     &ui )
{
    switch (Global_EndgameDatabaseMode)
    {
    case 0:     // generate endgame databases
    {
#ifdef _WIN32
        // Set thread priority to IDLE...

        HANDLE hProcess = GetCurrentProcess();
        SetPriorityClass(hProcess,IDLE_PRIORITY_CLASS);
#endif
        INT32  timeBefore = ChessTime();
        ui.SetAdHocText ( 3, "Starting endgame database calculation..." );

        GenerateEndgameDatabase ( EDB_ROOK_FILENAME, WROOK, board, ui );
        GenerateEndgameDatabase ( EDB_QUEEN_FILENAME, WQUEEN, board, ui );

        // IMPORTANT:  To generate the pawn database, the rook and queen
        // databases must already be finished.  This is because the database
        // generator needs to consult the rook and/or queen databases when
        // a pawn is promoted, in order to tell how many moves it will mate in.
        GenerateEndgameDatabase ( EDB_PAWN_FILENAME, WPAWN, board, ui );

        GenerateEndgameDatabaseBB ( EDB_BB_FILENAME, board, ui );
        GenerateEndgameDatabaseBN ( EDB_BN_FILENAME, board, ui );

        board.Init();   // signal that endgame stuff is done
        ui.DrawBoard (board);

        UINT32  timeAfter = ChessTime();
        double minutes = double(timeAfter - timeBefore) / 6000.0;
        ui.SetAdHocText ( 3, "Done calculating endgame databases (%0.3lf minutes)", minutes );
        break;
    }

    case 1:         // dump endgame databases
    {
        DumpEndgameDatabaseBN (EDB_BN_FILENAME, "bnedb.txt");
        break;
    }

    case 2:         // study symmetries
    {
        AnalyzeSymmetriesBN (EDB_BN_FILENAME, "bnsym.txt");
        break;
    }
    }
}


//--------------------------------------------------------------------------


void GenerateEndgameDatabase (
    const char *filename,
    SQUARE whitePiece,
    ChessBoard &board,
    ChessUI &ui )
{
    // Do not generate the database if it is already complete...
    Move tempMove;
    if ( FetchMoveFromDatabase(filename,MOVE_TABLE_SIZE-1,tempMove) )
        return;

    // Generate the database assuming Black has only a king
    // and White has only a king and whatever is in 'whitePiece'.
    // In an actual game, if we need to use the database from
    // Black's point of view, we should rotate coordinates in case
    // the extra piece is actually a black pawn (moving the other way).

    // Make sure we can allocate the necessary memory...
    Move *whiteTable = new Move [MOVE_TABLE_SIZE];
    Move *blackTable = new Move [MOVE_TABLE_SIZE];
    FILE *outfile = 0;
    bool firstTimeDisplay = true;
    INT32 lastDisplayTime = ChessTime();
    if ( whiteTable && blackTable )
    {
        // Make sure we can open the output file...
        outfile = fopen ( filename, "wb" );
        if ( outfile )
        {
            memset ( whiteTable, 0, sizeof(Move) * MOVE_TABLE_SIZE );
            memset ( blackTable, 0, sizeof(Move) * MOVE_TABLE_SIZE );

            ui.SetAdHocText ( 3, "Creating endgame database '%s'", filename );

            // Here's where it gets interesting!

            board.Init();   // make sure it is White's turn to move
            board.ClearEverythingButKings();
            SCORE score = NEGINF;
            unsigned prevNumWinningPositions = 0;
            unsigned numLegalPositions = 0;
            unsigned numWinningPositions = 0;
            unsigned passCounter = 0;
            unsigned incrementalWins = 0;
            for(;;)
            {
                prevNumWinningPositions = numWinningPositions;
                numWinningPositions = 0;
                incrementalWins = 0;
                for ( unsigned wkIndex=0; wkIndex<64; ++wkIndex )
                {
                    unsigned wkOffset = CalcPieceOffset (wkIndex);
                    board.SetOffsetContents ( WKING, wkOffset, true );
                    unsigned lastDisplayedBkOffset = 0;
                    for ( unsigned bkIndex=0; bkIndex<64; ++bkIndex )
                    {
                        if ( bkIndex != wkIndex )
                        {
                            unsigned bkOffset = CalcPieceOffset (bkIndex);
                            board.SetOffsetContents ( BKING, bkOffset, true );
                            if ( EGDB_IsLegal(board) )  // optimization
                            {
                                for ( unsigned wpIndex=0; wpIndex<64; ++wpIndex )
                                {
                                    if ( whitePiece != WPAWN || (wpIndex/8>0 && wpIndex/8<7) )
                                    {
                                        if ( wpIndex!=wkIndex && wpIndex!=bkIndex )
                                        {
                                            unsigned wpOffset = CalcPieceOffset (wpIndex);
                                            board.SetOffsetContents ( whitePiece, wpOffset, true );
                                            if ( EGDB_IsLegal(board) )
                                            {
                                                if ( LocateNonKingPiece(board) == 0 )
                                                {
                                                    ChessFatal ( "Non-king piece missing in EGDB_IsLegal()" );
                                                }

                                                if ( passCounter == 0 )
                                                    ++numLegalPositions;

                                                SCORE prevScore = whiteTable[CalcTableIndex(board)].score;
                                                score = EGDB_WhiteSearch ( board, 0, whiteTable, blackTable, ui, whitePiece );
                                                if ( score >= FORCED_WIN )
                                                {
                                                    ++numWinningPositions;
                                                    if ( prevScore != score )
                                                    {
                                                        ++incrementalWins;
                                                        if ( lastDisplayedBkOffset!=bkOffset )
                                                        {
                                                            INT32 currentTime = ChessTime();
                                                            if ( currentTime - lastDisplayTime > 50 )
                                                            {
                                                                lastDisplayedBkOffset = bkOffset;
                                                                lastDisplayTime = currentTime;
                                                                BestPath bp;
                                                                CalcEndgameBestPath ( board, bp, whiteTable, blackTable, CalcTableIndex );
                                                                ui.DisplayBestPath (board, bp);
                                                                ui.DrawBoard(board);
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                            board.SetOffsetContents ( EMPTY, wpOffset, true );
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                ++passCounter;

                ui.SetAdHocText ( 3, "%s pass %u: found %u legal, %u new wins",
                                  filename,
                                  passCounter,
                                  numLegalPositions,
                                  incrementalWins );

                if ( incrementalWins == 0 )
                    break;
            }

            // When we get here, we are done, so write the white table to disk.

            unsigned numWritten = fwrite (
                                      whiteTable, sizeof(Move), MOVE_TABLE_SIZE, outfile );

            if ( numWritten != MOVE_TABLE_SIZE )
            {
                ui.SetAdHocText ( 3, "egdb: Cannot write move table to '%s'", filename );
            }
        }
        else
        {
            ui.SetAdHocText ( 3, "egdb: Cannot open '%s' for write", filename );
        }
    }
    else
    {
        ui.SetAdHocText ( 3, "egdb: Not enough memory!" );
    }

    if ( outfile )
    {
        fclose (outfile);
        outfile = 0;
    }

    if ( whiteTable )
    {
        delete[] whiteTable;
        whiteTable = 0;
    }

    if ( blackTable )
    {
        delete[] blackTable;
        blackTable = 0;
    }
}


//--------------------------------------------------------------------------

bool EGDB_IsLegal ( ChessBoard &board )
{
    board.Update();
    if ( !board.WhiteToMove() )
    {
        ChessFatal ( "Not White's turn to move in EGDB_IsLegal()" );
        return false;  // this should never happen
    }

    unsigned wk = board.GetWhiteKingOffset();
    if ( board.GetSquareContents(wk) != WKING )
    {
        ChessFatal ( "White king missing/misplaced in EGDB_IsLegal()" );
        return false;
    }

    unsigned wkx = XPART(wk);
    unsigned wky = YPART(wk);

    unsigned bk = board.GetBlackKingOffset();
    if ( board.GetSquareContents(bk) != BKING )
    {
        ChessFatal ( "Black king missing/misplaced in EGDB_IsLegal()" );
        return false;
    }
    unsigned bkx = XPART(bk);
    unsigned bky = YPART(bk);

    int dx = int(bkx) - int(wkx);
    if ( dx < 0 )  dx = -dx;

    int dy = int(bky) - int(wky);
    if ( dy < 0 )  dy = -dy;

    if ( dx<2 && dy<2 )
        return false;  // kings are touching

    if ( board.IsAttackedByWhite(bk) )
        return false;  // it's White's turn, so Black cannot be in check

    return true;
}


//--------------------------------------------------------------------------

SCORE EGDB_WhiteSearch (
    ChessBoard  &board,
    int         depth,
    Move        *whiteTable,
    Move        *blackTable,
    ChessUI     &ui,
    SQUARE      whitePiece )
{
    unsigned whiteKingOffset  = board.GetWhiteKingOffset();
    unsigned whitePieceOffset = LocateNonKingPiece (board);
    unsigned blackKingOffset  = board.GetBlackKingOffset();

    if ( whitePieceOffset == 0 )
    {
        return 0;   // Black captured White's only means of winning, so it's a draw.
    }

    unsigned ti = CalcTableIndex (
                      whiteKingOffset,
                      whitePieceOffset,
                      blackKingOffset );

    if ( whiteTable[ti].source != 0 )
    {
        // This position has already been fully evaluated
        return whiteTable[ti].score;
    }

    // See if game is over...
    MoveList ml;

    bool whiteCanMove = false;
    if ( depth == 0 )
    {
        // We are going to do a full search anyway, so might as
        // well generate the move list to determine whether there are any.
        board.GenWhiteMoves ( ml );
        whiteCanMove = (ml.num > 0);
    }
    else
    {
        whiteCanMove = board.WhiteCanMove();
    }

    if ( !whiteCanMove )
    {
        // The game is over.  Must be a stalemate because Black has only a king.
        whiteTable[ti].source = 2;
        whiteTable[ti].score = 0;
        return 0;
    }

    SCORE bestscore = 1000;

    if ( depth == 0 )
    {
        bestscore = NEGINF;
        Move *move = &ml.m[0];
        UnmoveInfo unmove;
        Move *bestmove = 0;
        for ( int i=0; i<ml.num; ++i, ++move )
        {
            board.MakeMove ( *move, unmove );
            SCORE score = EGDB_BlackSearch ( board, 1+depth, whiteTable, blackTable, ui, whitePiece );
            board.UnmakeMove ( *move, unmove );

            if ( score > 0 )
            {
                if ( score >= WON_FOR_WHITE )
                    score -= WIN_DELAY_PENALTY;
                else
                    --score;
            }

            move->score = score;

            if ( score > bestscore )
            {
                bestscore = score;
                bestmove = move;
            }
        }

        if ( bestscore >= FORCED_WIN )
            whiteTable[ti] = *bestmove;
    }
    else if ( whitePiece == WPAWN )
    {
        SQUARE promPiece = board.GetSquareContents(whitePieceOffset);
        Move move;
        bool foundit = false;
        if ( promPiece & WQ_MASK )
            foundit = ConsultEndgameDatabase ( board, move, EDB_QUEEN_FILENAME );
        else if ( promPiece & WR_MASK )
            foundit = ConsultEndgameDatabase ( board, move, EDB_ROOK_FILENAME );

        if ( foundit )
            bestscore = move.score;
    }

    return bestscore;
}

//--------------------------------------------------------------------------

SCORE EGDB_BlackSearch (
    ChessBoard  &board,
    int         depth,
    Move        *whiteTable,
    Move        *blackTable,
    ChessUI     &ui,
    SQUARE      whitePiece )
{
    unsigned whiteKingOffset  = board.GetWhiteKingOffset();
    unsigned whitePieceOffset = LocateNonKingPiece (board);
    unsigned blackKingOffset  = board.GetBlackKingOffset();

    if ( whitePieceOffset == 0 )
    {
        return 0;   // black must have captured White's only means of winning
    }

    unsigned ti = CalcTableIndex (
                      whiteKingOffset,
                      whitePieceOffset,
                      blackKingOffset );

    // Before doing anything else, see if this position has already been handled...
    if ( blackTable[ti].source != 0 )
    {
        // Getting here means the position has already been fully evaluated
        return blackTable[ti].score;
    }

    // See if game is over...
    MoveList ml;
    board.GenBlackMoves (ml);
    if ( ml.num == 0 )
    {
        // The game is over.  See if this is a checkmate or a stalemate.
        if ( board.BlackInCheck() )
        {
            blackTable[ti].source = 4;
            blackTable[ti].score = WHITE_WINS;
            return WHITE_WINS;
        }

        blackTable[ti].source = 2;
        blackTable[ti].score = 0;
        return 0;   // stalemate
    }

    SCORE bestscore = POSINF;
    Move *move = &ml.m[0];
    UnmoveInfo unmove;
    Move *bestmove = 0;
    for ( int i=0; i<ml.num; ++i, ++move )
    {
        board.MakeMove ( *move, unmove );
        SCORE score = EGDB_WhiteSearch ( board, 1+depth, whiteTable, blackTable, ui, whitePiece );
        board.UnmakeMove ( *move, unmove );

        if ( score > 0 )
        {
            if ( score >= WON_FOR_WHITE )
                score -= WIN_DELAY_PENALTY;
            else
                --score;
        }

        move->score = score;

        if ( score < bestscore )
        {
            bestscore = score;
            bestmove = move;
            if (score < FORCED_WIN)     // A kind of pruning:  no need to continue if we won't store this result!
            {
                break;
            }
        }
    }

    if ( bestscore >= FORCED_WIN )
        blackTable[ti] = *bestmove;

    return bestscore;
}


bool FetchMoveFromDatabase (
    const char  *databaseFilename,
    unsigned    tableIndex,
    Move        &move )
{
    FILE *database = fopen ( databaseFilename, "rb" );
    bool gotmove = false;
    if ( database )
    {
        if ( fseek ( database, tableIndex * sizeof(Move), SEEK_SET ) == 0 )
        {
            if ( fread ( &move, sizeof(Move), 1, database ) == 1 )
                gotmove = true;
        }

        fclose (database);
        database = 0;
    }

    return gotmove;
}

//--------------------------------------------------------------------------

bool ConsultEndgameDatabaseBN (
    ChessBoard  &board,
    Move        &move )
{
    bool moveIsValid = false;

    unsigned winnerBishopOffset, winnerKnightOffset;
    if (!LocateBishopAndKnight (board, winnerBishopOffset, winnerKnightOffset))
    {
        return false;
    }

    unsigned winnerKingOffset, loserKingOffset;
    if (board.BlackToMove())
    {
        // Black is winning, so his king is the winner king, and White's is the loser king
        winnerKingOffset = board.GetBlackKingOffset();
        loserKingOffset  = board.GetWhiteKingOffset();
    }
    else
    {
        // White is winning
        winnerKingOffset = board.GetWhiteKingOffset();
        loserKingOffset  = board.GetBlackKingOffset();
    }

    // The database always assumes it is on a white square, so if it's actually
    // the opposite, we need to mirror image the board left/right.
    // The rotation trick of (ofs=143-ofs) will NOT work, because that preserves square color!

    bool flip = SquareIsBlack (winnerBishopOffset);
    if (flip)
    {
        winnerBishopOffset = LeftRightMirror (winnerBishopOffset);
        winnerKnightOffset = LeftRightMirror (winnerKnightOffset);
        winnerKingOffset   = LeftRightMirror (winnerKingOffset);
        loserKingOffset    = LeftRightMirror (loserKingOffset);
    }

    unsigned ti = CalcTableIndexBN (
                      winnerKingOffset,
                      loserKingOffset,
                      winnerBishopOffset,
                      winnerKnightOffset
                  );

    if (FetchMoveFromDatabase (EDB_BN_FILENAME, ti, move))
    {
        if (move.source != 0)
        {
            move.source &= BOARD_OFFSET_MASK;
            if (flip)
            {
                move.source = LeftRightMirror (move.source);
                move.dest   = LeftRightMirror (move.dest);
            }
            MoveList ml;
            board.GenMoves (ml);
            if (ml.IsLegal (move))
            {
                moveIsValid = true;
                if (board.BlackToMove())
                {
                    move.score = -move.score;
                }
            }
        }
    }

    return moveIsValid;
}

//--------------------------------------------------------------------------


bool ConsultEndgameDatabaseBB (
    ChessBoard  &board,
    Move        &move )
{
    const bool blackToMove = board.BlackToMove();
    bool moveIsValid = false;

    unsigned winnerKingOffset = 0;
    unsigned loserKingOffset = 0;
    unsigned wbwOffset=0, wbbOffset=0;
    if ( !LocateBishops(board,wbwOffset,wbbOffset) )
        return false;

    if ( blackToMove )
    {
        winnerKingOffset    = 143 - board.GetBlackKingOffset();
        wbwOffset           = 143 - wbwOffset;
        wbbOffset           = 143 - wbbOffset;
        loserKingOffset     = 143 - board.GetWhiteKingOffset();
    }
    else
    {
        winnerKingOffset    = board.GetWhiteKingOffset();
        loserKingOffset     = board.GetBlackKingOffset();
    }

    unsigned ti = CalcTableIndexBB (
                      winnerKingOffset,
                      loserKingOffset,
                      wbwOffset,
                      wbbOffset );

    if ( FetchMoveFromDatabase(EDB_BB_FILENAME,ti,move) )
    {
        if ( move.source != 0 )
        {
            move.source &= BOARD_OFFSET_MASK;
            if ( blackToMove )
            {
                move.source = 143 - move.source;
                move.dest   = 143 - move.dest;
            }

            MoveList ml;
            board.GenMoves (ml);
            if ( ml.IsLegal(move) )
            {
                moveIsValid = true;
                if ( blackToMove )
                    move.score = -move.score;
            }
        }
    }

    return moveIsValid;
}


//--------------------------------------------------------------------------

bool ConsultEndgameDatabase (
    ChessBoard &board,
    Move &move,
    const char *databaseFilename )
{
    // Getting into this function means that the board
    // is appropriate for the given database filename.

    const bool blackToMove = board.BlackToMove();
    bool moveIsValid = false;

    unsigned winnerKingOffset = 0;
    unsigned winnerPieceOffset = 0;
    unsigned loserKingOffset = 0;
    if ( blackToMove )
    {
        winnerKingOffset    = 143 - board.GetBlackKingOffset();
        winnerPieceOffset   = 143 - LocateNonKingPiece(board);
        loserKingOffset     = 143 - board.GetWhiteKingOffset();
    }
    else
    {
        winnerKingOffset    = board.GetWhiteKingOffset();
        winnerPieceOffset   = LocateNonKingPiece(board);
        loserKingOffset     = board.GetBlackKingOffset();
    }

    unsigned ti = CalcTableIndex (
                      winnerKingOffset,
                      winnerPieceOffset,
                      loserKingOffset );

    if ( FetchMoveFromDatabase(databaseFilename,ti,move) )
    {
        if ( move.source != 0 )
        {
            bool giveup = false;
            move.source &= BOARD_OFFSET_MASK;
            if ( blackToMove )
            {
                move.source = 143 - move.source;
                if ( move.dest & 0x80 )
                {
                    // Handling pawn promotion for Black is special.
                    // We have to calculate the rotated destination square.
                    switch ( move.dest & 0xf0 )
                    {
                    case SPECIAL_MOVE_PROMOTE_NORM:
                        // do nothing
                        break;

                    case SPECIAL_MOVE_PROMOTE_CAP_EAST:
                        move.dest = (move.dest & PIECE_MASK) | SPECIAL_MOVE_PROMOTE_CAP_WEST;   // change east to west
                        break;

                    case SPECIAL_MOVE_PROMOTE_CAP_WEST:
                        move.dest = (move.dest & PIECE_MASK) | SPECIAL_MOVE_PROMOTE_CAP_EAST;   // change west to east
                        break;

                    default:
                        giveup = true;     // must be en passant or something?  but that should not be in the database!
                        break;
                    }
                }
                else
                {
                    move.dest   = 143 - move.dest;
                }
            }

            if ( !giveup )
            {
                MoveList ml;
                board.GenMoves (ml);
                if ( ml.IsLegal(move) )
                {
                    moveIsValid = true;
                    if ( blackToMove )
                        move.score = -move.score;
                }
            }
        }
    }

    return moveIsValid;
}

//--------------------------------------------------------------------------

// The following function is for consulting the endgame databases
// during appropriate circumstances.

bool  ComputerChessPlayer::WhiteDbEndgame (
    ChessBoard &board,
    Move &move )
{
    // See if this is one of the circumstances we know about...

    const INT16 *inv = board.inventory;
    if (inv[BP_INDEX]==0 && inv[BN_INDEX]==0 && inv[BB_INDEX]==0 && inv[BR_INDEX]==0 && inv[BQ_INDEX]==0)
    {
        // Getting here means that Black has only his king left.

        if (inv[WB_INDEX]==0 && inv[WN_INDEX]==0)
        {
            // White has no bishops or knights

            if (inv[WQ_INDEX] == 1)
            {
                if (inv[WR_INDEX]==0 && inv[WP_INDEX]==0)
                {
                    return ConsultEndgameDatabase ( board, move, EDB_QUEEN_FILENAME );
                }
            }
            else if (inv[WR_INDEX] == 1)
            {
                if (inv[WQ_INDEX]==0 && inv[WP_INDEX]==0)
                {
                    return ConsultEndgameDatabase ( board, move, EDB_ROOK_FILENAME );
                }
            }
            else if (inv[WP_INDEX] == 1)
            {
                if (inv[WR_INDEX]==0 && inv[WQ_INDEX]==0)
                {
                    return ConsultEndgameDatabase ( board, move, EDB_PAWN_FILENAME );
                }
            }
        }
        else
        {
            // White has bishops and/or knights

            if (inv[WP_INDEX]==0 && inv[WR_INDEX]==0 && inv[WQ_INDEX]==0)
            {
                // White has ONLY bishops and/or knights

                if (inv[WB_INDEX]==2 && inv[WN_INDEX]==0)
                {
                    return ConsultEndgameDatabaseBB ( board, move );    // White has only 2 bishops
                }
                else if (inv[WB_INDEX]==1 && inv[WN_INDEX]==1)
                {
                    return ConsultEndgameDatabaseBN ( board, move );    // White has only 1 bishop and 1 knight
                }
            }
        }
    }

    return false;
}


bool  ComputerChessPlayer::BlackDbEndgame (
    ChessBoard &board,
    Move &move )
{
    // See if this is one of the circumstances we know about...

    const INT16 *inv = board.inventory;
    if (inv[WP_INDEX]==0 && inv[WN_INDEX]==0 && inv[WB_INDEX]==0 && inv[WR_INDEX]==0 && inv[WQ_INDEX]==0)
    {
        // Getting here means that White has only his king left.

        if (inv[BB_INDEX]==0 && inv[BN_INDEX]==0)
        {
            // Black has no bishops or knights

            if (inv[BQ_INDEX] == 1)
            {
                if (inv[BR_INDEX]==0 && inv[BP_INDEX]==0)
                {
                    return ConsultEndgameDatabase ( board, move, EDB_QUEEN_FILENAME );
                }
            }
            else if (inv[BR_INDEX] == 1)
            {
                if (inv[BQ_INDEX]==0 && inv[BP_INDEX]==0)
                {
                    return ConsultEndgameDatabase ( board, move, EDB_ROOK_FILENAME );
                }
            }
            else if (inv[BP_INDEX] == 1)
            {
                if (inv[BR_INDEX]==0 && inv[BQ_INDEX]==0)
                {
                    return ConsultEndgameDatabase ( board, move, EDB_PAWN_FILENAME );
                }
            }
        }
        else
        {
            // Black has bishops and/or knights

            if (inv[BP_INDEX]==0 && inv[BR_INDEX]==0 && inv[BQ_INDEX]==0)
            {
                // Black has ONLY bishops and/or knights

                if (inv[BB_INDEX]==2 && inv[BN_INDEX]==0)
                {
                    return ConsultEndgameDatabaseBB ( board, move );    // Black has only 2 bishops
                }
                else if (inv[BB_INDEX]==1 && inv[BN_INDEX]==1)
                {
                    return ConsultEndgameDatabaseBN ( board, move );    // Black has only 1 bishop and 1 knight
                }
            }
        }
    }

    return false;
}


//--------------------------------------------------------------------------

void CalcEndgameBestPath (
    ChessBoard          &board,
    BestPath            &bp,
    const Move          *whiteTable,
    const Move          *blackTable,
    TABLE_INDEX_FUNC    tableIndexFunc )
{
    UnmoveInfo unmove [MAX_BESTPATH_DEPTH];

    bp.depth = 0;
    int i;
    for ( i=0; i<MAX_BESTPATH_DEPTH; ++i )
    {
        unsigned ti = (*tableIndexFunc)(board);
        const Move *table = board.WhiteToMove() ? whiteTable : blackTable;
        if ( table[ti].source < OFFSET(2,2) )
            break;

        if ( !board.isLegal(table[ti]) )
        {
            ChessFatal ("bogus move in CalcEndgameBestPath");
            break;
        }

        bp.depth = 1 + i;
        bp.m[i] = table[ti];
        board.MakeMove ( bp.m[i], unmove[i] );

        /*!!!*/
        if ( i==0 &&
                bp.m[0].score == 29990 &&
                !(!board.BlackCanMove() && board.BlackInCheck()) )
        {
            ChessFatal ("bogus checkmate score");
        }
        /*!!!*/
    }

    for ( i=bp.depth-1; i >= 0; --i )
    {
        board.UnmakeMove ( bp.m[i], unmove[i] );
    }
}


//--------------------------------------------------------------------------

char *GetAlgebraic (char *buffer, int offset)
{
    buffer[0] = XPART(offset) - 2 + 'a';
    buffer[1] = YPART(offset) - 2 + '1';
    buffer[2] = '\0';
    return buffer;
}

//--------------------------------------------------------------------------


void DumpEndgameDatabaseBN (
    const char          *inDatabaseFileName,
    const char          *outTextFileName )
{
    unsigned    wkOffset, bkOffset, wbOffset, wnOffset;
    char        wk[3], bk[3], wb[3], wn[3];     // algebraic coordinate strings
    char        ssource[3], sdest[3];

    FILE *outfile = fopen (outTextFileName, "wt");
    if (outfile)
    {
        // First make sure the entire file appears to be there...
        Move    move;
        if (!FetchMoveFromDatabase (inDatabaseFileName, MOVE_TABLE_SIZE_BN-1, move))
        {
            fprintf (outfile, "Database file '%s' is missing or truncated.\n", inDatabaseFileName);
        }
        else
        {
            FILE *dbfile = fopen (inDatabaseFileName, "rb");
            if (dbfile)
            {
                // The database file is just an array of Move structures.
                // The position is encoded implicitly as the offset in the file.
                // We will iterate through all positions, impossible or not.

                for (int ti=0; ti < MOVE_TABLE_SIZE_BN; ++ti)
                {
                    if (1 == fread (&move, sizeof(move), 1, dbfile))
                    {
                        move.source &= BOARD_OFFSET_MASK;
                        if (move.source >= OFFSET(2,2))
                        {
                            int mate_in;
                            if (move.score >= FORCED_WIN)
                            {
                                mate_in = ((WHITE_WINS - move.score) / WIN_DELAY_PENALTY) / 2;
                            }
                            else
                            {
                                mate_in = -1;   // should never happen
                            }

                            CalcPiecePositionsBN (ti, wkOffset, bkOffset, wbOffset, wnOffset);
                            fprintf (
                                outfile,
                                "%s %s %s %s %s%s %02d\n",
                                GetAlgebraic (wk, wkOffset),
                                GetAlgebraic (bk, bkOffset),
                                GetAlgebraic (wb, wbOffset),
                                GetAlgebraic (wn, wnOffset),
                                GetAlgebraic (ssource, move.source),
                                GetAlgebraic (sdest, move.dest),
                                mate_in
                            );
                        }
                    }
                    else
                    {
                        fprintf (outfile, "Failure to read at table index %d\n", ti);
                        break;
                    }
                }

                fclose (dbfile);
                dbfile = NULL;
            }
        }

        fclose (outfile);
        outfile = NULL;
    }
}

//--------------------------------------------------------------------------
// GenerateEndgameDatabaseBN generates a 32-MB endgame database
// for a king, bishop, and knight vs a lone king.
// In the database, the bishop is assumed to be on a white square.
// If the bishop is actually on a black square, we flip the board
// to toggle the square color, do the lookup, then flip the resulting coords.

void GenerateEndgameDatabaseBN (
    const char  *filename,
    ChessBoard  &board,
    ChessUI     &ui )
{
    // Do not generate the database if it is already complete...
    Move tempMove;
    if (FetchMoveFromDatabase (filename, MOVE_TABLE_SIZE_BN-1, tempMove))
    {
        return;
    }

    // Make sure we can allocate the necessary memory... and it's a LOT!
    Move *whiteTable = new Move [MOVE_TABLE_SIZE_BN];
    Move *blackTable = new Move [MOVE_TABLE_SIZE_BN];
    bool firstTimeDisplay = true;
    INT32 lastDisplayTime = ChessTime();
    if (whiteTable && blackTable)
    {
        FILE *outfile = fopen (filename, "wb");
        if (outfile)
        {
            memset (whiteTable, 0, sizeof(Move) * MOVE_TABLE_SIZE_BN);
            memset (blackTable, 0, sizeof(Move) * MOVE_TABLE_SIZE_BN);
            ui.SetAdHocText (3, "Creating endgame database '%s'", filename);

            board.Init();   // make sure it is White's turn to move
            board.ClearEverythingButKings();
            SCORE score = NEGINF;
            unsigned prevNumWinningPositions = 0;
            unsigned numLegalPositions = 0;
            unsigned numWinningPositions = 0;
            unsigned passCounter = 0;
            unsigned incrementalWins = 0;
            do
            {
                prevNumWinningPositions = numWinningPositions;
                numWinningPositions = 0;
                incrementalWins = 0;
                for (unsigned wkIndex=0; wkIndex<64; ++wkIndex)
                {
                    unsigned wkOffset = CalcPieceOffset (wkIndex);
                    board.SetOffsetContents ( WKING, wkOffset, true );
                    unsigned lastDisplayedBkOffset = 0;
                    for (unsigned bkIndex=0; bkIndex<64; ++bkIndex)
                    {
                        if (bkIndex != wkIndex)
                        {
                            unsigned bkOffset = CalcPieceOffset (bkIndex);
                            board.SetOffsetContents ( BKING, bkOffset, true );
                            if (EGDB_IsLegal(board))    // optimization
                            {
                                for (unsigned wbIndex=0; wbIndex<32; ++wbIndex)
                                {
                                    unsigned wbOffset = CalcBishopWhiteOffset(wbIndex);
                                    if (wbOffset!=wkOffset && wbOffset!=bkOffset)
                                    {
                                        board.SetOffsetContents (WBISHOP, wbOffset, true);
                                        for (unsigned wnIndex=0; wnIndex<64; ++wnIndex)
                                        {
                                            unsigned wnOffset = CalcPieceOffset(wnIndex);
                                            if (wnOffset!=wkOffset && wnOffset!=bkOffset && wnOffset!=wbOffset)
                                            {
                                                board.SetOffsetContents (WKNIGHT, wnOffset, true);
                                                if (EGDB_IsLegal(board))
                                                {
                                                    if (passCounter == 0)
                                                    {
                                                        ++numLegalPositions;
                                                    }

                                                    int ti = CalcTableIndexBN (wkOffset, bkOffset, wbOffset, wnOffset);
                                                    SCORE prevScore = whiteTable[ti].score;
                                                    score = EGDB_WhiteSearchBN (board, 0, passCounter*2, whiteTable, blackTable, ui, wbOffset, wnOffset);
                                                    if (score >= FORCED_WIN)
                                                    {
                                                        ++numWinningPositions;
                                                        if (prevScore != score)
                                                        {
                                                            ++incrementalWins;
                                                            if (lastDisplayedBkOffset != bkOffset)
                                                            {
                                                                INT32 currentTime = ChessTime();
                                                                if (currentTime - lastDisplayTime > 50)
                                                                {
                                                                    lastDisplayTime = currentTime;
                                                                    lastDisplayedBkOffset = bkOffset;
                                                                    if (!SquareIsWhite(wbOffset))
                                                                        ChessFatal("bishop not supposed to be on black");
                                                                    if (board.GetSquareContents(wbOffset) != WBISHOP)
                                                                        ChessFatal("misplaced bishop on white");
                                                                    if (board.GetSquareContents(wnOffset) != WKNIGHT)
                                                                        ChessFatal("misplaced knight");

                                                                    BestPath bp;
                                                                    CalcEndgameBestPath (board, bp, whiteTable, blackTable, CalcTableIndexBN);
                                                                    ui.DisplayBestPath (board, bp);
                                                                    ui.DrawBoard(board);
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                                board.SetOffsetContents (EMPTY, wnOffset, true);
                                            }
                                        }
                                        board.SetOffsetContents (EMPTY, wbOffset, true);
                                    }
                                }
                            }
                        }
                    }
                }
                ++passCounter;

                ui.SetAdHocText (
                    3, "%s pass %u: found %u legal, %u new wins",
                    filename,
                    passCounter,
                    numLegalPositions,
                    incrementalWins
                );
            }
            while (incrementalWins > 0);

            // When we get here, we are done, so write the white table to disk.

            unsigned numWritten = fwrite (whiteTable, sizeof(Move), MOVE_TABLE_SIZE_BN, outfile);
            if (numWritten != MOVE_TABLE_SIZE_BN)
            {
                ui.SetAdHocText (3, "egdb: Cannot write move table to '%s'", filename);
            }

            fclose (outfile);
            outfile = NULL;
        }
        else
        {
            ui.SetAdHocText (3, "egdb: Cannot open '%s' for write", filename);
        }
    }

    delete[] whiteTable;
    delete[] blackTable;
}


//--------------------------------------------------------------------------
// GenerateEndgameDatabaseBB  generates a 16-MB endgame database for
// a king and two bishops (opposite square colors) vs a lone king.
// The indexing takes advantage of the fact that a given bishop can
// be only on one of 32 squares instead of 64.  The bishop on white
// squares is the 'high order' bishop.

void GenerateEndgameDatabaseBB (
    const char *filename,
    ChessBoard &board,
    ChessUI &ui )
{
    // Do not generate the database if it is already complete...
    Move tempMove;
    if ( FetchMoveFromDatabase(filename,MOVE_TABLE_SIZE_BB-1,tempMove) )
        return;

    // Generate the database assuming Black has only a king
    // and White has only a king and whatever is in 'whitePiece'.
    // In an actual game, if we need to use the database from
    // Black's point of view, we should rotate coordinates in case
    // the extra piece is actually a black pawn (moving the other way).

    // Make sure we can allocate the necessary memory... and it's a LOT!
    Move *whiteTable = new Move [MOVE_TABLE_SIZE_BB];
    Move *blackTable = new Move [MOVE_TABLE_SIZE_BB];
    FILE *outfile = 0;
    bool firstTimeDisplay = true;
    INT32 lastDisplayTime = ChessTime();
    if ( whiteTable && blackTable )
    {
        // Make sure we can open the output file...
        outfile = fopen ( filename, "wb" );
        if ( outfile )
        {
            memset ( whiteTable, 0, sizeof(Move) * MOVE_TABLE_SIZE_BB );
            memset ( blackTable, 0, sizeof(Move) * MOVE_TABLE_SIZE_BB );

            ui.SetAdHocText ( 3, "Creating endgame database '%s'", filename );

            // Here's where it gets interesting!

            board.Init();   // make sure it is White's turn to move
            board.ClearEverythingButKings();
            SCORE score = NEGINF;
            unsigned prevNumWinningPositions = 0;
            unsigned numLegalPositions = 0;
            unsigned numWinningPositions = 0;
            unsigned incrementalWins = 0;
            for (unsigned passCounter=0; ; ++passCounter)
            {
                prevNumWinningPositions = numWinningPositions;
                numWinningPositions = 0;
                incrementalWins = 0;
                for ( unsigned wkIndex=0; wkIndex<64; ++wkIndex )
                {
                    unsigned wkOffset = CalcPieceOffset (wkIndex);
                    board.SetOffsetContents ( WKING, wkOffset, true );
                    unsigned lastDisplayedBkOffset = 0;
                    for ( unsigned bkIndex=0; bkIndex<64; ++bkIndex )
                    {
                        if ( bkIndex != wkIndex )
                        {
                            unsigned bkOffset = CalcPieceOffset (bkIndex);
                            board.SetOffsetContents ( BKING, bkOffset, true );
                            if ( EGDB_IsLegal(board) )  // optimization
                            {
                                for ( unsigned wbwIndex=0; wbwIndex<32; ++wbwIndex )
                                {
                                    unsigned wbwOffset = CalcBishopWhiteOffset(wbwIndex);
                                    if ( wbwOffset!=wkOffset && wbwOffset!=bkOffset )
                                    {
                                        board.SetOffsetContents ( WBISHOP, wbwOffset, true );
                                        for ( unsigned wbbIndex=0; wbbIndex<32; ++wbbIndex )
                                        {
                                            unsigned wbbOffset = CalcBishopBlackOffset(wbbIndex);
                                            if ( wbbOffset!=wkOffset && wbbOffset!=bkOffset )
                                            {
                                                board.SetOffsetContents ( WBISHOP, wbbOffset, true );
                                                ValidateBB(board,wbwOffset,wbbOffset);
                                                if ( EGDB_IsLegal(board) )
                                                {
                                                    if ( passCounter == 0 )
                                                        ++numLegalPositions;

                                                    SCORE prevScore = whiteTable[CalcTableIndexBB(board)].score;
                                                    score = EGDB_WhiteSearchBB ( board, 0, whiteTable, blackTable, ui, passCounter*2 );
                                                    if ( score >= FORCED_WIN )
                                                    {
                                                        ++numWinningPositions;
                                                        if ( prevScore != score )
                                                        {
                                                            ++incrementalWins;
                                                            if ( lastDisplayedBkOffset!=bkOffset )
                                                            {
                                                                INT32 currentTime = ChessTime();
                                                                if ( currentTime - lastDisplayTime > 50 )
                                                                {
                                                                    lastDisplayTime = currentTime;
                                                                    lastDisplayedBkOffset = bkOffset;
                                                                    if ( !SquareIsWhite(wbwOffset) )
                                                                        ChessFatal("bishop not supposed to be on black");
                                                                    if ( !SquareIsBlack(wbbOffset) )
                                                                        ChessFatal("bishop not supposed to bo on white");
                                                                    if ( board.GetSquareContents(wbwOffset) != WBISHOP )
                                                                        ChessFatal("misplaced bishop on white");
                                                                    if ( board.GetSquareContents(wbbOffset) != WBISHOP )
                                                                        ChessFatal("misplaced bishop on black");

                                                                    BestPath bp;
                                                                    CalcEndgameBestPath (board, bp, whiteTable, blackTable, CalcTableIndexBB);
                                                                    ui.DisplayBestPath (board, bp);
                                                                    ui.DrawBoard(board);
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                                board.SetOffsetContents ( EMPTY, wbbOffset, true );
                                            }
                                        }
                                        board.SetOffsetContents ( EMPTY, wbwOffset, true );
                                    }
                                }
                            }
                        }
                    }
                }
                ui.SetAdHocText ( 3, "%s pass %u: found %u legal, %u new wins",
                                  filename,
                                  1+passCounter,
                                  numLegalPositions,
                                  incrementalWins );

                if ( incrementalWins == 0 )
                    break;
            }

            // When we get here, we are done, so write the white table to disk.

            unsigned numWritten = fwrite (
                                      whiteTable, sizeof(Move), MOVE_TABLE_SIZE_BB, outfile );

            if ( numWritten != MOVE_TABLE_SIZE_BB )
            {
                ui.SetAdHocText ( 3, "egdb: Cannot write move table to '%s'", filename );
            }
        }
        else
        {
            ui.SetAdHocText ( 3, "egdb: Cannot open '%s' for write", filename );
        }
    }
    else
    {
        ui.SetAdHocText ( 3, "egdb: Not enough memory!" );
    }

    if ( outfile )
    {
        fclose (outfile);
        outfile = 0;
    }

    if ( whiteTable )
    {
        delete[] whiteTable;
        whiteTable = 0;
    }

    if ( blackTable )
    {
        delete[] blackTable;
        blackTable = 0;
    }
}


//-----------------------------------------------------------------------------

bool LocateBishopAndKnight (ChessBoard &board, unsigned &wbOffset, unsigned &wnOffset)
{
    const SQUARE *bptr = board.queryBoardPointer();
    int wnCount = 0;
    int wbCount = 0;

    wbOffset = wnOffset = 0;

    for ( int y=OFFSET(2,2); y <= OFFSET(2,9); y += NORTH )
    {
        for ( int x=0; x<8; ++x )
        {
            int ofs = x + y;
            switch (bptr[ofs])
            {
            case BKING:
            case WKING:
            case EMPTY:
                break;

            case WBISHOP:
            case BBISHOP:
                wbOffset = ofs;
                ++wbCount;
                break;

            case WKNIGHT:
            case BKNIGHT:
                wnOffset = ofs;
                ++wnCount;
                break;

            default:
                ChessFatal ("Unexpected piece found in LocateBishopAndKnight");
            }
        }
    }

    if (wbCount > 1)
    {
        ChessFatal ("More than one bishop in LocateBishopAndKnight");
    }

    if (wnCount > 1)
    {
        ChessFatal ("More than one knight in LocateBishopAndKnight");
    }

    return wbCount==1 && wnCount==1;
}

//-----------------------------------------------------------------------------

Move RotateMove (Move move)
{
    move.source &= BOARD_OFFSET_MASK;
    move.source = 143 - move.source;
    move.dest   = 143 - move.dest;
    return move;
}

//-----------------------------------------------------------------------------

/*
    Correct number of legal BN positions:  5,437,752.   ratio = 0.64823
    Number of forced checkmate positions:  5,411,092.
    Table size = 64 * 64 * 64 * 32      =  8,388,608.
*/

SCORE EGDB_WhiteSearchBN (
    ChessBoard  &board,
    int         depth,
    int         required_mate_plies,
    Move        *whiteTable,
    Move        *blackTable,
    ChessUI     &ui,
    unsigned    wbOffset,
    unsigned    wnOffset )
{
    // If either the bishop or the knight have been captured, this position is a draw...
    const SQUARE *bptr = board.queryBoardPointer();
    if ((bptr[wbOffset] != WBISHOP) || (bptr[wnOffset] != WKNIGHT))
    {
        return 0;
    }

    unsigned whiteKingOffset  = board.GetWhiteKingOffset();
    unsigned blackKingOffset  = board.GetBlackKingOffset();

    unsigned ti = CalcTableIndexBN (whiteKingOffset, blackKingOffset, wbOffset, wnOffset);

    if ( whiteTable[ti].source != 0 )
    {
        // This position has already been fully evaluated
        return whiteTable[ti].score;
    }

    SCORE bestscore = 0;

    if ( depth == 0 )   // This limits the search to 2 plies
    {
        MoveList ml;
        board.GenWhiteMoves ( ml );
        bestscore = NEGINF;
        Move *move = &ml.m[0];
        UnmoveInfo unmove;
        Move *bestmove = 0;

        for ( int i=0; i<ml.num; ++i, ++move )
        {
            // We must pass the correct bishop/knight offsets to EGDB_BlackSearchBN.
            // This method is much faster than searching the whole board for them...
            int msource, mdest;
            move->actualOffsets (board, msource, mdest);
            unsigned updated_wbOffset = (msource == (int)wbOffset) ? mdest : wbOffset;
            unsigned updated_wnOffset = (msource == (int)wnOffset) ? mdest : wnOffset;

            board.MakeMove ( *move, unmove );
            SCORE score = EGDB_BlackSearchBN ( board, 1+depth, required_mate_plies, whiteTable, blackTable, ui, updated_wbOffset, updated_wnOffset);
            board.UnmakeMove ( *move, unmove );

            if ( score >= WON_FOR_WHITE )
            {
                score -= WIN_DELAY_PENALTY;
            }

            move->score = score;

            if ( score > bestscore )
            {
                bestscore = score;
                bestmove = move;
            }
        }

        int required_score = WHITE_WINS - WIN_POSTPONEMENT(required_mate_plies);
        if ( bestscore >= required_score )
        {
            // use rotational symmetry to prevent doing twice as much work as needed
            unsigned ri = CalcTableIndexBN (143-whiteKingOffset, 143-blackKingOffset, 143-wbOffset, 143-wnOffset);
            whiteTable[ti] = *bestmove;
            whiteTable[ri] = RotateMove (*bestmove);
        }
    }

    return bestscore;
}

//-----------------------------------------------------------------------------


SCORE EGDB_BlackSearchBN (
    ChessBoard  &board,
    int         depth,
    int         required_mate_plies,
    Move        *whiteTable,
    Move        *blackTable,
    ChessUI     &ui,
    unsigned    wbOffset,
    unsigned    wnOffset )
{
    // If either the bishop or the knight have been captured, this position is a draw...
    const SQUARE *bptr = board.queryBoardPointer();
    if ((bptr[wbOffset] != WBISHOP) || (bptr[wnOffset] != WKNIGHT))
    {
        return 0;
    }

    unsigned whiteKingOffset  = board.GetWhiteKingOffset();
    unsigned blackKingOffset  = board.GetBlackKingOffset();

    unsigned ti = CalcTableIndexBN (whiteKingOffset, blackKingOffset, wbOffset, wnOffset);

    // Before doing anything else, see if this position has already been handled...
    if ( blackTable[ti].source != 0 )
    {
        // Getting here means the position has already been fully evaluated
        return blackTable[ti].score;
    }

    // See if game is over...
    MoveList ml;
    board.GenBlackMoves (ml);
    if ( ml.num == 0 )
    {
        // The game is over.  See if this is a checkmate or a stalemate.
        if ( board.BlackInCheck() )
        {
            blackTable[ti].source = 4;
            blackTable[ti].score = WHITE_WINS;
            return WHITE_WINS;
        }

        blackTable[ti].source = 2;
        blackTable[ti].score = 0;
        return 0;   // stalemate
    }

    int required_score = WHITE_WINS - WIN_POSTPONEMENT(required_mate_plies-depth);

    SCORE bestscore = POSINF;
    Move *move = &ml.m[0];
    UnmoveInfo unmove;
    Move *bestmove = 0;
    for ( int i=0; i<ml.num; ++i, ++move )
    {
        // We must pass the correct bishop/knight offsets to EGDB_WhiteSearchBN.
        // This method is much faster than searching the whole board for them...
        int msource, mdest;
        move->actualOffsets (board, msource, mdest);
        unsigned updated_wbOffset = (msource == (int)wbOffset) ? mdest : wbOffset;
        unsigned updated_wnOffset = (msource == (int)wnOffset) ? mdest : wnOffset;

        board.MakeMove ( *move, unmove );
        SCORE score = EGDB_WhiteSearchBN (board, 1+depth, required_mate_plies, whiteTable, blackTable, ui, updated_wbOffset, updated_wnOffset);
        board.UnmakeMove ( *move, unmove );

        if ( score >= WON_FOR_WHITE )
        {
            score -= WIN_DELAY_PENALTY;
        }

        move->score = score;

        if ( score < bestscore )
        {
            bestscore = score;
            bestmove = move;
            if (score < required_score)     // A kind of pruning:  no need to continue if we won't store this result!
            {
                break;
            }
        }
    }

    if ( bestscore >= required_score )
    {
        // use rotational symmetry to prevent doing twice as much work as needed
        unsigned ri = CalcTableIndexBN (143-whiteKingOffset, 143-blackKingOffset, 143-wbOffset, 143-wnOffset);
        blackTable[ti] = *bestmove;
        blackTable[ri] = RotateMove (*bestmove);
    }

    return bestscore;
}


//-----------------------------------------------------------------------------


SCORE EGDB_WhiteSearchBB (
    ChessBoard  &board,
    int         depth,
    Move        *whiteTable,
    Move        *blackTable,
    ChessUI     &ui,
    int          required_mate_plies )
{
    unsigned wbwOffset=0, wbbOffset=0;
    if ( !LocateBishops(board,wbwOffset,wbbOffset) )
    {
        return 0;   // Black captured White's only means of winning, so it's a draw.
    }

    unsigned whiteKingOffset  = board.GetWhiteKingOffset();
    unsigned blackKingOffset  = board.GetBlackKingOffset();

    unsigned ti = CalcTableIndexBB (board, wbwOffset, wbbOffset);

    if ( whiteTable[ti].source != 0 )
    {
        // This position has already been fully evaluated
        return whiteTable[ti].score;
    }

    // See if game is over...
    MoveList ml;

    bool whiteCanMove = false;
    if ( depth == 0 )
    {
        // We are going to do a full search anyway, so might as
        // well generate the move list to determine whether there are any.
        board.GenWhiteMoves ( ml );
        whiteCanMove = (ml.num > 0);
    }
    else
    {
        whiteCanMove = board.WhiteCanMove();
    }

    if ( !whiteCanMove )
    {
        // The game is over.  Must be a stalemate because Black has only a king.
        whiteTable[ti].source = 2;
        whiteTable[ti].score = 0;
        return 0;
    }

    SCORE bestscore = 1000;

    if ( depth == 0 )
    {
        bestscore = NEGINF;
        Move *move = &ml.m[0];
        UnmoveInfo unmove;
        Move *bestmove = 0;
        for ( int i=0; i<ml.num; ++i, ++move )
        {
            board.MakeMove ( *move, unmove );
            SCORE score = EGDB_BlackSearchBB ( board, 1+depth, whiteTable, blackTable, ui, required_mate_plies );
            board.UnmakeMove ( *move, unmove );

            if ( score > 0 )
            {
                if ( score >= WON_FOR_WHITE )
                    score -= WIN_DELAY_PENALTY;
                else
                    --score;
            }

            move->score = score;

            if ( score > bestscore )
            {
                bestscore = score;
                bestmove = move;
            }
        }

        if ( bestscore >= FORCED_WIN )
            whiteTable[ti] = *bestmove;
    }

    return bestscore;
}


//-----------------------------------------------------------------------------



SCORE EGDB_BlackSearchBB (
    ChessBoard  &board,
    int         depth,
    Move        *whiteTable,
    Move        *blackTable,
    ChessUI     &ui,
    int          required_mate_plies )
{
    unsigned wbwOffset=0, wbbOffset=0;
    if ( !LocateBishops(board,wbwOffset,wbbOffset) )
    {
        return 0;   // Black captured White's only means of winning, so it's a draw.
    }

    unsigned whiteKingOffset  = board.GetWhiteKingOffset();
    unsigned blackKingOffset  = board.GetBlackKingOffset();

    unsigned ti = CalcTableIndexBB (board, wbwOffset, wbbOffset);

    // Before doing anything else, see if this position has already been handled...
    if ( blackTable[ti].source != 0 )
    {
        // Getting here means the position has already been fully evaluated
        return blackTable[ti].score;
    }

    // See if game is over...
    MoveList ml;
    board.GenBlackMoves (ml);
    if ( ml.num == 0 )
    {
        // The game is over.  See if this is a checkmate or a stalemate.
        if ( board.BlackInCheck() )
        {
            blackTable[ti].source = 4;
            blackTable[ti].score = WHITE_WINS;
            return WHITE_WINS;
        }

        blackTable[ti].source = 2;
        blackTable[ti].score = 0;
        return 0;   // stalemate
    }

    SCORE bestscore = POSINF;
    Move *move = &ml.m[0];
    UnmoveInfo unmove;
    Move *bestmove = 0;
    for ( int i=0; i<ml.num; ++i, ++move )
    {
        board.MakeMove ( *move, unmove );
        SCORE score = EGDB_WhiteSearchBB ( board, 1+depth, whiteTable, blackTable, ui, required_mate_plies );
        board.UnmakeMove ( *move, unmove );

        if ( score > 0 )
        {
            if ( score >= WON_FOR_WHITE )
                score -= WIN_DELAY_PENALTY;
            else
                --score;
        }

        move->score = score;

        if ( score < bestscore )
        {
            bestscore = score;
            bestmove = move;
            if (score < FORCED_WIN)     // A kind of pruning:  no need to continue if we won't store this result!
            {
                break;
            }
        }
    }

    if ( bestscore >= FORCED_WIN )
        blackTable[ti] = *bestmove;

    return bestscore;
}


//-----------------------------------------------------------------------------



/*
    $Log: egdb.cpp,v $
    Revision 1.14  2006/01/18 19:58:11  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.13  2006/01/07 13:19:54  dcross
    1. I found and fixed a serious bug in the bn.edb generator:
       It was not generating optimal checkmates.  I discovered this by running
       a symmetry validator algorithm, which found inconsistencies among equivalent positions.
       I fixed this by making sure that in each pass, we find only checkmates in that number of moves.
       This requires a lot more passes, but at least it is correct!
    2. Because the same bug exists in all the other block-copied versions of the algorithm,
       and because I want to get away from the complexities of requiring bishops to be on a
       given color, I am starting all over and replacing egdb.cpp with a new file egdbase.cpp.
       I will keep the existing code around for reference, but I have put a #error in it to prevent
       me from accidentally trying to compile it in the future.

    Revision 1.12  2006/01/06 20:11:07  dcross
    Fixed linker error in portable.exe:  moved  Global_EndgameDatabaseMode from wingui.cpp to egdb.cpp.

    Revision 1.11  2006/01/06 19:12:13  dcross
    No code change - just fixed comments.

    Revision 1.10  2006/01/06 17:40:48  dcross
    Oops!  Fixed bug in bn.edb generator:  it was trying knights only on half of the squares of the board in each pass!
    Changed 32 to 64 for the limit of wnIndex.  I noticed this problem because of seeing that the following line
    did not look right:
        pass 1: found 2,718,876 legal,      948 new wins
    This was wrong because the generated output had 5,411,092 legal positions in it!

    Revision 1.9  2006/01/06 17:16:55  dcross
    Now "winchen.exe -dbd" will generate a text listing of bn.edb.

    Revision 1.8  2006/01/06 13:58:13  dcross
    Made bishop+knight search faster:  no longer search for the pieces in the board.
    Instead, we keep track of where each piece is as we move it.

    Revision 1.7  2006/01/06 13:08:34  dcross
    Added pruning to the other endgame database generators.

    Revision 1.6  2006/01/06 02:25:46  dcross
    1. Fixed bug in ComputerChessPlayer::BlackDbEndgame... had WB_INDEX where I should have had BB_INDEX.
    2. Made bn.edb generation much faster by adding symmetrically rotated moves at the same time I add a move.
    3. Made faster also by adding pruning to EGDB_BlackSearchBN.  It's not really alpha-beta pruning, but similar.
       Alpha-beta pruning would be wrong, because it would corrupt the values in the blackTable[].
       But what I can get away with, without any problem, is bailing out whenever black has a reply that
       evades any (known) forced checkmate.  That is a case where we just aren't going to add anything to the
       table in the current pass, so there is no point to continuing searching through Black's other replies.

    Revision 1.5  2006/01/05 23:12:26  dcross
    Just for fun, I am adding a King+Bishop+Knight vs King endgame database.
    The bn.edb generator is still running and I probably won't know if it worked until tomorrow.

    Revision 1.4  2006/01/05 19:24:36  dcross
    Added comments about some confusing stuff in ConsultEndgameDatabase.

    Revision 1.3  2005/11/23 21:30:29  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:38  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!


         Revision history:

    2001 January 8 [Don Cross]
         Adding support for K+B+B vs K database.
         No longer generate a database if it appears to be complete.

    2001 January 7 [Don Cross]
         Finished rook, queen, and pawn endgame databases.
         They are tested and seem to work.  There is still some
         weirdness about 'numWinningPositions' not changing at
         random times from pass to pass in GenerateEndgameDatabase,
         but I have reworked the logic so this doesn't matter.
         Still, there is something troubling about this.

    2001 January 5 [Don Cross]
         Started writing.
         Adding support for generation of endgame databases,
         specifically for K+R vs K, K+Q vs K, K+P vs K.
         When there are three pieces on the board, there is
         an upper limit of 64*63*62 = 249984 possible positions.
         We can pre-compute a file of optimal moves for these
         situations.

*/

