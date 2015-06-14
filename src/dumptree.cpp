#include <stdio.h>
#include <iostream.h>
#include "chess.h"
#include "lrntree.h"

int main ( int argc, char *argv[] )
{
    const char *treeFilename = DEFAULT_CHENARD_TREE_FILENAME;
    if ( argc > 1 )
        treeFilename = argv[1];

    LearnTree tree;
    if ( !tree.open(treeFilename) )
    {
        cerr << "Cannot open input file '" << treeFilename << "'" << endl;
        return 1;
    }

    cout << "Successfully opened input file '" << treeFilename << "'" << endl;

    const char *listFilename = "dumptree.txt";
    FILE *f = fopen (listFilename, "wt");
    if ( !f )
    {
        cerr << "Cannot open output list file '" << listFilename << "'" << endl;
        return 1;
    }

    LearnBranch branch;
    for ( INT32 offset=0; tree.read(offset,branch); offset++ )
    {
        fprintf ( f,
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

    fclose (f);
    tree.close();

    return 0;
}

/*
    $Log: dumptree.cpp,v $
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

    Revision 1.1  2005/11/25 19:47:27  dcross
    Recovered lots of old chess source files from the dead!
    Found these on CDROM marked "14 September 1999".
    Added cvs log tag and moved old revision history after that.

*/

