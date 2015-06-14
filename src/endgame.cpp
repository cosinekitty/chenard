/*=======================================================================

     endgame.cpp   -   Copyright (C) 1993-2005 by Don Cross

     Knows how to play certain endgame positions better
     than the min-max search does.
     Its opinion (when it offers one) will supersede
     doing a search.

=======================================================================*/

#include "chess.h"
#include "profiler.h"

int Distance2 ( int ofs1, int ofs2 )
{
    int dx = XPART(ofs1) - XPART(ofs2);
    int dy = YPART(ofs1) - YPART(ofs2);
    return dx*dx + dy*dy;
}


//--------------------------------------------------------------------------

// If will checkmate with queen or rook, force king toward any corner.
const int ComputerChessPlayer::KingPosTableQR [144] =
{
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

     0,   0,   100,  90,  70,  50,  50,  70,  90, 100,     0,   0,
     0,   0,    90,  70,  35,  20,  20,  35,  70,  90,     0,   0,
     0,   0,    70,  35,  15,  10,  10,  15,  35,  70,     0,   0,
     0,   0,    50,  20,  10,   0,   0,  10,  20,  50,     0,   0,
     0,   0,    50,  20,  10,   0,   0,  10,  20,  50,     0,   0,
     0,   0,    70,  35,  15,  10,  10,  15,  35,  70,     0,   0,
     0,   0,    90,  70,  35,  20,  20,  35,  70,  90,     0,   0,
     0,   0,   100,  90,  70,  50,  50,  70,  90, 100
};


// If will checkmate with bishop on white, force toward white corner
const int ComputerChessPlayer::KingPosTableBW [144] =
{
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

     0,   0,  -100, -90, -70, -50,  50,  70,  90, 100,     0,   0,
     0,   0,   -90, -70, -35, -20,  20,  35,  70,  90,     0,   0,
     0,   0,   -70, -35, -15, -10,  10,  15,  35,  70,     0,   0,
     0,   0,   -50, -20, -10,  -5,   0,  10,  20,  50,     0,   0,
     0,   0,    50,  20,  10,   0,  -5, -10, -20, -50,     0,   0,
     0,   0,    70,  35,  15,  10, -10, -15, -35, -70,     0,   0,
     0,   0,    90,  70,  35,  20, -20, -35, -70, -90,     0,   0,
     0,   0,   100,  90,  70,  50, -50, -70, -90,-100
};


// If will checkmate with bishop on black, force toward black corner
const int ComputerChessPlayer::KingPosTableBB [144] =
{
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

     0,   0,   100,  90,  70,  50, -50, -70, -90,-100,     0,   0,
     0,   0,    90,  70,  35,  20, -20, -35, -70, -90,     0,   0,
     0,   0,    70,  35,  15,  10, -10, -15, -35, -70,     0,   0,
     0,   0,    50,  20,  10,   0,  -5, -10, -20, -50,     0,   0,
     0,   0,   -50, -20, -10,  -5,   0,  10,  20,  50,     0,   0,
     0,   0,   -70, -35, -15, -10,  10,  15,  35,  70,     0,   0,
     0,   0,   -90, -70, -35, -20,  20,  35,  70,  90,     0,   0,
     0,   0,  -100, -90, -70, -50,  50,  70,  90, 100
};


SCORE ComputerChessPlayer::EndgameEval1 ( 
    ChessBoard &board,
    int depth,
    SCORE, SCORE )
{
    PROFILER_ENTER(PX_EVAL);
    ++evaluated;

    // Look for definite draw...
    if ( board.IsDefiniteDraw() )
        return DRAW;

    // Look for stalemates and checkmates...
    if ( board.white_to_move )
    {
        if ( !board.WhiteCanMove() )
        {
            if ( board.flags & SF_WCHECK )
                return BLACK_WINS + WIN_POSTPONEMENT(depth);    // White has been checkmated
            else
                return DRAW;    // White has been stalemated
        }
    }
    else  // It's Black's turn to move
    {
        if ( !board.BlackCanMove() )
        {
            if ( board.flags & SF_BCHECK )
                return WHITE_WINS - WIN_POSTPONEMENT(depth);    // Black has been checkmated
            else
                return DRAW;    // Black has been stalemated
        }
    }

    // If we are using this eval, exactly one side had a lone king
    // at the root of the search tree.  Figure out which side that
    // is by looking at material balance.

    SCORE score = MaterialEval ( board.wmaterial, board.bmaterial );
    int kdist = Distance2 ( board.wk_offset, board.bk_offset );
    if ( score < 0 )
    {
        // Black is winning...see how bad off White's lone king is now...
        score += (kdist - 10000 - KingPosTable[board.wk_offset]);
        score -= ProximityBonus ( board, board.wk_offset, BR_MASK|BQ_MASK|BN_MASK|BB_MASK );
    }
    else
    {
        // White is winning...see how bad off Black's lone king is now...
        score -= (kdist - 10000 - KingPosTable[board.bk_offset]);
        score += ProximityBonus ( board, board.bk_offset, WR_MASK|WQ_MASK|WN_MASK|WB_MASK );
    }

    PROFILER_EXIT()

    return score;
}


SCORE ComputerChessPlayer::ProximityBonus ( 
    ChessBoard &board,
    int         target_ofs,
    int         mask )
{
    SCORE bonus = 0;

    for ( int y=OFFSET(2,2); y <= OFFSET(2,9); y += NORTH )
        for ( int x=0; x < 8; ++x )
            if ( board.board[x+y] & mask )
            {
                SCORE delta = Distance2 ( x + y, target_ofs );
                if ( !(board.board[x+y] & (BR_MASK|WR_MASK|BQ_MASK|WQ_MASK)) )
                    delta /= 12;
                bonus -= delta;
            }

    return bonus / 4;
}



/*
    $Log: endgame.cpp,v $
    Revision 1.3  2005/11/23 21:30:29  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:38  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!



         Revision History:
    
    1994 January 19 [Don Cross]
         Started adding real endgame algorithms!
    
    1994 January 22 [Don Cross]
         I have decided to try something different, and abandon
         the Chester-esque use of special endgame heuristics.
         Instead, I am going to adapt these heuristics into
         a special eval function, and use the normal search code.
         Before starting a search, the ComputerChessPlayer object
         will determine which evaluation function is appropriate
         and use it throughout the search.
    
    1994 January 25 [Don Cross]
         Adding small bonus for getting pieces other than strong king
         close to weak king in EndgameEval1.
    
    1999 January 14 [Don Cross]
         Updated coding style.
    
    2001 January 4 [Don Cross]
         Proximity bonus for bishops and knights too (but 1/12 as much).
         Now use special KingPos tables when checkmate must be performed
         with a bishop.  This way, we don't accidentally force the king
         into the wrong color corner.
    
*/

