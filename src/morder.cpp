/*=========================================================================

     morder.cpp  -  Copyright (C) 1993-2005 by Don Cross

     Contains move-ordering heuristics for the min-max search.

=========================================================================*/

#include "chess.h"
#include "profiler.h"

#define  PREV_SQUARE_BONUS      (gene.v[73])
#define  CHECK_BONUS            (gene.v[74])
#define  KILLER_MOVE_BONUS      (gene.v[75])
#define  HASH_HIST_SHIFT        (gene.v[76])
#define  PAWN_CAPTURE_PENALTY   (gene.v[77])
#define  PAWN_DANGER_PENALTY    (gene.v[78])
#define  FORWARD_BONUS          (gene.v[79])
#define  CASTLE_BONUS           (gene.v[80])

#define  WHITE_BEST_PATH     (20000)
#define  BLACK_BEST_PATH    (-20000)

#define  WHITE_XPOS       (10000)
#define  BLACK_XPOS      (-10000)


#ifdef __BORLANDC__
    #pragma argsused
#endif

void ComputerChessPlayer::WhiteMoveOrdering (
    const ChessBoard &board,
    Move &move,
    const UnmoveInfo &unmove,
    int depth,
    bool bestPathFlag )
{
#ifndef NO_SEARCH
    PROFILER_ENTER(PX_MOVEORDER)

    // First, check for best path.  It overrides everything!

    if ( bestPathFlag &&
        depth <= currentBestPath.depth &&
        depth < MAX_BESTPATH_DEPTH &&
        currentBestPath.m[depth] == move )
    {
        move.score = WHITE_BEST_PATH;
        return;
    }

    if ( move == moveOrder_xposBestMove )
    {
        move.score = WHITE_XPOS;
        return;
    }

    move.score = MaterialEval ( board.wmaterial, board.bmaterial );

    const int source = move.source & BOARD_OFFSET_MASK;

    // "killer move" heuristic...

    if ( depth > 0 && depth < MAX_BESTPATH_DEPTH &&
         depth <= nextBestPath[depth-1].depth &&
         nextBestPath[depth-1].m[depth] == move )
        move.score += KILLER_MOVE_BONUS;

    if ( board.flags & SF_BCHECK )
        move.score += CHECK_BONUS;

    if ( move.dest == unmove.prev_move.dest )
        move.score += PREV_SQUARE_BONUS;

    if ( move.dest <= OFFSET(9,9) )
    {
        SQUARE piece = board.board [move.dest];
        move.score -= UPIECE_INDEX(piece);

        if ( piece & WP_MASK )
        {
            const int delta = move.dest - source;
            if ( delta==NORTHEAST || delta==NORTHWEST )
                move.score -= PAWN_CAPTURE_PENALTY;
        }
        else
        {
            const SQUARE *p = & board.board [move.dest];
            if ( (p[NORTHEAST] | p[NORTHWEST]) & BP_MASK )
                move.score -= PAWN_DANGER_PENALTY;
        }

        if ( source <= OFFSET(9,4) && move.dest >= source+10 )
            move.score += FORWARD_BONUS;
    }
    else
    {
        int special = move.dest & SPECIAL_MOVE_MASK;
        if ( special == SPECIAL_MOVE_KCASTLE || 
             special == SPECIAL_MOVE_QCASTLE )
        {
            move.score += CASTLE_BONUS;
        }
    }

    move.score += whiteHist [ move.whiteHash() ] >> HASH_HIST_SHIFT;

    PROFILER_EXIT();
#endif
}


#ifdef __BORLANDC__
    #pragma argsused
#endif

void ComputerChessPlayer::BlackMoveOrdering (
    const ChessBoard &board,
    Move &move,
    const UnmoveInfo &unmove,
    int depth,
    bool bestPathFlag )
{
#ifndef NO_SEARCH
    PROFILER_ENTER(PX_MOVEORDER);

    // First, check for best path.  It overrides everything!

    if ( bestPathFlag &&
        depth <= currentBestPath.depth &&
        depth < MAX_BESTPATH_DEPTH &&
        currentBestPath.m[depth] == move )
    {
        move.score = BLACK_BEST_PATH;
        return;
    }

    if ( move == moveOrder_xposBestMove )
    {
        move.score = BLACK_XPOS;
        return;
    }

    move.score = MaterialEval ( board.wmaterial, board.bmaterial );


    const int source = move.source & BOARD_OFFSET_MASK;

    // "killer move" heuristic...

    if ( depth > 0 && depth < MAX_BESTPATH_DEPTH &&
         depth <= nextBestPath[depth-1].depth &&
         nextBestPath[depth-1].m[depth] == move )
        move.score -= KILLER_MOVE_BONUS;

    if ( board.flags & SF_WCHECK )
        move.score -= CHECK_BONUS;

    if ( move.dest == unmove.prev_move.dest )
        move.score -= PREV_SQUARE_BONUS;

    if ( move.dest <= OFFSET(9,9) )
    {
        const SQUARE piece = board.board [move.dest];
        move.score += UPIECE_INDEX(piece);
        if ( piece & BP_MASK )
        {
            const int delta = move.dest - source;
            if ( delta==SOUTHEAST || delta==SOUTHWEST )
                move.score += PAWN_CAPTURE_PENALTY;
        }
        else
        {
            const SQUARE *p = & board.board [move.dest];
            if ( (p[SOUTHEAST] | p[SOUTHWEST]) & WP_MASK )
                move.score += PAWN_DANGER_PENALTY;
        }

        if ( source >= OFFSET(2,7) && move.dest <= source-10 )
            move.score -= FORWARD_BONUS;
    }
    else
    {
        int special = move.dest & SPECIAL_MOVE_MASK;
        if ( special == SPECIAL_MOVE_KCASTLE ||
             special == SPECIAL_MOVE_QCASTLE )
        {
            move.score -= CASTLE_BONUS;
        }
    }

    move.score -= blackHist [ move.blackHash() ] >> HASH_HIST_SHIFT;

    PROFILER_EXIT();
#endif
}


//----------------------------------------------------------------------

void MoveList::WhiteSort()
{
#ifndef NO_SEARCH
    PROFILER_ENTER(PX_MOVEORDER);

    if ( num > 1 )
    {
        SCORE bestscore;
        int   besti, i;
        int   limit = num - 1;

        for ( int dest=0; dest < limit; dest++ )
        {
            bestscore = m [besti = dest].score;
            for ( i=dest + 1; i < num; i++ )
            {
                if ( m[i].score > bestscore )
                {
                    bestscore = m[i].score;
                    besti = i;
                }
            }

            if ( besti != dest )
            {
                Move t = m[dest];
                m[dest] = m[besti];
                m[besti] = t;
            }
        }
    }

    PROFILER_EXIT()
#endif
}


void MoveList::BlackSort()
{
#ifndef NO_SEARCH
    PROFILER_ENTER(PX_MOVEORDER)

    if ( num > 1 )
    {
        SCORE bestscore;
        int   besti, i;
        int   limit = num - 1;

        for ( int dest=0; dest < limit; dest++ )
        {
            bestscore = m [besti = dest].score;
            for ( i=dest + 1; i < num; i++ )
            {
                if ( m[i].score < bestscore )
                {
                    bestscore = m[i].score;
                    besti = i;
                }
            }

            if ( besti != dest )
            {
                Move t = m[dest];
                m[dest] = m[besti];
                m[besti] = t;
            }
        }
    }

    PROFILER_EXIT();
#endif
}


void MoveList::Shuffle()
{
    for (int i=1; i < num; ++i)
    {
        int r = ChessRandom (i+1);
        if (r < i)
        {
            Move t = m[r];
            m[r]   = m[i];
            m[i]   = t;
        }
    }
}


/*
    $Log: morder.cpp,v $
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

    Revision 1.4  2006/01/18 19:58:12  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.3  2005/11/23 21:30:30  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:41  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!



         Revision history:

    1999 February 16 [Don Cross]
         Added CASTLE_BONUS.

    1999 January 5 [Don Cross]
         Updated coding style.

    1997 June 18 [Don Cross]
         Added PAWN_CAPTURE_PENALTY and FORWARD_BONUS move ordering
         heuristics.  These improved benchmark score by 13%.

    1995 April 5 [Don Cross]
         Added experiment with shuffling move list before sorting it.

    1994 February 20 [Don Cross]
         Adding most-probable capture combination min-maxing.

    1994 February 10 [Don Cross]
         Adding piece indexing.

    1994 February 5 [Don Cross]
         Added "killer move" heuristics.

    1994 February 3 [Don Cross]
         Adding BestPath support.

    1993 August 30 [Don Cross]
         Changing pointers to references in the interfaces where
         appropriate.
*/

