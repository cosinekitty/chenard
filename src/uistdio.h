/*============================================================================

      uistdio.h  -  Copyright (C) 1993-2005 by Don Cross

      This is a portable user interface class for my new chess code.

============================================================================*/

#define CHENARD_RELEASE 1

#ifdef __GNUG__
    // [23 November 2005] Don says:  I had to turn this off, because I lost the file lichess.h !!!
    //#define SUPPORT_LINUX_INTERNET  1
    #define SUPPORT_LINUX_INTERNET  0
#else
    #define SUPPORT_LINUX_INTERNET  0
#endif


enum BoardDisplayType
{
    BDT_STANDARD,      // original, small board (no color, no frills)
    BDT_LARGE_COLOR    // uses ANSI terminal color escape sequences
};


enum PLAYER_TYPE
{
    PT_UNKNOWN,
    PT_HUMAN,
    PT_COMPUTER,
    PT_INTERNET
};


BoardDisplayType Next ( BoardDisplayType );   // allows to cycle through all


class ChessUI_stdio: public ChessUI
{
public:
    ChessUI_stdio();
    ~ChessUI_stdio();

    ChessPlayer *CreatePlayer ( ChessSide );
    bool ReadMove ( ChessBoard &, int &source, int &dest, SQUARE &promIndex );
    SQUARE PromotePawn ( int PawnDest, ChessSide );
    void DisplayMove ( ChessBoard &, Move );
    void RecordMove ( ChessBoard &, Move, INT32 thinkTime );
    void DrawBoard ( const ChessBoard & );
    void ReportEndOfGame ( ChessSide winner );
    void DisplayBestMoveSoFar ( const ChessBoard &, Move, int level );
    void DisplayCurrentMove ( const ChessBoard &, Move, int level );
    void DisplayBestPath ( const ChessBoard &, const BestPath & );
    void PredictMate ( int numMoves );

    void Resign ( ChessSide, QuitGameReason )  {}
    void NotifyUser ( const char *message );

    void ReportComputerStats ( INT32   thinkTime,
                               UINT32  nodesVisited,
                               UINT32  nodesEvaluated,
                               UINT32  nodesGenerated,
                               int     fwSearchDepth,
                               UINT32  vis [NODES_ARRAY_SIZE],
                               UINT32  gen [NODES_ARRAY_SIZE] );

    virtual void ReportSpecial ( const char *msg );
    void SetScoreDisplay ( bool _showScores )
    {
        showScores = _showScores;
    }

    bool QueryScoreDisplay () const
    {
        return showScores;
    }

    void EnableCombatMode();

private:
    void ClearScreen();
    void DrawBoard_Standard ( const ChessBoard & );
    void DrawBoard_LargeColor ( const ChessBoard & );
    Move ConvertStringToMove ( const ChessBoard &,
                               const char from_to [4] );

    PLAYER_TYPE whitePlayerType;
    PLAYER_TYPE blackPlayerType;

    bool showScores;   // should UI show moves' min-max scores?
    bool rotateBoard;  // should board be shown opposite of "sensible" way?
    bool beepFlag;     // should beep when it is human's turn?

    int numPlayersCreated;

    ChessPlayer *whitePlayer;
    ChessPlayer *blackPlayer;
    BoardDisplayType  boardDisplayType;
};


class MoveUndoPath
{
public:
    MoveUndoPath ():
        size(0),
        moveList(0),
        current(0),
        length(0)
        {}
    ~MoveUndoPath();
    void resetFromBoard ( const ChessBoard & );
    bool undo ( ChessBoard & );
    bool redo ( ChessBoard & );

private:
    int size;
    Move *moveList;   // dynamically allocated array of 'size'
    int current;      // index of current position in undo path
    int length;       // current total length of path
};

extern MoveUndoPath Global_MoveUndoPath;


/*
    $Log: uistdio.h,v $
    Revision 1.6  2008/11/06 22:25:06  Don.Cross
    Now ParseFancyMove accepts either PGN or longmove (e7e8q) style notation only.
    We tolerate of check '+' or checkmate '#' characters being absent or extraneous in the input.
    The former is especially important because I often see PGN notation where '+' is missing on checking moves.
    To allow for PGN input, I had to allow avoidance of the PromotePawn callback, so other code was changed as well.
    Version numbers are now always constently yyyy.mm.dd across all builds of Chenard!

    Revision 1.5  2006/01/18 19:58:15  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.4  2005/11/23 22:52:55  dcross
    Ported stdio version of Chenard to SUSE Linux:
    1. Needed #include <string.h> in chenga.cpp.
    2. Needed temporary Move object in InsertPrediction in search.cpp, because function takes Move &.
    3. Have to turn SUPPORT_LINUX_INTERNET off for the time being, because I lost lichess.h, dang it!

    Revision 1.3  2005/11/23 21:30:31  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:44  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!



          Revision history:

    1999 January 23 [Don Cross]
         Adding support for InternetChessPlayer.

    1999 January 16 [Don Cross]
         Cloning Win32 GUI Chenard's MoveUndoPath data structure so
         that text-mode version of Chenard can also undo moves.

    1999 January 15 [Don Cross]
         Modified prototype for ChessUI_stdio::Resign member function
         due to changes in chess.h

*/

