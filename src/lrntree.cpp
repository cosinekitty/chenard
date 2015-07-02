/*==============================================================================

    lrntree.cpp  -  Copyright (C) 1998-2005 by Don Cross

    This is a new module for learning new openings.
    This file replaces the old file 'learn.cpp'.
    Now, instead of reading the entire tree into memory, we encapsulate
    direct I/O to a binary file which contains the tree.  This saves
    a lot of dynamic memory and prevents us from having to read and write
    the entire tree all at once.

=============================================================================*/

// standard includes...
#ifdef _WIN32
// for setting idle priority
    #include <windows.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

// project includes...
#include "chess.h"
#include "lrntree.h"
#include "gamefile.h"

// Sadly, the following must come after include of chess.h, 
// even though this is a <conio.h> is a system header on Windows,
// because chess.h is where I determine the value of CHENARD_LINUX.
#if !CHENARD_LINUX
    #include <conio.h>
#endif


int Learn_Output = 0;


LearnTree::~LearnTree()    
{
    close();
}


int LearnTree::create ( const char *filename )
{
    close();
    f = fopen ( filename, "w+b" );
    if (!f)
        return 0;

    return 1;
}


int LearnTree::open ( const char *filename )
{
    close();
    f = fopen ( filename, "r+b" );
    if (!f)
        return 0;   // failure

    return 1;  // success
}


int LearnTree::flush()
{
    if (!f)
        return 0;    // failure

    fflush(f);
    return 1;   // success
}


int LearnTree::close()
{
    if (f) {
        if (fclose(f) == EOF) {
            f = 0;
            return 0;
        }
        f = 0;
    }

    return 1;   // success
}


INT32 LearnTree::numNodes()
{
    fseek ( f, 0, SEEK_END );
    long pos = ftell(f);
    return pos / LearnBranchFileSize;
}


int LearnTree::read ( INT32 offset, LearnBranch &branch )
{
    if (!f)
        return 0;   // failure

    if (fseek ( f, offset * LearnBranchFileSize, SEEK_SET ) != 0)
        return 0;   // failure

    if (!read (branch.move))
        return 0;

    if (!read (branch.timeAnalyzed))
        return 0;

    if (!read (branch.winsAndLosses))
        return 0;

    if (!read (branch.child))
        return 0;

    if (!read (branch.sibling))
        return 0;

    if (!read (branch.nodesEvaluated))
        return 0;

    if (!read (branch.numAccesses))
        return 0;

    for (size_t i=0; i < sizeof(branch.reserved)/sizeof(INT32); i++) {
        if (!read (branch.reserved[i]))
            return 0;
    }

    return 1;   // success
}


int LearnTree::write ( INT32 offset, const LearnBranch &branch )
{
    if (!f)
        return 0;   // failure

    if (fseek ( f, offset * LearnBranchFileSize, SEEK_SET ) != 0)
        return 0;   // failure

    if (!write (branch.move))
        return 0;

    if (!write (branch.timeAnalyzed))
        return 0;

    if (!write (branch.winsAndLosses))
        return 0;

    if (!write (branch.child))
        return 0;

    if (!write (branch.sibling))
        return 0;

    if (!write (branch.nodesEvaluated))
        return 0;

    if (!write (branch.numAccesses))
        return 0;

    for (size_t i=0; i < sizeof(branch.reserved)/sizeof(INT32); i++) {
        if (!write (branch.reserved[i]))
            return 0;
    }

    return 1;   // success
}


int LearnTree::append ( INT32 &offset, const LearnBranch &branch )
{
    if (!f)
        return 0;

    fflush(f);

    if (fseek ( f, 0, SEEK_END ) != 0)    // seek to end of file
        return 0;

    long pos = ftell(f);         // get current file location (file size)
    if (pos == -1)
        return 0;

    assert ( pos % LearnBranchFileSize == 0 );
    offset = pos / LearnBranchFileSize;
    return write ( offset, branch );
}


int LearnTree::write ( INT32 x )
{
    if (fputc ( x & 0xff, f ) == EOF)          return 0;
    if (fputc ( (x >> 8) & 0xff, f ) == EOF)   return 0;
    if (fputc ( (x >> 16) & 0xff, f ) == EOF)  return 0;
    if (fputc ( (x >> 24) & 0xff, f ) == EOF)  return 0;
    return 1;
}


int LearnTree::read ( INT32 &x )
{
    int a = fgetc(f);
    int b = fgetc(f);
    int c = fgetc(f);
    int d = fgetc(f);
    if (a==EOF || b==EOF || c==EOF || d==EOF) {
        x = -1;
        return 0;
    }

    unsigned long aa = unsigned(a) & 0xff;
    unsigned long bb = unsigned(b) & 0xff;
    unsigned long cc = unsigned(c) & 0xff;
    unsigned long dd = unsigned(d) & 0xff;

    x = aa | (bb<<8) | (cc<<16) | (dd<<24);
    return 1;
}


int LearnTree::write ( Move m )
{
    if (fputc ( m.source, f ) == EOF)               return 0;
    if (fputc ( m.dest, f ) == EOF)                 return 0;
    if (fputc ( m.score & 0xff, f ) == EOF)         return 0;
    if (fputc ( (m.score >> 8) & 0xff, f ) == EOF)  return 0;
    return 1;
}


int LearnTree::read ( Move &m )
{
    int a = fgetc(f);
    int b = fgetc(f);
    int c = fgetc(f);
    int d = fgetc(f);

    if (a==EOF || b==EOF || c==EOF || d==EOF) {
        m.source = 0;
        m.dest = 0;
        m.score = 0;
        return 0;
    }
    m.source = a;
    m.dest   = b;
    m.score = c | (d<<8);
    return 1;
}


int LearnTree::insert (
    LearnBranch &branch,
    INT32 parent,
    INT32 &offset )
{
    branch.sibling   =  -1;
    branch.child     =  -1;
    if (!append ( offset, branch )) return 0;

    LearnBranch tbranch;
    if (parent == -1) {
        if (offset != 0) {
            // Special case: link new node as sibling to node at offset 0
            if (!read ( 0, tbranch )) return 0;
            branch.sibling = tbranch.sibling;
            tbranch.sibling = offset;
            if (!write ( 0, tbranch )) return 0;
        } else {
            // This is the first node to be written to the tree.
            // No need to do anything else.
        }
    } else {
        // Need to backpatch parent to point at this...
        if (!read ( parent, tbranch )) return 0;
        branch.sibling = tbranch.child;
        tbranch.child = offset;
        if (!write ( parent, tbranch )) return 0;
    }

    return write ( offset, branch );
}


//--------------------- high level functions ----------------------


#ifndef LEARNTREE_NO_HIGH_LEVEL

static INT32 ScoreBranch ( const LearnBranch &b, bool whiteToMove )
{
    INT32 score = b.timeAnalyzed / 100;
    const INT32 gameValue = 5;

    if (whiteToMove)
        score += gameValue * b.winsAndLosses;
    else
        score -= gameValue * b.winsAndLosses;

    if (score < 0)
        score = 0;

    return score;
}


bool LearnTree::locateAndReadBranch (
    ChessBoard      &board,
    Move            moveToFind,
    LearnBranch     &branch )
{
    if (board.HasBeenEdited())
        return false;

    int n = board.GetCurrentPlyNumber();
    INT32 offset = 0;

    int i;
    for (i=0; i<n; i++) {
        Move move = board.GetPastMove(i);

        bool foundContinuation = false;
        while (offset != -1 && !foundContinuation) {
            if (!read ( offset, branch ))
                return false;

            if (branch.move == move)
                foundContinuation = true;
            else
                offset = branch.sibling;
        }

        if (!foundContinuation)
            return false;

        offset = branch.child;
    }

    // Search for the target move in the list of siblings

    while (offset != -1) {
        if (!read ( offset, branch ))
            offset = -1;
        else {
            if (branch.move == moveToFind)
                return true;

            offset = branch.sibling;
        }
    }

    return false;
}



bool LearnTree::familiarPosition (
    ChessBoard &board,
    Move &bestmove,
    long timeLimit,
    const MoveList &legalMoves )
{
    if (board.HasBeenEdited())
        return false;

    if (!open(DEFAULT_CHENARD_TREE_FILENAME))
        return false;

    int n = board.GetCurrentPlyNumber();
    INT32 offset = 0;
    LearnBranch branch;

    int i;
    for (i=0; i<n; i++) {
        Move move = board.GetPastMove(i);

        bool foundContinuation = false;
        while (offset != -1 && !foundContinuation) {
            if (!read ( offset, branch ))
                return false;

            if (branch.move == move)
                foundContinuation = true;
            else
                offset = branch.sibling;
        }

        if (!foundContinuation)
            return false;

        offset = branch.child;
    }

    // Find the maximum time we have thought about this position.

    INT32 maxTime = 0;
    int numChildren = 0;
    INT32 scores [128];
    Move  moves [128];
    INT32 keepOffset [128];
    INT32 total = 0;

    while (offset != -1) {
        if (!read ( offset, branch ))
            offset = -1;
        else {
            total += scores[numChildren] = ScoreBranch(branch,board.WhiteToMove());
            moves[numChildren] = branch.move;
            keepOffset[numChildren] = offset;
            ++numChildren;

            if (branch.timeAnalyzed > maxTime)
                maxTime = branch.timeAnalyzed;

            offset = branch.sibling;
        }
    }

    if (maxTime >= timeLimit && total > 0) {
        // Choose randomly between the moves with probabilities
        // proportional to the max amount of time Chenard has ever
        // spent deciding that move was best.  This is a kind of credibility
        // factor which should allow play that improves as max training time
        // is progressively increased.

        INT32 r = ChessRandom(total);
        for (i=0; i<numChildren; i++) {
            r -= scores[i];
            if (r < 0) {
                bestmove = moves[i];

                // Must do sanity check to make sure file has not been corrupted.
                if (legalMoves.IsLegal(bestmove)) {
                    // Update access stats for this branch.
                    if (read ( keepOffset[i], branch )) {
                        ++branch.numAccesses;
                        write ( keepOffset[i], branch );
                    }

                    return true;
                } else
                    return false;   // If we get here, it means corrupted experience!!!
            }
        }
    }

    return false;
}


void LearnTree::absorbFile (const char *gameFileName)
{
    // Figure out which kind of file it is.
    const char *ext = strrchr (gameFileName, '.');
    if (ext == NULL) {
        if (Learn_Output) {
            printf ("The filename '%s' has no extension.\n", gameFileName);
        }
    } else {
        if (0 == stricmp (ext, ".pgn")) {
            absorbPgnFile (gameFileName);
        } else if (0 == stricmp (ext, ".gam")) {
            absorbGameFile (gameFileName);
        } else {
            if (Learn_Output) {
                printf ("Unknown game file extension in filename '%s'\n", gameFileName);
            }
        }
    }
}


void LearnTree::absorbPgnFile (const char *pgnFileName)
{
    if (Learn_Output) {
        printf ("PGN file: %s ", pgnFileName);
    }

    FILE *infile = fopen (pgnFileName, "rt");
    if (infile) {
        Move            move;
        UnmoveInfo      unmove;
        ChessBoard      board;
        PgnExtraInfo    info;
        PGN_FILE_STATE  state;
        char            movestr [1 + MAX_MOVE_STRLEN];
        int             updates  = 0;
        int             branches = 0;
        int             column = 0;
        long            timeEquivalent = 0;
        unsigned long   nodesEquivalent = 0;

        const int  MIN_ELO_FOR_TRUST = 2500;
        const long STRONG_PLAYER_TIME_EQUIVALENT = 120L * 100L;     // 2 minutes
        const unsigned long STRONG_PLAYER_NODES_EQUIVALENT = STRONG_PLAYER_TIME_EQUIVALENT * 4000;     // (400,000 nodes/second) * (1 second / 100 cs)

        for(;;) {
            if (GetNextPgnMove (infile, movestr, state, info)) {
                if (state == PGN_FILE_STATE_NEWGAME) {
                    if (info.fen[0]) {
                        // We can't handle edited board positions!
                        if (Learn_Output) {
                            printf ("\nIgnoring file that contains edited board positions (FEN)\n");
                        }
                        break;
                    }
                    column = printf ("\nNew Game:  ") - 1;
                    board.Init();       // start over with a new chess board

                    // See if this is a game between very strong players.
                    // If so, pretend Chenard thought about this for a long time.
                    // The comparison logic looks at both nodes evaluated and think time, so fake them both out.
                    // We don't care whether the players won/lost/drawed, only that they are both very strong.
                    // A game lost by a grandmaster is still invariably stronger play than Chenard is capable of!
                    if (info.whiteElo >= MIN_ELO_FOR_TRUST && info.blackElo >= MIN_ELO_FOR_TRUST) {
                        timeEquivalent  = STRONG_PLAYER_TIME_EQUIVALENT;
                        nodesEquivalent = STRONG_PLAYER_NODES_EQUIVALENT;
                    } else {
                        timeEquivalent  = 0;
                        nodesEquivalent = 0;
                    }
                }

                // See if we can figure out what this move is.
                if (ParseFancyMove (movestr, board, move)) {
                    if (board.GetCurrentPlyNumber() <= static_cast<int>(MaxLearnDepth)) {     // stop trying to learn positions, but keep scanning moves, so we find the next game(s)
                        if (Learn_Output) {
                            if (board.GetCurrentPlyNumber() > 0) {
                                column += printf ( ", " );
                            }

                            if (column > 65) {
                                column = 0;
                                printf ( "\n" );
                            }
                        }

                        if (Learn_Output) {
                            column += printf ( "%s", movestr );
                        }
                        int result = rememberPosition ( board, move, timeEquivalent, nodesEquivalent, 1, 0 );
                        if (result == 0) {
                            if (Learn_Output) {
                                printf ( "\nWARNING:  could not add move to tree.\n" );
                            }
                            break;
                        } else if (result == 1)
                            ++updates;
                        else if (result == 2) {
                            ++branches;
                            if (Learn_Output) {
                                column += printf ( "@" );   // indicated added move
                            }
                        } else {
                            if (Learn_Output) {
                                column += printf ( "(?%d)", result );
                            }
                        }
                    }

                    board.MakeMove (move, unmove);
                } else {
                    if (Learn_Output) {
                        printf ("\nUnrecognized or illegal move '%s'\n", movestr);
                        break;
                    }
                }
            } else {
                if (state != PGN_FILE_STATE_GAMEOVER) {
                    break;      // nothing left in this PGN file, or we encountered a syntax error
                }
            }
        }

        if (Learn_Output) {
            printf ("\n");
        }

        fclose (infile);
        infile = NULL;
    } else {
        if (Learn_Output) {
            printf ("NOT opened!\n");
        }
    }
}


void LearnTree::absorbGameFile ( const char *gameFilename )
{
    if (Learn_Output) {
        printf ( "Game file: %s ", gameFilename );
    }

    FILE *gameFile = fopen ( gameFilename, "rb" );
    if (!gameFile) {
        if (Learn_Output) {
            printf ( "NOT opened!\n" );
        }
        return;
    }

    if (Learn_Output) {
        printf ( "opened...\n{ " );
    }

    int column = 2;
    ChessBoard board;
    MoveList legal;
    Move move;
    UnmoveInfo unmove;
    unsigned ply = 0;
    char moveString [MAX_MOVE_STRLEN + 1];
    int updates=0, branches=0;

    for (;;) {
        board.GenMoves (legal);
        if (legal.num == 0)
            break;

        int c1 = fgetc(gameFile);
        int c2 = fgetc(gameFile);
        int c3 = fgetc(gameFile);
        int c4 = fgetc(gameFile);
        if (c4 == EOF)
            break;

        move.source = BYTE(c1);
        move.dest   = BYTE(c2);
        move.score  = SCORE ( (c4 << 8) | c3 );

        if (!legal.IsLegal(move)) {
            if (Learn_Output) {
                printf (
                    "\nWARNING:  Illegal move source=%d dest=%d score=%d\n",
                    int(move.source),
                    int(move.dest),
                    int(move.score) );
            }

            break;
        }

        if (Learn_Output) {
            if (ply > 0)
                column += printf ( ", " );

            if (column > 65) {
                column = 0;
                printf ( "\n" );
            }
        }

        FormatChessMove ( board, move, moveString );
        if (Learn_Output)
            column += printf ( "%s", moveString );

        int result = rememberPosition ( board, move, 0, 0, 1, 0 );
        if (result == 0) {
            if (Learn_Output)
                printf ( "\nWARNING:  could not add move to tree.\n" );
            break;
        } else if (result == 1)
            ++updates;
        else if (result == 2) {
            ++branches;
            if (Learn_Output)
                column += printf ( "@" );   // indicated added move
        } else {
            if (Learn_Output)
                column += printf ( "(?%d)", result );
        }

        board.MakeMove (move, unmove);
        if (++ply > MaxLearnDepth)
            break;
    }

    if (Learn_Output)
        printf ( " }\n\n" );

    fclose (gameFile);
}


int LearnTree::rememberPosition (
    ChessBoard      &board,
    Move            bestmove,
    long            timeLimit,
    unsigned long   nodesEvaluated,
    unsigned long   numAccesses,
    long            winsAndLosses )
{
    if (board.HasBeenEdited())
        return 0;

    if (!board.isLegal(bestmove))
        return 0;   // safety valve to avoid corrupting the tree with junk

    int n = board.GetCurrentPlyNumber();
    if (n > static_cast<int>(MaxLearnDepth))
        return 0;

    if (!f) {
        if (!open(DEFAULT_CHENARD_TREE_FILENAME)) {
            // Assume this is Chenard's maiden voyage on this computer.
            // Create a brand new, empty tree...

            if (!create(DEFAULT_CHENARD_TREE_FILENAME))
                return 0;   // Ain't nothin' we can do!
        }
    }

    // Traverse path to current position, creating missing branches as we go.

    LearnBranch branch;
    INT32 offset = 0;
    INT32 parent = -1;
    for (int i=0; i<n; i++) {
        INT32 child = -1;
        bool foundReply = false;
        Move move = board.GetPastMove(i);
        while (offset != -1 && !foundReply) {
            if (read ( offset, branch )) {
                if (branch.move == move) {
                    foundReply = true;
                    child = branch.child;
                } else
                    offset = branch.sibling;
            } else {
                assert ( offset == 0 );   // otherwise the file is corrupt
                child = offset = -1;
            }
        }

        if (!foundReply) {
            // Add an intermediate branch leading toward the branch we want to insert.
            // (Assume we won't get here in the tree packer.)
            // Need to add new node now because it doesn't exist already.
            branch.move             = move;
            branch.timeAnalyzed     = 0;     // computer did NOT think about this
            branch.nodesEvaluated   = 0;
            branch.numAccesses      = 0;
            branch.winsAndLosses    = 0;
            insert ( branch, parent, offset );   // sets 'offset' to new node
            child = -1;
        }

        parent = offset;
        offset = child;
    }

    // Search for this move in this level of the tree.
    // If we find it, update it if necessary.
    // Otherwise, add a new branch.

    while (offset != -1) {
        if (!read ( offset, branch ))
            break;

        if (branch.move == bestmove) {
            // We found it!  Update only if new think time is greater.

            bool newNodeBetter;
            if (branch.nodesEvaluated > 0)
                newNodeBetter = INT32(nodesEvaluated) > branch.nodesEvaluated;
            else
                newNodeBetter = timeLimit > branch.timeAnalyzed;

            if (newNodeBetter) {
                branch.timeAnalyzed     = timeLimit;
                branch.move.score       = bestmove.score;
                branch.nodesEvaluated   = nodesEvaluated;
                branch.numAccesses      = numAccesses;
                branch.winsAndLosses    = winsAndLosses;
                ++branch.numAccesses;
                write ( offset, branch );
                flush();
            }

            return 1;
        }

        offset = branch.sibling;
    }

    branch.move             = bestmove;
    branch.timeAnalyzed     = timeLimit;
    branch.nodesEvaluated   = nodesEvaluated;
    branch.numAccesses      = numAccesses;
    branch.winsAndLosses    = winsAndLosses;
    insert ( branch, parent, offset );
    flush();

    return 2;
}


void LearnTree::learnFromGame (
    ChessBoard &board,
    ChessSide winner )
{
    if (winner == SIDE_NEITHER) {
        // Currently, we ignore draw games.
        // This might change in the future, so callers should
        // not assume this behavior.

        return;
    }

    if (!open(DEFAULT_CHENARD_TREE_FILENAME))  return;

    LearnBranch branch;
    INT32 score = (winner == SIDE_WHITE) ? 1 : -1;
    INT32 offset = 0;
    const int numPlies = board.GetCurrentPlyNumber();
    for (int ply=0; ply < numPlies; ply++) {
        Move move = board.GetPastMove(ply);

        // search for this move in the current location of the tree.

        bool foundMove = false;
        while (offset != -1) {
            if (!read(offset,branch))
                return;

            if (branch.move == move) {
                branch.winsAndLosses += score;
                write ( offset, branch );
                foundMove = true;
                offset = branch.child;
                break;
            } else
                offset = branch.sibling;
        }

        if (!foundMove)
            break;   // We didn't find the continuation, so give up
    }

    flush();
}


//---------------------------- tree trainer -----------------------------

INT32  NodesAtDepthTable [MaxLearnDepth+16];



INT32 LearnTree::numNodesAtDepth (
    INT32       root,
    int         targetDepth,
    int         currentDepth,
    INT32       searchTime,
    ChessBoard  &board )
{
    LearnBranch branch;
    INT32 total = 0;
    MoveList ml;
    UnmoveInfo unmove;

    board.GenMoves(ml);

    if (currentDepth < targetDepth) {
        while (root != -1) {
            if (!read(root,branch))
                return 0;

            if (branch.child != -1 && ml.IsLegal(branch.move)) {
                board.MakeMove ( branch.move, unmove );

                total += numNodesAtDepth (
                    branch.child,
                    targetDepth,
                    currentDepth + 1,
                    searchTime,
                    board );

                board.UnmakeMove ( branch.move, unmove );
            }

            root = branch.sibling;
        }
    } else if (targetDepth == currentDepth) {
        while (root != -1) {
            if (!read(root,branch))
                return 0;

            if (branch.timeAnalyzed >= searchTime && ml.IsLegal(branch.move))
                return 0;

            root = branch.sibling;
        }

        total = 1;
    }

    return total;
}


static void EstimateRemainingTime ( int currentDepth, double thinkTime )
{
    long seconds = long(thinkTime * NodesAtDepthTable[currentDepth]);
    long minutes = seconds/60;
    seconds %= 60;
    long hours = minutes/60;
    minutes %= 60;
    long days = hours/24;
    hours %= 24;

    printf ( "(" );

    if (days > 0)
        printf ( "%ld day%s + ", days, (days==1 ? "" : "s") );

    printf ( "%02ld:%02ld:%02ld)", hours, minutes, seconds );

    fflush ( stdout );
}


void LearnTree::checkForPause()
{
#if !CHENARD_LINUX
    char garbage [64];
    if (_kbhit()) {
        int key = _getch();
        if (key == 'p') {
            printf("**** PAUSED ****  Press Enter to continue.  Enter 'q' to quit.\n");
            fflush (stdin);
            if (fgets (garbage, sizeof(garbage), stdin)) {
                if (0 == strcmp(garbage,"q\n")) {
                    if (close()) {
                        printf("Successfully closed file.\n");
                    } else {
                        printf("!!! ERROR CLOSING FILE !!!\n");
                    }
                    exit(0);
                }
            }
        } else {
            printf("**** KEY IGNORED *****  Press 'p' to pause after next move completes.\n");
        }
    }
#endif

    // Look for remote signal to quit via 'lrntree.close'...
    static const char * const SignalFileName = "lrntree.close";
    FILE *signal = fopen (SignalFileName, "rb");
    if (signal) {
        printf("**** FOUND SIGNAL FILE %s ****\n", SignalFileName);
        fclose (signal);
        signal = NULL;
        if (close()) {
            printf("Successfully closed file.\n");
            remove (SignalFileName);    // delete signal file to let signaller know that chenard.trx is closed now.
            exit(0);
        } else {
            printf("\x07!!! ERROR CLOSING FILE !!!\x07\n");     // beeps might help get attention
        }
    }
}


void LearnTree::trainDepth (
    INT32 offset,
    INT32 timeLimit,
    ChessUI &ui,
    ComputerChessPlayer *whitePlayer,
    ComputerChessPlayer *blackPlayer,
    ChessBoard &board,
    int targetDepth,
    int currentDepth )
{
    LearnBranch branch;
    MoveList ml;
    board.GenMoves (ml);
    if (ml.num == 0 || board.IsDefiniteDraw())
        return;

    if (currentDepth < targetDepth) {
        while (offset != -1) {
            if (!read(offset,branch))
                return;

            UnmoveInfo unmove;
            Move move = branch.move;
            if (ml.IsLegal(move)) {
                board.MakeMove ( move, unmove );

                trainDepth (
                    branch.child,
                    timeLimit,
                    ui,
                    whitePlayer,
                    blackPlayer,
                    board,
                    targetDepth,
                    1 + currentDepth );

                board.UnmakeMove ( move, unmove );
            }

            offset = branch.sibling;
        }
    } else if (currentDepth == targetDepth) {
        if (Learn_Output && NodesAtDepthTable[targetDepth]>0) {
            printf("\r###  depth=%d --- Remaining nodes at this depth: %-10d",
                targetDepth,
                (int) NodesAtDepthTable[targetDepth] );

            fflush (stdout);
            EstimateRemainingTime ( targetDepth, 0.01*timeLimit );
        }

        // Before we spend time analyzing this position,
        // make sure it hasn't been done so well (or better)
        // before now.

        while (offset != -1) {
            if (!read(offset,branch))
                return;

            if (branch.timeAnalyzed >= timeLimit) {
                // Don't bother...it's already been done as well or better.
                return;
            }

            offset = branch.sibling;
        }

        if (Learn_Output) {
            printf ( "\n" );
            fflush (stdout);
            checkForPause();
        }

        --NodesAtDepthTable [targetDepth];

        ui.DrawBoard ( board );
        INT32 timeSpent = 0;
        Move bestmove;
        UINT32 nodesEvaluated = 0;

        if (board.WhiteToMove()) {
            whitePlayer->GetMove ( board, bestmove, timeSpent );
            nodesEvaluated = whitePlayer->queryNodesEvaluated();
        } else {
            blackPlayer->GetMove ( board, bestmove, timeSpent );
            nodesEvaluated = blackPlayer->queryNodesEvaluated();
        }

        char moveString [MAX_MOVE_STRLEN + 1];
        FormatChessMove ( board, bestmove, moveString );
        if (Learn_Output) {
            printf ( ">>> Best move = [%s]    score = %d\n", 
                moveString, int(bestmove.score) );
            fflush (stdout);
        }

        // See if this move matches any existing replies...
        static unsigned long NumPositionsFinished = 0;
        static unsigned long NumBranches = 0;
        static unsigned long NumUpdates = 0;

        int rc = rememberPosition (board, bestmove, timeLimit, nodesEvaluated, 1, 0);

        ++NumPositionsFinished;
        if (rc == 1) {
            ++NumUpdates;
            if (Learn_Output) {
                printf ( ">>> Just modified an existing branch.\n" );
                fflush (stdout);
            }
        } else if (rc == 2) {
            ++NumBranches;
            ++NodesAtDepthTable [targetDepth + 1];
            if (Learn_Output) {
                printf ( ">>> Just added a new branch!\n" );
                fflush (stdout);
            }
        }

        if (Learn_Output) {
            printf ( ">>> finished=%lu  branches=%lu  updates=%lu\n",
                NumPositionsFinished,
                NumBranches,
                NumUpdates );

            fflush (stdout);
        }
    }
}


void LearnTree::trainer (
    ComputerChessPlayer *whitePlayer,
    ComputerChessPlayer *blackPlayer,
    ChessBoard          &theBoard,
    ChessUI             &theUserInterface,
    INT32               timeLimit )
{
    printf ( "Entering tree trainer!\n\n" );
    fflush (stdout);
    if (!open(DEFAULT_CHENARD_TREE_FILENAME)) {
        printf ( "Cannot open training file '%s'\n", DEFAULT_CHENARD_TREE_FILENAME );
        fflush (stdout);
        return;
    }

    UINT32 totalNodes = numNodes();
    printf ( "Number of nodes in tree = %lu\n", (unsigned long)totalNodes );

    printf ( "[ " );
    unsigned d;
    for (d=0; d <= MaxLearnDepth+1; d++) {
        NodesAtDepthTable[d] = numNodesAtDepth ( 0, d, 0, timeLimit, theBoard );
        printf ( "%lu ", (unsigned long)(NodesAtDepthTable[d]) );
        fflush (stdout);
    }

    printf ( "]\n\nStarting training...\n\n" );

#ifdef _WIN32
    // Set thread priority to IDLE...

    HANDLE hProcess = GetCurrentProcess();
    if (SetPriorityClass(hProcess,IDLE_PRIORITY_CLASS)) {
        printf ( "***  Set process priority class to IDLE\n" );
    } else {
        printf ( "***  Error %ld setting process priority class to IDLE.\n",
            long(GetLastError()) );
    }
#endif

    for (d=0; d <= MaxLearnDepth; d++) {
        if (NodesAtDepthTable[d] > 0) {
            printf ( "###  Examining depth %u\n", d );
            fflush (stdout);

            trainDepth (
                0,
                timeLimit,
                theUserInterface,
                whitePlayer,
                blackPlayer,
                theBoard,
                d,
                0 );
        }
    }
}


int LearnTree::packer (
    INT32       offset,
    LearnTree   &outTree,
    int         depth,
    ChessBoard  &board,
    SCORE       window )
{
    MoveList ml;
    board.GenMoves (ml);
    int rc = 1;

    INT32 prev_offset = -1;

    while (rc && offset != -1) {
        LearnBranch branch;
        if (!read(offset,branch)) {
            printf ( "LearnTree::packer:  cannot read branch at offset %ld (prev=%ld)\n", long(offset), long(prev_offset) );
            //rc = 0;   // WRONG!  Need to return success, so that we keep recovering as much of the tree as possible!
            break;
        } else {
            Move move = branch.move;
            if (ml.IsLegal(move)) {
                outTree.rememberPosition (
                    board,
                    branch.move,
                    branch.timeAnalyzed,
                    branch.nodesEvaluated,
                    branch.numAccesses,
                    branch.winsAndLosses 
                );

                #if 0   /* turn on to enable debugging */
                    LearnBranch branchWritten;
                    if (outTree.locateAndReadBranch(board,branch.move,branchWritten)) {
                        if (branch.move != branchWritten.move)
                            printf ( "Warning:  Read-back branch move different!!!  offset=%ld\n", long(offset) );
    
                        if (branch.nodesEvaluated != branchWritten.nodesEvaluated)
                            printf ( "Warning:  Read-back branch nodesEvaluated different!  offset=%ld\n", long(offset) );
    
                        if (branch.numAccesses != branchWritten.numAccesses)
                            printf ( "Warning:  Read-back branch numAccesses different!  offset=%ld\n", long(offset) );
    
                        if (branch.timeAnalyzed != branchWritten.timeAnalyzed)
                            printf ( "Warning:  Read-back branch timeAnalyzed different!  offset=%ld\n", long(offset) );
    
                        if (branch.winsAndLosses != branchWritten.winsAndLosses)
                            printf ( "Warning:  Read-back branch winsAndLosses different!  offset=%ld\n", long(offset) );
                    } else {
                        printf ( "Warning:  Could not read back branch %ld\n", long(offset) );
                    }
                #endif

                UnmoveInfo unmove;
                board.MakeMove ( move, unmove );

                if (branch.child != -1 && 
                    (branch.nodesEvaluated==0 || 
                    (branch.move.score >= -window && branch.move.score <= window))) {
                    rc = packer (
                        branch.child,
                        outTree,
                        1 + depth,
                        board,
                        window 
                    );
                }

                board.UnmakeMove ( move, unmove );
            } else {
                printf("LearnTree::packer:  Illegal move (%d,%d) at offset %ld\n", move.source, move.dest, long(offset));
            }

            prev_offset = offset;
            offset = branch.sibling;
        }
    }

    return rc;  // success
}


int LearnTree::Pack (
    const char  *inTreeFilename,
    const char  *outTreeFilename,
    SCORE       window )
{
    LearnTree inTree;
    int rc = inTree.open (inTreeFilename);
    if (rc) {
        LearnTree outTree;
        rc = outTree.create (outTreeFilename);
        if (rc) {
            ChessBoard board;
            rc = inTree.packer ( 0, outTree, 0, board, window );
            outTree.close();
        } else {
            printf ( "LearnTree::Pack:  cannot open output file '%s'\n", outTreeFilename );
        }

        inTree.close();
    } else {
        printf ( "LearnTree::Pack: cannot open input file '%s'\n", inTreeFilename );
    }

    return rc;
}



#endif   // LEARNTREE_NO_HIGH_LEVEL


/*
    $Log: lrntree.cpp,v $
    Revision 1.15  2008/12/10 19:00:31  Don.Cross
    Minor fixes to make Linux g++ warnings go away.

    Revision 1.14  2008/12/07 18:25:53  Don.Cross
    Fixed Linux compile error.

    Revision 1.13  2008/12/01 23:08:53  Don.Cross
    Norm Pollock announced on the WinBoard forum that he has updated his PGN database:
        http://www.open-aurec.com/wbforum/viewtopic.php?t=49522
        http://www.hoflink.com/~npollock/chess.html
    This is just what I was looking for, so that I could beef up chenard.trx (training file).
    I modified the PGN scanner to recognize WhiteElo, BlackElo, and Result tags.
    I am using WhiteElo and BlackElo to decide whether a game is trustworthy enough to pretend like Chenard thought
    of the move itself, so that it will play that move in actual games, instead of just as a reference point for opponent moves.
    I am currently in the process of slurping an enormous amount of openings into chenard.trx.
    I decided to move GetNextPgnMove from board.cpp to gamefile.cpp, where it really belongs.

    Revision 1.12  2008/11/24 11:21:58  Don.Cross
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

    Revision 1.11  2008/10/27 21:47:07  Don.Cross
    Made fixes to help people build text-only version of Chenard in Linux:
    1. The function 'stricmp' is called 'strcasecmp' in Linux Land.
    2. Found a fossil 'FALSE' that should have been 'false'.
    3. Conditionally remove some relatively unimportant conio.h code that does not translate nicely in the Linux/Unix keyboard model.
    4. Added a script for building using g++ in Linux.

    Revision 1.10  2006/03/20 20:44:10  dcross
    I finally figured out the weird slowness I have been seeing in Chenard accessing
    its experience tree.  It turns out it only happens the first time it accessed the file
    chenard.tre after a reboot.  It was caused by Windows XP thinking chenard.tre was
    an important system file, because of its extension:
    http://msdn.microsoft.com/library/default.asp?url=/library/en-us/sr/sr/monitored_file_extensions.asp
    We write 36 bytes to the middle of the file, then close it
    So I have changed the filename from chenard.tre to chenard.trx.
    I also added DEFAULT_CHENARD_TREE_FILENAME in lrntree.h so that I can more
    easily rename this again if need be.

    Revision 1.9  2006/01/26 19:29:36  dcross
    Added check for signal file lrntree.close, so that remote batch file can tell tree
     trainer to exit.

    Revision 1.8  2006/01/18 19:58:12  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.7  2006/01/01 20:06:12  dcross
    Finished getting PGN-to-tree slurper working.

    Revision 1.6  2005/12/28 15:25:05  dcross
    1. Fixed a bug in the experience tree packer:
       I ran into some corruption in chenard.tre, apparently left over from an endianness problem
       from running Chenard on a non-Intel platform.  When the packer found an invalid branch.sibling,
       it would stop everything and lose the rest of the tree.  Now it continues and recovers as much of
       the tree as possible.
    2. Display better debug diagnostics when something is fishy inside the tree.
    3. Minor coding style tweaks.

    Revision 1.5  2005/12/19 03:41:09  dcross
    Now we can safely shut down the trainer.  First press 'p' to pause,
    then enter the command 'q' to quit.  This closes the file and exits cleanly.
    I was worried about pressing Ctrl+Break because chenard.tre is kept open,
    though we do flush the file every time we write to it.  I am concerned about
    corruption in the file that seems to be there.  I want to investigate this some
    time and figure out how to clean out the problems, if any are really there.

    Revision 1.4  2005/12/19 00:44:22  dcross
    Added ability to press 'p' to pause tree trainer after the current position is finished.
    Later, I will either need to fix this to work on Linux, or conditionally comment it out.

    Revision 1.3  2005/11/23 21:30:30  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:02:51  dcross
    Because of weird extra '\r' characters, I went ahead and used Visual Slickedit source beautifier.
    Also moved old manual revision history to end of file and added cvs log tag.



        Revision history:
    
    2001 January 12 [Don Cross]
         Adding support for tree file packing... LearnTree::Pack().
    
    2001 January 9 [Don Cross]
         Adding a 'ChessBoard &' parameter to LearnTree::numNodesAtDepth(),
         so that it can tell whether moves are legal or not.  That way it
         can exclude the illegal moves just like the real trainer does.
    
    1999 March 4 [Don Cross]
         Adding code to make sure that moves are legal before being traversed
         in the tree trainer.  This is because I found some illegal moves in
         the main chenard.tre file I have been training for a long time.
         They look like they are really old moves that were caused by an old
         bug.  Also, this change is important because it will ignore nodes
         marked as deleted.  This is accomplished by setting branch.move to
         source=0 and dest=0.
    
    
    1999 February 9 [Don Cross]
         Adding the fields 'nodesEvaluated' and 'numAccesses' to LearnBranch.
         'nodesEvaluated' holds the number of nodes evaluated in the search
         to come up with the move, unless it was a non-computer move, in which
         case it is zero.  'numAccesses' is a counter that is incremented
         every time the computer player retrieves a given move from the 
         experience tree.
         Made the decision on whether to replace an existing node with a new
         node depend on the number of nodes evaluated being greater, but only
         when the old value is greater than zero (for legacy tree entries).
         When the existing value is zero, revert to the old behavior of replacing
         entries based on think time.  This is an attempt to make the tree code
         scale itself based on processor speed.
    
    1999 January 18 [Don Cross]
         New member function LearnTree::absorbGameFile() allows caller
         to add the continuation in a game file to the experience tree.
    
    1999 January 1 [Don Cross]
         Added calls to fflush(stdout) after every printf() call, so that
         output will be seen immediately on operating systems with buffered
         stdout (e.g. Linux).
    
    1998 December 25 [Don Cross]
         Made NodesAtDepthTable bigger, because it looks like I was going
         past the end of the array!  (Maybe this explains mysterious crashes
         in trainer every now and then.)
    
    1996 September 29 [Don Cross]
         Fixed bug in LearnTree::familiarPosition() which allowed ChessRandom
         to be called with an argument of 0.  Now, if total==0, we know that
         there is no move which is good enough to be trusted from experience,
         so we return false, causing the move to be thought about.
    
    1996 September 9 [Don Cross]
         Making LearnTree::trainDepth() call LearnTree::rememberPosition(), so
         as to fix a bug in the former.
         Making rememberPosition() return an integer code indicating whether
         it updated an existing branch or added a new one.
    
    1996 September 8 [Don Cross]
         Fixed a bug in LearnTree::rememberPosition().  It was not writing
         to the tree file when an existing branch was confirmed at a higher
         search time limit.
    
    1996 September 5 [Don Cross]
         Fixed a bug in LearnTree::familiarPosition().  It was using the
         variable 'n' instead of 'numChildren' in the loop where the random
         number 'r' is used to pick the move.
    
    1996 August 22 [Don Cross]
         Started writing this code.
    
*/

