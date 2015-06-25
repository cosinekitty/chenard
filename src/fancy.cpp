/*=============================================================

    fancy.cpp  -  Copyright (C) 1993-2015 by Don Cross
    http://cosinekitty.com/chenard

    ParseMove() is a function to recognize and interpret
    two different formats of chess move notation:

    - Portable Game Notation (PGN), with some tolerance
      of slight variations that other software may produce.

    - Longmove notation, for example: e2e4 (most moves)
      or e7e8q (pawn promotion to Queen).

    If the notation is valid AND it represents a legal chess
    move in the given board position, the function returns
    true and sets the following output parameters correctly:
        source, dest, promIndex, move

    Otherwise, all the output parameters are set to zero
    and the function returns false.

=============================================================*/

#include <string.h>

#include "chess.h"

bool ParseMove(
    const char  *string,
    ChessBoard  &board,
    int         &source,
    int         &dest,
    SQUARE      &promIndex,
    Move        &move)         // redundant with (source,dest,promIndex), but may be more immediately useful
{
    if (string == NULL) 
    {
        goto bad_move_notation;
    }

    MoveList ml;
    int numMatchingMoves = 0;

    if (string[0] >= 'a' && string[0] <= 'h' &&
        string[1] >= '1' && string[1] <= '8' &&
        string[2] >= 'a' && string[2] <= 'h' &&
        string[3] >= '1' && string[3] <= '8' &&
        (string[4] == '\0' || string[5] == '\0'))
    {
        source = OFFSET(string[0] - 'a' + 2, string[1] - '1' + 2);
        dest = OFFSET(string[2] - 'a' + 2, string[3] - '1' + 2);

        switch (string[4]) {
        case '\0':
            promIndex = P_INDEX;    // numerically 0
            break;

        case 'q':
            promIndex = Q_INDEX;
            break;

        case 'r':
            promIndex = R_INDEX;
            break;

        case 'b':
            promIndex = B_INDEX;
            break;

        case 'n':
            promIndex = N_INDEX;
            break;

        default:
            goto bad_move_notation;     // cannot possibly be a valid PGN formatted move, so give up
        }

        // Generate all legal moves from this position and see if we can find the same
        // source/destination pair and pawn promotion.  If exactly one move matches,
        // we have found a legal move.
        board.GenMoves(ml);
        for (int i = 0; i < ml.num; ++i)
        {
            int mSource;
            int mDest;
            SQUARE mProm = ml.m[i].actualOffsets(board, mSource, mDest);
            SQUARE mPromIndex = UPIECE_INDEX(mProm);
            if ((source == mSource) && (dest == mDest) && (promIndex == mPromIndex))
            {
                if (++numMatchingMoves != 1)
                {
                    ChessFatal("Longmove ambiguity in move parser.");
                    goto bad_move_notation;     // on some platforms we might not exit immediately from ChessFatal().
                }
                move = ml.m[i];
            }
        }

        if (numMatchingMoves != 1)
        {
            goto bad_move_notation;
        }

        return true;
    }

    size_t  stringLength = strlen(string);
    if (stringLength < 2 || stringLength > 7) 
    {
        goto bad_move_notation;   // the length of this string is incompatible with any possible PGN formatted move!
    }

    // http://www.very-best.de/pgn-spec.htm  Section 8.2.3 includes the following:
    //
    //    "Neither the appearance nor the absence of either a check or checkmating indicator 
    //     is used for disambiguation purposes. This means that if two (or more) pieces of the 
    //     same type can move to the same square the differences in checking status of the moves 
    //     does not allieviate the need for the standard rank and file disabiguation described above."
    //
    // To be a little bit on the tolerant side, we will match moves whether or not 'string' has a '+' or '#' on the end,
    // since this cannot possibly lead to ambiguity in move notation.
    // To implement this parser tolerance, we will strip off '+' or '#' whenever it appears on either
    // the input value 'string', or in the generated local variable 'pgn'.

    char copy[8];              // we just checked length of 'string', so...
    strcpy(copy, string);      // ... this is perfectly safe!
    switch (copy[stringLength - 1]) 
    {
    case '+':
    case '#':
        copy[stringLength - 1] = '\0';
        break;
    }

    char pgn[MAX_MOVE_STRLEN + 1];

    board.GenMoves(ml);
    for (int i = 0; i < ml.num; ++i) 
    {
        // Convert move to PGN...
        FormatChessMove(board, ml, ml.m[i], pgn);
        // We assume the PGN generator (FormatChessMove) is working correctly,
        // meaning that it will not generate the same PGN notation for two distinct chess moves.

        size_t pgnLength = strlen(pgn);
        if (pgnLength >= 2 && pgnLength <= 7) 
        {
            switch (pgn[pgnLength - 1]) 
            {
            case '+':
            case '#':
                pgn[pgnLength - 1] = '\0';
                break;
            }
        }
        else 
        {
            ChessFatal("Invalid PGN length in move parser.");
            goto bad_move_notation;     // on some platforms we might not exit immediately from ChessFatal().
        }

        if (0 == strcmp(copy, pgn)) 
        {
            SQUARE piece = ml.m[i].actualOffsets(board, source, dest);

            if (++numMatchingMoves != 1) 
            {
                ChessFatal("PGN ambiguity in move parser.");
                goto bad_move_notation;     // on some platforms we might not exit immediately from ChessFatal().
            }

            promIndex = UPIECE_INDEX(piece);
            move = ml.m[i];
        }
    }

    if (numMatchingMoves == 1) 
    {
        return true;
    }
    else 
    {
        // I have seen cases where a PGN file generated by other software
        // will have a move like "Ngf3", even though "Nf3" was unambiguous.
        // Let's try to hack around that here by checking for that case and using recursion.
        // Note that in a case like "Na1b3", we may recurse twice:  "Na1b3" -> "N1b3" -> "Nb3".
        // If deleting the second character generates a truly ambiguous move, it will never match
        // any PGN string we generate above in the comparison loop, so we will always return
        // false after either 1 or 2 recursions.
        if (numMatchingMoves == 0) 
        {    
            // didn't match anything at all, even disregarding '+' or '#' at end
            if (stringLength >= 4) 
            {    
                // long enough to contain an unnecessary source rank/file/square disambiguator
                if (string[0] == 'N' || string[0] == 'B' || string[0] == 'R' || string[0] == 'Q' || string[0] == 'K') 
                {     
                    // not moving a pawn
                    if ((string[1] >= 'a' && string[1] <= 'h') || (string[1] >= '1' && string[1] <= '8')) 
                    {     
                        // rank or file in second character
                        char shorterString[8];                      // we made sure string had a length in the range 2..7 above
                        shorterString[0] = string[0];               // keep first character
                        strcpy(&shorterString[1], &string[2]);      // skip over second character but keep rest of string
                        return ParseMove(shorterString, board, source, dest, promIndex, move);
                    }
                }
            }
        }

    bad_move_notation:
        // Getting here means the move notation is invalid, malformed, or illegal.
        // Clear all output parameters and return false.
        // If the caller is ill-behaved and ignores the return value, 
        // at least give them output parameters that don't match any legal move.
        // This is especially important when numMatchingMoves > 1,
        // because we have already set the output parameters to the most recent matching move
        // (assuming ChessFatal has not yet ended execution: it can behave differently on different platforms).
        source = 0;
        dest = 0;
        promIndex = 0;
        memset(&move, 0, sizeof(Move));
        return false;
    }
}


/*
    $Log: fancy.cpp,v $
    Revision 1.6  2008/11/24 11:21:58  Don.Cross
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

    Revision 1.5  2008/11/06 22:25:06  Don.Cross
    Now ParseFancyMove accepts either PGN or longmove (e7e8q) style notation only.
    We tolerate of check '+' or checkmate '#' characters being absent or extraneous in the input.
    The former is especially important because I often see PGN notation where '+' is missing on checking moves.
    To allow for PGN input, I had to allow avoidance of the PromotePawn callback, so other code was changed as well.
    Version numbers are now always constently yyyy.mm.dd across all builds of Chenard!

    Revision 1.4  2006/01/18 19:58:11  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.3  2005/11/23 21:30:29  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:39  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!


    Revision history:

    1996 January 1 [Don Cross]
    Started writing.

*/

