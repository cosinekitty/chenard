/*===========================================================================

    clntree.cpp  -  Don Cross <dcross@intersrv.com>
    http://www.intersrv.com/~dcross/

    This program cleans up Chenard's learning tree file.
    It verifies that all the moves in the tree are legal,
    removing any subtrees which stem from a corrupted node,
    and also removes subtrees from nodes that result from
    a rediculous position (i.e. the score deviates sufficiently
    from zero).

===========================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <iostream.h>
#include "chess.h"
#include "lrntree.h"


INT32  NumIllegalMoves = 0;
INT32  NumPrunedSubtrees = 0;
INT32  NodesRead = 0;
INT32  NodesWritten = 0;
INT32  MessedUpLinks = 0;
INT32  NumNodesInTree = 0;

const SCORE KeepThreshold = 400;


void Abort (
    LearnTree &oldTree,
    LearnTree &newTree )
{
    oldTree.close();
    newTree.close();
    exit(1);
}


void ChessFatal ( const char *msg )
{
    cerr << "ChessFatal: " << msg << endl;
    exit(1);
}


void CleanTree (
    LearnTree &oldTree,
    LearnTree &newTree,
    ChessBoard &board,
    INT32 offset = 0,
    const LearnBranch *parent = 0,
    INT32 parentOffset = -1 )
{
    MoveList ml;
    board.GenMoves (ml);

    INT32 prev = -1;
    INT32 newOffset = -1;
    while ( offset >= 0 )
    {
        LearnBranch branch;
        if ( !oldTree.read (offset, branch) )
        {
            cerr << "Error reading branch at offset " << offset << endl;
            Abort ( oldTree, newTree );
        }

        branch.reserved[0] = 0;
        oldTree.write (offset, branch);

        ++NodesRead;

        if ( !ml.IsLegal (branch.move) )
        {
            cerr << "Found illegal move:  [" <<
                 int(branch.move.source) << ", " <<
                 int(branch.move.dest) << "]" <<
                 " at offset " << offset << endl;
            ++NumIllegalMoves;
        }
        else if ( parent &&
                  (parent->move.score < -KeepThreshold ||
                   parent->move.score > KeepThreshold) )
        {
            ++NumPrunedSubtrees;
        }
        else if ( branch.sibling < -1 || branch.sibling >= NumNodesInTree )
        {
            ++MessedUpLinks;
            cerr << "Messed up sibling=" << branch.sibling <<
                 " at offset " << offset << endl;
        }
        else if ( branch.child < -1 || branch.child >= NumNodesInTree )
        {
            ++MessedUpLinks;
            cerr << "Messed up child=" << branch.child <<
                 " at offset " << offset << endl;
        }
        else  // This branch looks sane
        {
            LearnBranch copy = branch;
            copy.child = -1;
            copy.sibling = -1;
            if ( !newTree.append (newOffset, copy) )
            {
                cerr << "Error writing branch to output file" << endl;
                Abort ( oldTree, newTree );
            }

            ++NodesWritten;

            LearnBranch temp;
            if ( prev < 0 )
            {
                if ( parentOffset >= 0 )   // Need to backpatch parent
                {
                    if ( !newTree.read (parentOffset, temp) )
                    {
                        cerr << "Error reading parent (" << parentOffset <<
                             ") of offset " << offset << endl;
                        Abort ( oldTree, newTree );
                    }

                    temp.child = newOffset;

                    if ( !newTree.write (parentOffset, temp) )
                    {
                        cerr << "Error writing parent (" << parentOffset <<
                             ") of offset " << offset << endl;
                        Abort ( oldTree, newTree );
                    }
                }
            }
            else     // Need to backpatch left sibling.
            {
                if ( !newTree.read (prev, temp) )
                {
                    cerr << "Error reading left sibling (" << prev <<
                         ") of offset " << newOffset << endl;
                    Abort ( oldTree, newTree );
                }

                temp.sibling = newOffset;

                if ( !newTree.write (prev, temp) )
                {
                    cerr << "Error write left sibling (" << prev <<
                         ") of offset " << newOffset << endl;
                    Abort ( oldTree, newTree );
                }
            }

            if ( branch.child >= 0 )
            {
                // Now do recursion...

                UnmoveInfo unmove;
                board.MakeMove ( branch.move, unmove );

                CleanTree (
                    oldTree,
                    newTree,
                    board,
                    branch.child,
                    &copy,
                    newOffset );

                board.UnmakeMove ( branch.move, unmove );
            }
        }

        prev = newOffset;
        offset = branch.sibling;
    }
}


void SetAllMarkers ( LearnTree &tree, INT32 value )
{
    LearnBranch branch;
    for ( INT32 offset=0; tree.read (offset, branch); offset++ )
    {
        branch.reserved[0] = value;
        tree.write (offset, branch);
    }
}


long ReportOrphans ( LearnTree &tree )
{
    long orphanCount = 0;
    FILE *list = fopen ( "orphan.lst", "wt" );
    LearnBranch branch;
    for ( INT32 offset=0; tree.read (offset, branch); offset++ )
    {
        if ( branch.reserved[0] )
        {
            ++orphanCount;
            branch.reserved[0] = 0;
            tree.write (offset, branch);

            if ( list )
            {
                fprintf ( list,
                    "%10ld:  m=[%3u,%3u,%6d] t=%-6.2lf wl=%-3ld  c=%-9ld  s=%-9ld\n",
                    long (offset),
                    unsigned (branch.move.source),
                    unsigned (branch.move.dest),
                    int (branch.move.score),
                    double (branch.timeAnalyzed / 100.0),
                    long (branch.winsAndLosses),
                    long (branch.child),
                    long (branch.sibling) );
            }
        }
    }

    if ( list )  fclose (list);
    return orphanCount;
}


int main ( int argc, char *argv[] )
{
    if ( argc > 2 )
    {
        cerr << "Use:  clntree [<filename>]\n";
        cerr << "Default filename is '" << DEFAULT_CHENARD_TREE_FILENAME << "'\n" << flush;
        return 1;
    }

    const char *treeFilename = DEFAULT_CHENARD_TREE_FILENAME;
    if ( argc > 1 )
    {
        treeFilename = argv[1];
    }

    const char *tempFilename = "clntree.tmp";

    LearnTree oldTree;
    if ( !oldTree.open (treeFilename) )
    {
        cerr << "Error:  Cannot open input tree file '" <<
                treeFilename << "' for read." << endl;
        return 1;
    }

    cout << "Successfully opened input tree file '" <<
            treeFilename << "'." << endl;

    NumNodesInTree = oldTree.numNodes();
    cout << "There are " << NumNodesInTree << " nodes in the input." << endl;

    cout << "Marking all branches..." << endl;
    SetAllMarkers ( oldTree, 1 );

    LearnTree newTree;
    if ( !newTree.create (tempFilename) )
    {
        cerr << "Error:  Cannot open temporary tree file '" <<
                tempFilename << "' for write!" << endl;
        return 1;
    }

    cout << "Successfully opened output tree file '" <<
            tempFilename << "'." << endl;

    cout << "Beginning cleanup process..." << endl;

    ChessBoard board;
    CleanTree ( oldTree, newTree, board );

    cout << "Doing orphan report..." << endl;
    long numOrphans = ReportOrphans ( oldTree );

    oldTree.close();
    if ( !newTree.close() )
    {
        cerr << "Error closing output tree!" << endl;
    }
    else
    {
        cout << "Finished!" << endl;
        cout << "Nodes read    = " << NodesRead << endl;
        cout << "Nodes written = " << NodesWritten << endl;
        cout << "Num illegal   = " << NumIllegalMoves << endl;
        cout << "Num pruned    = " << NumPrunedSubtrees << endl;
        cout << "Num orphans   = " << numOrphans << endl;
        cout << "Bad links     = " << MessedUpLinks << endl;
    }

    return 0;
}


/*
    $Log: clntree.cpp,v $
    Revision 1.2  2006/03/20 20:44:10  dcross
    I finally figured out the weird slowness I have been seeing in Chenard accessing
    its experience tree.  It turns out it only happens the first time it accessed the file
    chenard.tre after a reboot.  It was caused by Windows XP thinking chenard.tre was
    an important system file, because of its extension:
    http://msdn.microsoft.com/library/default.asp?url=/library/en-us/sr/sr/monitored_file_extensions.asp
    We write 36 bytes to the middle of the file, then close it
    So I have changed the filename from chenard.tre to chenard.trx.
    I also added DEFAULT_CHENARD_TREE_FILENAME in lrntree.h so that I can more
    easily rename this again if need be.

    Revision 1.1  2005/11/25 19:47:26  dcross
    Recovered lots of old chess source files from the dead!
    Found these on CDROM marked "14 September 1999".
    Added cvs log tag and moved old revision history after that.




    Revision history:

1996 September 6 [Don Cross]
     Started writing.

1996 September 8 [Don Cross]
     Added check for bad child, sibling links.
*/

