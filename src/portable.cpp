/*=========================================================================

     portable.cpp  -  Copyright (C) 1993-2005 by Don Cross

     Contains main() for a completely portable (stdio.h UI)
     version of NewChess.

=========================================================================*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "chess.h"
#include "gamefile.h"
#include "chenga.h"
#include "uistdio.h"
#include "lrntree.h"


int AnalyzeGameFile (
    double thinkTimeInSeconds,
    const char *inGameFilename,
    const char *outListFilename );

const char * const CHENARD_VERSION = ConvertDateToVersion(__DATE__);

static void Advertise()
{
    printf ( "\nChenard - a portable chess program, v %s", CHENARD_VERSION);
    printf ( "\n" );
    printf ( "by Don Cross  -  http://cosinekitty.com/chenard/\n" );

    #if CHENARD_LINUX
        #if defined(__apple_build_version__)
            printf ( "Mac OS X version; compiled with Apple Clang %s\n\n", __VERSION__ );
        #elif defined(__GNUG__)
            printf ( "Linux version; compiled with GNU C++ %s\n\n", __VERSION__ );
        #else
            printf ( "Linux version; unknown compiler used.\n\n");
        #endif
    #elif defined(WIN32)
        #if defined(_MSC_VER)
            printf ( "Win32 version; compiled with Microsoft Visual C++ %0.1lf\n\n", (double)(_MSC_VER - 600)/100.0);
        #else
            printf ( "Win32 version; unknown compiler used.\n\n");
        #endif
    #else
        printf ( "(Unknown operating system build - may have incorrect behavior.)\n\n");
    #endif
}


bool LoadGameFile(ChessBoard& board, const char *inFileName)
{
    tChessMoveStream *stream = tChessMoveStream::OpenFileForRead(inFileName);
    if (stream != NULL)
    {
        PGN_FILE_STATE state;
        Move move;
        UnmoveInfo unmove;
        while (stream->GetNextMove(move, state))
        {
            if (state == PGN_FILE_STATE_NEWGAME)
            {
                board.Init();
            }
            board.MakeMove(move, unmove);
        }
        delete stream;

        if (state != PGN_FILE_STATE_FINISHED)
        {
            // Something went wrong parsing this PGN file.
            printf("ERROR loading PGN file '%s': %s\n", inFileName, GetPgnFileStateString(state));
            return false;
        }

        return true;
    }
    else
    {
        printf("Cannot open file '%s'\n", inFileName);
    }
    return false;
}


int MakeFlywheelUnitTest(const char *inPgnFileName, const char *outFileName, const char *varname)
{
    int rc = 1;
    remove(outFileName);    // delete file if already exists
    FILE *infile = fopen(inPgnFileName, "rt");
    if (infile == NULL)
    {
        fprintf(stderr, "ERROR: Cannot open input PGN file '%s'\n", inPgnFileName);
        return 1;
    }
    
    FILE *outfile = fopen(outFileName, "wt");
    if (outfile)
    {
        PgnExtraInfo    info;
        PGN_FILE_STATE  state;
        char            movestr [1 + MAX_MOVE_STRLEN];
        ChessBoard      board;
        Move            move;
        UnmoveInfo      unmove;
        
        rc = 0;
        fprintf(outfile, "var %s = [\n", varname);
    
        bool first = true;
        
        for(;;)
        {
            while (GetNextPgnMove (infile, movestr, state, info))
            {
                if (first)
                {
                    first = false;
                    fprintf(outfile, "  { ");
                }
                else
                {
                    fprintf(outfile, ", { ");
                }                
            
                if (state == PGN_FILE_STATE_NEWGAME)                
                {
                    fprintf(outfile, "\"reset\": true\n");
                    board.Init();
                }
                else
                {
                    fprintf(outfile, "\"reset\": false\n");
                }
                
                MoveList legal;
                board.GenMoves(legal);
                
                if (!ParseFancyMove (movestr, board, move))
                {
                    fprintf(stderr, "ERROR: illegal move '%s'\n", movestr);
                    rc = 1;
                    goto failure_exit;
                }
                
                char notation[LONGMOVE_MAX_CHARS];
                if (!FormatLongMove(board.WhiteToMove(), move, notation))
                {
                    fprintf(stderr, "ERROR: cannot format move\n");
                    rc = 1;
                    goto failure_exit;
                }
                
                fprintf(outfile, ",   \"move\": \"%s\"\n", notation);
                
                fprintf(outfile, ",   \"legal\": [");
                for (int i=0; i < legal.num; ++i)
                {
                    if (!FormatLongMove(board.WhiteToMove(), legal.m[i], notation))
                    {
                        fprintf(stderr, "ERROR: cannot format legal move %d\n", i);
                        rc = 1;
                        goto failure_exit;
                    }
                    if (i > 0) 
                    {
                        fprintf(outfile, ", ");
                    }
                    fprintf(outfile, "\"%s\"", notation);
                }
                fprintf(outfile, "]\n");
                
                board.MakeMove(move, unmove);
                
                char fen[200];
                if (!board.GetForsythEdwardsNotation(fen, sizeof(fen)))
                {
                    fprintf(stderr, "ERROR: cannot calculate FEN\n");
                    rc = 1;
                    goto failure_exit;
                }
                
                bool check    = board.CurrentPlayerInCheck();
                bool mobile   = board.CurrentPlayerCanMove();
                int  reps = 0;
                int  draw     = (!mobile && !check) || board.IsDefiniteDraw(&reps);
                
                fprintf(outfile, ",   \"fen\": \"%s\"\n",  fen);
                fprintf(outfile, ",   \"check\": %s\n",    check  ? "true" : "false");
                fprintf(outfile, ",   \"mobile\": %s\n",   mobile ? "true" : "false");
                fprintf(outfile, ",   \"draw\": %s }\n\n", draw   ? "true" : "false");
            }
            
            if (state != PGN_FILE_STATE_FINISHED)
            {
                break;      // nothing left in this PGN file, or we encountered a syntax error
            }
        }        
        
failure_exit:            
        fprintf(outfile, "];\n");
        fclose(outfile);
        if (rc != 0)
        {
            remove(outFileName);
        }
    }
    else
    {
        fprintf(stderr, "ERROR: cannot open file '%s' for write\n", outFileName);
    }

    fclose(infile);
    
    return rc;
}


int main ( int argc, const char *argv[] )
{
    char line [256];
    ChessUI_stdio  theUserInterface;
    ChessBoard     theBoard;

    Advertise();

    if ( argc == 1 ||
         (argc>1 && argv[1][0] != '-') )
    {
        const char *gameFilename = "game.pgn";
        if ( argc >= 2 )
        {
            gameFilename = argv[1];
            // Load the game from the given filename.
            if ( LoadGameFile ( theBoard, gameFilename ) )
            {
                printf ( "Continuing game in file '%s'\n", gameFilename );
                for (int i=2; i < argc; ++i)
                {
                    const char *option = argv[i];
                    if (0 == strcmp(option, "--combat"))
                    {
                        theUserInterface.EnableCombatMode(theBoard.SideToMove());
                    }
                    else
                    {
                        printf("Error: unknown option '%s'\n", option);
                        return 1;
                    }
                }
            }
            else
            {
                return 1;
            }
        }

        ChessGame theGame ( theBoard, theUserInterface );

        if ( gameFilename )
        {
            if ( !theGame.AutoSaveToFile ( gameFilename ) )
            {
                fprintf (
                    stderr,
                    "Error:  Auto-save '%s' option has failed (out of memory?)\x07\n",
                    gameFilename );

                return 1;
            }
        }

        theGame.Play();
    }
    else if ( argc >= 2 )
    {
        if ( strncmp ( argv[1], "-t", 2 ) == 0 )
        {
            // Tree trainer

            int timeToThink = atoi(argv[1]+2);
            if ( timeToThink < 1 )
            {
                timeToThink = 10;
            }

            ComputerChessPlayer *whitePlayer = new ComputerChessPlayer (theUserInterface);
            ComputerChessPlayer *blackPlayer = new ComputerChessPlayer (theUserInterface);

            whitePlayer->SetTimeLimit ( INT32(timeToThink) * INT32(100) );
            blackPlayer->SetTimeLimit ( INT32(timeToThink) * INT32(100) );

            whitePlayer->SetOpeningBookEnable ( false );
            blackPlayer->SetOpeningBookEnable ( false );

            whitePlayer->SetTrainingEnable ( false );
            blackPlayer->SetTrainingEnable ( false );

            theUserInterface.SetScoreDisplay ( true );

            Learn_Output = 1;

            LearnTree tree;
            for ( int argindex=2; argindex < argc; ++argindex )
            {
                tree.absorbFile ( argv[argindex] );
            }

            tree.trainer (
                whitePlayer,
                blackPlayer,
                theBoard,
                theUserInterface,
                INT32(timeToThink) * INT32(100)
            );

            delete whitePlayer;
            delete blackPlayer;
        }
        else if ( strncmp ( argv[1], "-s", 2 ) == 0 )
        {
            // Self play

            int timeToThink = atoi(argv[1]+2);
            if ( timeToThink < 1 )
                timeToThink = 10;

            ComputerChessPlayer *whitePlayer =
                new ComputerChessPlayer ( theUserInterface );

            ComputerChessPlayer *blackPlayer =
                new ComputerChessPlayer ( theUserInterface );

            if ( !whitePlayer || !blackPlayer )
                ChessFatal ( "Out of memory!\n" );

            whitePlayer->SetTimeLimit ( INT32(timeToThink) * INT32(100) );
            blackPlayer->SetTimeLimit ( INT32(timeToThink) * INT32(100) );

            whitePlayer->setExtendedSearchFlag ( true );
            blackPlayer->setExtendedSearchFlag ( true );

            theUserInterface.SetScoreDisplay ( true );

            unsigned long uniqueTag = 1;
            extern const char *LogFilename;
            const char *tagFilename = "chenard.id";
            FILE *tagSaveFile = fopen ( tagFilename, "rt" );
            if ( tagSaveFile )
            {
                if ( fscanf ( tagSaveFile, "%lu", &uniqueTag ) != 1 )
                    uniqueTag = 1;

                fclose ( tagSaveFile );
                tagSaveFile = 0;
            }

            tagSaveFile = fopen ( tagFilename, "wt" );
            if ( !tagSaveFile )
            {
                sprintf ( line,
                          "Cannot open unique tag file '%s' for write!\n",
                          tagFilename );

                ChessFatal ( line );
            }

            fprintf ( tagSaveFile, "%lu\n", 1+uniqueTag );
            fclose ( tagSaveFile );

            static char uniqueFilename [256];
            sprintf ( uniqueFilename, "self%04lu.txt", uniqueTag );
            LogFilename = uniqueFilename;

            ChessGame theGame (
                theBoard,
                theUserInterface,
                false,     // white is played by a ComputerChessPlayer object
                false,     // black is played by a ComputerChessPlayer object
                whitePlayer,
                blackPlayer );

            // When playing against self, improve training by telling
            // one of the players (randomly) to not use opening book.

            if ( ChessRandom(2) )
                whitePlayer->SetOpeningBookEnable ( false );
            else
                blackPlayer->SetOpeningBookEnable ( false );

            theGame.Play();
            Advertise();
        }
        else if ( strcmp ( argv[1], "-g" ) == 0 )
        {
            // Genetic algorithm...

            theUserInterface.SetScoreDisplay (false);
            ChessGA  ga (theUserInterface, 1);

            if ( !ga.load() )
            {
                printf ( "!!!!  Cannot load existing gene pool.\n" );
                printf ( "!!!!  Enter desired gene pool size: " );
                fflush (stdout);
                int population = 0;
                if ( !fgets(line,sizeof(line),stdin) || sscanf(line,"%d",&population)!=1 )
                    return 1;

                ga.createRandomPopulation (population);
                ga.save();
            }

            ga.run();
        }
        else if ( strcmp ( argv[1], "-b" ) == 0 )
        {
            // Battle two genes against each other...
            if ( argc < 3 )
            {
                fprintf ( stderr,
                          "Gene battle usage:\n\n"
                          "    chenard -b genefile1 [genefile2]\n\n" );

                return 1;
            }

            const char *genFn1 = argv[2];
            const char *genFn2 = (argc > 3) ? argv[3] : 0;
            int rc = GeneBattle ( theUserInterface, genFn1, genFn2 );
            return rc;
        }
        else if ( strcmp ( argv[1], "-G" ) == 0 )
        {
            if ( argc < 5 )
            {
                fprintf ( stderr,
                          "Gene pool merge usage:\n\n"
                          "    chenard -G poolsize otherdir1 otherdir2 ...\n\n" );

                return 1;
            }

            // Slurp "cream of the crop" from other GAs into current directory.
            int outputPoolSize = atoi(argv[2]);

            return ChessGA::MergePools (
                       outputPoolSize,
                       argc-3,
                       argv+3,
                       theUserInterface,
                       1 );
        }
#if !CHENARD_LINUX
        else if ( strcmp ( argv[1], "-E" ) == 0 )
        {
            extern int EditLearnTree();
            return EditLearnTree();
        }
#endif
        else if ( strcmp ( argv[1], "-p" ) == 0 )
        {
            SCORE window = 0;
            if ( argc>=3 && (window=atoi(argv[2])) >= 50 )
            {
                const char *inTreeFilename  = DEFAULT_CHENARD_TREE_FILENAME;
                const char *outTreeFilename = "packed.trx";
                printf ( "Packing experience tree file...\n" );
                printf ( "   Input  filename = '%s'\n", inTreeFilename );
                printf ( "   Output filename = '%s'\n", outTreeFilename );
                return LearnTree::Pack ( inTreeFilename, outTreeFilename, window );
            }
            else
            {
                fprintf ( stderr,
                          "Use:  %s -p scoreWindow\n\nwhere 'scoreWindow' is at least 50\n\n",
                          argv[0] );

                return 0;
            }
        }
        else if ( strcmp ( argv[1], "-a" ) == 0 )
        {
            // chenard -a thinkTimeSeconds inGameFile outListingFile

            if ( argc != 5 )
            {
                fprintf ( stderr,
                          "Use:  %s -a thinkTimeSeconds inGameFile outListingFile\n\n",
                          argv[0] );

                return 0;
            }

            const double thinkTimeSeconds = atof ( argv[2] );
            if ( thinkTimeSeconds < 0.1 )
            {
                fprintf ( stderr,
                          "Invalid think time '%s'\n",
                          argv[2] );

                return 1;
            }

            const char *inGameFilename = argv[3];
            const char *outListFilename = argv[4];
            return AnalyzeGameFile ( thinkTimeSeconds, inGameFilename, outListFilename );
        }
        else if (strcmp(argv[1], "--flytest") == 0)
        {
            if (argc != 5)
            {
                fprintf(stderr, "Use: %s --flytest infile.pgn outfile.js varname\n", argv[0]);
                return 1;
            }
            const char *inPgnFileName = argv[2];
            const char *outFileName = argv[3];
            const char *varname = argv[4];
            return MakeFlywheelUnitTest(inPgnFileName, outFileName, varname);
        }
        else
        {
            fprintf ( stderr,
                      "Use:  %s [-s<time_per_move_in_seconds>]\n", argv[0] );

            fprintf ( stderr,
                      "The option '-s' causes Chenard to play against itself.\n" );

            return 1;
        }
    }

    return 0;
}


class ChessUI_stdio_InterceptBestPath: public ChessUI_stdio
{
public:
    void DisplayBestPath ( const ChessBoard &, const BestPath & );
    BestPath snagBestPath;
    FILE *listFile;     // !!! temp debug
};


void ChessUI_stdio_InterceptBestPath::DisplayBestPath (
    const ChessBoard &board,
    const BestPath &path )
{
    snagBestPath = path;
    ChessUI_stdio::DisplayBestPath ( board, path );

#if 0 // !!! temp debug
    if (listFile)
    {
        fprintf ( listFile, "\n{ " );
        ChessBoard temp = board;
        Move move;
        UnmoveInfo unmove;
        char moveString [64];
        for ( int i=0; i <= path.depth; ++i )
        {
            move = path.m[i];
            FormatChessMove ( temp, move, moveString );
            if ( i > 0 ) fprintf ( listFile, ", " );
            fprintf ( listFile, "%s", moveString );
            temp.MakeMove ( move, unmove );
        }
        fprintf ( listFile, "}\n" );
    }
#endif
}


void AnalyzePosition (
    ChessUI_stdio_InterceptBestPath &ui,
    ComputerChessPlayer &thinker,
    ChessBoard &_board,
    FILE *listFile,
    int column )
{
    INT32 timeSpent = 0;
    Move bestMove;
    bool gotMove = thinker.GetMove ( _board, bestMove, timeSpent );

    char moveString [64];
    ChessBoard board = _board;

    const BestPath &path = ui.snagBestPath;
    if ( gotMove && path.depth >= 0 )
    {
        column += fprintf ( listFile, "[%d: ", int(bestMove.score) );

        for ( int i=0; i <= path.depth; i++ )
        {
            Move move = path.m[i];
            UnmoveInfo unmove;

            FormatChessMove ( board, move, moveString );
            board.MakeMove ( move, unmove );

            int newcol = (int) (column + strlen(moveString) + 2);
            if ( newcol > 78 )
            {
                fprintf ( listFile, "\n" );
                column = fprintf ( listFile, "                   " );
            }

            column += fprintf ( listFile, "%s", moveString );

            if ( i < path.depth )
                column += fprintf ( listFile, ", " );
        }

        column += fprintf ( listFile, "]" );
    }
}


int AnalyzeGameFile (
    double thinkTimeInSeconds,
    const char *inGameFilename,
    const char *outListFilename )
{
    FILE *gameFile = fopen ( inGameFilename, "rb" );
    if ( !gameFile )
    {
        fprintf ( stderr, "Error:  Cannot open game file '%s' for read\n\n", inGameFilename );
        return 1;
    }

    fclose (gameFile);  // close file, because stream object will open it again
    gameFile = NULL;

    FILE *listFile = fopen ( outListFilename, "wt" );
    if ( !listFile )
    {
        fprintf ( stderr, "Error:  Cannot open list file '%s' for write\n\n", outListFilename );
        return 2;
    }

    fprintf ( listFile, "Chenard auto-analysis of game file '%s'\n", inGameFilename );
    fprintf ( listFile, "Think time = %0.2lf seconds.\n\n", thinkTimeInSeconds );

    ChessUI_stdio_InterceptBestPath ui;
    ui.listFile = NULL; // !!! temp debug: causes thinking output to be dumped to output file
    ui.SetScoreDisplay (true);

    ComputerChessPlayer thinker(ui);
    thinker.SetTrainingEnable (false);
    thinker.setExtendedSearchFlag (false);
    thinker.setResignFlag (false);
    thinker.SetTimeLimit ( INT32(100.0 * thinkTimeInSeconds) );

    ChessBoard board;
    Move move;
    UnmoveInfo unmove;
    int moveNumber = 0;
    char moveString [64];

    tChessMoveStream *stream = tChessMoveStream::OpenFileForRead (inGameFilename);
    if (stream)
    {
        PGN_FILE_STATE state;
        for (int i=0; stream->GetNextMove(move, state); ++i)
        {
            if (state == PGN_FILE_STATE_NEWGAME)
            {
                board.Init();
                fprintf (listFile, "\n----------- NEW GAME -----------\n\n");
            }
            ui.DrawBoard (board);
            int column = 0;
            if ( (i & 1) == 0 )
                column += fprintf ( listFile, "%4d. ", ++moveNumber );
            else
                column += fprintf ( listFile, "      " );

            if ( (move.dest & SPECIAL_MOVE_MASK) == SPECIAL_MOVE_EDIT )
            {
                fprintf ( listFile, "*** board was edited -- ending analysis.\n" );
                break;
            }

            FormatChessMove ( board, move, moveString );
            column += fprintf ( listFile, "%-7s  ", moveString );
            ui.snagBestPath.depth = -1;
            AnalyzePosition ( ui, thinker, board, listFile, column );
            fprintf ( listFile, "\n" );
            if ( i & 1 )
                fprintf ( listFile, "\n" );
            fflush ( listFile );
            board.MakeMove ( move, unmove );
        }

        delete stream;
        if (state != PGN_FILE_STATE_FINISHED)
        {
            fprintf(stderr, "Error parsing PGN file '%s': %s\n", inGameFilename, GetPgnFileStateString(state));
        }
    }
    else
    {
        fprintf (stderr, "Error:  Failure to create stream object for '%s'\n", inGameFilename);
        return 3;
    }

    fclose ( listFile );

    return 0;
}



/*
    $Log: portable.cpp,v $
    Revision 1.16  2008/12/02 01:54:05  Don.Cross
    Identify O/S and compiler consistently in initial print statements.

    Revision 1.15  2008/12/01 23:08:53  Don.Cross
    Norm Pollock announced on the WinBoard forum that he has updated his PGN database:
        http://www.open-aurec.com/wbforum/viewtopic.php?t=49522
        http://www.hoflink.com/~npollock/chess.html
    This is just what I was looking for, so that I could beef up chenard.trx (training file).
    I modified the PGN scanner to recognize WhiteElo, BlackElo, and Result tags.
    I am using WhiteElo and BlackElo to decide whether a game is trustworthy enough to pretend like Chenard thought
    of the move itself, so that it will play that move in actual games, instead of just as a reference point for opponent moves.
    I am currently in the process of slurping an enormous amount of openings into chenard.trx.
    I decided to move GetNextPgnMove from board.cpp to gamefile.cpp, where it really belongs.

    Revision 1.14  2008/11/24 11:21:58  Don.Cross
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

    Revision 1.13  2008/11/06 22:25:06  Don.Cross
    Now ParseFancyMove accepts either PGN or longmove (e7e8q) style notation only.
    We tolerate of check '+' or checkmate '#' characters being absent or extraneous in the input.
    The former is especially important because I often see PGN notation where '+' is missing on checking moves.
    To allow for PGN input, I had to allow avoidance of the PromotePawn callback, so other code was changed as well.
    Version numbers are now always constently yyyy.mm.dd across all builds of Chenard!

    Revision 1.12  2008/10/27 21:47:07  Don.Cross
    Made fixes to help people build text-only version of Chenard in Linux:
    1. The function 'stricmp' is called 'strcasecmp' in Linux Land.
    2. Found a fossil 'FALSE' that should have been 'false'.
    3. Conditionally remove some relatively unimportant conio.h code that does not translate nicely in the Linux/Unix keyboard model.
    4. Added a script for building using g++ in Linux.

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

    Revision 1.10  2006/01/20 21:12:40  dcross
    Renamed "loadpgn" command to "import", because now it can read in
    PGN, Yahoo, or GAM formats.  To support PGN files that contain multiple
    game listings, I had to add a bool& parameter to GetMove method.
    Caller should check this and reset the board if needed.

    Revision 1.9  2006/01/18 19:58:13  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.8  2006/01/14 18:10:27  dcross
    Turn of debug output to analysis list file.

    Revision 1.7  2006/01/14 17:13:06  dcross
    Added new class library for auto-detecting chess game file format and reading moves from it.
    Now using this new class for "portable.exe -a" game analysis.
    Still need to add support for PGN files:  now it supports only GAM and Yahoo formats.
    Also would be nice to go back and make File/Open in winchen.exe use this stuff.

    Revision 1.6  2006/01/01 19:19:54  dcross
    Starting to add support for loading PGN data files into Chenard.
    First I am starting with the ability to read in an opening library into the training tree.
    Not finished yet.

    Revision 1.5  2005/12/31 21:37:02  dcross
    1. Added the ability to generate Forsyth-Edwards Notation (FEN) string, representing the state of the game.
    2. In stdio version of chess UI, display FEN string every time the board is printed.

    Revision 1.4  2005/11/25 19:12:20  dcross
    Recovered edittree.cpp from the dead!
    Found this on CDROM marked "14 September 1999".
    This allowed me to reinstate the experience tree editor in Portable Chenard.
    Had to move some stuff from protected to public in class LearnTree.

    Revision 1.3  2005/11/23 21:30:31  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:42  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!


         Revision history:

    1993 May [Don Cross]
         Started writing.

    1993 August 30 [Don Cross]
         Changing pointers to references in the interfaces where
         appropriate.

    1995 March 26 [Don Cross]
         Added self-play capability.

    1995 April 2 [Don Cross]
         Added command line option to load game from file.
         This enables Dr. Chenard to play against itself in
         an arbitrary position.
         Made separate functions LoadGame and SaveGame.

    1996 February 28 [Don Cross]
         Put in advertising for my email address and web page.

    1996 July 28 [Don Cross]
         Made it so that when you pass a game filename as argv[1],
         Chenard will use it whether or not the file already exists.
         If the file exists, Chenard loads the game state from it.
         Whether or not it exists, the filename is used for autosave.
         If no filename is provided, Chenard now always starts new
         game and autosaves to "chess.gam".
         These changes make it more convenient to play games on ChessLive.

    1996 August 23 [Don Cross]
         Replacing old memory-based learning tree with disk-based tree file

    1999 January 1 [Don Cross]
         Minor code style changes.
         Added conditionally compiled code to recognize GNU C++ version.

    1999 January 15 [Don Cross]
         Merging in lots of minor changes from Windows/DOS versions.

    1999 January 18 [Don Cross]
         Now tree trainer can absorb game files specified on command line
         into the experience tree before beginning training.

    1999 February 17 [Don Cross]
         Adding a genetic algorithm for evolving better eval and
         moving ordering heuristics.

    1999 July 19 [Don Cross]
         Adding a game analysis option '-a'.  This goes through a
         given game file move by move and performs a search and
         prints out the score and best path for each position in the
         game.  This is useful for studying human-vs-human games.

    2001 January 12 [Don Cross]
         Adding tree packer option '-p'.

*/

