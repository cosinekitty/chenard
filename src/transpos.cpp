/*==========================================================================

    transpos.cpp  -  Copyright (C) 1999-2005 by Don Cross

    Contains transposition table for chess search.

==========================================================================*/

#include <stdio.h>
#include <string.h>
#include "chess.h"
#include "profiler.h"

#define TRANSPOSE_POOL_SIZE   49663
#define COLLISION_RESOLVE     16       // number of entries to search when collisions found


TranspositionTable  *ComputerChessPlayer::XposTable = 0;   // users must lazy init


TranspositionTable::TranspositionTable (int memorySizeInMegabytes)
{
    if (memorySizeInMegabytes < 1) {
        if (sizeof(int) < 4) {
            numTableEntries = 65500 / sizeof(TranspositionEntry);
        } else {
            numTableEntries = TRANSPOSE_POOL_SIZE;      // about 800K worth of table
        }
    } else {
        if (memorySizeInMegabytes > 1024) {
            memorySizeInMegabytes = 1024;   // enforce a 1GB limit to avoid integer overflow
        }

        // divide by 2 because there are 2 transposition tables (one for White and one for Black)...
        int entriesPerMegabyte = (1024 * 1024 / 2) / sizeof(TranspositionEntry);      // will result in a tiny amount of rounding down... who cares
        numTableEntries = memorySizeInMegabytes * entriesPerMegabyte;
    }

    init ();
}


void TranspositionTable::init ()
{
    whiteTable = new TranspositionEntry [numTableEntries];
    blackTable = new TranspositionEntry [numTableEntries];

    if ( !whiteTable || !blackTable )
        ChessFatal ( "Out of memory allocating TranspositionTable" );

    reset();
}


TranspositionTable::~TranspositionTable()
{
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


void TranspositionTable::reset()
{
    PROFILER_ENTER(PX_XPOS);

    memset ( whiteTable, 0, numTableEntries * sizeof(TranspositionEntry) );
    memset ( blackTable, 0, numTableEntries * sizeof(TranspositionEntry) );
    numHits = numTries = 0;
    numStomps = numStores = numFresh = numStales = numFailures = numInferior = 0;

    PROFILER_EXIT();
}


void TranspositionTable::startNewSearch()
{
    PROFILER_ENTER(PX_XPOS);

    // set all staleness flags...
    for ( unsigned i=0; i < numTableEntries; ++i )
    {
        whiteTable[i].flags |= XF_STALE;
        blackTable[i].flags |= XF_STALE;
    }

    PROFILER_EXIT();
}


void TranspositionTable::rememberWhiteMove (
    ChessBoard &board,
    int level,
    int depth,
    Move bestReply,
    SCORE alpha,
    SCORE beta )
{
    PROFILER_ENTER(PX_XPOS);

    int searchedDepth = level - depth;
    if ( searchedDepth < 0 )
        searchedDepth = 0;

    rememberMove ( board, searchedDepth, depth, bestReply, alpha, beta, whiteTable, true );

    PROFILER_EXIT();
}
    

void TranspositionTable::rememberBlackMove (
    ChessBoard &board,
    int level,
    int depth,
    Move bestReply,
    SCORE alpha,
    SCORE beta )
{
    PROFILER_ENTER(PX_XPOS);

    int searchedDepth = level - depth;
    if ( searchedDepth < 0 )
        searchedDepth = 0;

    rememberMove ( board, searchedDepth, depth, bestReply, alpha, beta, blackTable, false );

    PROFILER_EXIT();
}


const TranspositionEntry *TranspositionTable::locateWhiteMove (
    ChessBoard &board )
{
    PROFILER_ENTER(PX_XPOS);
    const TranspositionEntry *xpos = locateMove ( board, whiteTable );
    PROFILER_EXIT();
    return xpos;
}
    

const TranspositionEntry *TranspositionTable::locateBlackMove (
    ChessBoard &board )
{
    PROFILER_ENTER(PX_XPOS);
    const TranspositionEntry *xpos = locateMove ( board, blackTable );
    PROFILER_EXIT();
    return xpos;
}


void TranspositionTable::rememberMove (
    ChessBoard &board,
    int searchedDepth,
    int future,
    Move bestReply,
    SCORE alpha,
    SCORE beta,
    TranspositionEntry *table,
    bool whiteToMove )
{
    ++numStores;
    UINT32 hashCode = board.Hash();
    const unsigned idealIndex = hashCode % numTableEntries;

    // Go through 3 passes to find a slot to store the new <board,reply> tuple...

    // Pass 1: search for a completely unused slot (hash == 0), or old
    // copies of the same board position. 

    unsigned index = idealIndex;
    int retry;
    for ( retry=0; retry < COLLISION_RESOLVE; ++retry )
    {
        TranspositionEntry &x = table[index];
        bool better = false;

        if ( x.boardHash == hashCode )
        {
            if ( searchedDepth > x.searchedDepth )
                better = true;
            else if ( searchedDepth == x.searchedDepth )
            {
                // If the searches are of equal depth, employ tie-breakers
                // for replacement.

                bool newInsideWindow = 
                    alpha <= bestReply.score && bestReply.score <= beta;

                if ( !x.scoreIsInsideWindow() )
                {
                    better = newInsideWindow || future < x.future;
                }
            }
        }
        else if ( x.boardHash == 0 )
            better = true;

        if ( better )
        {
            ++numFresh;
            x.boardHash = hashCode;
            x.searchedDepth = searchedDepth;
            x.future = future;
            x.flags &= ~XF_STALE;
            x.bestReply = bestReply;
            x.alpha = alpha;
            x.beta = beta;
            return;
        }
        else if ( x.boardHash == hashCode )
        {
            ++numInferior;
            return;   // the position was inside the table, so don't search any more
        }

        if ( ++index >= numTableEntries )
            index = 0;
    }

    // Pass 2: search for slot with "stale" bit set.

    index = idealIndex;
    for ( retry=0; retry < COLLISION_RESOLVE; ++retry )
    {
        TranspositionEntry &x = table[index];
        if ( x.flags & XF_STALE )
        {
            ++numStales;
            x.boardHash = hashCode;
            x.searchedDepth = searchedDepth;
            x.future = future;
            x.flags &= ~XF_STALE;
            x.bestReply = bestReply;
            x.alpha = alpha;
            x.beta = beta;
            return;
        }

        if ( ++index >= numTableEntries )
            index = 0;
    }

    // Pass 3: as a last resort, try to find an "inferior" 
    // entry in the table to stomp on.

    for ( retry=0; retry < COLLISION_RESOLVE; ++retry )
    {
        TranspositionEntry &x = table[index];

        bool newMoveBetter = (searchedDepth > x.searchedDepth);
        if ( !newMoveBetter )
        {
            if ( searchedDepth == x.searchedDepth )
            {
                // we need tie-breaker!

                if ( future < x.future )
                {
                    // Board positions found higher in the tree are likelier to
                    // be requested in the future

                    newMoveBetter = true;
                }
                else if ( future == x.future )
                {
                    // secondary tie-breaker... better score means that this
                    // move might be more likely to cause pruning in the future.

                    if ( whiteToMove )
                    {
                        if ( bestReply.score > x.bestReply.score )
                            newMoveBetter = true;
                    }
                    else
                    {
                        if ( bestReply.score < x.bestReply.score )
                            newMoveBetter = true;
                    }
                }
            }
        }

        if ( newMoveBetter )
        {
            ++numStomps;
            x.boardHash = hashCode;
            x.searchedDepth = searchedDepth;
            x.future = future;
            x.flags &= ~XF_STALE;
            x.bestReply = bestReply;
            x.alpha = alpha;
            x.beta = beta;
            return;
        }

        if ( ++index >= numTableEntries )
            index = 0;
    }

    ++numFailures;
}


const TranspositionEntry *TranspositionTable::locateMove (
    ChessBoard &board,
    TranspositionEntry *table )
{
    ++numTries;
    UINT32 hashCode = board.Hash();
    unsigned index = 0;
    if ( findEntry(hashCode,table,index) )
    {
        ++numHits;
        return &table[index];
    }

    return 0;
}


void TranspositionTable::debugDump ( const char *filename ) const
{
    FILE *f = fopen ( filename, "wt" );
    if ( f )
    {
        if ( numTries > 0 )
        {
            fprintf ( f, "numTries=%u  numHits=%d  percent=%0.1lf\n",
                numTries, numHits, 
                100.0 * double(numHits) / double(numTries) );
        }

        fprintf ( f, "numStores=%u numInferior=%u numStomps=%u\nnumFresh=%u numStales=%u numFailures=%u\n", 
            numStores, numInferior, numStomps, numFresh, numStales, numFailures );

        fprintf ( f, "store checksum = %u\n\n", 
            numStomps + numInferior + numFresh + numStales + numFailures );

        fprintf ( f, "%8s %3s %3s %2s %3s %3s %6s %6s %6s  %8s %3s %3s %2s %3s %3s %6s %6s %6s: %9s\n\n",
            "hash", "dep", "fut", "fl", "src", "dst", "score", "alpha", "beta",
            "hash", "dep", "fut", "fl", "src", "dst", "score", "alpha", "beta",
            "index" );

        for ( unsigned i=0; i < numTableEntries; ++i )
        {
            const TranspositionEntry &w = whiteTable[i];
            const TranspositionEntry &b = blackTable[i];

            fprintf ( f, "%8x %3u %3u %2x %3u %3u %6d %6d %6d  %8x %3u %3u %2x %3u %3u %6d %6d %6d: %9u\n",
                w.boardHash, unsigned(w.searchedDepth), unsigned(w.future), unsigned(w.flags),
                unsigned(w.bestReply.source), unsigned(w.bestReply.dest), int(w.bestReply.score), 
                int(w.alpha), int(w.beta),
                b.boardHash, unsigned(b.searchedDepth), unsigned(b.future), unsigned(b.flags),
                unsigned(b.bestReply.source), unsigned(b.bestReply.dest), int(b.bestReply.score),
                int(b.alpha), int(b.beta),
                i );
        }

        fclose (f);
    }
}


bool TranspositionTable::findEntry ( 
    UINT32 hashCode, 
    TranspositionEntry *table,
    unsigned &findIndex )
{
    // figure out ideal position in given hash table...
    unsigned index = hashCode % numTableEntries;
    for ( int retry=0; retry < COLLISION_RESOLVE; ++retry )
    {
        if ( table[index].boardHash == hashCode )
        {
            findIndex = index;
            return true;
        }

        if ( table[index].boardHash == 0 )
            return false;   // would have found it by now

        if ( ++index >= numTableEntries )
            index = 0;  // wrap around end of table
    }

    return false;
}


/*
    $Log: transpos.cpp,v $
    Revision 1.5  2009/09/16 16:49:34  Don.Cross
    Added support for the WinBoard/xboard "memory" command to set hash table sizes.

    Revision 1.4  2006/01/18 19:58:13  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.3  2005/11/23 21:30:31  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:43  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!


        Revision history:
    
    1999 January 25 [Don Cross]
         Started writing.  This is my second attempt at using transposition
         tables.  I'm going to start out simple by concentrating on using
         the transposition tables as a way of influencing the move ordering.
    
    1999 January 27 [Don Cross]
         Storing alpha and beta values in transposition entries now.
         This is used to short-circuit recursion in the search when
         certain window criteria are met.
    
    1999 January 14 [Don Cross]
         Adding conditional to use smaller transposition tables when
         sizeof(int) < 4, i.e., MS-DOS 64K limit.
    
*/

