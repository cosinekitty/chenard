/*=============================================================================

    winguich.h  -  Don Cross, July 1996
    Header file for Win32 GUI version of Chenard.

=============================================================================*/

#include "ichess.h"


// The following determines whether code for thinking on opponent's
// time will be included...

#define  ENABLE_OPP_TIME    1


// The following symbol should be defined to 1 to support Internet connection,
// or 0 for "traditional" Chenard...

#define SUPPORT_INTERNET  1

// The following symbol should be 1 to support named pipe connection, 0 otherwise...

#define SUPPORT_NAMED_PIPE 1
#include "npchess.h"


#define CHESS_PROGRAM_NAME  "Chenard"
extern const char ChessProgramName[];

extern HWND HwndMain;    // Handle of main program window
extern HINSTANCE global_hInstance;


extern char TitleBarText_WhitesTurn[];
extern char TitleBarText_BlacksTurn[];
extern char TitleBarText_GameOver[];

extern bool Global_ResetGameFlag;           // set when File|New selected
extern bool Global_StartNewGameConfirmed;   // set when user confirms decision by OK in create players dialog

extern bool Global_ClearBoardFlag;
extern bool Global_AbortFlag;
extern bool Global_GameSaveFlag;
extern bool Global_GameOpenFlag;
extern bool Global_HaveFilename;
extern bool Global_GameModifiedFlag;
extern bool Global_GameOverFlag;
extern bool Global_TacticalBenchmarkFlag;
extern char Global_GameFilename[];
extern bool Global_UndoMoveFlag;
extern bool Global_RedoMoveFlag;
extern bool Global_SuggestThinkOnly;
extern bool Global_DingAfterSuggest;
extern bool Global_TransientCenteredMessage;

extern char GameSave_filename[];

extern int Global_button2click;

// Width and length of a chess square bitmap...
extern int BitmapScaleFactor, BitmapScaleDenom;
extern int RawBitmapDX, RawBitmapDY;
extern int ChessBoardSize;   // facilitates saving/loading option (0=small, 1=medium, 2=large)
void NewBoardSize ( int );

#define CHENARD_MONOSPACE_FONT      1001
#define CHENARD_PROPORTIONAL_FONT   1002
#define CHENARD_GAMERESULT_FONT     1003

// Added following 3 Jan 1999 for displaying rank numbers and file letters...
#define  CHESS_LEFT_MARGIN       8
#define  CHESS_BOTTOM_MARGIN    16

#define  CHESS_BITMAP_DX   ((RawBitmapDX * BitmapScaleFactor)/BitmapScaleDenom)
#define  CHESS_BITMAP_DY   ((RawBitmapDY * BitmapScaleFactor)/BitmapScaleDenom)

#define  CHESS_BOARD_BORDER_DX   10
#define  CHESS_BOARD_BORDER_DY    8
#define  MOVE_TEXT_DY  14

#define  CHESS_WINDOW_DX  \
    (8*CHESS_BITMAP_DX + 2*CHESS_BOARD_BORDER_DX + CHESS_LEFT_MARGIN)

#define  CHESS_WINDOW_DY  \
    (8*CHESS_BITMAP_DX + 2*CHESS_BOARD_BORDER_DY + MOVE_TEXT_DY + CHESS_BOTTOM_MARGIN)

#define  SQUARE_SCREENX1(x)    ((x)*CHESS_BITMAP_DX + CHESS_BOARD_BORDER_DX + CHESS_LEFT_MARGIN)
#define  SQUARE_SCREENY1(y)    ((7-(y))*CHESS_BITMAP_DY + CHESS_BOARD_BORDER_DY)
#define  SQUARE_SCREENX2(x)    (SQUARE_SCREENX1((x)+1) - 1)
#define  SQUARE_SCREENY2(y)    (SQUARE_SCREENY1((y)-1) - 1)

#define  SQUARE_MIDX(x)  ((SQUARE_SCREENX1(x) + SQUARE_SCREENX2(x))/2)
#define  SQUARE_MIDY(y)  ((SQUARE_SCREENY1(y) + SQUARE_SCREENY2(y))/2)

#define  SQUARE_CHESSX(x)    (((x) - CHESS_BOARD_BORDER_DX - CHESS_LEFT_MARGIN)/CHESS_BITMAP_DX)
#define  SQUARE_CHESSY(y)    (7-(((y)-CHESS_BOARD_BORDER_DY)/CHESS_BITMAP_DY))

// Width of the text debug display area in the client window...
#define  DEBUG_WINDOW_DX    240

// Extra window height when either player has blunder alert enabled...
#define  BLUNDER_WINDOW_DY   50

// Static text control IDs that I use in the debug area.
// In Win32, these are not actual control window IDs but tags
// I use for updating a table of strings and redrawing them
// manually on the display.
#define  STATIC_ID_CURRENT_MOVE        1
#define  STATIC_ID_NODES_EVALUATED     2
#define  STATIC_ID_NODES_EVALPERSEC    3
#define  STATIC_ID_ELAPSED_TIME        4
#define  STATIC_ID_WHITES_MOVE         100
#define  STATIC_ID_BLACKS_MOVE         101
#define  STATIC_ID_THINKING            102
#define  STATIC_ID_ADHOC1              103
#define  STATIC_ID_ADHOC2              104

#define  STATIC_ID_COORD_RANK_BASE     200
#define  STATIC_ID_COORD_FILE_BASE     210
#define  STATIC_ID_GAME_RESULT         220
#define  STATIC_ID_BLUNDER_ANALYSIS    221
#define  STATIC_ID_BETTER_ANALYSIS     222

#define  MAX_BESTPATH_DISPLAY         23
#define  STATIC_ID_BESTPATH(depth)    (6+(depth))

#define  FIRST_RIGHT_TEXTID    STATIC_ID_CURRENT_MOVE
#define  LAST_RIGHT_TEXTID     STATIC_ID_BESTPATH(MAX_BESTPATH_DISPLAY)

#define MAX_CHESS_TEXT             128
#define CHESS_TEXT_COLOR           RGB(255,255,255)
#define CHESS_TEXT_COLOR2          RGB(255,255,0)
#define CHESS_BACKGROUND_COLOR     RGB(0,0,127)
#define CHESS_LIGHT_SQUARE         RGB(213, 219, 200)
#define CHESS_DARK_SQUARE          RGB(155, 174, 199)

bool ProcessChessCommands ( ChessBoard &board, int &source, int &dest );

class ChessDisplayTextBuffer
{
public:
    enum marginType
    {
        margin_none,
        margin_right,           // best/current path analysis area to the right of the chess board
        margin_bottom,          // recent moves made by the two players
        margin_rank,            // used only for rank numbers to left of board.
        margin_file,            // used only for file letters below board.
        margin_center,          // centered text right on top of the board, used for game-over text.
        margin_blunderAnalysis, // analysis of a supposed blunder made by human player.
        margin_betterAnalysis,  // analysis of a better move than alleged blunder made by human player.
    };

    ChessDisplayTextBuffer ( HWND, int _id, int _x, int _y, 
        marginType _margin=margin_right, int _textFont=CHENARD_MONOSPACE_FONT,
        int _highlight = 0,
        bool _isBackgroundOpaque = true);

    ~ChessDisplayTextBuffer();
    void setText ( const char *newText );
    const char *queryText() { return text; }
    void draw (HDC) const;   // should be called by window proc only.
    void calcRegion ( RECT & ) const;
    void reposition();
    void invalidate();
    void toggleHighlight()
    {
        highlight = !highlight;
        invalidate();
    }
    void setColorOverride(COLORREF color)
    {
        highlight = 2;
        colorOverride = color;
        invalidate();
    }

    static void SetText ( int _id, const char *newText );
    static ChessDisplayTextBuffer *Find ( int _id );
    static void DrawAll (HDC);
    static void DeleteAll();
    static void RepositionAll();

protected:
    HFONT createFont() const;
    void deleteFont(HFONT font) const;

private:
    int     id;
    int     x;
    int     y;
    HWND    hwnd;
    char   *text;
    ChessDisplayTextBuffer *next;
    marginType  margin;
    int         textFont;
    int         highlight;
    bool        isBackgroundOpaque;
    COLORREF    colorOverride;

    static ChessDisplayTextBuffer *All;
};

void UpdateGameStateText(ChessBoard& board);
void UpdateGameStateText(bool whiteToMove);     // updates title bar and clears game-over text in center of board.

// User-defined window message types....

#define  WM_DDC_PROMOTE_PAWN            (WM_USER + 101)
#define  WM_DDC_GAME_RESULT             (WM_USER + 102)
#define  WM_DDC_PREDICT_MATE            (WM_USER + 103)
#define  WM_DDC_ENTER_HUMAN_MOVE        (WM_USER + 104)
#define  WM_DDC_LEAVE_HUMAN_MOVE        (WM_USER + 105)
#define  WM_DDC_GAME_OVER               (WM_USER + 106)
#define  WM_DDC_FATAL                   (WM_USER + 107)
#define  WM_DDC_DRAW_VECTOR             (WM_USER + 108)
#define  WM_UPDATE_CHESS_WINDOW_SIZE    (WM_USER + 109)

void ChessBeep ( int freq, int milliseconds );

inline bool DisplayCoordsValid (int x, int y)
{
    return (x >= 0) && (x <= 7) && (y >= 0) && (y <= 7);
}

class BoardDisplayBuffer
{
public:
    BoardDisplayBuffer();
    ~BoardDisplayBuffer();

    void freshenSquare ( int x, int y );
    void freshenBoard();

    //------------------- display stuff ---------------------

    void loadBitmaps ( HINSTANCE );
    void unloadBitmaps();

    void draw ( HDC,
                int minx = 0, int maxx = 7,
                int miny = 0, int maxy = 7 );

    void drawSquare ( HDC, SQUARE, int x, int y );

    void update ( const ChessBoard & );
    void setSquareContents ( int x, int y, SQUARE );

    void setView ( bool newWhiteView );
    void toggleView() { whiteViewFlag = !whiteViewFlag; }
    bool queryWhiteView() const { return whiteViewFlag; }
    void updateAlgebraicCoords();

    bool isChanged ( int x, int y ) const
    {
        return DisplayCoordsValid(x,y) && changed[x][y];
    }

    //------------------- read move stuff ---------------------

    //Things called by chess thread...

    void   startReadingMove ( bool whiteIsMoving );
    bool   isReadingMove() const;
    void   copyMove ( int &source, int &dest );

    //Things called by main thread...
    void squareSelectedNotify ( int x, int y );

    // Called by chess thread's UI to select/deselect squares
    void selectSquare ( int x, int y );
    void deselectSquare()
    {
        selectSquare(-1,-1);
    }

    // The following is used for highlighting...
    void informMoveCoords ( int sourceX, int sourceY, int destX, int destY );

    void showVector ( int x1, int y1, int x2, int y2 )
    {
        vx1 = x1;
        vx2 = x2;
        vy1 = y1;
        vy2 = y2;
    }

    void hideVector()
    {
        vx1 = vx2 = vy1 = vy2 = -1;
    }

    void drawVector ( HDC );

    void keyMove (int dx, int dy);
    int getKeySelectX() const { return hiliteKeyX; }
    int getKeySelectY() const { return hiliteKeyY; }

    void sendAlgebraicChar(char letterOrNumber);

private:
    void keySelectSquare ( int x, int y );
    void touchSquare (int x, int y);

private:
    int vx1, vx2, vy1, vy2;

    SQUARE    board [8] [8];
    bool  changed [8] [8];

    int  selX, selY;   // square currently selected

    int  hiliteSourceX,  hiliteSourceY;
    int  hiliteDestX,    hiliteDestY;
    int  hiliteKeyX,     hiliteKeyY;     // coords for allowing user to make moves using the keyboard

    HBITMAP wp, wn, wb, wr, wq, wk;
    HBITMAP bp, bn, bb, br, bq, bk;

    int  bitmapsLoadedFlag;
    bool whiteViewFlag;
    bool algebraicCoordWhiteViewFlag;

    bool  readingMoveFlag;
    bool  gotSource;
    bool  moverIsWhite;
    int   moveSource;
    int   moveDest;

    char algebraicRank;          // 'a'..'h' if keyboard rank character pressed, '\0' otherwise
    HDC *tempHDC;
};


extern BoardDisplayBuffer TheBoardDisplayBuffer;

// FIXFIXFIX:  Should refactor DefPlayerInfo to have a single struct with same info for White and Black!
struct DefPlayerInfo
{
    enum Type
    {
        computerPlayer,
        humanPlayer,
        internetPlayer,
        namedPipePlayer,
        unknownPlayer
    };

    Type   whiteType;
    Type   blackType;

    // The following are used only for computerPlayer type...

    long   whiteThinkTime;    // expressed in centi-seconds
    long   blackThinkTime;    // expressed in centi-seconds

    int    whiteUseSeconds;   // search in centi-seconds or plies?
    int    blackUseSeconds;   // search in centi-seconds or plies?

    int    whiteSearchBias;
    int    blackSearchBias;

    bool   whiteBlunderAlert;
    bool   blackBlunderAlert;

    // The following are used only for internetPlayer type...

    InternetConnectionInfo  whiteInternetConnect;
    InternetConnectionInfo  blackInternetConnect;

    // The following are used only for namedPipePlayer type...
    char whiteServerName [64];
    char blackServerName [64];
};

bool DefinePlayerDialog(HWND hwnd);      // shows dialog box for setting up player sides, think times.


#if ENABLE_OPP_TIME

struct OppTimeParms
{
    // input parameters...
    bool                    wakeUpSignal;
    const ChessBoard        *inBoard;
    Move                    myMove_current;
    Move                    oppPredicted_current;
    ComputerChessPlayer     *oppThinker;

    // working parameters...
    PackedChessBoard        packedNextBoard;    // board predicted we will see next

    // output parameters...
    bool            finished;
    bool            foundMove;
    INT32           timeSpent;
    Move            myMove_next;
    Move            oppPredicted_next;
    INT32           timeStartedThinking;

    OppTimeParms():
        wakeUpSignal(false),
        inBoard(0),
        oppThinker(0),
        finished(false),
        foundMove(false),
        timeSpent(0),
        timeStartedThinking(0)
        {}
};

#endif /* ENABLE_OPP_TIME */

class ChessUI_win32_gui;

struct BlunderAlertParms
{
    BlunderAlertParms(const ChessBoard* _inBoard, ComputerChessPlayer *_thinker, SCORE _blunderScoreThreshold)
        : inBoard(_inBoard)
        , thinker(_thinker)
        , hurry(false)
        , minLevel(1)
        , maxLevel(3)
        , isStarted(false)
        , isFinished(false)
        , numLegalMoves(0)
        , levelCompleted(0)
        , blunderScoreThreshold(_blunderScoreThreshold)
        , wasUserWarned(false)
    {
    }

    ~BlunderAlertParms()
    {
        delete thinker;
        thinker = NULL;
    }

    // Input parameters, set by parent thread, read by blunder alert thread.
    const ChessBoard*       inBoard;    // chess board to be copied; parent may not modify until isStarted==true.
    ComputerChessPlayer*    thinker;    // computer chess player to be used by child thread for analysis; parent creates/deletes.
    bool                    hurry;      // set to true when parent thread needs an answer quickly (human just moved)
    int                     minLevel;   // minimum search depth that must be completed
    int                     maxLevel;   // limit to how far to search into the future

    // Output parameters, set by blunder alert thread, read by parent thread.
    bool        isStarted;          // set to true when thread has started.
    bool        isFinished;         // set to true when thread has finished.
    int         numLegalMoves;      // how many slots in 'path' are valid.
    int         levelCompleted;     // max search level completed, or 0 if not finished with first pass.
    BestPath    path[MAX_MOVES];    // one path for each legal move, with human's move at path[i][0].
    PackedChessBoard  packedBoard;  // for making sure board is the same before blunder info is used.

    // Placeholders, used by parent thread only.
    SCORE   blunderScoreThreshold;      // amount by which move must be worse than ideal to be called a blunder
    bool    wasUserWarned;              // have we already told the user he has made a blunder in this position?
};


class ChessUI_win32_gui: public ChessUI
{
public:
    ChessUI_win32_gui ( HWND );
    ~ChessUI_win32_gui();

    void ResetPlayers();     // call to forget any previous player definitions.
    ChessPlayer *CreatePlayer ( ChessSide );
    bool ReadMove ( ChessBoard &, int &source, int &dest, SQUARE &promIndex );
    SQUARE PromotePawn ( int PawnDest, ChessSide );
    void DisplayMove ( ChessBoard &, Move );
    void RecordMove ( ChessBoard &, Move, INT32 thinkTime );
    void DrawBoard ( const ChessBoard & );
    void ReportEndOfGame ( ChessSide winner );
    void Resign ( ChessSide iGiveUp, QuitGameReason );

    void DisplayBestMoveSoFar ( const ChessBoard &, Move, int level );
    void DisplayCurrentMove ( const ChessBoard &, Move, int level );
    void DisplayBestPath ( const ChessBoard &, const BestPath & );
    void PredictMate ( int numMovesFromNow );
    void ReportSpecial ( const char *msg );
    void ComputerIsThinking ( bool entering, ComputerChessPlayer & );
    void NotifyUser ( const char *message );

    void ReportComputerStats ( INT32   thinkTime,
                               UINT32  nodesVisited,
                               UINT32  nodesEvaluated,
                               UINT32  nodesGenerated,
                               int     fwSearchDepth,
                               UINT32  visnodes [NODES_ARRAY_SIZE],
                               UINT32  gennodes [NODES_ARRAY_SIZE] );

    // Force the computer to make a move NOW!
    void forceMove();

    struct gameReport
    {
        ChessSide       winner;
        bool            resignation;
        QuitGameReason  quitReason;
    };

    double runTacticalBenchmark();   //returns elapsed time in seconds

    DefPlayerInfo::Type queryWhitePlayerType() const { return whitePlayerType; }
    DefPlayerInfo::Type queryBlackPlayerType() const { return blackPlayerType; }

    void setWhiteThinkTime ( INT32 hundredthsOfSeconds );
    void setBlackThinkTime ( INT32 hundredthsOfSeconds );
    INT32 queryWhiteThinkTime() const;
    INT32 queryBlackThinkTime() const;

    virtual void DebugPly ( int depth, ChessBoard &, Move );
    virtual void DebugExit ( int depth, ChessBoard &, SCORE );
    bool allowMateAnnounce ( bool allow )
    {
        bool prev = enableMateAnnounce;
        enableMateAnnounce = allow;
        return prev;
    }
    void allowOppTime ( bool allow ) { enableOppTime = allow; }

    ChessPlayer *queryWhitePlayer() const { return whitePlayer; }
    ChessPlayer *queryBlackPlayer() const { return blackPlayer; }
    virtual void SetAdHocText ( int index, const char *, ... );

#if ENABLE_OPP_TIME
    //----------------- thinking on opponent's time ------------------------

    virtual bool oppTime_startThinking (
        const ChessBoard    &currentPosition,
        Move                myMove,
        Move                predictedOpponentMove );

    virtual bool oppTime_finishThinking (
        const ChessBoard    &newPosition,
        INT32               timeToThink,
        INT32               &timeSpent,
        Move                &myResponse,
        Move                &predictedOpponentMove );

    virtual void oppTime_abortSearch();

    //----------------------------------------------------------------------
#endif  /* ENABLE_OPP_TIME */

    BlunderAlertParms* blunderAlert_Initialize(const ChessBoard& board);
    void blunderAlert_StopThinking(BlunderAlertParms* parms);
    bool blunderAlert_IsLegalMove(const BlunderAlertParms* parms, const ChessBoard& board, int source, int dest);
    bool blunderAlert_IsAnalysisSufficient(const BlunderAlertParms* parms) const;
    bool blunderAlert_LooksLikeBlunder(BlunderAlertParms* parms, ChessBoard& board, int source, int dest);
    void blunderAlert_WarnUser(BlunderAlertParms* parms, ChessBoard& board, int humanMoveIndex, int bestMoveIndex);
    void blunderAlert_ClearWarning();

    void blunderAlert_FormatContinuation(
        const BlunderAlertParms* parms,
        ChessBoard& board,
        int moveIndex,
        const char* prefixText,
        char* text,
        int maxTextLength
    );

    bool blunderAlert_EnabledForEitherPlayer() const
    {
        // A little tricky: blunder alert flag can be true even though the player isn't human.
        // We need to make sure the flag is actually relevant to the player type.
        return
            ((whitePlayerType == DefPlayerInfo::humanPlayer) && blunderAlertEnabled_White) ||
            ((blackPlayerType == DefPlayerInfo::humanPlayer) && blunderAlertEnabled_Black);
    }

    bool blunderAlert_EnabledForCurrentPlayer(bool whiteToMove) const
    {
        return whiteToMove ? blunderAlertEnabled_White : blunderAlertEnabled_Black;
    }

    bool blunderAlert_EnabledForCurrentPlayer(const ChessBoard& board) const
    {
        return blunderAlert_EnabledForCurrentPlayer(board.WhiteToMove());
    }

private:
    HWND hwnd;
    ChessPlayer *whitePlayer;
    ChessPlayer *blackPlayer;

    DefPlayerInfo::Type  whitePlayerType;
    DefPlayerInfo::Type  blackPlayerType;

    bool tacticalBenchmarkMode;
    bool enableMateAnnounce;
    bool speakMovesFlag;
    Move benchmarkExpectedMove;
    int vofs1[MAX_BESTPATH_DISPLAY];  // temporary offsets for display debug vectors on screen
    int vofs2[MAX_BESTPATH_DISPLAY];
    ComputerChessPlayer *benchmarkPlayer;

#if ENABLE_OPP_TIME
    bool            enableOppTime;
    OppTimeParms    oppTimeParms;
#endif  /* ENABLE_OPP_TIME */

    bool blunderAlertEnabled_White;
    bool blunderAlertEnabled_Black;
};


enum BOARD_EDIT_REQUEST_STATE
{
    BER_NOTHING,
    BER_FEN,
    BER_SQUARE
};


class BoardEditRequest
{
private:
    BOARD_EDIT_REQUEST_STATE       state;      // Set by WindProc, cleared by ChessUI
    int        x, y;         // coordinates to be edited
    SQUARE     square;       // what to put in square
    char       fen [500];    // Forsyth Edwards Notation

public:
    BoardEditRequest():
        state (BER_NOTHING),
        x(0),
        y(0),
        square(EMPTY)
    {
        fen[0] = '\0';
    }

    BOARD_EDIT_REQUEST_STATE getState() const
    {
        return state;
    }

    void sendSingleSquareEdit (int _x, int _y, SQUARE _square)
    {
        x = _x;
        y = _y;
        square = _square;

        state = BER_SQUARE;
    }

    void sendFen (const char *_fen)
    {
        strncpy (fen, _fen, sizeof(fen)-1);
        fen[sizeof(fen)-1] = '\0';

        state = BER_FEN;
    }

    void getSingleSquareEdit (int &_x, int &_y, SQUARE &_square)
    {
        _x = x;
        _y = y;
        _square = square;
    }

    SQUARE getMostRecentSquare() const
    {
        return square;
    }

    const char *getFen()
    {
        return fen;
    }

    void reset()
    {
        state = BER_NOTHING;
    }
};


class MoveUndoPath
{
public:
    MoveUndoPath ():
        moveList(0),
        size(0),
        current(0),
        length(0)
        {}
    ~MoveUndoPath();
    void resetFromBoard ( const ChessBoard & );
    void undo ( ChessBoard & );
    void redo ( ChessBoard & );

private:
    int size;
    Move *moveList;   // dynamically allocated array of 'size'
    int current;      // index of current position in undo path
    int length;       // current total length of path
};


extern BoardEditRequest Global_BoardEditRequest;
extern DefPlayerInfo    DefPlayer;
extern ChessUI_win32_gui *Global_UI;
extern MoveUndoPath Global_MoveUndoPath;
extern bool Global_SpeakMovesFlag;
extern bool Global_EnableMateAnnounce;
extern INT32 SuggestThinkTime;
extern SCORE Global_BlunderAlertThreshold;
const int  MIN_BLUNDER_ALERT_THRESHOLD      =   50;
const int  MAX_BLUNDER_ALERT_THRESHOLD      = 1000;
const int  DEFAULT_BLUNDER_ALERT_THRESHOLD  =   80;

void Speak (const char *moveString);

/*
    $Log: winguich.h,v $
    Revision 1.12  2008/11/17 18:47:07  Don.Cross
    WinChen.exe now has an Edit/Enter FEN function for setting a board position based on a FEN string.
    Tweaked ChessBoard::SetForsythEdwardsNotation to skip over any leading white space.
    Fixed incorrect Chenard web site URL in the about box.
    Now that computers are so much faster, reduced the

    Revision 1.11  2008/11/08 16:54:40  Don.Cross
    Fixed a bug in WinChen.exe:  Pondering continued even when the opponent resigned.
    Now we abort the pondering search when informed of an opponent's resignation.

    Revision 1.10  2008/11/06 22:25:06  Don.Cross
    Now ParseFancyMove accepts either PGN or longmove (e7e8q) style notation only.
    We tolerate of check '+' or checkmate '#' characters being absent or extraneous in the input.
    The former is especially important because I often see PGN notation where '+' is missing on checking moves.
    To allow for PGN input, I had to allow avoidance of the PromotePawn callback, so other code was changed as well.
    Version numbers are now always constently yyyy.mm.dd across all builds of Chenard!

    Revision 1.9  2006/02/25 22:22:19  dcross
    Added support for connecting to Flywheel named pipe server as a Chenard chess player.
    This allows me to use my new Flywheel engine via the Chenard GUI.

    Revision 1.8  2006/02/24 20:55:39  dcross
    Added menu option for copying Forsyth Edwards Notation (FEN)
    for current board position into clipboard.

    Revision 1.7  2006/02/19 23:25:07  dcross
    I have started playing around with using Chenard to solve chess puzzles.
    I start with Human vs Human, edit the board to match the puzzle, and
    use the move suggester.  I made the following fixes/improvements based
    on my resulting experiences:

    1. Fixed off-by-one errors in announced mate predictions.
       The standard way of counting moves until mate includes the move being made.
       For example, the program was saying "mate in 3", when it should have said, "mate in 4".

    2. Temporarily turn off mate announce as a separate dialog box when suggesting a move.
       Instead, if the move forces mate, put that information in the suggestion text.

    3. Display the actual think time in the suggestion text.  In the case of finding
       a forced mate, the actual think time can be much less than the requested
       think time, and I thought it would be interesting as a means for testing performance.
       When I start working on Flywheel, I can use this as a means of comparing the
       two algorithms for performance.

    Revision 1.6  2006/01/22 20:20:22  dcross
    Added an option to enable/disable announcing forced mates via dialog box.

    Revision 1.5  2006/01/18 20:50:08  dcross
    The ability to speak move notation through the sound card was broken when
    I went from goofy move notation to PGN.  This is a quickie hack to make it
    work, though it's not perfect because it should say "moves to", and "at", but it doesn't.
    Also removed the board and move parameters to Speak(), because they were never used.

    Revision 1.4  2006/01/18 19:58:17  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.3  2005/11/26 19:16:26  dcross
    Added support for using the keyboard to make moves.

    Revision 1.2  2005/11/23 21:14:45  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!



    Revision history:

    2001 January 13 [Don Cross]
         Adding support for thinking on opponent's time.

    1999 February 19 [Don Cross]
        Adding new STATIC_ID_xxx fields for special things
        (e.g. Genetic Algorithm stuff).

    1999 February 12 [Don Cross]
        Adding ChessDisplayTextBuffer::queryText(), so that
        we can put visited node counts after best path moves.

    1999 January 3 [Don Cross]
        Displaying algebraic coordinates around board.
        Adding code to display "thinking" at bottom margin
        when a ComputerChessPlayer is deciding upon a move.

    1998 November 15 [Don Cross]
        Changing the meaning of 'whiteThinkTime' and 'blackThinkTime' to
        be centi-seconds in DefPlayerInfo.  (Was seconds before version 1.030).

    1997 March 3 [Don Cross]
        Changed bool Global_ViewDebugFlag into int Global_AnalysisType.
        Now the allowed values are 0=none, 1=bestpath, 2=currentpath.

    1997 January 30 [Don Cross]
        Adding the class MoveUndoPath, which tracks moves made in the game.
        When the user does an Undo, this program backs up in the game two plies
        so that the user can play a different move in a previous position, but
        still remembers the moves already in the path until the user chooses a new
        move, in which case all "futures" are forgotten.

    1996 July ?? [Don Cross]
        Started writing.

*/

