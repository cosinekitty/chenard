/*============================================================================

    chenga.cpp  -  Copyright (C) 1999-2005 by Don Cross

    I'm changing Chenard's evaluation heuristic constants from #defines
    to an array held in memory.  This will allow me to construct a
    genetic algorithm to evolve better heuristics.

    This file contains code needed for the genetic algorithm but not
    the release version of the search.

============================================================================*/

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define  DISPLAY_GAME_RESULTS  1
#define  DISPLAY_GAME_MILLISECONDS  500
#define  LOG_EVERY_GAME  0


const char * const CHENARD_STATS_FILENAME = "chenard.sta";

#ifdef _WIN32
    #include <windows.h>
#endif

#include "chess.h"
#include "chenga.h"


void ChessGene::randomize()
{
    int i;
    for ( i=0; i < NUM_CHESS_GENES; ++i )
    {
        if ( !DefTable[i].name )
            ChessFatal ( "ChessGene::DefTable[] is too short!" );

        v[i] = DefTable[i].random();
    }

    if ( DefTable[i].name )
        ChessFatal ( "ChessGene::DefTable[] is too long!" );
}


void ChessGene::blend (
    const ChessGene &a,
    const ChessGene &b )
{
    for ( int i=0; i < NUM_CHESS_GENES; ++i )
    {
        // First, decide whether to do random interpolation,
        // random selection, or complete mutation.

        int spliceRand = ChessRandom(1000);
        if ( spliceRand < 20 )  // Complete mutation
        {
            v[i] = DefTable[i].random();
        }
        else if ( spliceRand < 200 )  // Interpolate
        {
            double r = ChessRandom(1000) / 999.0;
            double range = b.v[i] - a.v[i];
            double interp = a.v[i] + r*range;
            v[i] = SCORE ( floor(interp + 0.5) );
        }
        else  // Copy from one of parents
        {
            if ( spliceRand & 1 )
                v[i] = a.v[i];
            else
                v[i] = b.v[i];
        }
    }
}


//--------------------------------------------------------------------------


double ChessGeneInfo::fitness() const
{
    long totalGames = numWins + numLosses + numDraws;
    if ( totalGames <= 0 )
        return 0;

    return double(numWins - numLosses) / double(16 + totalGames);
}


//--------------------------------------------------------------------------

const int MIN_GENES =    10;
const int MAX_GENES = 10000;
static const char *ChessGA_main_filename = "chenard.ga";


ChessGA::ChessGA ( ChessUI &_ui, int _allowTextOutput ):
    allowTextOutput(_allowTextOutput),
    ui(&_ui),
    numGenes(0),
    info(0),
    currentGene(0),
    numRounds(0),
    numGamesCompleted(0),
    startTime(0),
    nextGeneId(0),
    numWhiteWins(0),
    numBlackWins(0),
    numDraws(0),
    resignThreshold(800),
    minTime(100),
    spreadTime(300),
    rooksAndKingsPlies(0)
{
}


ChessGA::ChessGA():
    allowTextOutput(0),
    ui(0),
    numGenes(0),
    info(0),
    currentGene(0),
    numRounds(0),
    numGamesCompleted(0),
    startTime(0),
    nextGeneId(0),
    numWhiteWins(0),
    numBlackWins(0),
    numDraws(0),
    resignThreshold(800),
    minTime(100),
    spreadTime(300),
    rooksAndKingsPlies(0)
{
}


ChessGA::~ChessGA()
{
    if ( info )
    {
        delete[] info;
        info = 0;
    }

    numGenes = 0;
}


void ChessGA::createRandomPopulation ( int _numGenes )
{
    char message [256];
    sprintf ( message, "*$*$*$*$*$*$*  creating new pool of %d genes", _numGenes );
    logdate ( message );

    if ( info )
    {
        delete[] info;
        info = 0;
    }

    info = new ChessGeneInfo [numGenes = _numGenes];
    if ( !info )
    {
        numGenes = 0;
        ChessFatal ( "Out of memory in ChessGA constructor" );
        return;
    }

    nextGeneId = 0;
    for ( int i=0; i < numGenes; ++i )
    {
        info[i].gene.randomize();
        info[i].numWins         = 0;
        info[i].numLosses       = 0;
        info[i].numDraws        = 0;
        info[i].numGamesAsWhite = 0;
        info[i].numGamesAsBlack = 0;
        info[i].id = nextGeneId++;
    }

    currentGene = numRounds = 0;
    numWhiteWins = numBlackWins = numDraws = 0;
}


int CopyFile ( const char *src, const char *dest )
{
    FILE *f = fopen (src, "rb");
    if ( !f )
        return 0;

    FILE *g = fopen (dest, "wb");
    if ( !g )
    {
        fclose(f);
        return 0;
    }

    int c;
    while ( (c = fgetc(f)) != EOF )
    {
        if ( fputc(c,g) == EOF )
        {
            fclose(g);
            fclose(f);
            return 0;       // disk full???
        }
    }

    fclose(g);
    fclose(f);
    return 1;
}


int ChessGA::saveGaOnly()
{
    // Make backups...
    CopyFile ( "chenard.ga2", "chenard.ga3" );
    CopyFile ( "chenard.ga1", "chenard.ga2" );
    CopyFile ( ChessGA_main_filename, "chenard.ga1" );

    FILE *f = fopen ( ChessGA_main_filename, "wt" );
    if ( !f )
    {
        printf ( "Error opening file '%s' for write.\n", ChessGA_main_filename );
        return 0;
    }

    fprintf ( f, "numGenes=%d\n",       numGenes );
    fprintf ( f, "currentGene=%d\n",    currentGene );
    fprintf ( f, "numRounds=%ld\n",     numRounds );
    fprintf ( f, "nextGeneId=%ld\n",    nextGeneId );

    for ( int i=0; i < numGenes; ++i )
    {
        const ChessGeneInfo &x = info[i];
        fprintf ( f, 
            "%04d wins=%-5ld losses=%-5ld draws=%-5ld nwhite=%-5ld nblack=%-5ld id=%ld\n",
            i, x.numWins, x.numLosses, x.numDraws, 
            x.numGamesAsWhite, x.numGamesAsBlack, x.id );
    }

    fclose (f);
    return 1;
}


int ChessGA::saveGenFiles()
{
    char geneFilename [128];

    for ( int i=0; i < numGenes; ++i )
    {
        const ChessGeneInfo &x = info[i];
        sprintf ( geneFilename, "%04d.gen", i );
        if ( !x.gene.save(geneFilename) )
        {
            printf ( "Error saving gene %d to file '%s'.", i, geneFilename );
            return 0;
        }
    }

    return 1;
}


int ChessGA::save()
{
    FILE *f = fopen ( ChessGA_main_filename, "wt" );
    if ( !f )
    {
        printf ( "Error opening file '%s' for write.\n", ChessGA_main_filename );
        return 0;
    }

    fprintf ( f, "numGenes=%d\n",       numGenes );
    fprintf ( f, "currentGene=%d\n",    currentGene );
    fprintf ( f, "numRounds=%ld\n",     numRounds );
    fprintf ( f, "nextGeneId=%ld\n",    nextGeneId );

    for ( int i=0; i < numGenes; ++i )
    {
        const ChessGeneInfo &x = info[i];
        fprintf ( f, 
            "%04d wins=%-5ld losses=%-5ld draws=%-5ld nwhite=%-5ld nblack=%-5ld id=%ld\n",
            i, x.numWins, x.numLosses, x.numDraws, 
            x.numGamesAsWhite, x.numGamesAsBlack, x.id );

        char geneFilename [128];
        sprintf ( geneFilename, "%04d.gen", i );
        if ( !x.gene.save(geneFilename) )
        {
            printf ( "Error saving gene %d to file '%s'.", i, geneFilename );
            return 0;
        }
    }

    fclose (f);
    return 1;
}


int ChessGA::load ( const char *dirPrefix )
{
    char gaFilename [512];
    sprintf ( gaFilename, "%s%s", dirPrefix, ChessGA_main_filename );

    FILE *f = fopen ( gaFilename, "rt" );
    if ( !f )
    {
        printf ( "Error opening file '%s' for read.\n", gaFilename );
        return 0;
    }

    printf ( "Opened genetic algorithm file '%s'\n", gaFilename );

//-------------------- process first line: number of genes

    char line [512];
    if ( !fgets(line,sizeof(line),f) || sscanf(line,"numGenes=%d",&numGenes) != 1 )
    {
        printf("Error reading number of genes from first line of '%s'\n", gaFilename);

        return 0;
    }

    printf ( "Number of genes = %d.\n", numGenes );
    if ( numGenes > MAX_GENES )
    {
        printf ( "Number of genes is too large!\n" );
        return 0;
    }

    if ( numGenes < MIN_GENES )
    {
        printf ( "Number of genes is too small!\n" );
        return 0;
    }

//-------------------- process second line: current gene index

    if ( !fgets(line,sizeof(line),f) || sscanf(line,"currentGene=%d",&currentGene) != 1 )
    {
        printf("Error reading currentGene index from second line of '%s'\n", gaFilename);

        return 0;
    }

    if ( currentGene < 0 || currentGene >= numGenes )
    {
        printf ( "Invalid current gene index %d in file '%s'.\n",
                 currentGene,
                 gaFilename );

        return 0;
    }

    printf ( "Current gene index = %d.\n", currentGene );

//-------------------- process third line: number of rounds completed

    if ( !fgets(line,sizeof(line),f) || sscanf(line,"numRounds=%ld",&numRounds) != 1 )
    {
        printf("Error reading number of rounds from third line of '%s'\n", gaFilename);

        return 0;
    }

    printf ( "Total number of rounds completed so far:  %ld\n", numRounds );

    if ( numRounds < 0 )
    {
        printf ( "Invalid value for numRounds!\n" );
        return 0;
    }

//-------------------- process fourth line: next gene ID

    if ( !fgets(line,sizeof(line),f) || sscanf(line,"nextGeneId=%ld",&nextGeneId) != 1 )
    {
        printf("Error reading nextGeneId from fourth line of '%s'\n", gaFilename);
        return 0;
    }

//-------------------- read gene array...

    if ( info )
        delete[] info;

    info = new ChessGeneInfo [numGenes];
    if ( !info )
    {
        printf ( "Out of memory allocating info[] array!\n" );
        return 0;
    }

    for ( int i=0; i < numGenes; ++i )
    {
        if ( !fgets(line,sizeof(line),f) )
        {
            printf ( "Error reading line %d from file '%s'.\n", i, gaFilename );
            return 0;
        }

        ChessGeneInfo &x = info[i];
        int check_i = 0;
        int numScanned = sscanf ( line,
                                  "%d wins=%ld losses=%ld draws=%ld nwhite=%ld nblack=%ld id=%ld",
                                  &check_i, &x.numWins, &x.numLosses, &x.numDraws,
                                  &x.numGamesAsWhite, &x.numGamesAsBlack, &x.id );

        if ( numScanned < 6 )
        {
            printf("Error scanning items at index %d of file '%s'\n", i, gaFilename);
            printf ( "numScanned = %d\n", numScanned );
            return 0;
        }

        if ( numScanned < 7 )
        {
            x.id = nextGeneId++;
        }

        if ( check_i != i )
        {
            printf ( "Mismatching index: expected %d, found %d in file '%s'\n",
                     i, check_i, gaFilename );

            return 0;
        }

        char geneFilename [128];
        sprintf ( geneFilename, "%s%04d.gen", dirPrefix, i );
        if ( !x.gene.load(geneFilename) )
        {
            printf ( "Error loading gene %d from file '%s'\n", i, geneFilename );
            return 0;
        }
    }

    fclose(f);

    f = fopen ( CHENARD_STATS_FILENAME, "rt" );
    if ( f )
    {
        int numScanned = fscanf ( f, 
            "W=%ld B=%ld D=%ld", 
            &numWhiteWins, &numBlackWins, &numDraws );

        if ( numScanned != 3 )
        {
            numWhiteWins = numBlackWins = numDraws = 0;
        }

        fclose(f);
    }
    else
        numWhiteWins = numBlackWins = numDraws = 0;

    numGamesCompleted = currentGene + numGenes*numRounds;
    printf ( "Successfully loaded all genes.\n" );

    return 1;
}


void ChessGA::logdate ( const char *description )
{
    time_t now;
    time(&now);
    char dateString [128];
    strcpy ( dateString, ctime(&now) );

    // Remove any '\n' from time string
    for ( char *p = dateString; *p; )
    {
        if ( *p == '\n' )
            *p = '\0';
        else
            ++p;
    }

    lprintf ( "\n%s - %s\n", dateString, description );
}


int ChessGA::lprintf ( const char *format, ... )
{
    const char * const logFilename = "chenga.log";
    FILE *f = fopen ( logFilename, "at" );
    if ( f )
    {
        va_list argptr;
        va_start ( argptr, format );
        int numChars = vfprintf ( f, format, argptr );
        va_end ( argptr );
        fclose (f);
        return numChars;
    }

    return 0;
}


int ChessGA::printf ( const char *format, ... )
{
    if ( allowTextOutput )
    {
        char s[512];
        va_list argptr;
        va_start ( argptr, format );
        int numChars = vsprintf ( s, format, argptr );
        va_end ( argptr );

        for ( const char *p = s; *p; ++p )
            fputc ( *p, stdout );

        fflush (stdout);

        return numChars;
    }

    return 0;
}


//------------------------------------------------------------------------------


void ChessGA::run()
{
    if ( !ui )
    {
        printf ( "No ChessUI defined in ChessGA::run()!\n" );
        exit(1);
    }

    printf ( "Entering genetic algorithm...\n" );
    logdate ( "Entering genetic algorithm" );

    if ( numGenes < MIN_GENES )
    {
        printf ( "Number of genes is too small!" );
        return;
    }

    if ( numGenes > MAX_GENES )
    {
        printf ( "Number of genes is too large!" );
        return;
    }

#ifdef _WIN32
    // Set thread priority to IDLE...

    HANDLE hProcess = GetCurrentProcess();
    if ( SetPriorityClass(hProcess,IDLE_PRIORITY_CLASS) )
    {
        lprintf ( "***  Set process priority class to IDLE\n" );
    }
    else
    {
        lprintf ( "***  Error %ld setting process priority class to IDLE.\n",
                  long(GetLastError()) );
    }
#endif

    displayGameResults ( "Started genetic algorithm" );

    for ( startTime = ChessTime(); saveGaOnly(); ++numGamesCompleted )
    {
#if LOG_EVERY_GAME
        lprintf ( "\nnum games completed = %d\n", numGamesCompleted );
#endif

        // Pick randomly from other genes available, but exclude
        // playing against self (that would be a waste of time
        // because it would have no net effect on fitness scores).

        int otherGene = ChessRandom ( numGenes - 1 );
        if ( otherGene >= currentGene )
            ++otherGene;

        printf ( "####  Game %d:  gene %d versus gene %d...\n",
                 numGamesCompleted, currentGene, otherGene );

        int result = battle ( currentGene, otherGene );

#if LOG_EVERY_GAME
        lprintf ( "BATTLE: %d vs %d - result = %d\n", currentGene, otherGene, result );
        lprintf ( "Stats for this run: white wins: %ld, black wins: %ld, draws: %ld\n",
                  numWhiteWins, numBlackWins, numDraws );
#endif // LOG_EVERY_GAME

        if ( result > 0 )
        {
            ++info[currentGene].numWins;
            ++info[otherGene].numLosses;
        }
        else if ( result < 0 )
        {
            ++info[currentGene].numLosses;
            ++info[otherGene].numWins;
        }
        else
        {
            ++info[currentGene].numDraws;
            ++info[otherGene].numDraws;
        }

        if ( ++currentGene >= numGenes )
        {
            // We have completed another round through the gene pool

            currentGene = 0;
            sortGenes();

            const int RoundsPerReap = 4;
            if ( ++numRounds % RoundsPerReap == 0 )
                grimReaper();

            saveGenFiles();
        }
    }
}


void ChessGA::loadGene (
    ComputerChessPlayer &player,
    int geneIndex,
    INT32 thinkTime )
{
    player.loadGene ( info[geneIndex].gene );   // install personality!
    player.SetTrainingEnable ( false );        // no help from experience
    player.SetOpeningBookEnable ( false );     // no help from opening book
    player.SetSearchBias ( 1 );
    player.setResignFlag ( true );
    player.setResignThreshold ( resignThreshold );
    player.SetMaxNodesEvaluated ( 100 * thinkTime );
}


int ChessGA::battle ( int g1, int g2 )
{
    // If it exists, open 'chenard.btl' and read
    // certain parameters to control every game.

    FILE *f = fopen ( "chenard.btl", "rt" );
    if ( f )
    {
        int t_resignThreshold=0, t_minTime=0, t_maxTime=0;
        int numScanned = fscanf (
            f,
            "resignThreshold=%d minTime=%d maxTime=%d",
            &t_resignThreshold,
            &t_minTime,
            &t_maxTime );

        if ( numScanned != 3 )
        {
            t_resignThreshold = 800;
            t_minTime = 100;
            t_maxTime = 400;
        }

        fclose(f);

        resignThreshold = t_resignThreshold;
        minTime = t_minTime;
        spreadTime = t_maxTime - t_minTime;
    }

    if ( !ui )
    {
        printf ( "No user interface defined in ChessGA::battle()\n" );
        exit(1);
    }

    // Someday I might consider disabling generating
    // the file 'gamelog.txt' altogether.  But for now...
    remove ( "gamelog.txt" );     // otherwise this gets larger and larger

    ChessGeneInfo &x1 = info[g1];
    ChessGeneInfo &x2 = info[g2];

    // Figure out which gene will play White.
    // Do this by looking at their respective "surpluses" of
    // times playing White over Black in the past...

    int g1_whiteSurplus = x1.numGamesAsWhite - x1.numGamesAsBlack;
    int g2_whiteSurplus = x2.numGamesAsWhite - x2.numGamesAsBlack;
    int firstIsWhite = 0;  // assume that 'g1' plays Black

    if ( g1_whiteSurplus < g2_whiteSurplus )
    {
        // 'g1' has played White less than 'g2' in the past,
        // so let 'g1' play White now.
        firstIsWhite = 1;
    }
    else if ( g1_whiteSurplus == g2_whiteSurplus )
    {
        // 'g1' and 'g2' have equal claim to playing White,
        // so flip a coin.
        firstIsWhite = ChessRandom(2);
    }

    // The following variables allow us to refer to the genes
    // in terms of Black and White, ignoring the order the
    // genes were passed into this function.

    int whiteIndex = firstIsWhite ? g1 : g2;
    int blackIndex = firstIsWhite ? g2 : g1;
    ChessGeneInfo &gw = info[whiteIndex];
    ChessGeneInfo &gb = info[blackIndex];

    // NOTE:  We increment the numGamesAs... counters here
    // instead of after the game is played.  This is OK because
    // even if the GA is interrupted by the user in the middle
    // of the game, the new state will not yet be saved to disk.

    ++gw.numGamesAsWhite;
    ++gb.numGamesAsBlack;

    // The following two variables help to translate from
    // White/Black winning to whether 'g1' wins or 'g2' wins.

    int whiteWin = firstIsWhite ?  1 : -1;   // return this if White wins
    int blackWin = firstIsWhite ? -1 :  1;   // return this if Black wins

    printf("####  White: gene %d,  Black: gene %d\n", whiteIndex, blackIndex);

#if LOG_EVERY_GAME
    lprintf ( "White: gene %d,  Black: gene %d\n", whiteIndex, blackIndex );
#endif

    ui->SetAdHocText ( 0, "game %d: %d vs %d", 
        numGamesCompleted, whiteIndex, blackIndex );

    ui->SetAdHocText ( 1, "%0.4lf*%d  %0.4lf*%d", 
        gw.fitness(), 
        gw.numGames(),
        gb.fitness(),
        gb.numGames() );

    MoveList     ml;
    Move         move;
    UnmoveInfo   unmove;

    INT32 thinkTime = minTime + ChessRandom(spreadTime);
    printf ( "####  Think time chosen:  %0.2lf\n", double(thinkTime) / 100.0 );

#if LOG_EVERY_GAME
    lprintf ( "Think time chosen:  %0.2lf\n", double(thinkTime) / 100.0 );
#endif

    ComputerChessPlayer whitePlayer(*ui);
    loadGene ( whitePlayer, whiteIndex, thinkTime );

    ComputerChessPlayer blackPlayer(*ui);
    loadGene ( blackPlayer, blackIndex, thinkTime );

    ChessBoard board;

    // Now we have a ChessBoard and two ComputerChessPlayers,
    // so we are ready to play the game.
    // Each time around the following 'for' loop represents a
    // ply in the game.
    // We limit the number of plies to keep the game from lasting
    // too long.

    rooksAndKingsPlies = 0;     // do this for ChessGA::isSpecialDraw()

    int maxPliesAllowed = 300;
    for ( int ply=0; ply < maxPliesAllowed; ++ply )
    {
        printf ( ">>>>  numGames=%ld, ply=%d;  gene[%d] (%0.4lf*%d) versus gene[%d] (%0.4lf*%d)\n",
            numGamesCompleted, 
            ply,
            whiteIndex, gw.fitness(), gw.numGames(),
            blackIndex, gb.fitness(), gb.numGames() );

        ComputerChessPlayer::XposTable->reset();

        ui->DrawBoard (board);

        if ( board.WhiteToMove() )
        {
            board.GenWhiteMoves ( ml );
            if ( ml.num == 0 )
            {
                // The game is over...

                whitePlayer.InformGameOver (board);
                ChessSide winner = board.WhiteInCheck() ? SIDE_BLACK : SIDE_NEITHER;
                ui->ReportEndOfGame ( winner );
                if ( winner == SIDE_NEITHER )
                {
                    ++numDraws;
                    displayGameResults ( "Black stalemated White" );
#if LOG_EVERY_GAME
                    lprintf ( "Stalemate at ply=%d.\n", ply );
#endif
                    return 0;
                }
                else
                {
                    ++numBlackWins;
                    displayGameResults ( "Black won" );
#if LOG_EVERY_GAME
                    lprintf ( "Black checkmated White at ply %d.\n", ply );
#endif
                    return blackWin;
                }
            }
            else if ( board.IsDefiniteDraw() )
            {
                ++numDraws;
                displayGameResults ( "Draw" );
                whitePlayer.InformGameOver (board);
                ui->ReportEndOfGame ( SIDE_NEITHER );
#if LOG_EVERY_GAME
                lprintf ( "Non-stalemate forced draw at ply %d.\n", ply );
#endif
                return 0;
            }
            else if ( isSpecialDraw(board) )
            {
                ++numDraws;
                displayGameResults ( "Draw" );
                whitePlayer.InformGameOver (board);
                ui->ReportEndOfGame ( SIDE_NEITHER );
                return 0;
            }

            INT32 timeSpent = 0;
            if ( !whitePlayer.GetMove ( board, move, timeSpent ) )
            {
                ++numBlackWins;
                displayGameResults ( "Black won" );
                blackPlayer.InformResignation();
                ui->Resign ( SIDE_WHITE, whitePlayer.QueryQuitReason() );
#if LOG_EVERY_GAME
                lprintf ( "White resigned at ply %d.\n", ply );
#endif
                return blackWin;
            }

            ui->RecordMove ( board, move, timeSpent );
            board.MakeWhiteMove ( move, unmove, true, true );
        }
        else   // Black's turn to move...
        {
            board.GenBlackMoves ( ml );
            if ( ml.num == 0 )
            {
                // The game is over...

                blackPlayer.InformGameOver (board);
                ChessSide winner = board.BlackInCheck() ? SIDE_WHITE : SIDE_NEITHER;
                ui->ReportEndOfGame ( winner );
                if ( winner == SIDE_NEITHER )
                {
                    ++numDraws;
                    displayGameResults ( "White stalemated black" );
#if LOG_EVERY_GAME
                    lprintf ( "Stalemate at ply %d.\n", ply );
#endif
                    return 0;
                }
                else
                {
                    ++numWhiteWins;
                    displayGameResults ( "White won" );
#if LOG_EVERY_GAME
                    lprintf ( "White checkmated Black at ply %d.\n", ply );
#endif
                    return whiteWin;
                }
            }
            else if ( board.IsDefiniteDraw() )
            {
                ++numDraws;
                displayGameResults ( "Draw" );
                blackPlayer.InformGameOver (board);
                ui->ReportEndOfGame ( SIDE_NEITHER );
#if LOG_EVERY_GAME
                lprintf ( "Non-stalemate draw at ply %d.\n", ply );
#endif
                return 0;
            }
            else if ( isSpecialDraw(board) )
            {
                ++numDraws;
                displayGameResults ( "Draw" );
                blackPlayer.InformGameOver (board);
                ui->ReportEndOfGame ( SIDE_NEITHER );
                return 0;
            }

            INT32 timeSpent = 0;
            if ( !blackPlayer.GetMove ( board, move, timeSpent ) )
            {
                ++numWhiteWins;
                displayGameResults ( "White won" );
                whitePlayer.InformResignation();
                ui->Resign ( SIDE_BLACK, blackPlayer.QueryQuitReason() );
#if LOG_EVERY_GAME
                lprintf ( "Black resigned at ply %d.\n", ply );
#endif
                return whiteWin;
            }

            ui->RecordMove ( board, move, timeSpent );
            board.MakeBlackMove ( move, unmove, true, true );
        }

        // recap for casual monitoring....

        INT32 runTime = ChessTime() - startTime;
        long r_seconds = runTime / 100;
        long r_minutes = r_seconds / 60;
        r_seconds %= 60;
        long r_hours = r_minutes / 60;
        r_minutes %= 60;

        printf ( ">>>>  runtime = %ld:%02ld:%02ld  score=%d  [W=%ld B=%ld D=%ld]\n",
                 r_hours, r_minutes, r_seconds, int(move.score),
                 numWhiteWins, numBlackWins, numDraws );
    }

#if LOG_EVERY_GAME
    lprintf ( "Game exceeded %d plies; ruled a draw.\n", maxPliesAllowed );
#endif

    ++numDraws;
    displayGameResults ( "Draw by attrition" );
    ui->ReportEndOfGame ( SIDE_NEITHER );

    return 0;
}


bool ChessGA::isSpecialDraw ( const ChessBoard &board )
{
    // Look for rook+king vs rook+king persisting for 10 plies.

    const INT16 *inv = board.queryInventoryPointer();
    int nonKingRook =
        inv[WP_INDEX] + inv[WN_INDEX] + inv[WB_INDEX] + inv[WQ_INDEX] +
        inv[BP_INDEX] + inv[BN_INDEX] + inv[BB_INDEX] + inv[BQ_INDEX];

    if ( nonKingRook==0 && inv[WR_INDEX]==1 && inv[BR_INDEX]==1 )
    {
        if ( ++rooksAndKingsPlies >= 10 )
        {
#if LOG_EVERY_GAME
            lprintf("Rook-and-king draw detected at ply %d\n", int(board.GetCurrentPlyNumber()));
#endif

            return true;
        }
    }

    return false;
}


void ChessGA::grimReaper()
{
    printf ( "###  Entering reaper.\n" );
    logdate ( "Entering reaper" );

    const int killPercent = 20;
    const int numberToKill = (numGenes * killPercent) / 100;
    const int numberToKeep = numGenes - numberToKill;

    for ( int baby = numberToKeep; baby < numGenes; ++baby )
    {
        // Note:  Choose parents in a way that compromises elitism with
        // egalitarianism:  mom is chosen in a way that skews toward
        // higher fitness individuals, while dad is chosen equally from
        // all genes to be kept.

        int mom = ChessRandom ( 1 + numberToKeep/10 );
        int dad = ChessRandom (numberToKeep - 1);
        if ( dad >= mom )
            ++dad;

        ChessGeneInfo &x = info[baby];
        x.gene.blend ( info[mom].gene, info[dad].gene );
        x.numWins = 0;
        x.numLosses = 0;
        x.numDraws = 0;
        x.numGamesAsWhite = 0;
        x.numGamesAsBlack = 0;
        x.id = nextGeneId++;

        lprintf ( "   gene[%d] = mix ( gene[%d], gene[%d] );  id=%ld\n", baby, mom, dad, x.id );
        printf ( "   ###  gene[%d] = mix ( gene[%d], gene[%d] );  id=%ld\n", baby, mom, dad, x.id );
    }

    // Now, get rid of old genes that are not up to snuff...

    const int cutoffAge = 64;   // number of games that defines old age
    const int cutoffKeepPercentage = 10;
    const int cutoffKeep = (numGenes * cutoffKeepPercentage) / 100;
    int numKeptOldGenes = 0;
    int *keptOldGenes = new int [cutoffKeep];
    int i;
    for ( i=0; i < cutoffKeep; ++i )
        keptOldGenes[i] = -1;

    for ( i=0; i < numberToKeep; ++i )
    {
        ChessGeneInfo &x = info[i];
        const int age = x.numGames();
        if ( age >= cutoffAge )
        {
            // Insert into the ordered heap with fixed size.

            const double agedFitness = x.fitness();
            bool addedFlag = false;
            for ( int h=0; !addedFlag && h < numKeptOldGenes; ++h )
            {
                if ( agedFitness > info[keptOldGenes[h]].fitness() )
                {
                    addedFlag = true;

                    // Scoot everything below this gene down a slot.
                    for ( int s=cutoffKeep-1; s > h; --s )
                        keptOldGenes[s] = keptOldGenes[s-1];

                    keptOldGenes[h] = i;
                }
            }

            if ( !addedFlag && numKeptOldGenes < cutoffKeep )
                keptOldGenes[numKeptOldGenes++] = i;
        }
    }

    for ( i=0; i < numKeptOldGenes; ++i )
    {
        lprintf ( "  keptOldGenes[%02d] = %3d\n", i, keptOldGenes[i] );
    }

    // Now go back in second pass and kill any old genes that are not
    // in the keep list.

    for ( i=0; i < numberToKeep; ++i )
    {
        ChessGeneInfo &x = info[i];
        const int age = x.numGames();
        if ( age >= cutoffAge )
        {
            bool keepFlag = false;
            for ( int h=0; !keepFlag && h < numKeptOldGenes; ++h )
                if ( i == keptOldGenes[h] )
                    keepFlag = true;

            if ( !keepFlag )
            {
                // This old fartknocker is out of here!

                int mom, dad;
                do
                {
                    mom = ChessRandom ( 1 + numberToKeep/10 );
                    dad = ChessRandom (numberToKeep - 1);
                    if ( dad >= mom )
                        ++dad;
                }
                while ( mom==i || dad==i );     // can't be your own parent!!!

                x.gene.blend ( info[mom].gene, info[dad].gene );
                x.numWins = 0;
                x.numLosses = 0;
                x.numDraws = 0;
                x.numGamesAsWhite = 0;
                x.numGamesAsBlack = 0;
                x.id = nextGeneId++;

                lprintf ( "   old gene[%d] = mix ( gene[%d], gene[%d] );  id=%ld\n", i, mom, dad, x.id );
                printf ( "   ###  old gene[%d] = mix ( gene[%d], gene[%d] );  id=%ld\n", i, mom, dad, x.id );
            }
        }
    }

    delete[] keptOldGenes;
}


void ChessGA::sortGenes()
{
    printf ( "Sorting gene pool... " );

    ChessGeneInfo temp;

    for ( int i=0; i < numGenes-1; ++i )
    {
        double bestFitness = info[i].fitness();
        int bestIndex = i;
        for ( int j=i+1; j < numGenes; ++j )
        {
            double fitness = info[j].fitness();
            if ( fitness > bestFitness )
            {
                bestFitness = fitness;
                bestIndex = j;
            }
        }

        if ( bestIndex != i )
        {
            temp = info[i];
            info[i] = info[bestIndex];
            info[bestIndex] = temp;
        }
    }

    FILE *f = fopen ( "chenga.log", "at" );
    if ( f )
    {
        fprintf ( f, "Sorted gene pool:  -----------------------------------\n" );
        for ( int i=0; i < numGenes; ++i )
        {
            ChessGeneInfo &x = info[i];
            fprintf ( f, "%04d  NG=%-5ld  W=%-5ld  L=%-5ld  D=%-5ld  fitness=%-10.4lf  id=%ld\n",
                i, 
                (x.numWins + x.numLosses + x.numDraws),
                x.numWins, x.numLosses, x.numDraws,
                x.fitness(),
                x.id );
        }
        fclose(f);
    }

    printf ( "done.\n" );
}


void ChessGA::displayGameResults ( const char *message )
{
#if DISPLAY_GAME_RESULTS
    char buffer [256];
    strcpy ( buffer, message );

    char temp [64];
    sprintf ( temp, " (W=%ld B=%ld D=%ld)", numWhiteWins, numBlackWins, numDraws );
    strcat ( buffer, temp );

    ui->SetAdHocText ( 2, buffer ); // 2 is new "title bar suffix" index
#endif

    FILE *f = fopen ( CHENARD_STATS_FILENAME, "wt" );
    if ( f )
    {
        fprintf ( f, "W=%ld B=%ld D=%ld\n", numWhiteWins, numBlackWins, numDraws );
        fclose(f);
    }
}


int ChessGA::MergePools (
    int outputPoolSize,
    int dirc,
    const char *dirv[],
    ChessUI &_ui,
    int _allowTextOutput )
{
    // First make sure that we are not attempting to overwrite
    // an existing gene pool.

    FILE *f = fopen ( "chenard.ga", "r" );
    if ( f != NULL )
    {
        fclose (f);
        if ( _allowTextOutput )
        {
            fprintf ( stderr, 
                "Error:  File 'chenard.ga' already exists.\n"
                "The merger will not overwrite an existing gene pool.\n"
                "To override, manually delete chenard.ga and *.gen before running.\n" );
        }

        return 1;
    }

    if ( dirc <= 2 )
    {
        if ( _allowTextOutput )
        {
            fprintf(stderr, "Error: Must pass two or more directories to gene pool merger.\n");
        }

        return 2;
    }

    if ( outputPoolSize < MIN_GENES || outputPoolSize > MAX_GENES )
    {
        if ( _allowTextOutput )
        {
            fprintf(stderr, "Invalid output pool size %d.\n", outputPoolSize);
        }

        return 3;
    }

    // First, slurp in all the input gene pools...

    ChessGA *inPool = new ChessGA [dirc];
    if ( !inPool )
    {
        if ( _allowTextOutput )
            fprintf ( stderr, "Out of memory allocating input pool array!\n" );

        return 4;
    }

    long totalInputGenes = 0;
    int i;
    for ( i=0; i < dirc; ++i )
    {
        if ( _allowTextOutput )
            ::printf ( "\n$$$$ Attempting to open pool in dir '%s'\n", dirv[i] );

        ChessGA &ga = inPool[i];
        ga.allowTextOutput = _allowTextOutput;
        ga.ui = &_ui;
        int rc = ga.load ( dirv[i] );
        if ( !rc )
        {
            if ( _allowTextOutput )
                fprintf ( stderr, "Error loading pool from dir '%s'\n", dirv[i] );

            return 5;
        }

        totalInputGenes += ga.numGenes;
    }

    if ( _allowTextOutput )
    {
        ::printf("\n$$$$ Successfully loaded %d pools, totalling %ld genes.\n", dirc, totalInputGenes);
    }

    if ( totalInputGenes < outputPoolSize )
    {
        if ( _allowTextOutput )
            fprintf ( stderr, "Error:  Not enough input genes to create output pool!\n" );

        return 6;
    }

    ChessGA outPool ( _ui, _allowTextOutput );
    outPool.numGenes = totalInputGenes;
    outPool.info = new ChessGeneInfo [totalInputGenes];
    if ( !outPool.info )
    {
        if ( _allowTextOutput )
            fprintf ( stderr, "Error allocating output gene pool!\n" );

        return 7;
    }

    // Copy all input genes into output gene array...

    int outGeneIndex = 0;
    for ( i=0; i < dirc; ++i )
    {
        ChessGA &gin = inPool[i];
        for ( int j=0; j < gin.numGenes; ++j )
            outPool.info[outGeneIndex++] = gin.info[j];
    }

    outPool.sortGenes();                    // sort all by fitness

    // Reset all persistent IDs...

    for ( i=0; i < totalInputGenes; ++i )
    {
        ChessGeneInfo &x = outPool.info[i];
        x.id = i;
    }

    outPool.numGenes = outputPoolSize;      // truncated to desired size
    outPool.nextGeneId = outputPoolSize;

    if ( !outPool.save() )                  // save to current directory
    {
        if ( _allowTextOutput )
            fprintf ( stderr, "!!! Error saving output genes !!!\n" );

        return 8;
    }

    if ( _allowTextOutput )
        ::printf ( "\n$$$$ Merge succeeded!\n" );

    return 0;
}


//-----------------------------------------------------------------------------------


int GeneBattleGame (
    ChessUI &ui,
    ChessGene &g1,
    ChessGene &g2,
    int firstIsWhite )
{
    const int resignThreshold = 800;
    const int minSearchNodes  = 10000;
    const int randSearchNodes = 40000;
    int maxNodes = minSearchNodes + 100*ChessRandom(randSearchNodes/100);

    ChessGene &gw = firstIsWhite ? g1 : g2;
    ChessGene &gb = firstIsWhite ? g2 : g1;

    const int whiteWin = firstIsWhite ?  1 : -1;
    const int blackWin = firstIsWhite ? -1 :  1;

    ComputerChessPlayer whitePlayer(ui);
    whitePlayer.loadGene ( gw );   // install personality!
    whitePlayer.SetTrainingEnable ( false );        // no help from experience
    whitePlayer.SetOpeningBookEnable ( false );     // no help from opening book
    whitePlayer.SetSearchBias ( 1 );
    whitePlayer.setResignFlag ( true );
    whitePlayer.setResignThreshold ( resignThreshold );
    whitePlayer.SetMaxNodesEvaluated ( maxNodes );

    ComputerChessPlayer blackPlayer(ui);
    blackPlayer.loadGene ( gb );   // install personality!
    blackPlayer.SetTrainingEnable ( false );        // no help from experience
    blackPlayer.SetOpeningBookEnable ( false );     // no help from opening book
    blackPlayer.SetSearchBias ( 1 );
    blackPlayer.setResignFlag ( true );
    blackPlayer.setResignThreshold ( resignThreshold );
    blackPlayer.SetMaxNodesEvaluated ( maxNodes );

    ChessBoard board;

    // Now we have a ChessBoard and two ComputerChessPlayers,
    // so we are ready to play the game.
    // Each time around the following 'for' loop represents a
    // ply in the game.
    // We limit the number of plies to keep the game from lasting
    // too long.

    MoveList ml;
    Move move;
    UnmoveInfo unmove;
    const int maxPliesAllowed = 300;
    for ( int ply=0; ply < maxPliesAllowed; ++ply )
    {
        ComputerChessPlayer::XposTable->reset();

        ui.DrawBoard (board);

        if ( board.WhiteToMove() )
        {
            board.GenWhiteMoves ( ml );
            if ( ml.num == 0 )
            {
                // The game is over...

                whitePlayer.InformGameOver (board);
                ChessSide winner = board.WhiteInCheck() ? SIDE_BLACK : SIDE_NEITHER;
                ui.ReportEndOfGame ( winner );
                if ( winner == SIDE_NEITHER )
                {
                    printf ( "Stalemate at ply=%d.\n", ply );
                    return 0;
                }
                else
                {
                    printf ( "Black checkmated White at ply %d.\n", ply );
                    return blackWin;
                }
            }
            else if ( board.IsDefiniteDraw() )
            {
                whitePlayer.InformGameOver (board);
                ui.ReportEndOfGame ( SIDE_NEITHER );
                printf ( "Non-stalemate forced draw at ply %d.\n", ply );
                return 0;
            }

            INT32 timeSpent = 0;
            if ( !whitePlayer.GetMove ( board, move, timeSpent ) )
            {
                blackPlayer.InformResignation();
                ui.Resign ( SIDE_WHITE, whitePlayer.QueryQuitReason() );
                printf ( "White resigned at ply %d.\n", ply );
                return blackWin;
            }

            ui.RecordMove ( board, move, timeSpent );
            board.MakeWhiteMove ( move, unmove, true, true );
        }
        else   // Black's turn to move...
        {
            board.GenBlackMoves ( ml );
            if ( ml.num == 0 )
            {
                // The game is over...

                blackPlayer.InformGameOver (board);
                ChessSide winner = board.BlackInCheck() ? SIDE_WHITE : SIDE_NEITHER;
                ui.ReportEndOfGame ( winner );
                if ( winner == SIDE_NEITHER )
                {
                    printf ( "Stalemate at ply %d.\n", ply );
                    return 0;
                }
                else
                {
                    printf ( "White checkmated Black at ply %d.\n", ply );
                    return whiteWin;
                }
            }
            else if ( board.IsDefiniteDraw() )
            {
                blackPlayer.InformGameOver (board);
                ui.ReportEndOfGame ( SIDE_NEITHER );
                printf ( "Non-stalemate draw at ply %d.\n", ply );
                return 0;
            }

            INT32 timeSpent = 0;
            if ( !blackPlayer.GetMove ( board, move, timeSpent ) )
            {
                whitePlayer.InformResignation();
                ui.Resign ( SIDE_BLACK, blackPlayer.QueryQuitReason() );
                printf ( "Black resigned at ply %d.\n", ply );
                return whiteWin;
            }

            ui.RecordMove ( board, move, timeSpent );
            board.MakeBlackMove ( move, unmove, true, true );
        }
    }

    printf ( "Game exceeded %d plies; ruled a draw.\n", maxPliesAllowed );
    return 0;   // success
}


int GeneBattle (
    ChessUI &ui,
    const char *geneFilename1,
    const char *geneFilename2 )
{
#ifdef _WIN32
    // Set thread priority to IDLE...

    HANDLE hProcess = GetCurrentProcess();
    if ( SetPriorityClass(hProcess,IDLE_PRIORITY_CLASS) )
    {
        printf ( "***  Set process priority class to IDLE\n" );
    }
    else
    {
        printf ( "***  Error %ld setting process priority class to IDLE.\n", long(GetLastError()) );

        return 1;
    }
#endif

    if ( !geneFilename1 )
    {
        fprintf ( stderr, "GeneBattle():  First filename parameter was NULL!\n" );
        return 1;
    }

    long wins = 0;      // how many times the first gene has won
    long losses = 0;    // how many times the first gene has lost
    long draws = 0;

    const char *battleFilename = "battle.ga";
    FILE *f = fopen ( battleFilename, "rt" );
    if ( f )
    {
        if ( fscanf ( f, "wins=%ld losses=%ld draws=%ld", &wins, &losses, &draws ) != 3 )
        {
            fprintf ( stderr, "Error scanning data from file '%s'\n", battleFilename );
            fclose(f);
            return 1;
        }
        fclose(f);
    }

    // Someday I might consider disabling generating
    // the file 'gamelog.txt' altogether.  But for now...
    remove ( "gamelog.txt" );     // otherwise this gets larger and larger

    ChessGene g1, g2;   // construct two default genes
    int rc = g1.load (geneFilename1);
    if ( !rc )
    {
        fprintf ( stderr, "Error loading gene file '%s'\n", geneFilename1 );
        return 1;
    }

    if ( geneFilename2 )
    {
        // Only load if filename was specified;
        // otherwise, leave it as the default gene.

        rc = g2.load (geneFilename2);
        if ( !rc )
        {
            fprintf ( stderr, "Error loading gene file '%s'\n", geneFilename2 );
            return 1;
        }
    }

    int firstIsWhite = ChessRandom(2);

    for(;;)
    {
        int result = GeneBattleGame (ui, g1, g2, firstIsWhite);
        if ( result == 1 )
            ++wins;
        else if ( result == -1 )
            ++losses;
        else if ( result == 0 )
            ++draws;
        else
        {
            fprintf ( stderr,
                      "!!!!! Nonsensical return value %d from GeneBattleGame()\n",
                      result );

            return 1;
        }

        f = fopen ( battleFilename, "wt" );
        if ( !f )
        {
            fprintf ( stderr, "!!!!! Error opening file '%s' for write!\n", battleFilename );
            return 1;
        }

        fprintf ( f, "wins=%ld losses=%ld draws=%ld\n", wins, losses, draws );
        fclose(f);
        firstIsWhite = !firstIsWhite;
    }

    return 0;
}


/*
    $Log: chenga.cpp,v $
    Revision 1.6  2009/09/16 16:49:34  Don.Cross
    Added support for the WinBoard/xboard "memory" command to set hash table sizes.

    Revision 1.5  2006/01/18 19:58:11  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.4  2005/11/23 22:52:55  dcross
    Ported stdio version of Chenard to SUSE Linux:
    1. Needed #include <string.h> in chenga.cpp.
    2. Needed temporary Move object in InsertPrediction in search.cpp, because function takes Move &.
    3. Have to turn SUPPORT_LINUX_INTERNET off for the time being, because I lost lichess.h, dang it!

    Revision 1.3  2005/11/23 21:30:29  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:38  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!



    Revision history:

    1999 September 9 [Don Cross]
         Made tallying of numWhiteWins, numBlackWins, and numDraws be
         persistent, by saving to a little text file called 'chenard.sta'.

    1999 September 7 [Don Cross]
         Adding code to display game results.  Every time a game ends,
         the new member function ChessGA::displayGameResults(const char *)
         is called with an appropriate message string.
         Can turn this on and off by setting the preprocessor definition
         of the symbol DISPLAY_GAME_RESULTS.

    1999 April 2 [Don Cross]
         Adding GeneBattle(), a function to play two specified gene files against
         each other.  If the second filename paramemter is NULL, it means that
         the first gene should be played against the "default" (pre-GA) gene.

    1999 March 21 [Don Cross]
         Instead of always calling ChessGA::save() on each iteration,
         ChessGA::run() now calls ChessGA::saveGaOnly().  Another function,
         ChessGA::saveGenFiles(), now is responsible for saving the *.GEN
         files, but only after they are sorted or culled (the only time
         they change values).

    1999 March 12 [Don Cross]
         Adding code (for Win32 build only) to adjust thread priority to
         IDLE when running genetic algorithm.
         ***
         I have noticed that a large percentage of games end up with
         King and Rook vs King and Rook, ending in a draw.  Adding a
         special rule that if both sides have only a king and a rook,
         that the game is a draw if the material stays the same for
         10 plies.

    1999 February 23 [Don Cross]
         Changed fitness function to (W-L)/(16+N).
         This way new genes have to fight a little harder before taking
         over the top spots in the pool.
         Adding ChessGA::MergePools() static member function. This combines
         the best of several gene pools into a single new pool.

    1999 February 22 [Don Cross]
         Now reap every 4 rounds instead of 10.
         Fixed unintentionally silly way of choosing 'mom' for new gene;
         now it's properly elitist.
         Added a persistent ID for every gene to track it through the
         evolution process.

    1999 February 17 [Don Cross]
         Started writing.

*/

