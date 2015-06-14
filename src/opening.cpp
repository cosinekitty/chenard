/*========================================================================

     opening.cpp  -  Copyright (C) 1993-2005 by Don Cross

     Contains code for specially handling chess openings.
     These functions are called only by the search code,
     and only after the 'rootml' MoveList contained in
     class ComputerChessPlayer is filled out with a valid
     list of legal moves.

     ComputerChessPlayer::WhiteOpening() and
     ComputerChessPlayer::BlackOpening() return true when
     they have a definite opinion about what move to make.
     Otherwise, they return false, meaning that they don't
     know what to do in the current situation.

========================================================================*/

#include "chess.h"

typedef unsigned long big;
extern big OpeningData[];

#define  BAD_MOVE_BIT     0x10000L
#define  BAD_MOVE_SCORE   (-100)


void Move::Fix ( 
    unsigned long openBookData, 
    ChessBoard &board )
{
    source = BYTE ( (openBookData >> 8) & 0x7F );
    dest   = BYTE ( openBookData & 0x7F );

    SQUARE piece = board.GetSquareContents ( source );

    if ( piece & (WK_MASK|BK_MASK) )
    {
        if ( dest - source == 2*EAST )
            dest = SPECIAL_MOVE_KCASTLE;
        else if ( dest - source == 2*WEST )
            dest = SPECIAL_MOVE_QCASTLE;
    }
    else if ( piece & WP_MASK )
    {
        if ( YPART(dest) == 9 )
        {
            // assume promotion to Queen

            switch ( dest - source )
            {
                case NORTH:      dest = SPECIAL_MOVE_PROMOTE_NORM;       break;
                case NORTHEAST:  dest = SPECIAL_MOVE_PROMOTE_CAP_EAST;   break;
                case NORTHWEST:  dest = SPECIAL_MOVE_PROMOTE_CAP_WEST;   break;
            }

            dest |= Q_INDEX;
        }
        else if ( (dest - source) != NORTH &&
                (dest - source) != 2*NORTH &&
                board.GetSquareContents(dest) == EMPTY )
        {
            // assume e.p. capture

            if ( dest - source == NORTHEAST )
                dest = SPECIAL_MOVE_EP_EAST;
            else
                dest = SPECIAL_MOVE_EP_WEST;
        }
    }
    else if ( piece & BP_MASK )
    {
        if ( YPART(dest) == 2 )
        {
            // assume promotion to Queen

            switch ( dest - source )
            {
                case SOUTH:      dest = SPECIAL_MOVE_PROMOTE_NORM;       break;
                case SOUTHEAST:  dest = SPECIAL_MOVE_PROMOTE_CAP_EAST;   break;
                case SOUTHWEST:  dest = SPECIAL_MOVE_PROMOTE_CAP_WEST;   break;
            }

            dest |= Q_INDEX;
        }
        else if ( (dest - source) != SOUTH &&
                  (dest - source) != 2*SOUTH &&
                  board.GetSquareContents(dest) == EMPTY )
        {
            // assume e.p. capture

            if ( dest - source == SOUTHEAST )
                dest = SPECIAL_MOVE_EP_EAST;
            else
                dest = SPECIAL_MOVE_EP_WEST;
        }
    }

    score  = (openBookData & BAD_MOVE_BIT) ? BAD_MOVE_SCORE : 0;
}


bool OB_FindMove ( 
    Move move, 
    big &oindex, 
    ChessBoard &board )
{
    // Start with oindex given, and try to find this move.
    // If found, return true and set oindex to new branchlist offset.
    // Otherwise, return false.

    big numBranches = OpeningData [oindex];
    big bcount;
    big bindex = oindex + 1;

    for ( bcount=0; bcount < numBranches; bcount++ )
    {
        big branchSize = OpeningData [bindex];
        big moveData = OpeningData [bindex + 1];

        Move bookMove;
        bookMove.Fix ( moveData, board );

        if ( move == bookMove )
        {
            oindex = bindex + 2;
            return true;
        }

        bindex += branchSize;
    }

    return false;   // could not find move
}


bool OB_FindContinuation ( 
    ChessBoard &board, 
    big &oindex )
{
    // Start from beginning of game and try to find the continuation
    // that has been played so far in the opening book data.

    int currentPly = board.GetCurrentPlyNumber();
    int plyIndex;
    oindex = 0;

    static ChessBoard localBoard;
    localBoard.Init();

    for ( plyIndex=0; plyIndex < currentPly; plyIndex++ )
    {
        Move move = board.GetPastMove ( plyIndex );

        if ( !OB_FindMove ( move, oindex, localBoard ) )
            return false;  // out of opening book

        // Make the move in the local board...

        UnmoveInfo unmove;

        if ( localBoard.WhiteToMove() )
            localBoard.MakeWhiteMove ( move, unmove, true, true );
        else
            localBoard.MakeBlackMove ( move, unmove, true, true );
    }

    return true;
}


bool OB_PickMove ( 
    big oindex, 
    Move &move, 
    ChessBoard &board )
{
    MoveList ml;
    ml.num = 0;

    big numBranches = OpeningData [oindex];
    big bcount;
    big branchIndex = oindex + 1;

    for ( bcount = 0; bcount < numBranches; bcount++ )
    {
        big branchSize = OpeningData [branchIndex];
        big moveData   = OpeningData [branchIndex + 1];

        if ( !(moveData & BAD_MOVE_BIT) )
        {
            Move bookMove;
            bookMove.Fix ( moveData, board );
            ml.AddMove ( bookMove );
        }

        branchIndex += branchSize;
    }

    if ( ml.num == 0 )
        return false;

    int r = ChessRandom ( ml.num );
    move = ml.m[r];

    return true;
}


bool ComputerChessPlayer::WhiteOpening ( 
    ChessBoard &board,
    Move       &bestmove )
{
    if ( !openingBookSearchEnabled )
        return false;

    if ( board.HasBeenEdited() )
        return false;

    bool foundMove = false;

    big nodeIndex = 0;
    if ( OB_FindContinuation ( board, nodeIndex ) )
        if ( OB_PickMove ( nodeIndex, bestmove, board ) )
            foundMove = true;

    if ( foundMove )
    {
        // Sanity check!
        if ( !rootml.IsLegal ( bestmove ) )
            foundMove = false;

        bestmove.score = 0;
    }

    return foundMove;
}


bool ComputerChessPlayer::BlackOpening ( 
    ChessBoard &board,
    Move       &bestmove )
{
    if ( !openingBookSearchEnabled )
        return false;

    if ( board.HasBeenEdited() )
        return false;

    bool foundMove = false;

    big nodeIndex;
    if ( OB_FindContinuation ( board, nodeIndex ) )
        if ( OB_PickMove ( nodeIndex, bestmove, board ) )
            foundMove = true;

    if ( foundMove )
    {
        // Sanity check!
        if ( !rootml.IsLegal ( bestmove ) )
            foundMove = false;

        bestmove.score = 0;
    }

    return foundMove;
}


/*
    $Log: opening.cpp,v $
    Revision 1.4  2006/01/18 19:58:13  dcross
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
    
    1999 January 5 [Don Cross]
         Updating coding style.
    
    1994 February 15 [Don Cross]
         Implementing true opening library using OBT (Open Book Translator).
    
    1993 October 19 [Don Cross]
         Started writing.  The first version just knows about P-K4, P-K4
         kind of stuff.
    
*/

