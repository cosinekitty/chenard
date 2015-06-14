/*============================================================================

    chengene.cpp  -  Copyright (C) 1999-2005 by Don Cross

    I'm changing Chenard's evaluation heuristic constants from #defines
    to an array held in memory.  This will allow me to construct a
    genetic algorithm to evolve better heuristics.

    This file contains code and data needed by the bare-bones Chenard
    search without including the overhead needed for the GA.

============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chess.h"

ChessGeneDefinition ChessGene::DefTable[] =
{
    { "EscapeCheckDepth",          0,     0,      2 },    //   0
    { "MaxCheckDepth",             2,     0,      4 },    //   1
    { "MO_HashHistMax",         2900,   500,   6000 },    //   2
    { "MO_HashHistIncr",          35,    10,    100 },    //   3
    { "S_SafeEvalPruneMargin",   220,   180,    500 },    //   4
    { "CheckBonus",                2,     0,      6 },    //   5
    { "TempoBonus",                1,     1,      4 },    //   6
    { "KnightAttackKpos",         10,     3,     20 },    //   7
    { "BishopAttackKpos",         12,     3,     20 },    //   8
    { "RookAttackKpos",           18,     3,     30 },    //   9
    { "QueenAttackKpos",          20,     3,     30 },    //  10
    { "KnightProtectKpos",         5,     1,     20 },    //  11
    { "BishopProtectKpos",         6,     1,     20 },    //  12
    { "RookProtectKpos",           9,     1,     30 },    //  13
    { "QueenProtectKpos",         10,     1,     30 },    //  14
    { "RookTrappedByKing",        30,     5,     50 },    //  15
    { "PawnProtectsKing1",        25,     7,     40 },    //  16
    { "PawnProtectsKing2",        20,     8,     40 },    //  17
    { "PawnProtectsKing3",         9,     3,     30 },    //  18
    { "CastleKnightGuard",        10,     5,     30 },    //  19
    { "CastleHole1",              15,     5,     50 },    //  20
    { "CastleHole2",              35,     7,     55 },    //  21
    { "CastleHole3",              29,     6,     50 },    //  22
    { "CastleHoleDanger",         23,     5,     50 },    //  23
    { "KingOpposition",           20,     5,     40 },    //  24
    { "CanKCastleBonus",          15,     6,     30 },    //  25
    { "CanQCastleBonus",          12,     5,     30 },    //  26
    { "CanKQCastleBonus",         20,     7,     40 },    //  27
    { "KCastlePathEmpty",          6,     2,     12 },    //  28
    { "QCastlePathEmpty",          5,     2,     12 },    //  29
    { "CtekHole",                  5,     1,     12 },    //  30
    { "CtekHoleQ",                50,     5,    150 },    //  31
    { "CtekPawn1",                15,     2,     40 },    //  32
    { "CtekPawn2",                 5,     1,     30 },    //  33
    { "CtekKnight",                8,     1,     25 },    //  34
    { "CtekBishop",                5,     1,     20 },    //  35
    { "CtekRook",                 10,     1,     30 },    //  36
    { "CtekQueen3",               13,     1,     40 },    //  37
    { "CtekQueen2",               18,     1,     45 },    //  38
    { "BishopImmobile",           20,     3,     40 },    //  39
    { "CenterBlockBishop1",       20,     3,     40 },    //  40
    { "CenterBlockBishop2",        7,     1,     30 },    //  41
    { "TwoBishopSynergy",         10,     3,     25 },    //  42
    { "BishopPinK",               12,     2,     30 },    //  43
    { "BishopPinQ",                8,     1,     25 },    //  44
    { "BishopPinR",                3,     0,     20 },    //  45
    { "PawnFork",                 30,     5,     60 },    //  46
    { "PawnSideFile",             10,     3,     40 },    //  47
    { "PawnDoubled",              14,     3,     50 },    //  48
    { "PawnSplit",                12,     2,     40 },    //  49
    { "PawnProtect1",              3,     0,     10 },    //  50
    { "PawnProtect2",              5,     0,     12 },    //  51
    { "BishopProtectPawn",         2,     0,      8 },    //  52
    { "PassedPawnProtect1",       40,     5,     80 },    //  53
    { "PassedPawnProtect2",       45,     6,     80 },    //  54
    { "PassedPawnAlone",          18,     2,     50 },    //  55
    { "PassedPawnVulnerable",      8,     1,     35 },    //  56
    { "Passed3FromProm",          50,    10,     90 },    //  57
    { "Passed2FromProm",          75,    15,    100 },    //  58
    { "Passed1FromProm",         150,    20,    200 },    //  59
    { "PassedPieceBlock",          8,     0,     20 },    //  60
    { "Blocked2FromProm",         10,     1,     50 },    //  61
    { "RookPinQ",                  9,     0,     25 },    //  62
    { "RookPinK",                 13,     0,     30 },    //  63
    { "RookOpenFile",              6,     1,     20 },    //  64
    { "RookReachSeventh",          7,     1,     25 },    //  65
    { "RookOnSeventh",            12,     2,     40 },    //  66
    { "RookConnectVert",           4,     0,     15 },    //  67
    { "RookConnectHor",            2,     0,     10 },    //  68
    { "RookImmobileHor",           5,     0,     20 },    //  69
    { "RookImmobile",             15,     1,     35 },    //  70
    { "RookBacksPassedPawn1",      9,     1,     25 },    //  71
    { "RookBacksPassedPawn2",     12,     1,     35 },    //  72
    { "MO_PrevSquare",           210,    80,    350 },    //  73
    { "MO_Check",                 45,    15,    150 },    //  74
    { "MO_KillerMove",            45,    15,    150 },    //  75
    { "MO_HashHistShift",          4,     2,      6 },    //  76
    { "MO_PawnCapture",            3,     0,     15 },    //  77
    { "MO_PawnDanger",             1,     0,      8 },    //  78
    { "MO_Forward",                5,     0,     20 },    //  79
    { "MO_Castle",                10,     0,     40 },    //  80
    { "KnightForkUncertainty",     7,     0,      9 },    //  81

    { 0, 0, 0, 0 }  // marks end of list
};


SCORE ChessGeneDefinition::random() const
{
    SCORE range = maxValue - minValue + 1;
    SCORE value = minValue + ChessRandom(range);
    return value;
}


//-------------------------------------------------------------------------


ChessGene::ChessGene()
{
    reset();
}


void ChessGene::reset()
{
    int i;
    for ( i=0; i < NUM_CHESS_GENES; ++i )
    {
        if ( !DefTable[i].name )
            ChessFatal ( "ChessGene::DefTable[] is too short!" );

        v[i] = DefTable[i].defaultValue;
    }

    if ( DefTable[i].name )
        ChessFatal ( "ChessGene::DefTable[] is too long!" );
}


int ChessGene::save ( const char *filename ) const
{
    FILE *f = fopen ( filename, "wt" );
    if ( !f )
        return 0;   // failure

    for ( int i=0; i < NUM_CHESS_GENES; ++i )
        fprintf ( f, "%s=%d\n", DefTable[i].name, int(v[i]) );

    fclose(f);
    return 1;   // success
}


int ChessGene::load ( const char *filename )
{
    FILE *f = fopen ( filename, "rt" );
    if ( !f )
        return 0;   // failure

    reset();
    char line [512];

    for ( int i=0; fgets(line,sizeof(line),f); ++i )
    {
        // Find equal sign as delimiter between name and value...
        char *p = line;
        while ( *p && *p != '=' )
            ++p;

        if ( *p == '=' )
        {
            *p++ = '\0';
            int index = ChessGene::Locate(line,i);
            if ( index >= 0 && index < NUM_CHESS_GENES )
                v[index] = atoi(p);
        }       
    }

    fclose(f);
    return 1;   // success
}


int ChessGene::Locate ( const char *name, int expectedIndex )
{
    if ( expectedIndex >= NUM_CHESS_GENES )
        expectedIndex = NUM_CHESS_GENES - 1;
    else if ( expectedIndex < 0 )
        expectedIndex = 0;

    if ( strcmp ( name, DefTable[expectedIndex].name ) == 0 )
        return expectedIndex;

    for ( int delta=1; ; ++delta )
    {
        int firstBad = 0;
        int i1 = expectedIndex + delta;
        if ( i1 < NUM_CHESS_GENES )
        {
            if ( strcmp ( name, DefTable[i1].name ) == 0 )
                return i1;
        }
        else
            firstBad = 1;

        int i2 = expectedIndex - delta;
        if ( i2 >= 0 )
        {
            if ( strcmp ( name, DefTable[i2].name ) == 0 )
                return i2;
        }
        else if ( firstBad )
            break;
    }

    return -1;
}



/*
    $Log: chengene.cpp,v $
    Revision 1.3  2005/11/23 21:30:29  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:38  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!


        Revision history:
    
    1999 February 17 [Don Cross]
         Started writing.
    
*/

