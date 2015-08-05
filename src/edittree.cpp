/*==============================================================================

    edittree.cpp  -  Copyright (C) 1999-2005 by Don Cross

    This file allows navigating and editing the Chenard learning tree.
    The code here is intended for text mode operation.

==============================================================================*/

// standard includes...
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <ctype.h>
#include <io.h>

// project includes...
#include "chess.h"
#include "gamefile.h"
#include "lrntree.h"

ChessBoard *EditBoard = 0;
INT32 EditOffset [64];
int EditPly = 0;

Move MoveTable [64];
UnmoveInfo *UnmoveTable = 0;
int UnmoveTableSize = 0;
int EditLogFlag = 1;

//------------------------------------------------------------------------


typedef int (* TreeEditFunc) ( int argc, const char *argv[] );

// Function prototypes for all command functions...

int ETEC_help       ( int argc, const char *argv[] );
int ETEC_quit       ( int argc, const char *argv[] );
int ETEC_list       ( int argc, const char *argv[] );
int ETEC_move       ( int argc, const char *argv[] );
int ETEC_root       ( int argc, const char *argv[] );
int ETEC_up         ( int argc, const char *argv[] );
int ETEC_integrity  ( int argc, const char *argv[] );
int ETEC_delete     ( int argc, const char *argv[] );
int ETEC_path       ( int argc, const char *argv[] );
int ETEC_jump       ( int argc, const char *argv[] );
int ETEC_parents    ( int argc, const char *argv[] );
int ETEC_import     ( int argc, const char *argv[] );
int ETEC_import     ( int argc, const char *argv[] );
int ETEC_xpos       ( int argc, const char *argv[] );

// Help strings for all command functions...


//-------------------------------------------------------------------------

const char *HTXT_help =
    "help [Command]\n"
    "\n"
    "If 'Command' is missing, lists all commands available.\n"
    "If 'Command' is present, describes usage of that command.\n";

//-------------------------------------------------------------------------

const char *HTXT_quit =
    "quit\n"
    "\n"
    "Ends the edit session and exits the Chenard program.\n";

//-------------------------------------------------------------------------

const char *HTXT_list =
    "list\n"
    "\n"
    "Display the board and available moves for current position.\n";

//-------------------------------------------------------------------------

const char *HTXT_move =
    "move Ordinal\n"
    "\n"
    "Make the move with the given sibling Ordinal.\n"
    "Use 'list' command to see all moves and their ordinals.\n";

//-------------------------------------------------------------------------

const char *HTXT_root =
    "root\n"
    "\n"
    "Return to the root of the experience tree\n"
    "(i.e., the beginning of the game).\n";

//-------------------------------------------------------------------------

const char *HTXT_up =
    "up\n"
    "\n"
    "Undo move and return to parent of this node in tree.\n";

//-------------------------------------------------------------------------

const char *HTXT_integrity =
    "integrity\n"
    "\n"
    "Perform integrity check of this node and all children.\n";

//-------------------------------------------------------------------------

const char *HTXT_delete =
    "delete Ordinal\n"
    "\n"
    "Delete the subtree from the given position with the\n"
    "specified Ordinal.  Be careful -- no undo is available!\n";

//-------------------------------------------------------------------------

const char *HTXT_path =
    "path\n"
    "\n"
    "Displays the path of moves leading up to the current node.\n";

//-------------------------------------------------------------------------

const char *HTXT_jump =
    "jump Offset\n"
    "\n"
    "Searches for the path which leads to given offset and\n"
    "enters that state.\n";

//-------------------------------------------------------------------------

const char *HTXT_parents =
    "parents Offset\n"
    "\n"
    "Tries to find all nodes claiming to be parents of the\n"
    "node at the given offset.  This is useful for detecting\n"
    "munged branches in the tree.\n";

//-------------------------------------------------------------------------

const char *HTXT_import =
    "import game_file_spec\n"
    "\n"
    "Reads the game(s) listed in the file and\n"
    "absorbs them into the tree.\n"
    "The file may be PGN, Yahoo email text, or GAM.\n"
    "Wildcards (e.g. '*.pgn') may be used.\n";

//-------------------------------------------------------------------------

const char *HTXT_xpos =
    "xpos\n"
    "\n"
    "Scans the entire experience tree and looks for\n"
    "multiple paths that lead to transposed positions.\n"
    ;

//-------------------------------------------------------------------------


struct TreeEditCommandEntry
{
    const char      *verb;
    TreeEditFunc     func;
    const char      *help;
};


TreeEditCommandEntry TreeEditTable[] =
{
    { "delete",     ETEC_delete,    HTXT_delete     },
    { "help",       ETEC_help,      HTXT_help       },
    { "import",     ETEC_import,    HTXT_import     },
    { "integrity",  ETEC_integrity, HTXT_integrity  },
    { "jump",       ETEC_jump,      HTXT_jump       },
    { "list",       ETEC_list,      HTXT_list       },
    { "move",       ETEC_move,      HTXT_move       },
    { "parents",    ETEC_parents,   HTXT_parents    },
    { "path",       ETEC_path,      HTXT_path       },
    { "quit",       ETEC_quit,      HTXT_quit       },
    { "root",       ETEC_root,      HTXT_root       },
    { "up",         ETEC_up,        HTXT_up         },
    { "xpos",       ETEC_xpos,      HTXT_xpos       },

    {0,0,0} // marks end of list
};


TreeEditCommandEntry *LocateCommand ( const char *verb )
{
    for ( int i=0; TreeEditTable[i].verb; ++i )
    {
        if ( strcmp(verb,TreeEditTable[i].verb) == 0 )
        {
            return &TreeEditTable[i];
        }
    }

    return 0;
}


void ListBranches();


void IllegalSquareFormat ( unsigned ofs, char *string )
{
    int x = XPART(ofs);
    int y = YPART(ofs);
    if ( x >= 2 && x <= 9 && y >= 2 && y <= 9 )
    {
        string[0] = x - 2 + 'a';
        string[1] = y - 2 + '1';
        string[2] = '\0';
    }
    else
    {
        sprintf ( string, "%u", ofs );
    }
}


void IllegalMoveFormat ( Move move, char *string )
{
    char src[64], dest[64];
    IllegalSquareFormat ( move.source & BOARD_OFFSET_MASK, src );
    IllegalSquareFormat ( move.dest, dest );
    sprintf ( string, "?(%s,%s)", src, dest );
}


int lprintf ( const char *format, ... )
{
    va_list marker;
    va_start ( marker, format );
    int retcode = vprintf ( format, marker );
    if ( EditLogFlag )
    {
        FILE *f = fopen("edittree.txt","at");
        if ( f )
        {
            vfprintf ( f, format, marker );
            fclose(f);
        }
    }
    va_end (marker);
    return retcode;
}


int CommandScanner ( const char *argv[], char *command )
{
    int argc = 0;
    int index = 0;
    for(;;)
    {
        // Find beginning of token or end of string...

        while ( command[index] && isspace(command[index]) )
            ++index;

        if ( !command[index] )
            break;

        argv[argc++] = &command[index];

        // Find end of token (whether or not it is end of string)

        while ( command[index] && !isspace(command[index]) )
            ++index;

        if ( command[index] )
            command[index++] = '\0';
        else
            break;
    }

    return argc;
}


int ExecuteTreeEditCommand ( char *command )
{
    const char *argv[512];
    int argc = CommandScanner ( argv, command );

    if ( argc > 0 )
    {
        // Search for the verb 'argv[0]' in the table...

        TreeEditCommandEntry *e = LocateCommand(argv[0]);
        if ( e )
            return (*e->func) (argc, argv);

        fprintf ( stderr, "\nError: Unknown edit command verb '%s'\n\n", argv[0] );
    }

    return 0;
}


int EditLearnTree()
{
    ChessBoard board;
    EditBoard = &board;

    const int unmoveBufferSize = 128;
    UnmoveTableSize = unmoveBufferSize;
    UnmoveInfo unmoveBuffer [unmoveBufferSize];
    UnmoveTable = unmoveBuffer;

    EditPly = 0;
    EditOffset[EditPly] = 0;

    printf ( "Entering Chenard experience tree editor.\n" );
    printf ( "Enter 'help' for information about commands.\n\n" );
    ListBranches();

    char command [1024];
    for(;;)
    {
        printf ( "TreeEdit> " );
        fflush (stdout);

        if ( !fgets(command,sizeof(command),stdin) )
            break;

        if ( ExecuteTreeEditCommand (command) )
            break;
    }

    EditBoard = 0;
    UnmoveTable = 0;
    UnmoveTableSize = 0;
    return 0;
}


int OpenForRead ( LearnTree &tree )
{
    if ( !tree.open(DEFAULT_CHENARD_TREE_FILENAME) )
    {
        fprintf ( stderr,
                  "Error opening tree file '%s' for read\n",
                  DEFAULT_CHENARD_TREE_FILENAME );

        return 0;
    }

    return 1;
}


void Edit_DrawBoard ( const ChessBoard &board )
{
    const SQUARE *b = board.queryBoardPointer();
    printf ( "   +--------+\n" );
    for ( int y=9; y >= 2; --y )
    {
        printf ( " %d |", y-1 );
        for ( int x=2; x <= 9; ++x )
        {
            char c = PieceRepresentation ( b[OFFSET(x,y)] );
            printf ( "%c", c );
        }

        printf ( "|" );

        if ( y == 6 )
            printf ( "   ply  = %d", board.GetCurrentPlyNumber() );
        else if ( y == 5 )
            printf ( "   hash = %08lx", (unsigned long)(board.Hash()) );

        printf ( "\n" );
    }
    printf ( "   +--------+\n" );
    printf ( "    abcdefgh\n\n" );
}


//-------------------------------------------------------------------


int ETEC_help ( int argc, const char *argv[] )
{
    if ( argc > 1 )
    {
        for ( int i=1; i < argc; ++i )
        {
            TreeEditCommandEntry *e = LocateCommand(argv[i]);
            if ( e )
                printf ( "\n%s\n", e->help );
            else
                printf ( "Unknown command '%s'\n", argv[i] );
        }
    }
    else
    {
        printf ( "Help is available for the following commands:\n\n" );
        int col = 0;
        for ( int i=0; TreeEditTable[i].verb; ++i )
        {
            if ( i > 0 )
                printf ( "," );

            if ( col > 65 )
            {
                printf ( "\n" );
                col = 0;
            }

            col += printf ( " %s", TreeEditTable[i].verb );
        }

        printf ( "\n\nFor example, try 'help list'.\n\n" );
    }

    return 0;
}


int ETEC_quit ( int argc, const char *argv[] )
{
    return 1;   // flag to exit command loop
}


void DumpMovePath()
{
    MoveList ml;
    ChessBoard board;
    int outOfWhack = 0;

    lprintf ( "\n{" );
    int col = 1;

    for ( int i=0; i < EditPly; ++i )
    {
        if ( !outOfWhack )
        {
            board.GenMoves (ml);
            if ( !ml.IsLegal(MoveTable[i]) )
                outOfWhack = 1;
        }

        if ( i > 0 )
            col += lprintf ( "," );

        if ( col > 50 )
        {
            lprintf ( "\n" );
            col = 0;
        }

        char moveString [64];
        if ( outOfWhack )
            IllegalMoveFormat ( MoveTable[i], moveString );
        else
            FormatChessMove ( board, MoveTable[i], moveString );

        col += lprintf ( " %s", moveString );

        UnmoveInfo unmove;
        board.MakeMove ( MoveTable[i], unmove );
    }

    lprintf ( " }\n" );
}


void ListBranches()
{
    assert (EditBoard != NULL);
    Edit_DrawBoard (*EditBoard);

    MoveList ml;
    EditBoard->GenMoves (ml);
    if ( ml.num == 0 || EditBoard->IsDefiniteDraw() )
    {
        printf ( ">>> At end of game.\n" );
        return;
    }

    LearnTree tree;
    if ( !OpenForRead(tree) )
        return;

    LearnBranch branch;
    int ordinal = 0;
    for ( INT32 offset = EditOffset[EditPly]; offset >= 0; ++ordinal )
    {
        printf ( "%3d.  ", ordinal );
        if ( !tree.read(offset,branch) )
        {
            printf ( "<!!! error reading branch at offset=%ld !!!>\n", long(offset) );
            break;
        }

        char temp [256];
        if ( ml.IsLegal(branch.move) )
            FormatChessMove ( *EditBoard, branch.move, temp );
        else
            IllegalMoveFormat ( branch.move, temp );

        if ( branch.move.source == 0 )
        {
            printf ( "[deleted]     %09ld\n", long(offset) );
        }
        else
        {
            printf ( "%c %-10s  %09ld: s=%-6d t=%-5.1lf WL=%-5d e=%-8ld a=%-4ld\n",
                     (branch.child == -1) ? ' ' : '*',
                     temp,
                     long(offset),
                     int(branch.move.score),
                     double(branch.timeAnalyzed) / 100.0,
                     long(branch.winsAndLosses),
                     long(branch.nodesEvaluated),
                     long(branch.numAccesses) );
        }

        offset = branch.sibling;
    }
}


int ETEC_list ( int argc, const char *argv[] )
{
    ListBranches();
    return 0;
}


int ETEC_move ( int argc, const char *argv[] )
{
    assert (EditBoard != NULL);
    assert (UnmoveTable != NULL);

    int ply = EditBoard->GetCurrentPlyNumber();
    assert ( ply == EditPly );

    MoveList ml;
    EditBoard->GenMoves (ml);
    if ( ml.num == 0 || EditBoard->IsDefiniteDraw() )
    {
        printf ( ">>> At end of game.\n" );
        return 0;
    }

    if ( argc != 2 )
    {
        fprintf ( stderr, "Use:  move Ordinal\n" );
        return 0;
    }

    int seekord = 0;
    if ( sscanf(argv[1],"%d",&seekord) != 1 || seekord < 0 )
    {
        fprintf ( stderr, "Invalid integer argument '%s'\n", argv[1] );
        return 0;
    }

    if ( EditOffset[EditPly] == -1 )
    {
        printf ( ">>> At leaf of experience tree.\n" );
        return 0;
    }

    LearnTree tree;
    if ( !OpenForRead(tree) )
        return 1;

    LearnBranch branch;

    int ordinal = 0;
    for ( INT32 offset = EditOffset[EditPly]; offset >= 0; ++ordinal )
    {
        if ( !tree.read(offset,branch) )
        {
            fprintf ( stderr, "!!! Error reading offset %ld !!!\n", long(offset) );
            return 0;
        }

        if ( seekord == ordinal )
        {
            if ( ml.IsLegal(branch.move) )
            {
                MoveTable[EditPly] = branch.move;
                EditBoard->MakeMove ( branch.move, UnmoveTable[EditPly] );
                EditOffset[++EditPly] = branch.child;
                ListBranches();
            }
            else
            {
                char temp [64];
                IllegalMoveFormat ( branch.move, temp );
                fprintf ( stderr, "!!! Error: illegal move %s at offset %ld\n",
                          temp,
                          long(offset) );
            }
            break;
        }

        offset = branch.sibling;
    }

    return 0;
}


int ETEC_root ( int argc, const char *argv[] )
{
    EditPly = 0;
    EditBoard->Init();
    ListBranches();
    return 0;
}


int ETEC_up ( int argc, const char *argv[] )
{
    if ( EditPly <= 0 )
    {
        printf ( ">>> Already at root of tree.\n" );
    }
    else
    {
        --EditPly;
        EditBoard->UnmakeMove ( MoveTable[EditPly], UnmoveTable[EditPly] );
        ListBranches();
    }

    return 0;
}


void PerformIntegrityCheck (
    LearnTree &tree,
    INT32 baseOffset )
{
    char temp [256];

    assert (EditBoard != NULL);
    assert (UnmoveTable != NULL);

    int ply = EditBoard->GetCurrentPlyNumber();
    assert ( ply == EditPly );

    MoveList ml;
    EditBoard->GenMoves (ml);
    if ( ml.num == 0 || EditBoard->IsDefiniteDraw() )
        return;

    LearnBranch branch;
    for ( INT32 offset = baseOffset; offset >= 0; offset = branch.sibling )
    {
        int ignoreNode = 0;
        if ( !tree.read(offset,branch) )
        {
            lprintf ( "Error reading branch at offset %ld\n", long(offset) );
            return;
        }

        ++branch.reserved[0];
        tree.write(offset,branch);

        if ( !ml.IsLegal(branch.move) )
        {
            ignoreNode = 1;
            if ( branch.move.source != 0 )
            {
                IllegalMoveFormat ( branch.move, temp );
                lprintf ( "Illegal move=%s offset=%ld path=",
                          temp, long(offset) );
                DumpMovePath();
                lprintf ( "\n" );
            }
        }

        if ( branch.child >= 0 && !ignoreNode )
        {
            MoveTable[EditPly] = branch.move;
            EditBoard->MakeMove ( branch.move, UnmoveTable[EditPly] );
            EditOffset[EditPly] = offset;
            EditOffset[++EditPly] = branch.child;

            PerformIntegrityCheck ( tree, branch.child );

            --EditPly;
            EditBoard->UnmakeMove ( branch.move, UnmoveTable[EditPly] );
        }
    }

    EditOffset[EditPly] = baseOffset;
}


int ETEC_integrity ( int argc, const char *argv[] )
{
    LearnTree tree;
    if ( !OpenForRead(tree) )
        return 1;

    lprintf ( ">>> Starting depth-first integrity check at offset %ld...\n",
              long(EditOffset[EditPly]) );

    PerformIntegrityCheck ( tree, EditOffset[EditPly] );

    lprintf ( ">>> Second pass: linear check...\n" );
    INT32 numNodes = tree.numNodes();
    LearnBranch branch;
    for ( INT32 offset = 0; offset < numNodes; ++offset )
    {
        if ( !tree.read(offset,branch) )
        {
            lprintf ( ">>> Error reading offset %ld\n", long(offset) );
            return 0;
        }

        if ( branch.reserved[0] > 0 )
        {
            if ( branch.reserved[0] > 1 )
            {
                lprintf ( ">>> Branch at offset %ld has %ld referents!\n",
                          long(offset),
                          long(branch.reserved[0]) );
            }

            branch.reserved[0] = 0;
            if ( !tree.write(offset,branch) )
            {
                lprintf ( ">>> Error writing back branch %ld\n", long(offset) );
                return 0;
            }
        }
    }

    return 0;
}


int ETEC_delete ( int argc, const char *argv[] )
{
    if ( argc != 2 )
    {
        printf ( "Missing ordinal argument.\n" );
        return 0;
    }

    int ord = 0;
    if ( sscanf(argv[1],"%d",&ord) != 1 || ord < 0 )
    {
        printf ( "Invalid ordinal '%s'\n", argv[1] );
        return 0;
    }

    LearnTree tree;
    if ( !OpenForRead(tree) )
        return 1;

    LearnBranch branch;

    int ordinal = 0;
    for ( INT32 offset = EditOffset[EditPly]; offset >= 0; ++ordinal )
    {
        if ( !tree.read(offset,branch) )
        {
            printf ( ">>> Error reading offset %ld !!!\n", long(offset) );
            return 0;
        }

        if ( ordinal == ord )
        {
            INT32 saveSibling = branch.sibling;
            memset ( &branch, 0, sizeof(LearnBranch) );
            branch.sibling = saveSibling;
            branch.child = -1;

            if ( !tree.write(offset,branch) )
                printf ( ">>> Error writing branch back to offset %ld.\n", long(offset) );
            else
                printf ( "*** Deleted ordinal=%d, offset=%ld.\n", ordinal, long(offset) );

            return 0;
        }

        offset = branch.sibling;
    }

    printf ( ">>> Error finding ordinal %d.\n", ord );
    return 0;
}


int ETEC_path ( int argc, const char *argv[] )
{
    int saveFlag = EditLogFlag;
    EditLogFlag = 0;

    DumpMovePath();

    EditLogFlag = saveFlag;
    return 0;
}


int FindPathToOffset ( LearnTree &tree, INT32 current, INT32 target )
{
    static char errorBuffer [128];

    MoveList ml;
    EditBoard->GenMoves(ml);

    LearnBranch branch;
    for ( INT32 offset = current; offset >= 0; offset = branch.sibling )
    {
        if ( offset == target )
            return 1;   // found it!

        if ( !tree.read(offset,branch) )
        {
            sprintf ( errorBuffer, "Cannot read offset %ld", long(offset) );
            throw errorBuffer;
        }

        if ( branch.child >= 0 && ml.IsLegal(branch.move) )
        {
            MoveTable[EditPly] = branch.move;
            EditBoard->MakeMove ( branch.move, UnmoveTable[EditPly] );
            EditOffset[++EditPly] = branch.child;

            int found = FindPathToOffset ( tree, branch.child, target );
            if ( found )
                return 1;

            --EditPly;
            EditBoard->UnmakeMove ( branch.move, UnmoveTable[EditPly] );
        }
    }

    return 0;
}


int ETEC_parents ( int argc, const char *argv[] )
{
    assert ( EditBoard != NULL );

    if ( argc != 2 )
    {
        printf ( "Missing offset parameter\n" );
        return 0;
    }

    long scanTarget = 0;
    if ( sscanf(argv[1],"%ld",&scanTarget) != 1 || scanTarget < 0 )
    {
        printf ( "Invalid target offset %ld\n", scanTarget );
        return 0;
    }

    INT32 target = INT32(scanTarget);

    LearnTree tree;
    if ( !OpenForRead(tree) )
        return 1;

    int numMatches = 0;
    LearnBranch branch;
    INT32 numNodes = tree.numNodes();
    for ( INT32 offset = 0; offset < numNodes; ++offset )
    {
        if ( !tree.read(offset,branch) )
        {
            printf ( ">>> Error reading offset %ld\n", long(offset) );
            return 0;
        }

        if ( branch.child == target )
            printf ( "#%d:  parent  offset = %ld\n", ++numMatches, long(offset) );

        if ( branch.sibling == target )
            printf ( "#%d:  sibling offset = %ld\n", ++numMatches, long(offset) );
    }

    return 0;
}


int ETEC_jump ( int argc, const char *argv[] )
{
    assert ( EditBoard != NULL );

    if ( argc != 2 )
    {
        printf ( "Error: Missing offset argument\n" );
        return 0;
    }

    LearnTree tree;
    if ( !OpenForRead(tree) )
        return 1;

    long scanofs = 0;
    if ( sscanf(argv[1],"%ld",&scanofs) != 1 || scanofs < 0 )
    {
        printf ( "Invalid offset '%s'\n", argv[1] );
        return 0;
    }

    EditPly = 0;
    EditBoard->Init();

    int found = 0;

    try
    {
        found = FindPathToOffset (tree, 0, INT32(scanofs));
    }
    catch ( const char *message )
    {
        printf ( ">>> %s\n", message );
    }

    if ( found )
    {
        printf ( "Successfully found offset %ld\n", scanofs );
        ListBranches();
    }
    else
    {
        printf ( ">>> Could not find path to offset %ld\n", scanofs );
        printf ( ">>> Returned to root of tree.\n" );
    }

    return 0;
}


LearnBranch *LoadEntireTree (int &tablesize)
{
    tablesize = 0;
    LearnBranch *table = NULL;

    FILE *infile = fopen (DEFAULT_CHENARD_TREE_FILENAME, "rb");
    if (infile)
    {
        // Figure out how many total branches there are in the file...
        if (0 == fseek (infile, 0, SEEK_END))
        {
            long pos = ftell (infile);
            if (pos > 0)
            {
                if (pos % sizeof(LearnBranch) == 0)
                {
                    tablesize = pos / sizeof(LearnBranch);
                    printf("There are %d branches in %s\n", tablesize, DEFAULT_CHENARD_TREE_FILENAME);
                    // Now the scary part... allocating all that memory!
                    table = new LearnBranch [tablesize];
                    if (table)
                    {
                        rewind (infile);
                        size_t numread = fread (table, sizeof(LearnBranch), tablesize, infile);
                        if (tablesize == numread)
                        {
                            printf("Successfully loaded file.\n");
                        }
                        else
                        {
                            printf("Error: Could read only %d branches.\n", numread);
                            delete[] table;
                            table = NULL;
                            tablesize = 0;
                        }
                    }
                    else
                    {
                        printf("Out of memory!\n");
                        tablesize = 0;
                    }
                }
                else
                {
                    printf("File %s is not a multiple of sizeof(LearnBranch) == %d\n", DEFAULT_CHENARD_TREE_FILENAME, sizeof(LearnBranch));
                }
            }
            else
            {
                printf("Zero byte file %s\n", DEFAULT_CHENARD_TREE_FILENAME);
            }
        }
        else
        {
            printf("Error seeking to EOF %s\n", DEFAULT_CHENARD_TREE_FILENAME);
        }

        fclose (infile);
        infile = NULL;
    }
    else
    {
        printf("Cannot open file %s\n", DEFAULT_CHENARD_TREE_FILENAME);
    }

    return table;
}


int NumHashPositions = 0;


void CalculatePositionHashes (LearnBranch *table, int tablesize, ChessBoard &board, int node)
{
    UnmoveInfo  unmove;

    assert (node >= 0);
    assert (node < tablesize);

    int myhash = board.Hash();

    do
    {
        ++NumHashPositions;
        table[node].reserved[0] = myhash;
        if (table[node].child >= 0)
        {
            board.MakeMove (table[node].move, unmove);
            CalculatePositionHashes (table, tablesize, board, table[node].child);
            board.UnmakeMove (table[node].move, unmove);
            int checkhash = board.Hash();
            assert (checkhash == myhash);
        }
        node = table[node].sibling;
    }
    while (node >= 0);

    assert (node == -1);
}


int HackTableSortIndex;
int BranchTableCompare (const void *aptr, const void *bptr)
{
    const LearnBranch &a = *((const LearnBranch *) aptr);
    const LearnBranch &b = *((const LearnBranch *) bptr);
    return a.reserved[HackTableSortIndex] - b.reserved[HackTableSortIndex];
}


int CountDuplicateHashes (const LearnBranch *table, int tablesize)
{
    int count = 0;
    for (int i=1; i<tablesize; ++i)
    {
        if (table[i-1].reserved[0] == table[i].reserved[0])
        {
            ++count;
        }
    }
    return count;
}


void SearchForTranspositions (LearnBranch *table, int tablesize)
{
    ChessBoard board;
    int i;

    printf("Calculating hash values of all positions...\n");
    CalculatePositionHashes (table, tablesize, board, 0);
    printf("Num positions hashed = %d\n", NumHashPositions);

    int OriginalDuplicates = CountDuplicateHashes (table, tablesize);

    // Store index of each position in reserved[1].
    // This allows us to unsort the table back later.
    for (i=0; i<tablesize; ++i)
    {
        table[i].reserved[1] = i;
    }

    // Sort the table an ascending order of hash value.
    // Hack:  so that we can use the same sort for
    // reserved[0] (hash) and reserved[1] (original index),
    // set a global variable..
    printf("Sorting table by hash value...\n");
    HackTableSortIndex = 0;
    qsort (table, tablesize, sizeof(LearnBranch), BranchTableCompare);

    // Count up how many positions have the same hash value as another...
    int SortedDuplicates = CountDuplicateHashes (table, tablesize);

    printf("Sorting table back to original order...\n");
    HackTableSortIndex = 1;
    qsort (table, tablesize, sizeof(LearnBranch), BranchTableCompare);

    for (i=0; i<tablesize; ++i)
    {
        assert (table[i].reserved[1] == i);
    }

    int CheckOriginalDuplicates = CountDuplicateHashes (table, tablesize);
    assert (CheckOriginalDuplicates == OriginalDuplicates);

    printf ("Duplicates:  original=%d, sorted=%d, excess=%d\n", OriginalDuplicates, SortedDuplicates, (SortedDuplicates-OriginalDuplicates));

    const char * const OUTFILENAME = "hash.trx";
    FILE *outfile = fopen (OUTFILENAME, "wb");
    if (outfile)
    {
        if (tablesize == (int) fwrite (table, sizeof(LearnBranch), tablesize, outfile))
        {
            printf("Saved table with hash values to %s\n", OUTFILENAME);
        }
        else
        {
            printf("!!! Could not write table to %s\n", OUTFILENAME);
        }
        fclose (outfile);
        outfile = NULL;
    }
    else
    {
        printf("Could not open %s for write.\n", OUTFILENAME);
    }
}


int ETEC_xpos ( int argc, const char *argv[] )
{
    int           tablesize;
    LearnBranch  *table = LoadEntireTree (tablesize);
    if (table)
    {
        SearchForTranspositions (table, tablesize);
        delete[] table;
        table = NULL;
    }

    return 0;
}


bool bscat (char *fullpath, int fullpathsize, const char *dir, const char *name)
{
    bool success = false;
    int dirlen = (int) strlen(dir);
    if ((int)(2 + dirlen + strlen(name)) < fullpathsize)
    {
        strcpy (fullpath, dir);

        if ((dirlen > 0) && (dir[dirlen-1] != '\\'))
        {
            fullpath[dirlen++] = '\\';
        }

        strcpy (&fullpath[dirlen], name);
        success = true;
    }
    return success;
}


bool ImportGameFile (LearnTree &tree, const char *filename)
{
    int  updates  = 0;
    int  branches = 0;
    PGN_FILE_STATE state = PGN_FILE_STATE_UNDEFINED;
    bool success  = false;

    tChessMoveStream *stream = tChessMoveStream::OpenFileForRead (filename);
    if (!stream)
    {
        printf("Cannot open file for read: '%s'\n", filename);
    }
    else
    {
        ChessBoard  board;
        Move        move;
        UnmoveInfo  unmove;

        while (stream->GetNextMove(move, state))
        {
            if (state == PGN_FILE_STATE_NEWGAME)
            {
                board.Init();
            }
            if (board.GetCurrentPlyNumber() < (int)MaxLearnDepth)
            {
                int result = tree.rememberPosition (board, move, 0, 0, 1, 0);
                if (result == 0)
                {
                    printf ("??? Could not add move to tree ???\n");
                    break;
                }
                else if (result == 1)
                {
                    ++updates;
                }
                else if (result == 2)
                {
                    ++branches;
                }
                else
                {
                    printf ("??? tree.RememberPosition returned %d\n", result);
                    break;
                }
            }
            board.MakeMove (move, unmove);
        }
        delete stream;
        
        if (state != PGN_FILE_STATE_FINISHED)
        {
            // We don't consider this a fatal error, just a warning.
            printf("??? Error loading PGN file '%s': %s\n", filename, GetPgnFileStateString(state));
        }

        success = true;

        printf(">>> file='%s', branches=%d, updates=%d\n", filename, branches, updates);
    }

    return success;
}


bool ExtractDir (char *dir, int dirsize, const char *filespec)
{
    bool success = false;

    const char *finalbs = strrchr (filespec, '\\');
    if (finalbs)
    {
        int length = (int) (finalbs - filespec);     // number of characters up to, but excluding, final backslash
        if (length < dirsize)
        {
            memcpy (dir, filespec, length);
            dir[length] = '\0';
            success = true;
        }
    }
    else
    {
        if (dirsize >= 2)
        {
            strcpy (dir, ".");
            success = true;
        }
    }

    return success;
}


int ETEC_import ( int argc, const char *argv[] )
{
    if (argc != 2)
    {
        printf("Missing filespec parameter.\n");
        return 1;
    }

    LearnTree tree;
    if ( OpenForRead(tree) )       // actually, this opens existing file for modification
    {
        try
        {
            char            dir [512];
            char            fullpath [512];
            int             numfiles = 0;
            _finddata_t     finfo;

            if (ExtractDir (dir, sizeof(dir), argv[1]))
            {
                intptr_t handle = _findfirst (argv[1], &finfo);
                if (handle != -1)
                {
                    do
                    {
                        if (!(finfo.attrib & _A_SUBDIR))
                        {
                            if (bscat (fullpath, sizeof(fullpath), dir, finfo.name))
                            {
                                if (ImportGameFile (tree, fullpath))
                                {
                                    ++numfiles;
                                }
                            }
                            else
                            {
                                printf("!!! ERROR forming full path !!!\n");
                                break;
                            }
                        }
                    }
                    while (0 == _findnext(handle,&finfo));
                    _findclose (handle);
                }

                if (numfiles == 0)
                {
                    printf ("!!! ERROR importing files using '%s' as filespec.\n", argv[1]);
                }
                else
                {
                    printf ("Imported %d game files.\n", numfiles);
                }
            }
            else
            {
                printf("!!! ERROR extracting dir from filespec.\n");
            }
        }
        catch (const char *message)
        {
            printf (">>> %s\n", message);
        }
    }

    return 0;
}


/*
    $Log: edittree.cpp,v $
    Revision 1.11  2006/03/20 20:44:10  dcross
    I finally figured out the weird slowness I have been seeing in Chenard accessing
    its experience tree.  It turns out it only happens the first time it accessed the file
    chenard.tre after a reboot.  It was caused by Windows XP thinking chenard.tre was
    an important system file, because of its extension:
    http://msdn.microsoft.com/library/default.asp?url=/library/en-us/sr/sr/monitored_file_extensions.asp
    We write 36 bytes to the middle of the file, then close it
    So I have changed the filename from chenard.tre to chenard.trx.
    I also added DEFAULT_CHENARD_TREE_FILENAME in lrntree.h so that I can more
    easily rename this again if need be.

    Revision 1.10  2006/02/19 23:29:47  dcross
    Fixed typo in prior checkin comment.

    Revision 1.9  2006/01/29 21:01:55  dcross
    Added "xpos" command to explore the possible advantages of reworking the experience
    tree to handle transpositions instead of just assuming that every continuation leads to
    a unique board position.  Here is the output of the current chenard.tre:

        TreeEdit> xpos
        There are 454844 branches in chenard.tre
        Successfully loaded file.
        Calculating hash values of all positions...
        Num positions hashed = 454844
        Sorting table by hash value...
        Sorting table back to original order...
        Duplicates:  original=4581, sorted=103776, excess=99195
        Saved table with hash values to hash.tre

    So the conclusion is that almost 22% of the experience tree consists of duplicate positions,
    though I don't know how many of those are for the same side to move.
    It is also interesting that only 1% of the positions appear to have more than one
    legal move stored.  It would appear that it is hardly worthwhile to have multiple moves
    per position.

    Revision 1.8  2006/01/23 20:09:08  dcross
    Reworked IMPORT command so that I can use wildcards to import lots of game files all at once.

    Revision 1.7  2006/01/20 21:12:40  dcross
    Renamed "loadpgn" command to "import", because now it can read in
    PGN, Yahoo, or GAM formats.  To support PGN files that contain multiple
    game listings, I had to add a bool& parameter to GetMove method.
    Caller should check this and reset the board if needed.

    Revision 1.6  2006/01/14 22:21:00  dcross
    Added support for analyzing PGN games in addition to existing support for GAM and Yahoo formats.

    Revision 1.5  2006/01/02 19:40:29  dcross
    Added ability to use File/Open dialog to load PGN files into Win32 version of Chenard.

    Revision 1.4  2006/01/01 20:06:12  dcross
    Finished getting PGN-to-tree slurper working.

    Revision 1.3  2006/01/01 19:19:54  dcross
    Starting to add support for loading PGN data files into Chenard.
    First I am starting with the ability to read in an opening library into the training tree.
    Not finished yet.

    Revision 1.2  2005/11/25 19:06:32  dcross
    Added cvs log tag, moved old revision history to end, etc.



        Revision history:

    1999 March 3 [Don Cross]
        Started writing.

*/

