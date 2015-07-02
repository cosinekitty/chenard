/*=============================================================================

    chenga.h  -  Copyright (C) 1999-2005 by Don Cross

    This header file contains definitions for a genetic algorithm
    for evolving move ordering and eval heuristics.

=============================================================================*/
#ifndef __ddc_chenard_chenga_h
#define __ddc_chenard_chenga_h 1


struct ChessGeneInfo
{
    ChessGene   gene;
    long        numWins;
    long        numLosses;
    long        numDraws;
    long        numGamesAsWhite;
    long        numGamesAsBlack;
    long        id;

    double fitness() const;
    long numGames() const { return numGamesAsWhite + numGamesAsBlack; }

    ChessGeneInfo():
        numWins(0),
        numLosses(0),
        numDraws(0),
        numGamesAsWhite(0),
        numGamesAsBlack(0),
        id(0)
    {}
};


class ChessGA
{
public:
    ChessGA ( ChessUI &, int _allowTextOutput );
    ~ChessGA();

    void createRandomPopulation ( int _numGenes );
    int save();
    int saveGenFiles();   // saves state to files in current directory
    int saveGaOnly();   // save ga file, but not gen files
    int load ( const char *dirPrefix = "" );
    void run();

    static int MergePools (
        int outputPoolSize,
        int dirc,
        const char *dirv[],
        ChessUI &,
        int _allowTextOutput );

protected:
    ChessGA();

    // The following function competes two genes (specified by their indices)
    // against each other.  The function returns 1 if the first gene wins the game,
    // -1 if the second gene wins, and 0 if there is a draw.
    int battle ( int g1, int g2 );

    // The following function is called when the GA decides enough games
    // have been played to kill stupid genes and breed smart ones.
    void grimReaper();

    void sortGenes();   // sort in descending order of fitness

    void loadGene ( ComputerChessPlayer &, int geneIndex, INT32 thinkTime );

    int printf ( const char *format, ... );    // prints to stdout if allowTextOutput nonzero
    int lprintf ( const char *format, ... );   // appends to log file
    void logdate ( const char *description );   // appends current date and time to log file

    bool isSpecialDraw ( const ChessBoard & );

    void displayGameResults ( const char *message );

private:
    int allowTextOutput;
    ChessUI *ui;
    int numGenes;           // fixed population size
    ChessGeneInfo *info;
    int currentGene;        // index of next gene to be exercised
    long numRounds;         // total cycles through gene pool completed
    long numGamesCompleted;
    INT32 startTime;
    long nextGeneId;

    long numWhiteWins;
    long numBlackWins;
    long numDraws;

    SCORE resignThreshold;
    INT32 minTime;
    INT32 spreadTime;

    // The following are used by ChessGA::battle() to detect special draws.
    int rooksAndKingsPlies;  // number of plies board has had only R+K vs R+K
};


int GeneBattle (
    ChessUI &,
    const char *geneFilename1,
    const char *geneFilename2 );


#endif // __ddc_chenard_chenga_h

/*
    $Log: chenga.h,v $
    Revision 1.4  2006/01/18 19:58:11  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.3  2005/11/23 21:30:29  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:38  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!


        Revision history:

    1999 February 18 [Don Cross]
         Started writing.

*/

