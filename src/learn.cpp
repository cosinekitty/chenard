/*===============================================================================

    learn.cpp  -  Copyright (C) 1993-1998 by Don Cross
    email: dcross@intersrv.com
    WWW:   http://www.intersrv.com/~dcross/

    Created this code so that Dr. Chenard can
    actually improve its ability when played
    special "training games" at very high levels
    (i.e. think time per move).

=============================================================================*/

// standard includes...
#include <stdlib.h>
#include <stdio.h>
#include <iostream.h>

// project includes...
#include "contain.h"
#include "chess.h"
#include "lrntree.h"

int Learn_Output = 1;
static bool Learn_AutoSave = true;
static bool LibraryChanged = false;

void EnableLearningTree_AutoSave ( bool enableFlag )
{
    Learn_AutoSave = enableFlag;
}

const unsigned MaxLearnDepth = 30;

long  debugCount = 0;
long  passCount  = 0;
long  thisCount = 0;

#define LIST_LIBRARY 0
#define CHOICE_MODE 0

// The following are used only with CHOICE_MODE == 3
const SCORE  WL_WEIGHT  =    50;   // value of every game won/loss in experience min-max
const SCORE  WL_LIMIT   =  2000;   // max absolute value that win/loss can swing score
const SCORE  WL_DENOM   = WL_LIMIT / WL_WEIGHT;

// The following preprocessor construct has been
// created to keep different processor speed
// implementations from cross-polluting each
// others' experience pools.
// Later, I need to design a mechanism for recombining
// these experience pools (based on nodes visited? or
// maximum depth finished?) and moving from using an
// absolute think time heuristic toward using
// a heuristic which corrects for differing processing
// power.  Getting this to be compatible with a
// parallel processor network is a distinct possibility...
#ifdef __BORLANDC__
    const char * const ChenardTreeFilename       = "doscga.lrn";
    const char * const ChenardTreeBackupFilename = "dbackup.lrn";
#else
    const char * const ChenardTreeFilename       = "chenard.lrn";
    const char * const ChenardTreeBackupFilename = "backup.lrn";
#endif

struct OldLearnBranch;
typedef OldLearnBranch *LearnBranchPtr;

static LList<LearnBranchPtr> Chenards_Education;
static bool Chenards_Education_Loaded = false;

static unsigned long NodesAtDepthTable [100];


struct OldLearnBranch
{
    Move    move;           // The move associated with this branch
    INT32   timeAnalyzed;   // The max amount of time this move was analyzed by Chenard
    INT32   winsAndLosses;  // +1 for every White win, -1 for every black win resulting.

    LList<LearnBranchPtr>  replies;   // all previously experienced replies to the move

    void write ( FILE *out );
    void list ( FILE *textfile, int depth=0 );
    bool read ( FILE *in );

    OldLearnBranch():
        timeAnalyzed ( 0 ),
        replies(),
        winsAndLosses ( 0 )
    {
        move.source = 0;
        move.dest = 0;
        move.score = 0;
    }
};


void WriteBranchList ( LList<LearnBranchPtr> &blist, FILE *out );

#if LIST_LIBRARY
void ListBranchList ( LList<LearnBranchPtr> &blist, FILE *textfile, int depth );
#endif

void OldLearnBranch::write ( FILE *out )
{
    fwrite ( &move, sizeof(move), 1, out );
    fwrite ( &timeAnalyzed, sizeof(timeAnalyzed), 1, out );
    fwrite ( &winsAndLosses, sizeof(winsAndLosses), 1, out );

    WriteBranchList ( replies, out );
}


void Indent ( FILE *textfile, int depth )
{
    for ( int i=0; i < depth; i++ )
    {
        fputc ( ' ', textfile );
    }
}

#if LIST_LIBRARY
void OldLearnBranch::list ( FILE *textfile, int depth )
{
    Indent ( textfile, depth );
    fprintf ( textfile, "move=(%3u %3u)  score=%-5d   time=%-9ld  WL=%-9ld\n",
        unsigned(move.source),
        unsigned(move.dest),
        int(move.score),
        long(timeAnalyzed),
        long(winsAndLosses) );

    ListBranchList ( replies, textfile, depth );
}
#endif

void WriteBranchList ( LList<LearnBranchPtr> &blist, FILE *out )
{
    ++debugCount;
    ++thisCount;
    UINT32 n = blist.numItems();
    fwrite ( &n, sizeof(n), 1, out );
    for ( UINT32 i=0; i < n; i++ )
    {
        blist[i]->write ( out );
    }
}

#if LIST_LIBRARY
void ListBranchList ( LList<LearnBranchPtr> &blist, FILE *textfile, int depth )
{
    unsigned n = blist.numItems();
    for ( unsigned i=0; i < n; i++ )
    {
        blist[i]->list ( textfile, depth+1 );
    }
}
#endif


//--------------------------------------------------------------------------

bool ReadBranchList ( LList<LearnBranchPtr> &blist, FILE *in );

bool OldLearnBranch::read ( FILE *in )
{
    if ( fread ( &move, sizeof(move), 1, in ) == 1 )
    {
        if ( fread ( &timeAnalyzed, sizeof(timeAnalyzed), 1, in ) == 1 )
        {
            if ( fread ( &winsAndLosses, sizeof(winsAndLosses), 1, in ) == 1 )
            {
                return ReadBranchList ( replies, in );
            }
        }
    }

    return false;
}


bool ReadBranchList ( LList<LearnBranchPtr> &blist, FILE *in )
{
    UINT32 n = 0;
    if ( fread ( &n, sizeof(n), 1, in ) == 1 )
    {
        for ( UINT32 i=0; i < n; i++ )
        {
            OldLearnBranch *branch = new OldLearnBranch;
            if ( !branch )
                return false;

            if ( !branch->read(in) )
                return false;

            if ( blist.addToBack(branch) != DDC_SUCCESS )
                return false;
        }
    }

    return true;
}

#if LIST_LIBRARY
void ListLibrary ( const char *outFilename,
                   LList<LearnBranchPtr> &library )
{
    FILE *out = fopen ( outFilename, "wt" );

    if ( out )
    {
        setvbuf ( out, NULL, _IOFBF, 4096 );
        ListBranchList ( library, out, 0 );
        fclose ( out );
    }
}
#endif

void WriteLibrary ( const char *outFilename,
                    LList<LearnBranchPtr> &library )
{
    if ( !LibraryChanged )
        return;

#if LIST_LIBRARY
    ListLibrary ( "library.lst", library );
#endif

    remove ( ChenardTreeBackupFilename );
    rename ( outFilename, ChenardTreeBackupFilename );

    FILE *out = fopen ( outFilename, "wb" );
    if ( !out )
    {
        char msg [128];
        sprintf ( msg, "Error opening learning file '%s' for write\n", outFilename );
        ChessFatal ( msg );
    }

    setvbuf ( out, NULL, _IOFBF, 4096 );
    ++passCount;
    thisCount = 0;
    WriteBranchList ( library, out );
    fclose ( out );
    LibraryChanged = false;
}

#define DO_FIX_TREE 0

#if DO_FIX_TREE
void FixTree ( LList<LearnBranchPtr> &tree )
{
    static ChessBoard MyBoard;
    MoveList ml;

    // We are going to verify that every move in this tree
    // is legal.

    if ( MyBoard.WhiteToMove() )
        MyBoard.GenWhiteMoves ( ml );
    else
        MyBoard.GenBlackMoves ( ml );

StartOver:

    unsigned n = tree.numItems();
    for ( unsigned i=0; i < n; i++ )
    {
        OldLearnBranch *branch1 = tree[i];

        // See if this move is legal...
        if ( !ml.IsLegal ( branch1->move ) && Learn_Output )
        {
            printf ( "Found illegal move in tree!%c\n", 7 );
        }

        for ( unsigned j=i+1; j < n; j++ )
        {
            OldLearnBranch *branch2 = tree[j];
            if ( branch1->move == branch2->move )
            {
                if ( Learn_Output )
                {
                    printf ( "Found redundant tree item%c\n", 7 );
                }

                if ( branch1->timeAnalyzed > branch2->timeAnalyzed )
                {
                    tree.remove(j);
                    delete branch2;
                }
                else
                {
                    tree.remove(i);
                    delete branch1;
                }

                goto StartOver;
            }
        }
    }

    for ( i=0; i < n; i++ )
    {
        OldLearnBranch *branch = tree[i];
        Move move = branch->move;
        UnmoveInfo unmove;
        MyBoard.MakeMove ( move, unmove );
        FixTree ( branch->replies );
        MyBoard.UnmakeMove ( move, unmove );
    }
}
#endif DO_FIX_TREE


void ReadLibrary (
    const char *inFilename,
    LList<LearnBranchPtr> &library )
{
    FILE *in = fopen ( inFilename, "rb" );
    if ( in )
    {
        setvbuf ( in, NULL, _IOFBF, 4096 );
        if ( !ReadBranchList ( library, in ) )
        {
            cerr << "Error reading library from file '" << inFilename << "'" << endl;
            cerr << "This file seems to contain invalid data."<< endl;
            fclose ( in );
            exit(1);
        }

        fclose ( in );

#if DO_FIX_TREE
        FixTree ( library );
#endif
    }
}


void LoadLearningTree()
{
    if ( !Chenards_Education_Loaded )
    {
        Chenards_Education_Loaded = true;
        ReadLibrary ( ChenardTreeFilename, Chenards_Education );
        LibraryChanged = false;
    }
}


void SaveLearningTree()
{
    WriteLibrary ( ChenardTreeFilename, Chenards_Education );
}


void LearnFromGame ( const ChessBoard &board, ChessSide winner )
{
    if ( winner == SIDE_NEITHER )
    {
        // Currently, we ignore draw games.
        // This might change in the future, so callers should
        // not assume this behavior.

        return;
    }

    LoadLearningTree();

    INT32 score = (winner == SIDE_WHITE) ? 1 : -1;

    // For every corresponding branch in the game that
    // we know about, adjust the winsOrLosses field for
    // that move.

    LList<LearnBranchPtr> *node = &Chenards_Education;

    const unsigned numPlies = board.GetCurrentPlyNumber();
    for ( unsigned ply=0; ply < numPlies; ply++ )
    {
        Move move = board.GetPastMove (ply);

        // Search for this move at the current location in the tree.

        bool foundTheMove = false;
        const unsigned numBranches = node->numItems();
        for ( unsigned i=0; i < numBranches; i++ )
        {
            OldLearnBranch *branch = (*node)[i];
            if ( branch->move == move )
            {
                // We have found the next link in the path.
                // Update its win/loss statistic.

                branch->winsAndLosses += score;
                LibraryChanged = true;
                foundTheMove = true;
                node = &(branch->replies);
                break;
            }
        }

        if ( !foundTheMove )
        {
            break;  // This means we did not find the move, so give up.
        }
    }

    if ( LibraryChanged && Learn_AutoSave )
    {
        WriteLibrary ( ChenardTreeFilename, Chenards_Education );
    }
}


static INT32 ScoreBranch ( const OldLearnBranch &b, bool whiteToMove )
{
    INT32 score = b.timeAnalyzed / 100;     // seconds thought about move
    const INT32 gameValue = 5;              // an arbitrary scaling constant

    if ( b.winsAndLosses != 0 )
    {
        if ( whiteToMove )
            score += gameValue * b.winsAndLosses;
        else
            score -= gameValue * b.winsAndLosses;
    }

    if ( score < 0 )
        score = 0;

    return score;
}


#if CHOICE_MODE == 0
bool ChooseFromExperience (
    const LList<LearnBranchPtr> &branch,
    Move &chosenMove,
    bool whiteToMove )
{
    INT32 scores [128];
    INT32 total = 0;
    const int n = branch.numItems();
    for ( int i=0; i<n; i++ )
        total += scores[i] = ScoreBranch ( *branch[i], whiteToMove );

    if ( total == 0 )
        return false;

    INT32 r = ChessRandom(total);

    for ( i=0; i<n; i++ )
    {
        r -= scores[i];
        if ( r < 0 )
        {
            const OldLearnBranch &b = *(branch[i]);
            chosenMove = b.move;

            if ( Learn_Output )
            {
                printf ( "experience:  score=%d  winloss=%ld  time=%lu\n",
                         int (b.move.score),
                         long (b.winsAndLosses),
                         long (b.timeAnalyzed) );
            }

            return true;
        }
    }

    //----------------------------------------
    // [Don Cross, 10 Apr 1995]
    // I do not think it is possible to get
    // here in the code.  However, this
    // will make the compiler happy.
    //----------------------------------------
    return false;
}

#elif CHOICE_MODE == 2 || CHOICE_MODE == 3

Move ExpMin (
    const LList<LearnBranchPtr> &node,
    INT32 timeLimit,
    SCORE alpha = MIN_WINDOW,
    SCORE beta = MAX_WINDOW,
    int depth = 0 );

Move ExpMax (
    const LList<LearnBranchPtr> &node,
    INT32 timeLimit,
    SCORE alpha = MIN_WINDOW,
    SCORE beta = MAX_WINDOW,
    int depth = 0 )
{
    Move bestmove;
    bestmove.source = 0;
    bestmove.dest = 0;
    bestmove.score = NEGINF;
    const unsigned n = node.numItems();
    for ( unsigned i=0; i<n; i++ )
    {
        const OldLearnBranch &b = *(node[i]);
        if ( b.timeAnalyzed >= timeLimit )
        {
            Move reply = ExpMin ( b.replies, timeLimit, alpha, beta, depth+1 );
            if ( reply.source == 0 )
            {
                reply.score = b.move.score;
            }

#if CHOICE_MODE == 3
            if ( depth == 0 )
            {
                INT32 wal = b.winsAndLosses;
                if ( wal < -WL_DENOM )
                {
                    wal = -WL_DENOM;
                }
                else if ( wal > WL_DENOM )
                {
                    wal = WL_DENOM;
                }

                reply.score += SCORE(wal) * WL_WEIGHT;
            }
#endif

            if ( reply.score > bestmove.score )
            {
                bestmove.source = b.move.source;
                bestmove.dest = b.move.dest;
                bestmove.score = reply.score;
            }
        }

        if ( bestmove.score >= beta )
        {
            break;
        }

        if ( bestmove.score > alpha )
        {
            alpha = bestmove.score;
        }
    }

    return bestmove;
}


Move ExpMin (
    const LList<LearnBranchPtr> &node,
    INT32 timeLimit,
    SCORE alpha,
    SCORE beta,
    int depth )
{
    Move bestmove;
    bestmove.source = 0;
    bestmove.dest = 0;
    bestmove.score = POSINF;
    const unsigned n = node.numItems();
    for ( unsigned i=0; i<n; i++ )
    {
        const OldLearnBranch &b = *(node[i]);
        if ( b.timeAnalyzed >= timeLimit )
        {
            Move reply = ExpMax ( b.replies, timeLimit, alpha, beta, depth+1 );
            if ( reply.source == 0 )
            {
                reply.score = b.move.score;
            }

#if CHOICE_MODE == 3
            if ( depth == 0 )
            {
                INT32 wal = b.winsAndLosses;
                if ( wal < -WL_DENOM )
                {
                    wal = -WL_DENOM;
                }
                else if ( wal > WL_DENOM )
                {
                    wal = WL_DENOM;
                }

                reply.score += SCORE(wal) * WL_WEIGHT;
            }
#endif

            if ( reply.score < bestmove.score )
            {
                bestmove.source = b.move.source;
                bestmove.dest = b.move.dest;
                bestmove.score = reply.score;
            }
        }

        if ( bestmove.score <= alpha )
        {
            break;
        }

        if ( bestmove.score < beta )
        {
            beta = bestmove.score;
        }
    }

    return bestmove;
}


bool ChooseFromExperience_MinMax (
    const LList<LearnBranchPtr> &node,
    Move &chosenMove,
    bool whiteToMove,
    INT32 timeLimit )
{
    if ( whiteToMove )
    {
        chosenMove = ExpMax ( node, timeLimit );
    }
    else
    {
        chosenMove = ExpMin ( node, timeLimit );
    }

    bool foundGoodMove =
        chosenMove.score > MIN_WINDOW &&
        chosenMove.score < MAX_WINDOW;

    if ( foundGoodMove && Learn_Output )
    {
        printf ( "Experience score (min-max) = %d\n", chosenMove.score );
    }

    return foundGoodMove;
}

#endif  // CHOICE_MODE

bool FamiliarPosition ( ChessBoard &board, Move &bestmove, long timeLimit )
{
    if ( board.HasBeenEdited() )
    {
        return false;  // Education can't help us in an edited position.
    }

    LoadLearningTree();

    LList<LearnBranchPtr> *position = &Chenards_Education;
    LearnBranchPtr branch = 0;

    unsigned n = board.GetCurrentPlyNumber();
    for ( unsigned i=0; i < n; i++ )
    {
        Move move = board.GetPastMove(i);

        unsigned numReplies = position->numItems();
        bool foundContinuation = false;
        for ( unsigned r=0; r < numReplies; r++ )
        {
            branch = (*position)[r];
            if ( move == branch->move )
            {
                position = &(branch->replies);
                foundContinuation = true;
                break;
            }
        }

        if ( !foundContinuation )
        {
            return false;
        }
    }

    // Find the move which has been thought about the most and found to be best!
    unsigned numReplies = position->numItems();
    INT32 maxTime = 0;
    OldLearnBranch *bestBranch = 0;
    for ( unsigned r=0; r < numReplies; r++ )
    {
        OldLearnBranch *tempBranch = (*position)[r];
        if ( tempBranch->timeAnalyzed > maxTime )
        {
            maxTime = tempBranch->timeAnalyzed;
            bestBranch = tempBranch;
        }
    }

#if CHOICE_MODE == 0

    if ( maxTime >= timeLimit )
    {
        // Choose randomly between the moves with probabilities
        // proportional to the max amount of time Dr. Chenard has ever
        // spent deciding that move was best.  This is a kind of credibility
        // factor which should allow play that improves as max training time
        // is slowly ramped through a range of say, 301 seconds to increasing
        // up to some higher value, until all experience has been groomed.

        return ChooseFromExperience (
            *position,
            bestmove,
            board.WhiteToMove() );
    }

#elif CHOICE_MODE == 1

    if ( bestBranch && maxTime >= timeLimit )
    {
        bestmove = bestBranch->move;
        return true;
    }

#elif CHOICE_MODE == 2 || CHOICE_MODE == 3

    if ( maxTime >= timeLimit )
    {
        return ChooseFromExperience_MinMax (
            *position,
            bestmove,
            board.WhiteToMove(),
            timeLimit );
    }

#else
    #error Symbol 'CHOICE_MODE' is defined in a way that Dr. Chenard dislikes!
#endif // CHOICE_MODE

    return false;
}


void RememberPosition ( ChessBoard &board, Move bestmove, long timeLimit )
{
    if ( board.HasBeenEdited() )
    {
        return;  // Cannot usefully learn from an edited board.
    }

    // See if we have ever encountered this before.
    // If not, create the complete path to it, including bestmove.

    LList<LearnBranchPtr> *position = &Chenards_Education;
    LearnBranchPtr branch = 0;
    unsigned n = board.GetCurrentPlyNumber();
    if ( n > MaxLearnDepth )
    {
        return;   // Don't bother to learn a position this deep into the game.
    }

    // The following loop iterates through every move which
    // has been made in this game so far.  In other words,
    // only definite, past moves are looked at.
    // We use this past moves to traverse the tree.
    // When we find missing branches in the tree,
    // we add them automatically.

    for ( unsigned i=0; i < n; i++ )
    {
        unsigned numReplies = position->numItems();
        bool foundReply = false;
        Move move = board.GetPastMove(i);

        for ( unsigned r=0; r < numReplies && !foundReply; r++ )
        {
            branch = (*position)[r];
            if ( move == branch->move )
            {
                foundReply = true;
            }
        }

        if ( !foundReply )
        {
            // Create the reply now.

            branch = new OldLearnBranch;
            if ( !branch )
            {
                return;  // ran out of memory
            }

            branch->move = move;
            branch->timeAnalyzed = 0;   // IMPORTANT SIGNAL that computer didn't analyze!

            if ( position->addToBack(branch) != DDC_SUCCESS )
            {
                delete branch;
                return;  // ran out of memory
            }

            LibraryChanged = true;
        }

        position = &(branch->replies);
    }

    if ( branch )
    {
        // Search to see if 'bestmove' passed in is already under 'branch'.
        // If it isn't there, add it.
        // If it is already there, replace the existing one only
        // if this evaluation took more time than the existing one.

        unsigned numReplies = position->numItems();
        for ( unsigned r=0; r < numReplies; r++ )
        {
            OldLearnBranch *temp = (*position)[r];

            if ( temp->move == bestmove )
            {
                // We found it!  See if we need to replace the existing
                // information.

                if ( timeLimit > temp->timeAnalyzed )
                {
                    temp->timeAnalyzed = timeLimit;
                    temp->move.score = bestmove.score;
                    LibraryChanged = true;
                }

                return;
            }
        }

        // If we get here, it means that we have never before
        // encountered this move in this position.
        // Therefore, we need to add a new branch now!

        branch = new OldLearnBranch;
        if ( !branch )
        {
            return;  // out of memory!
        }

        branch->move = bestmove;
        branch->timeAnalyzed = timeLimit;
        if ( position->addToBack(branch) != DDC_SUCCESS )
        {
            delete branch;
            return;  // out of memory
        }

        if ( Learn_AutoSave && LibraryChanged )
        {
            // The library has changed, so save it to disk!
            WriteLibrary ( ChenardTreeFilename, Chenards_Education );
        }
    }
}


static UINT32 NumNodesInTree ( const LList<LearnBranchPtr> &tree )
{
    UINT32 count = 1;  // The current position

    unsigned n = tree.numItems();
    for ( unsigned i=0; i < n; i++ )
    {
        count += NumNodesInTree ( tree[i]->replies );
    }

    return count;
}


static UINT32 NumNodesAtDepth (
    const LList<LearnBranchPtr> &tree,
    int targetDepth,
    int currentDepth,
    INT32 searchTime )
{
    UINT32 count = 0;
    const unsigned n = tree.numItems();

    if ( currentDepth < targetDepth )
    {
        for ( unsigned i=0; i < n; i++ )
        {
            count += NumNodesAtDepth ( 
                tree[i]->replies, targetDepth, 1 + currentDepth, searchTime );
        }
    }
    else if ( currentDepth == targetDepth )
    {
        for ( unsigned i=0; i < n; i++ )
        {
            if ( tree[i]->timeAnalyzed >= searchTime )
                return 0;
        }

        count = 1;
    }

    return count;
}


static UINT32 MaxDepth ( const LList<LearnBranchPtr> &tree )
{
    UINT32 maxDepth = 0;

    unsigned n = tree.numItems();
    for ( unsigned i=0; i < n; i++ )
    {
        UINT32 depth = 1 + MaxDepth ( tree[i]->replies );
        if ( depth > maxDepth )
        {
            maxDepth = depth;
        }
    }

    return maxDepth;
}


static void EstimateRemainingTime ( int currentDepth, double thinkTime )
{
    double seconds = thinkTime * NodesAtDepthTable[currentDepth];
    double hours = seconds / 3600.0;
    if ( hours > 24.0 )
        printf ( "(%0.2lf days)", hours/24.0 );
    else
        printf ( "(%0.2lf hours)", hours );
}


static void TrainDepth (
    LList<LearnBranchPtr> &tree,
    INT32 timeLimit,
    ChessUI &ui,
    ComputerChessPlayer *whitePlayer,
    ComputerChessPlayer *blackPlayer,
    ChessBoard &board,
    int targetDepth,
    int currentDepth )
{
    unsigned numReplies = tree.numItems();

    if ( currentDepth < targetDepth )
    {
        for ( unsigned r=0; r < numReplies; r++ )
        {
            OldLearnBranch *branch = tree[r];

            UnmoveInfo unmove;
            Move move = branch->move;

            board.MakeMove ( move, unmove );

            TrainDepth (
                branch->replies,
                timeLimit,
                ui,
                whitePlayer,
                blackPlayer,
                board,
                targetDepth,
                1 + currentDepth );

            board.UnmakeMove ( move, unmove );
        }
    }
    else if ( currentDepth == targetDepth )
    {
        // Before we spend time analyzing this position,
        // make sure it hasn't been done so well (or better)
        // before now.

        if ( Learn_Output && NodesAtDepthTable[targetDepth]>0 )
        {
            printf ( "\r###  depth=%d --- Remaining nodes at this depth: %-10lu",
                     targetDepth,
                     NodesAtDepthTable[targetDepth] );

            EstimateRemainingTime ( targetDepth, 0.01*timeLimit );
        }

        for ( unsigned r=0; r < numReplies; r++ )
        {
            OldLearnBranch *branch = tree[r];

            if ( branch->timeAnalyzed >= timeLimit )
            {
                // Don't bother...we would waste time to think
                // about this position for only 'timeLimit' centiseconds.
                return;
            }
        }

        if ( Learn_Output )
        {
            printf ( "\n" );
        }

        --NodesAtDepthTable[targetDepth];

        ui.DrawBoard ( board );
        INT32 timeSpent = 0;
        Move bestmove;
        MoveList ml;

        if ( board.WhiteToMove() )
        {
            board.GenWhiteMoves ( ml );
            if ( ml.num > 0 )
            {
                whitePlayer->GetMove ( board, bestmove, timeSpent );
            }
        }
        else
        {
            board.GenBlackMoves ( ml );
            if ( ml.num > 0 )
            {
                blackPlayer->GetMove ( board, bestmove, timeSpent );
            }
        }

        char moveString [MAX_MOVE_STRLEN + 1];
        FormatChessMove ( board, bestmove, moveString );
        if ( Learn_Output )
        {
            printf ( ">>> Best move = [%s]    score = %d\n", moveString, int(bestmove.score) );
        }

        // See if this move matches any existing replies...
        static unsigned long NumPositionsFinished = 0;
        static unsigned long NumBranches = 0;
        static unsigned long NumUpdates = 0;

        for ( r=0; r < numReplies; r++ )
        {
            OldLearnBranch *branch = tree[r];

            if ( branch->move == bestmove )
            {
                // Update the statistics for this reply
                branch->move.score = bestmove.score;
                branch->timeAnalyzed = timeLimit;
                LibraryChanged = true;
                WriteLibrary ( ChenardTreeFilename, Chenards_Education );

                ++NumPositionsFinished;
                ++NumUpdates;

                if ( Learn_Output )
                {
                    printf ( ">>> Just modified an existing branch.\n" );

                    printf ( ">>> finished=%lu  branches=%lu  updates=%lu\n",
                             NumPositionsFinished,
                             NumBranches,
                             NumUpdates );
                }

                return;  // We have finished our job here
            }
        }

        // If we get here, it means we need to add a new
        // branch to the tree!

        OldLearnBranch *branch = new OldLearnBranch;
        if ( !branch )
        {
            return;  // out of memory!
        }

        branch->move = bestmove;
        branch->timeAnalyzed = timeLimit;
        if ( tree.addToBack(branch) != DDC_SUCCESS )
        {
            delete branch;
            return;  // out of memory
        }

        LibraryChanged = true;
        ++NodesAtDepthTable [targetDepth + 1];
        if ( Learn_Output )
        {
            printf ( ">>> Just added a new branch!\n" );
        }

        WriteLibrary ( ChenardTreeFilename, Chenards_Education );

        ++NumPositionsFinished;
        ++NumBranches;

        if ( Learn_Output )
        {
            printf ( ">>> finished=%lu  branches=%lu  updates=%lu\n",
                     NumPositionsFinished,
                     NumBranches,
                     NumUpdates );
        }
    }
}


void TreeTrainer (
    ComputerChessPlayer *whitePlayer,
    ComputerChessPlayer *blackPlayer,
    ChessBoard &theBoard,
    ChessUI &theUserInterface,
    long timeLimit )
{
    printf ( "Entering tree trainer!\n\n" );
    LoadLearningTree();

    UINT32 totalNodes = NumNodesInTree ( Chenards_Education );
    printf ( "Number of nodes in tree = %lu\n", (unsigned long)totalNodes );

    UINT32 maxDepth = MaxDepth ( Chenards_Education );
    if ( maxDepth > MaxLearnDepth )
    {
        maxDepth = MaxLearnDepth;
    }

    printf ( "Maximum depth of tree = %lu\n", (unsigned long)maxDepth );
    printf ( "[ " );

    for ( unsigned d=0; d <= maxDepth+1; d++ )
    {
        NodesAtDepthTable[d] = NumNodesAtDepth ( Chenards_Education, d, 0, timeLimit );
        printf ( "%lu ", (unsigned long)(NodesAtDepthTable[d]) );
    }

    printf ( "]\n" );
    printf ( "\nStarting training...\n\n" );

    for ( d=0; d <= maxDepth; d++ )
    {
        TrainDepth (
            Chenards_Education,
            timeLimit,
            theUserInterface,
            whitePlayer,
            blackPlayer,
            theBoard,
            d,
            0 );
    }
}


void ConvertBranch (
    LearnBranch &newBranch,
    const OldLearnBranch &oldBranch )
{
    newBranch.move           =  oldBranch.move;
    newBranch.timeAnalyzed   =  oldBranch.timeAnalyzed;
    newBranch.winsAndLosses  =  oldBranch.winsAndLosses;
    newBranch.child    = -1;
    newBranch.sibling  = -1;

    for ( int i=0; i < sizeof(newBranch.reserved)/sizeof(INT32); i++ )
    {
        newBranch.reserved[i] = 0;
    }
}


// This is a recursive function for converting the LRN format to the TRE format

void ConvertBranches (
    LearnTree &tree,
    INT32 &offset,
    const LList<LearnBranchPtr> &replies )
{
    int n = replies.numItems();
    LearnBranch newBranch;

    INT32 lsibling = -1;
    for ( int i=0; i<n; i++ )
    {
        const OldLearnBranch &oldBranch = *replies[i];
        ConvertBranch ( newBranch, oldBranch );
        if ( oldBranch.replies.numItems() > 0 )
        {
            // This branch has children, so we know the first child will
            // have an offset one greater than the current offset.
            newBranch.child = 1 + offset;
        }

        int rc = tree.write ( offset, newBranch );
        if ( !rc )
        {
            if ( Learn_Output )
            {
                printf ( "Error writing at offset %ld (byte %ld)\n", long(offset), long(offset*LearnBranchFileSize) );
                tree.close();
                exit(1);
            }
            return;
        }

        if ( i > 0 )
        {
            // Backpatch the left sibling of this branch to point to here.
            LearnBranch leftBranch;
            rc = tree.read ( lsibling, leftBranch );
            if ( !rc )
            {
                if ( Learn_Output )
                {
                    printf ( "Error reading left sibling at offset %ld (byte %ld)\n", long(lsibling), long(lsibling*LearnBranchFileSize) );
                    tree.close();
                    exit(1);
                }
                return;
            }
            leftBranch.sibling = offset;
            rc = tree.write ( lsibling, leftBranch );
            if ( !rc )
            {
                if ( Learn_Output )
                {
                    printf ( "Error writing left sibling at offset %ld\n", long(lsibling) );
                    tree.close();
                    exit(1);
                }
                return;
            }
        }

        lsibling = offset++;

        if ( oldBranch.replies.numItems() > 0 )
            ConvertBranches ( tree, offset, oldBranch.replies );
    }
}


// The following function converts the old tree format to the new
// tree format, which no longer requires loading the whole dang thing
// into memory!

void ConvertLearningTree ( const char *newFilename )
{
    LearnTree tree;
    int rc = tree.create ( newFilename );
    if ( !rc )
    {
        if ( Learn_Output )
            printf ( "Cannot create new tree file '%s'\n", newFilename );

        return;
    }

    INT32 offset = 0;    // the first branch from the root is at offset 0
    ConvertBranches ( tree, offset, Chenards_Education );
}


/*
    $Log: learn.cpp,v $
    Revision 1.2  2006/01/18 19:58:12  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.1  2005/11/25 19:47:27  dcross
    Recovered lots of old chess source files from the dead!
    Found these on CDROM marked "14 September 1999".
    Added cvs log tag and moved old revision history after that.


    Revision history:

1996 August 23 [Don Cross]
     Making a transition to the new .TRE format.
     Adding code to this file which can translate the .LRN file into .TRE.

1996 August 2 [Don Cross]
     Added calls to setvbuf() to make input and output files have 4K buffer.
     Found out that default buffer size is 512 bytes, and I think this is
     what is making things so slow in Win32 GUI version of Chenard when
     library is read/written for the first time.

1996 January 1 [Don Cross]
     Code now backs up old .LRN file before writing a new one.

1995 December 25 [Don Cross]
         Added symbol LIST_LIBRARY
     0 = Do not produce human-readable LIBRARY.LST
     1 = Produce human-readable LIBRARY.LST
     This file lists the same information as CHENARD.LRN
     except in human readable format.

1995 September 15 [Don Cross]
     Add a new definition of CHOICE_MODE:
     CHOICE_MODE = 3
     Same as CHOICE_MODE = 2, except that we add
     influence from the win/loss statistics to the
     min-max evaluation.

1995 September 14 [Don Cross]
     Change the symbol ALWAYS_CHOOSE_DEEPEST to
     CHOICE_MODE, where 0 and 1 are defined as in the
     1995 April 10 revision, and 2 is defined to mean:
     Perform a min-max of the tree based on an "acceptable minimum"
         think time (say 60 seconds).

1995 April 10 [Don Cross]
    Added the preprocessor symbol ALWAYS_CHOOSE_DEEPEST.
    Define this symbol to be one of the following values:

    0
    Choose randomly between the moves with probabilities
    proportional to the max amount of time Dr. Chenard has ever
    spent deciding that move was best.  This is a kind of credibility
    factor which should allow play that improves as max training time
    is slowly ramped through a range of say, 301 seconds to increasing
    up to some higher value, until all experience has been groomed.

    1
    Play only the moves in familiar positions with maximum think time.
*/

