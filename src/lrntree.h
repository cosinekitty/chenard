/*===============================================================================

    lrntree.h  -  Copyright (C) 1998-2005 by Don Cross

    This is a new module for learning new openings.
    This file replaces the old file 'learn.cpp'.
    Now, instead of reading the entire tree into memory, we encapsulate
    direct I/O to a binary file which contains the tree.  This saves
    a lot of dynamic memory and prevents us from having to read and write
    the entire tree all at once.

=============================================================================*/
#ifndef __ddc_chenard_lrntree_h
#define __ddc_chenard_lrntree_h


const char * const DEFAULT_CHENARD_TREE_FILENAME = "chenard.trx";


struct LearnBranch
{
    Move   move;              // The move source, dest, and score of this branch
    INT32  timeAnalyzed;      // Amount of time move was analyzed in centiseconds
    INT32  winsAndLosses;     // +1 for every White win, -1 for every Black win
    INT32  child;             // first branch which is a child of this one
    INT32  sibling;           // immediate sibling to this branch
    INT32  nodesEvaluated;    // number of nodes evaluated by computer
    INT32  numAccesses;       // number of times this move played by computer
    INT32  reserved [2];      // ... and some room to grow

    LearnBranch():
        timeAnalyzed (0),
        winsAndLosses (0),
        child (-1),
        sibling (-1),
        nodesEvaluated (0),
        numAccesses (0)
    {
        move.source = 0;
        move.dest = 0;
        move.score = 0;
        reserved[0] = 0;
        reserved[1] = 0;
    }
};


const long LearnBranchFileSize = 9*4;   // number of bytes per file record

const unsigned MaxLearnDepth = 30;

class LearnTree
{
public:
    LearnTree():  f(0) {}
    ~LearnTree();

    // NOTE:  All 'int' functions return 0 on failure, 1 on success.

    // High-level functions...

    bool familiarPosition (
        ChessBoard &board,
        Move &bestmove,
        long timeLimit,
        const MoveList & );   // legal moves: included so we can do sanity check

    // Returns the following values:
    //    0 = Tree not changed
    //    1 = Modified an existing branch
    //    2 = Added a new branch

    int rememberPosition (
        ChessBoard      &board,
        Move            bestmove,
        long            timeLimit,
        unsigned long   nodesEvaluated,
        unsigned long   numAccesses,
        long            winsAndLosses );

    void learnFromGame (
        ChessBoard &board,
        ChessSide winner );

    void absorbFile ( const char *gameFilename );

    void trainer (
        ComputerChessPlayer *whitePlayer,
        ComputerChessPlayer *blackPlayer,
        ChessBoard &board,
        ChessUI &ui,
        INT32 timeLimit );

    static int Pack ( 
        const char  *inTreeFilename,
        const char  *outTreeFilename,
        SCORE       window );

    int  open ( const char *filename );
    int  read  ( INT32 offset, LearnBranch & );
    int  write ( INT32 offset, const LearnBranch & );
    INT32  numNodes();

protected:
    void absorbGameFile ( const char *gameFilename );
    void absorbPgnFile  ( const char *pgnFileName );

    int  create ( const char *filename );
    int  flush();
    int  close();

    // Low-level functions...

    int append ( INT32 &offset, const LearnBranch & );

    int insert ( LearnBranch &branch, INT32 parent, INT32 &offset );

    int  write ( INT32 );
    int  read  ( INT32 & );
    int  write ( Move );
    int  read  ( Move & );

    INT32  numNodesAtDepth (
        INT32 root,
        int targetDepth,
        int currentDepth,
        INT32 searchTime,
        ChessBoard & );

    void trainDepth (
        INT32 offset,
        INT32 timeLimit,
        ChessUI &ui,
        ComputerChessPlayer *whitePlayer,
        ComputerChessPlayer *blackPlayer,
        ChessBoard &board,
        int targetDepth,
        int currentDepth );

    int packer (
        INT32       offset,
        LearnTree   &outTree,
        int         depth,
        ChessBoard  &board,
        SCORE       window );       // maximum absolute value of score allowed

    // the following function is used for auto-verification purposes...

    bool locateAndReadBranch (
        ChessBoard      &board,
        Move            moveToFind,
        LearnBranch     &branch );

    void checkForPause();

private:
    FILE *f;
};


#endif // __ddc_chenard_lrntree_h

/*
    $Log: lrntree.h,v $
    Revision 1.9  2008/11/24 11:21:58  Don.Cross
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

    Revision 1.8  2006/03/20 20:44:10  dcross
    I finally figured out the weird slowness I have been seeing in Chenard accessing
    its experience tree.  It turns out it only happens the first time it accessed the file
    chenard.tre after a reboot.  It was caused by Windows XP thinking chenard.tre was
    an important system file, because of its extension:
    http://msdn.microsoft.com/library/default.asp?url=/library/en-us/sr/sr/monitored_file_extensions.asp
    We write 36 bytes to the middle of the file, then close it
    So I have changed the filename from chenard.tre to chenard.trx.
    I also added DEFAULT_CHENARD_TREE_FILENAME in lrntree.h so that I can more
    easily rename this again if need be.

    Revision 1.7  2006/01/18 19:58:12  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.6  2006/01/01 20:06:12  dcross
    Finished getting PGN-to-tree slurper working.

    Revision 1.5  2005/12/19 03:41:09  dcross
    Now we can safely shut down the trainer.  First press 'p' to pause,
    then enter the command 'q' to quit.  This closes the file and exits cleanly.
    I was worried about pressing Ctrl+Break because chenard.tre is kept open,
    though we do flush the file every time we write to it.  I am concerned about
    corruption in the file that seems to be there.  I want to investigate this some
    time and figure out how to clean out the problems, if any are really there.

    Revision 1.4  2005/11/25 19:12:20  dcross
    Recovered edittree.cpp from the dead!
    Found this on CDROM marked "14 September 1999".
    This allowed me to reinstate the experience tree editor in Portable Chenard.
    Had to move some stuff from protected to public in class LearnTree.

    Revision 1.3  2005/11/23 21:30:30  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:41  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!


        Revision history:
    
    2001 January 12 [Don Cross]
         Adding support for tree file packing... LearnTree::Pack().
    
    2001 January 9 [Don Cross]
         Adding a 'ChessBoard &' parameter to LearnTree::numNodesAtDepth(),
         so that it can tell whether moves are legal or not.  That way it
         can exclude the illegal moves just like the real trainer does.
    
    1996 August 22 [Don Cross]
         Started writing this code.
    
*/

