// convert.cpp   -   main() for converting old tree to new tree.

#include <stdio.h>
#include <stdlib.h>

#include "chess.h"
#include "lrntree.h"

int main ()
{
    // Load the old learning tree

    Learn_Output = 1;
    printf ( "Loading old learning tree...\n" );
    LoadLearningTree();
    printf ( "Beginning conversion...\n" );
    ConvertLearningTree ( DEFAULT_CHENARD_TREE_FILENAME );
    printf ( "Conversion finished!\n" );

    return 0;
}


void  ChessFatal ( const char *message )
{
    fprintf ( stderr, message );
    exit(1);
}

/*
    $Log: convert.cpp,v $
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

*/

