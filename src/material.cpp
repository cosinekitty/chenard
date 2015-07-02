/*============================================================================

     material.cpp  -  Copyright (C) 1993-2005 by Don Cross

     Contains non-linear transformation lookup table for
     material on a chess board.

     This is important for implementing a bonus for trading when
     ahead in material, and a penalty for trading when behind in
     material.

============================================================================*/

#include <math.h>

#include "chess.h"

#define  INITIAL_MATERIAL       \
  (KING_VAL + QUEEN_VAL + 2*ROOK_VAL + 2*BISHOP_VAL + 2*KNIGHT_VAL + 8*PAWN_VAL)

#define  MAX_MATERIAL_POSSIBLE   \
  (KING_VAL + 9*QUEEN_VAL + 2*ROOK_VAL + 2*BISHOP_VAL + 2*KNIGHT_VAL)


SCORE MaterialData [MAX_MATERIAL_POSSIBLE + 1];


SCORE MaterialEval ( SCORE wmaterial, SCORE bmaterial )
{
#if 0
    if (   wmaterial < KING_VAL 
        || wmaterial > MAX_MATERIAL_POSSIBLE 
        || bmaterial < KING_VAL 
        || bmaterial > MAX_MATERIAL_POSSIBLE )
    {
        ChessFatal ( "Invalid material value in MaterialEval()" );
    }
#endif

    return MaterialData[wmaterial] - MaterialData[bmaterial];
}


class MaterialDataInitializer
{
public:
    MaterialDataInitializer();
};


MaterialDataInitializer::MaterialDataInitializer()
{
    SCORE x;

    const double D = 1.4;   // slope at queen down from init material

    const double A = (D - 1.0) / (2.0 * INITIAL_MATERIAL);
    const double B = 0.5 / A + INITIAL_MATERIAL;
    const double C = 0.25 / A + INITIAL_MATERIAL;

    for ( x=KING_VAL; x <= MAX_MATERIAL_POSSIBLE; x++ )
    {
        double g = -A * (x - B) * (x - B) + C;
        MaterialData[x] = SCORE ( 10.0 * g + 0.5 );
    }
}

static MaterialDataInitializer bob;   // Bob loves you!!!


/*
    $Log: material.cpp,v $
    Revision 1.3  2005/11/23 21:30:30  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:41  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!

*/

