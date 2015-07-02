/*
    uixboard.h  -  Don Cross  -  http://cosinekitty.com/chenard

    An adaptation of Chenard for xboard version 2.
    See:   http://www.tim-mann.org/xboard.html
*/

#define SUPPORT_LINUX_INTERNET  0

//----------------------------------------------------------------
// External functions for interacting with xboard...
void dprintf (const char *format, ...);
const size_t MAX_XBOARD_LINE = 256;    // I really hope this is big enough!
const char *ReadCommandFromXboard (char *verb);
void ReceiveForceCommand();
bool IsInputReady();
extern bool GlobalAbortFlag;
bool LooksLikeMove (const char *string, int &source, int &dest, int &prom);
inline bool LooksLikeMove (const char *string)
{
    int source, dest, prom;     // discard output parameters
    return LooksLikeMove (string, source, dest, prom);
}

void ChenardMoveToAlgebraic (bool whiteToMove, const Move &move, char string[6]);
inline void ChenardMoveToAlgebraic (ChessBoard &board, const Move &move, char string[6])
{
    ChenardMoveToAlgebraic (board.WhiteToMove(), move, string);
}

int  TimeBudgetForNextMove  (int centiseconds, bool computersTurn);
void AcceptMyRemainingTime  (int centiseconds);
void ReceiveMyRemainingTime (int centiseconds, bool computersTurn);
bool ExecuteCommand (const char *verb, const char *rest);        // returns true if 'quit' received
bool CommandMayBeIgnored (const char *verb);

//----------------------------------------------------------------

class ChessUI_xboard: public ChessUI
{
public:
    ChessUI_xboard();
    ~ChessUI_xboard();

    virtual void DebugPly ( int depth, ChessBoard &, Move );     // we use this as a hack for checking for xboard input while thinking


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
    virtual void SetAdHocText ( int index, const char *, ... );

    void SetComputerChessPlayer (ComputerChessPlayer *_player)
    {
        player = _player;
    }

    void EnableAdHocText()
    {
        adHocTextEnabled = true;
    }

    void EnableThinkingDisplay (bool _thinkingDisplayEnabled)
    {
        thinkingDisplayEnabled = _thinkingDisplayEnabled;
    }

    void OnStartSearch();
    void StartPondering (const char *_predictedAlgebraic, const char *_predictedPgn);
    void FinishPondering (char opponentMoveWhilePonderingAlgebraic [6]);
    int  MyPonderClock() const
    {
        return myPonderClock;
    }
    const char *PendingVerb() const
    {
        return pendingVerb;
    }
    const char *PendingRest() const
    {
        return pendingRest;
    }

protected:
    void OnIncomingUserMove (const char *string);
    void AbortSearch (const char *verb, const char *rest);

private:
    int     checkInputCounter;          // helps us check for xboard input every now and then
    bool    adHocTextEnabled;           // set to true to monitor progress of endgame database file generation
    bool    thinkingDisplayEnabled;     // enables/disables display of thinking to WinBoard/xboard (post/nopost commands)
    int     levelCache;                 // used to hold current search level for thinking display
    INT32   searchStartTime;            // when search was started, so we can display elapsed time
    ComputerChessPlayer *player;

    bool    pondering;                  // set to true only when we are currently pondering
    char    oppMoveWhilePondering[6];   // a move we received from the opponent while we were pondering
    char    predictedPgn[6];            // what we predicted the opponent is going to do, before starting pondering
    char    predictedAlgebraic[8];      // predicted move in PGN format, for display to xboard for best path
    int     myPonderClock;              // reported centiseconds remaining on my clock while I was pondering.

    char    pendingVerb [MAX_XBOARD_LINE];
    char    pendingRest [MAX_XBOARD_LINE];
};


/*
    $Log: uixboard.h,v $
    Revision 1.9  2009/12/08 21:59:22  Don.Cross
    Fixed embarrassing bugs in XChenard's timing code:
    I was using IsComputersTurn() to figure out whether to adjust the remaining moves in the current time period,
    but this was wrong because it in turn uses TheChessBoard to figure out whose turn it is; but TheChessBoard
    can be in a random state due to being called from the search!  Now the caller figures out whether or not the
    opponent's actual move has been received and recorded.
    Another problem was that I was finding out whose turn it was when I received the "time" command, which is received
    before the opponent's move has arrived!  A similar problem occurs at the beginning of the game, when the
    computer is playing neither White nor Black (yet).
    Also needed to reset the ply counter when the "new" command is received.

    Revision 1.8  2008/11/22 16:27:32  Don.Cross
    Massive overhaul to xchenard: now supports pondering.
    This required very complex changes, and will need a lot of testing before release to the public.
    Pondering support made it trivial to implement the "hint" command, so I added that too.
    Pondering required careful attention to every possible command we could receive from xboard,
    so now I have started explicitly sending back errors for any command we don't understand.
    I finally figured out how to get rid of the annoying /WP64 warning from the Microsoft C++ compiler.

    Revision 1.7  2008/11/21 03:16:45  Don.Cross
    I realized I could just call ComputerChessPlayer::getPredictedOpponentMove() instead of having to override a virtual method.
    It returns a move where dest==0 if there is no prediction.  This is much simpler!

    Revision 1.6  2008/11/21 03:05:16  Don.Cross
    First baby step toward pondering in xchenard: used a hack in the Chenard engine that supports multithreaded pondering in WinChen.
    We are not doing pondering the same way, but the hack allows us to share the part of WinChen that predicts the opponent's move.
    Did some other refactoring of code that may help finish pondering, but I'm still trying to figure out what I'm really going to do.

    Revision 1.5  2008/11/18 21:48:36  Don.Cross
    XChenard now supports the xboard commands post and nopost, to enable/disable display of thinking.

    Revision 1.4  2008/11/06 22:25:06  Don.Cross
    Now ParseFancyMove accepts either PGN or longmove (e7e8q) style notation only.
    We tolerate of check '+' or checkmate '#' characters being absent or extraneous in the input.
    The former is especially important because I often see PGN notation where '+' is missing on checking moves.
    To allow for PGN input, I had to allow avoidance of the PromotePawn callback, so other code was changed as well.
    Version numbers are now always constently yyyy.mm.dd across all builds of Chenard!

    Revision 1.3  2008/11/02 22:23:02  Don.Cross
    Made tweaks so I can use xchenard to generate endgame databases.
    No longer disable SIGTERM and SIGINT: this had the effect of making it so I could not Ctrl+C to kill xchenard!
    These are no longer needed because I send back the feature commands to request xboard not doing that.

    Revision 1.2  2008/11/01 21:55:32  Don.Cross
    Xboard force move command '?' is working.
    Chenard can now play White or Black.
    Crude ability to obey the 'level' command for think time.

    Revision 1.1  2008/11/01 19:01:11  Don.Cross
    The Linux/xboard version of Chenard is starting to work!
    Right now it only knows how to play Black.
    There are a lot more xboard commands it needs to understand in order to be fully functional.

*/
