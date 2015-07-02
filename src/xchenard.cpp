/*
    xchenard.cpp  -  Don Cross  -  http://cosinekitty.com/chenard
    
    An adaptation of Chenard for WinBoard/xboard protocol version 2.
    For full functionality, use WinBoard
    See:   http://www.open-aurec.com/wbforum/WinBoard/engine-intf.html
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <assert.h>
#include <time.h>
#include <ctype.h>

#ifdef WIN32
    #include <windows.h>    // Sleep()
    #include <process.h>    /* _beginthread, _endthread */
#endif

#include "chess.h"

#if CHENARD_LINUX
    #include <sys/time.h>
#endif

#include "gamefile.h"
#include "chenga.h"
#include "uixboard.h"
#include "lrntree.h"

const char * const CHENARD_VERSION = ConvertDateToVersion(__DATE__);
const char * const OPTION_OPENING_BOOK = "Use opening book";
bool OpeningBookEnableState = true;

int XboardVersion = 0;
ChessBoard TheChessBoard;
ChessUI_xboard TheUserInterface;
ComputerChessPlayer TheComputerPlayer (TheUserInterface);
bool ComputerIsPlayingBlack = false;
bool ComputerIsPlayingWhite = false;
bool GlobalAbortFlag = false;
bool BoardIsCorrupt = false;
int TotalMovesPerPeriod = 0;
int TotalSecondsPerPeriod = 0;
int IncrementSecondsPerMove = 0;
bool DebugPrintAllowed = false;         // whether or not this version of xboard allows "#" to mean a debug statement
bool UserMoveOptionGranted = false;     // tells us whether xboard accepted the "usermove=1" feature request
bool TimeManagement = false;            // set to true if we are in 'level' command mode for managing time across multiple moves
bool PonderingAllowed = false;
int  MyRemainingTime = 0;               // when in time management mode, this stores the most recently reported number of centiseconds remaining in the time period
int  MemoryAllotmentInMegabytes = 0;    // remains 0 unless overridden by "memory" command.  reset to 0 after used.

void dprintf (const char *format, ...)
{
    if (DebugPrintAllowed) {
        va_list args;
        va_start (args, format);
        double absoluteTime = 0.01 * ChessTime();

        printf ("# Chenard(%7.2lf): ", absoluteTime);      // prefix required for all debug output: assumes all calls are a complete line.
        vprintf (format, args);

        va_end (args);
    }
}


const int MAX_MOVE_UNDO = 4000;
struct MoveUndo {
    Move        move;
    UnmoveInfo  unmove;
};
static MoveUndo *MoveUndoStack = NULL;
static int MoveUndoIndex = 0;          // also serves as a convenient ply counter for time budgeting
static int BlackMovedFirst = 0;        // used to adjust ply counter by 1, in case "setboard" command made Black move first

void ResetUndoMoveStack (bool whiteToMove)
{
    MoveUndoIndex = 0;

    // It is possible for Black to have the first ply after receiving "setboard" command.
    // Keep track of this to adjust the ply counter by 1 in this case, to know how many
    // moves remain in the current time period.
    // http://www.open-aurec.com/wbforum/viewtopic.php?f=2&t=50346&p=192315#p192304
    BlackMovedFirst = whiteToMove ? 0 : 1;
    dprintf ("ResetUndoMoveStack:  BlackMovedFirst=%d\n", BlackMovedFirst);
}

int PlyCounter()
{
    // This function helps time budgeting by telling us how far we are into the
    // game's time period sent by the "level" command.
    // For example, if PlyCounter() returns an even integer (0, 2, 4, ...)
    // we know it is White's turn to move, and odd means it is Black's turn.
    // MoveNumber = (PlyCounter()/2) + 1.
    return MoveUndoIndex + BlackMovedFirst;
}

void UndoMove()
{
    if (MoveUndoStack && MoveUndoIndex > 0 && MoveUndoIndex <= MAX_MOVE_UNDO) {
        --MoveUndoIndex;
        TheChessBoard.UnmakeMove (MoveUndoStack[MoveUndoIndex].move, MoveUndoStack[MoveUndoIndex].unmove);
    } else {
        ChessFatal ("UndoMove:  cannot undo half-move");
    }
}

const char *ParseVerb (const char *line, char *verb)
{
    while (*line && !isspace(*line)) {
        *verb++ = *line++;
    }
    
    *verb = '\0';
    
    while (*line && isspace(*line)) {
        ++line;
    }
    
    return line;    // return a pointer to the first non-whitespace character after the verb
}

void TrimString (char *s)
{
    // This function corrects for an annoying "feature" of fgets().
    // That function may (or may not) leave a '\n' at the end of the string it returns.
    // This function finds the first '\n', if any, and truncates the string there.
    while (*s) {
        if (*s == '\n') {
            *s = '\0';
            break;
        } else {
            ++s;
        }
    }
}

#if defined (WIN32)

// Under Linux, we could use the select() function to peek and see if input is ready on stdin.
// But here in Windows World, we are going to start another thread dedicated to reading stdin for us.
// That way the main chess engine thread can instantly enquire about I/O being ready.


struct tLineNode {
    tLineNode (const char *_line)
    {
        line = new char [1 + strlen(_line)];
        strcpy (line, _line);
        next = NULL;
    }

    ~tLineNode()
    {
        delete[] line;
    }

    char        *line;
    tLineNode   *next;
};


tLineNode *LineQueueFront = NULL;
tLineNode *LineQueueBack  = NULL;
CRITICAL_SECTION LineQueueCriticalSection;
bool InputThreadHasStarted = false;
bool InputThreadHasExited  = false;


void InputThreadFunc (void *)
{
    char line [MAX_XBOARD_LINE];

    while (!GlobalAbortFlag) {
        if (fgets (line, sizeof(line), stdin)) {
            TrimString (line);
            tLineNode *node = new tLineNode (line);     // get a new line node ready for the queue

            EnterCriticalSection (&LineQueueCriticalSection);   // grab exclusive access to the queue
            if (LineQueueBack == NULL) {
                LineQueueFront = LineQueueBack = node;          // this is the only node in the queue
            } else {
                LineQueueBack = LineQueueBack->next = node;     // add to the end of the list of node(s) already there
            }
            LeaveCriticalSection (&LineQueueCriticalSection);

            if (0 == memcmp(line,"quit",4)) {
                // Special case: avoid calling fgets again because we aren't going to get any more lines of input!
                break;
            }
        } else {
            // For some reason there was an error reading from stdin (maybe EOF???)
            GlobalAbortFlag = true;     // go ahead and let this whole process end
        }
    }

    InputThreadHasExited = true;
}

const char *ReadCommandFromXboard (char *verb)
{
    static char line [MAX_XBOARD_LINE];

    while (!GlobalAbortFlag) {
        // See if anything is waiting in the line queue...
        EnterCriticalSection (&LineQueueCriticalSection);
        tLineNode *node = LineQueueFront;
        if (node) {
            LineQueueFront = LineQueueFront->next;      // remove the front node from the list
            if (LineQueueFront == NULL) {
                LineQueueBack = NULL;
            }
        }
        LeaveCriticalSection (&LineQueueCriticalSection);

        if (node) {
            strcpy (line, node->line);
            delete node;
            return ParseVerb (line, verb);
        }

        Sleep (10);     // avoid burning up CPU time
    }

    return NULL;
}

bool InitializeInput()
{
    GlobalAbortFlag = false;

    InitializeCriticalSection (&LineQueueCriticalSection);

    // http://msdn.microsoft.com/en-us/library/kdzttdcb(VS.80).aspx
    uintptr_t threadHandle = _beginthread (InputThreadFunc, 0, NULL);
    InputThreadHasStarted = (threadHandle != (uintptr_t) (-1));
    return InputThreadHasStarted;
}

bool IsInputReady()
{
    if (InputThreadHasStarted) {
        // Check here to see if the I/O thread has an an input line ready from stdin.
        EnterCriticalSection (&LineQueueCriticalSection);
        bool ready = (LineQueueFront != NULL);
        LeaveCriticalSection (&LineQueueCriticalSection);
        return ready;
    } else {
        return false;
    }
}

#elif CHENARD_LINUX


bool InitializeInput()
{
    return true;
}

const char *ReadCommandFromXboard (char *verb)
{
    static char line [MAX_XBOARD_LINE];
    if (fgets (line, sizeof(line), stdin)) {
        TrimString (line);
        return ParseVerb (line, verb);
    } else {
        return NULL;
    }
}

bool IsInputReady()
{
    const int STANDARD_INPUT = 0;
    fd_set fd;
    timeval noblock;      
    
    memset(&noblock, 0, sizeof(noblock));   // zero-valued timeout will prevent select() from blocking
    
    FD_ZERO (&fd);      // initialize fd so that all file descriptor bits are zero
    FD_SET (STANDARD_INPUT, &fd);       // specify that we are interested in stdin
    
    int rc = select (1, &fd, NULL, NULL, &noblock);
    if (rc < 0) {
        ChessFatal ("IsInputReady:  Error returned in select()");
    }
    return (rc > 0);
}

#else
    #error Need to write code for handling reading from unbuffered stdin here.
#endif


void XChenardCleanup()
{
    if (MoveUndoStack) {
        delete[] MoveUndoStack;
        MoveUndoStack = NULL;
    }

#if defined(WIN32)
    while (InputThreadHasStarted && !InputThreadHasExited) {
        Sleep (10);
    }

    DeleteCriticalSection (&LineQueueCriticalSection);
#endif // WIN32
}


void StartNewGame()
{
    // Allow "memory" command to optionally override transposition hash table size...
    delete ComputerChessPlayer::XposTable;
    ComputerChessPlayer::XposTable = new TranspositionTable (MemoryAllotmentInMegabytes);   // if MemoryAllotmentInMegabytes==0, retains original Chenard behavior

    TheChessBoard.Init();       // Reset the chess board back to its initial, beginning-of-game state.
    BoardIsCorrupt = false;     // We just fixed any problems there might have been in the board state
    ComputerIsPlayingBlack = true;
    ComputerIsPlayingWhite = false;

    MyRemainingTime = 0;             // reset clocks
    ResetUndoMoveStack (TheChessBoard.WhiteToMove());       // otherwise the global ply counter 'MoveUndoIndex' is wrong!
    TheComputerPlayer.cancelPredictedOpponentMove();    // prevent any confusion about thinking we have predicted the opponent's move!

    dprintf ("StartNewGame() has finished resetting the game state.\n");
}

bool LooksLikeMove (const char *verb, int &source, int &dest, int &prom)
{
    source = 0;     // initialize output parameter to invalid value
    dest = 0;       // initialize output parameter to invalid value
    prom = 0;       // initialize output parameter to invalid value

    int length = (int) strlen(verb);
    if (length == 4 || length == 5) {
        if (verb[0] >= 'a' && verb[0] <= 'h') {
            if (verb[1] >= '1' && verb[1] <= '8') {
                if (verb[2] >= 'a' && verb[2] <= 'h') {
                    if (verb[3] >= '1' && verb[3] <= '8') {
                        if (length == 5) {
                            // Must be a pawn promotion
                            switch (verb[4]) {
                                case 'q':
                                    prom = Q_INDEX;
                                    break;
                                    
                                case 'r':
                                    prom = R_INDEX;
                                    break;
                                    
                                case 'n':
                                    prom = N_INDEX;
                                    break;
                                    
                                case 'b':
                                    prom = B_INDEX;
                                    break;
                                    
                                default:
                                    return false;   // NOT A VALID PAWN PROMOTION PIECE!!!
                            }
                        }
                        source = OFFSET (verb[0] - 'a' + 2, verb[1] - '1' + 2);
                        dest   = OFFSET (verb[2] - 'a' + 2, verb[3] - '1' + 2);
                        return true;    // this looks like a valid move
                    }
                }
            }
        }
    }
    
    return false;   // this does not look like a valid move
}

void SendMoveRejection (const char *original)
{
    printf ("Illegal move: %s\n", original);
}

void MakeMove (Move move)
{
    UnmoveInfo unmove;
    TheChessBoard.MakeMove (move, unmove);
    if (MoveUndoStack && MoveUndoIndex >= 0 && MoveUndoIndex < MAX_MOVE_UNDO) {
        MoveUndoStack[MoveUndoIndex].move   = move;
        MoveUndoStack[MoveUndoIndex].unmove = unmove;
        ++MoveUndoIndex;
    } else {
        ChessFatal ("Unable to push move onto undo stack.");
    }

    if (TheChessBoard.GameIsOver()) {
        bool canMove = TheChessBoard.CurrentPlayerCanMove();
        bool inCheck = TheChessBoard.CurrentPlayerInCheck();
        if (canMove) {
            // This must be a draw by material, 50-move rule, or repetition (can't tell which!)
            // FIXFIXFIX:  Figure out how to disambiguate the draw cases.
            printf ("1/2-1/2 {Draw}\n");
        } else {
            // This is either checkmate or stalemate...
            if (inCheck) {
                // Checkmate...
                if (TheChessBoard.WhiteToMove()) {
                    printf ("0-1 {Black mates}\n");
                } else {
                    printf ("1-0 {White mates}\n");
                }
            } else {
                printf ("1/2-1/2 {Stalemate}\n");
            }
        }
    }
}

void MakeMove (const char *original)
{
    MoveList ml;
    TheChessBoard.GenMoves (ml);
    for (int i=0; i < ml.num; ++i) {
        char compare[6];
        ChenardMoveToAlgebraic (TheChessBoard, ml.m[i], compare);
        if (0 == strcmp (compare, original)) {
            MakeMove (ml.m[i]);
            return;     // the move is legal, so avoid sending rejection below
        }
    }

    SendMoveRejection (original);       // getting here means we did not find this as a legal move
}

void ReceiveMove (const char *original, int source, int dest, int prom)
{
    if (BoardIsCorrupt) {
        SendMoveRejection (original);
    } else {
        MoveList ml;
        TheChessBoard.GenMoves (ml);
        
        int found = 0;      // count of how many moves match what was fed to us
        int msource;
        int mdest;
        int matchIndex = -1;
        
        for (int i=0; i < ml.num; ++i) {
            SQUARE mp = ml.m[i].actualOffsets (TheChessBoard, msource, mdest);
            if (msource==source && mdest==dest) {
                if (mp == EMPTY) {
                    // ml[i] is not a pawn promotion
                    if (prom == 0) {
                        ++found;
                        matchIndex = i;
                    }
                } else {
                    // ml[i] is a pawn promotion
                    if (prom == (int)UPIECE_INDEX(mp)) {
                        ++found;
                        matchIndex = i;
                    }
                }
            }
        }
        
        if (found == 1) {
            MakeMove (ml.m[matchIndex]);
        } else {
            SendMoveRejection (original);
        }
    }
}


void ChenardMoveToAlgebraic (bool whiteToMove, const Move &move, char str[6])
{
    int source;
    int dest;
    SQUARE prom = move.actualOffsets (whiteToMove, source, dest);

    str[0] = XPART(source) - 2 + 'a';
    str[1] = YPART(source) - 2 + '1';
    str[2] = XPART(dest)   - 2 + 'a';
    str[3] = YPART(dest)   - 2 + '1';
    str[4] = '\0';
    str[5] = '\0';
    
    switch (prom) {
        case EMPTY:
            break;
        
        case WQUEEN:
        case BQUEEN:
            str[4] = 'q';
            break;
        
        case WROOK:
        case BROOK:
            str[4] = 'r';
            break;
        
        case WBISHOP:
        case BBISHOP:
            str[4] = 'b';
            break;
        
        case WKNIGHT:
        case BKNIGHT:
            str[4] = 'n';
            break;        
        
        default:
            ChessFatal ("Invalid promotion piece in ChenardMoveToAlgebraic");
    }
}

bool IsComputersTurn()
{
    bool chenardsTurn = false;
    if (ComputerIsPlayingBlack) {
        chenardsTurn = TheChessBoard.BlackToMove();
    }
    if (ComputerIsPlayingWhite) {
        chenardsTurn = TheChessBoard.WhiteToMove();
    }
    return chenardsTurn;
}

void ReceiveForceCommand()
{
    ComputerIsPlayingBlack = ComputerIsPlayingWhite = false;
    TheComputerPlayer.cancelPredictedOpponentMove();        // disable pondering for this move only
}


void Breathe()      // yield a little bit of CPU time before pondering, so WinBoard can update display promptly!
{
    const int MILLISECONDS_TO_BREATHE = 350;

#if defined (WIN32)
    Sleep (MILLISECONDS_TO_BREATHE);
#elif CHENARD_LINUX
    // http://linux.die.net/man/3/nanosleep
    timespec delay;
    delay.tv_sec  = 0;
    delay.tv_nsec = long(MILLISECONDS_TO_BREATHE) * 1000000L;    // 1 million nanoseconds = 1 millisecond
    nanosleep (&delay, NULL);
#else
    #error Need code here for yielding CPU for a small amount of time.
#endif
}


void ThinkIfChenardsTurn()
{
think_again:
    if (!BoardIsCorrupt && IsComputersTurn() && !TheChessBoard.GameIsOver()) {
        if (MyRemainingTime > 0) {     // do we have a pending time amount?  (we clear it after using it)
            ReceiveMyRemainingTime (MyRemainingTime, true);     // we just confirmed that it is the comptuer's turn, so pass 'true'
            MyRemainingTime = 0;        // prevent from using this a second time; pondering logic below will take care of the rest.
        }

        Move move;
        INT32 timeSpent;
        TheUserInterface.OnStartSearch();
        if (TheComputerPlayer.GetMove (TheChessBoard, move, timeSpent)) {
            // This is a little weird: we may have received and processed a 'force' command.
            // If this happens, it means it is no longer our turn!
            if (IsComputersTurn()) {
ponder_again:
                char moveString [6];
                ChenardMoveToAlgebraic (TheChessBoard, move, moveString);
                printf ("move %s\n", moveString);
                MakeMove (move);    // must do this *AFTER* transmitting 'move' command, in case it is end of game!
                if (PonderingAllowed && !TheChessBoard.GameIsOver()) {
                    Move predictedOppMove = TheComputerPlayer.getPredictedOpponentMove();
                    if (predictedOppMove.dest == 0) {
                        dprintf ("No prediction of opponent's move is available.\n");
                    } else {
                        // It looks like we now have a prediction about what the opponent will do.
                        // Make darn sure it is a legal move, otherwise bad things could happen.
                        if (TheChessBoard.isLegal (predictedOppMove)) {
                            // Let's do some pondering!
                            UnmoveInfo  predictionUnmove;
                            Move        myHypotheticalReply;
                            INT32       ponderingTimeSpent;
                            char        predictedPgn[8];
                            char        predictedAlgebraic[6];
                            char        actualOpponentAlgebraic[6];

                            FormatChessMove (TheChessBoard, predictedOppMove, predictedPgn);
                            ChenardMoveToAlgebraic (TheChessBoard, predictedOppMove, predictedAlgebraic);

                            dprintf ("Predicted opponent's move: %s (%s)\n", predictedAlgebraic, predictedPgn);

                            Breathe();      // yield a little bit of CPU time before pondering, so WinBoard can update display promptly!

                            TheChessBoard.MakeMove (predictedOppMove, predictionUnmove);
                            TheUserInterface.StartPondering (predictedAlgebraic, predictedPgn);
                            TheComputerPlayer.SetTimeLimit (24 * 3600 * 100);    // one day: might as well be forever - expect an xboard event to interrupt search.
                            bool foundHypotheticalReply = TheComputerPlayer.GetMove (TheChessBoard, myHypotheticalReply, ponderingTimeSpent);
                            TheChessBoard.UnmakeMove (predictedOppMove, predictionUnmove);
                            TheUserInterface.FinishPondering (actualOpponentAlgebraic);

                            if (actualOpponentAlgebraic[0] == '\0') {
                                // We probably received some kind of command that aborted the search, other than an opponent's move.
                                // Another, less likely possibility is that we really did think for 24 hours without the opponent ever moving.
                                const char *verb = TheUserInterface.PendingVerb();
                                if (verb[0] != '\0') {
                                    // The UI stashed away exactly one command, aborted the search, and stopped reading commands.
                                    const char *rest = TheUserInterface.PendingRest();
                                    if (ExecuteCommand (verb, rest)) {
                                        GlobalAbortFlag = true;     // We received a "quit" command... cause main task loop to bail out immediately!
                                    }
                                }
                            } else {
                                MakeMove (actualOpponentAlgebraic);
                                if (foundHypotheticalReply) {
                                    if (TheChessBoard.GameIsOver()) {
                                        dprintf ("Game is over after opponent's actual move.\n");
                                    } else {
                                        if (IsComputersTurn()) {    // check again: game might have been aborted while we were pondering
                                            // Did we correctly guess the opponent's move?
                                            if (0 == strcmp (actualOpponentAlgebraic, predictedAlgebraic)) {
                                                // Yes, we did predict correctly, so the pondering we did was useful.
                                                move = myHypotheticalReply;
                                                goto ponder_again;
                                            } else {
                                                // No, we guessed wrong, so we must start over and think normally.
                                                const int remainingTime = TheUserInterface.MyPonderClock();   // the full amount of time is still in effect since we guessed wrong
                                                dprintf ("Guessed wrong... starting over with ponder clock = %d.\n", remainingTime);
                                                ReceiveMyRemainingTime (remainingTime, true);           // we just checked to make sure it is our turn
                                                goto think_again;
                                            }
                                        } else {
                                            dprintf ("Finished pondering, but not my turn.\n");
                                        }
                                    }
                                } else {
                                    dprintf ("Pondering returned false - resignation?\n");
                                }
                            }
                        } else {
                            dprintf ("IGNORING ILLEGAL PREDICTED MOVE!  s=%d d=%d\n", predictedOppMove.source, predictedOppMove.dest);
                        }
                    }
                }
            }
        } else {
            // failure to get a move... ?????
            // FIXFIXFIX:  Make sure GetMove returning false is abnormal.
            // (I can't remember how the ComputerChessPlayer indicates resignation.)
            ChessFatal ("Could not find a chess move.");
        }
    }
}


void SetDepthCommand (const char *rest)
{
    int depth = atoi(rest);
    if (depth > 0) {
        TheComputerPlayer.SetSearchDepth (depth);
    }
    TimeManagement = false;
}


void SetTimeCommand (const char *rest)
{
    int numberOfSeconds = atoi(rest);
    if (numberOfSeconds > 0) {
        TheComputerPlayer.SetTimeLimit (100 * numberOfSeconds);
    }
    TimeManagement = false;
}


void SetLevelCommand (const char *rest)
{
    if (rest == NULL) {
        ChessFatal ("SetLevelCommand:  rest == NULL");
    }
    
    if (strlen(rest) >= MAX_XBOARD_LINE) {
        ChessFatal ("SetLevelCommand:  input string is too long to be valid");
    }
    
    int  moves = 0;
    int  increment = 0;
    int  minutes = 0;
    int  seconds = 0;

    bool valid = true;
    
    if (4 != sscanf (rest, "%d %d:%d %d", &moves, &minutes, &seconds, &increment)) {
        seconds = 0;
        if (3 != sscanf (rest, "%d %d %d", &moves, &minutes, &increment)) {
            valid = false;
            printf ("Error (cannot parse '%s'): level\n", rest);
        }
    }

    if (valid) {
        TotalSecondsPerPeriod   = (60 * minutes) + seconds;
        TotalMovesPerPeriod     = moves;
        IncrementSecondsPerMove = increment;
        TimeManagement = true;
    }
}


int TimeBudgetForNextMove (int centiseconds, bool computersTurn)
{
    double denominator;
    
    // Here is an interesting problem: figure out how much time to think on each move.
    // We don't know how many moves are left in the game, but we may know how many are left in the time period.
    // Special case:  TotalMovesPerPeriod can be zero, which means the entire game is all one time period.
    if (TotalMovesPerPeriod <= 0) {     // include negative values, just in case xboard did something wacky
        denominator = 80.0;        // Always assume there are a certain number of moves left... Dude, I just made this up.
    } else {
        // One tricky bit: this function is sometimes called before we have received the opponent's move, sometimes after.
        // [7 December 2009] Bug fix: we no longer call IsComputersTurn() because when called from pondering,
        // TheChessBoard is in a random state because we are calling from an arbitrary point in the search!
        // I.e. TheChessBoard can report either side having the move, and that information is meaningless here.
        // Now we require the caller to pass in the computersTurn flag.
        const int plyCounter = PlyCounter() + (computersTurn ? 0 : 1);     // if we have not yet received opponent move, pretend like we did by increasing ply count by 1.
        const int plyWithinPeriod = plyCounter % (2 * TotalMovesPerPeriod);
        const int myRemainingMoves = TotalMovesPerPeriod - (plyWithinPeriod/2);

        dprintf (
            "TotalMovesPerPeriod=%d, centiseconds=%d, computersTurn=%d, plyCounter=%d, plyWithinPeriod=%d, myRemainingMoves=%d\n", 
            TotalMovesPerPeriod,     centiseconds,  (int)computersTurn, plyCounter,    plyWithinPeriod,    myRemainingMoves
        );

        denominator = double (myRemainingMoves + 0.5);              // use less of our remaining time as we approach 1 remaining move
    }

    if (PonderingAllowed) {
        // Because we are pondering, if there are more than a few moves remaining in the time period,
        // let's give ourselves a bit longer to think, based on the idea that we will correctly predict 
        // the opponent's move often enough to relieve time pressure.
        // But we don't play risky with our time budget if only a few moves remain.
        if (denominator >= 8) {
            denominator *= 0.75;     // making the denominator smaller makes the ratio (think time) bigger
        }
    }

    static const double TIME_CUSHION_IN_SECONDS = 0.2;     // We need to leave a little slack for overhead in communications, etc.
    static const double MAX_THINK_TIME = 3600.0;           // not much point thinking longer than an hour, is there?

    double remainingSeconds = 0.01 * double(centiseconds);

    // Budget the time remaining for the remaining number of moves, and a percentage of the increment.
    double thinkTimeInSeconds = (remainingSeconds / denominator) + (0.80 * double(IncrementSecondsPerMove));

    // Check for special cases where we need to override the time budget formula.
    // Also adjust for time cushion, which is a small amount of safety overhead for communications, etc.

    // Avoid leaving less than 1 second on the clock if possible...
    if (remainingSeconds - thinkTimeInSeconds < 1.0) {
        if (remainingSeconds >= 2.0) {
            thinkTimeInSeconds = remainingSeconds - 1.0;
        } else {
            thinkTimeInSeconds = remainingSeconds / 2.0;
        }
    }

    if (thinkTimeInSeconds <= 2.0 * TIME_CUSHION_IN_SECONDS) {
        thinkTimeInSeconds = TIME_CUSHION_IN_SECONDS;       // we are in deep trouble!
    } else if (thinkTimeInSeconds <= MAX_THINK_TIME + TIME_CUSHION_IN_SECONDS) {        
        thinkTimeInSeconds -= TIME_CUSHION_IN_SECONDS;
    } else {
        thinkTimeInSeconds = MAX_THINK_TIME;    
    }

    dprintf ("Time budget = %0.2lf seconds\n", thinkTimeInSeconds);
    int thinkTimeInCentiSeconds = (int) floor ((100.0 * thinkTimeInSeconds) + 0.5);      // round fractional seconds to nearest centisecond
    return thinkTimeInCentiSeconds;
}


void AcceptMyRemainingTime (int centiseconds)
{
    dprintf ("AcceptMyRemainingTime:  %d centiseconds\n", centiseconds);
    MyRemainingTime = centiseconds;
}


void ReceiveMyRemainingTime (int centiseconds, bool computersTurn)
{
    if (TimeManagement) {
        int thinkTimeInCentiSeconds = TimeBudgetForNextMove (centiseconds, computersTurn);
        TheComputerPlayer.SetTimeLimit (thinkTimeInCentiSeconds);   // this locks in the target time: any time spent after this is covered by the time limit!
    }
}


void SetBoard (const char *fen)
{
    // Tim Mann (author of xboard) says if invalid FEN is provided,
    // keep responding with "Illegal move" until "new", "edit", or "setboard" encountered.
    // The "edit" and "setboard" commands are mutually exclusive, and we have specified
    // receiving "setboard", so we will never see "edit".
    // Setting the BoardIsCorrupt flag to true causes us to do this until board reset.
    // Likewise, if the board was corrupt, setboard to a valid position should clear it.
    // See:  "setboard FEN" in  http://www.tim-mann.org/xboard/engine-intf.html
    BoardIsCorrupt = !TheChessBoard.SetForsythEdwardsNotation (fen);
    if (BoardIsCorrupt) {
        // Timm Man also recommends responding this way to a bad setboard command:
        printf ("tellusererror Illegal position\n");
    }
    
    // In either case, we want to start over with a new undo-move stack...
    ResetUndoMoveStack (TheChessBoard.WhiteToMove());

    // We also want to prevent any attempt at pondering...
    TheComputerPlayer.cancelPredictedOpponentMove();
}


void SendHint()
{
    Move hint = TheComputerPlayer.getPredictedOpponentMove();
    if (hint.dest != 0) {
        if (TheChessBoard.isLegal (hint)) {
            char algebraic[6];
            ChenardMoveToAlgebraic (TheChessBoard, hint, algebraic);
            printf ("Hint: %s\n", algebraic);
        }
    }
}


bool CommandMayBeIgnored (const char *verb)
{
    static const char * const Ignorables[] = {
        "draw",         // we ignore offers of a draw, which is equivalent to declining them in the xboard protocol.
        "random",
        "otim",         // some day we may care about how much time there is on the opponent's clock, but not yet
        "rejected",     // sometimes we look for "accepted <command>", but we can ignore "rejected" messages as identical to never receiving "accepted"
        "bk",
        "name",
        "rating",
        "computer",
        NULL
    };

    for (int i=0; Ignorables[i] != NULL; ++i) {
        if (0 == strcmp(verb,Ignorables[i])) {
            return true;
        }
    }

    return false;
}


void OpeningBookEnableDisable (bool enable)
{
    TheComputerPlayer.SetOpeningBookEnable (enable);
    TheComputerPlayer.SetTrainingEnable (enable);
    OpeningBookEnableState = enable;    // remember current state so that we can report it back to WinBoard in "feature option" command.
    dprintf ("%sabled opening book and training file.\n", (enable ? "En" : "Dis"));
}


void ParseOption (const char *text)
{
    char name  [MAX_XBOARD_LINE];
    char value [MAX_XBOARD_LINE];

    const char *equal = strchr (text, '=');
    if (equal != NULL) {
        size_t nameLength = equal - text;
        memcpy (name, text, nameLength);
        name[nameLength] = '\0';
        strcpy (value, equal+1);
        int valueInt = atoi(value);

        if (0 == strcmp (name, OPTION_OPENING_BOOK)) {
            OpeningBookEnableDisable (valueInt != 0);
        } else if (0 == strcmp (name, "memory")) {
            // This is a little bit squirrelly, because it is not an advertised XChenard option.
            // Normally memory is configured not by an "option" command, but by the older "memory" command.
            // I am just providing a back door for someone to send a memory command via xchenard.ini if needed.
            MemoryAllotmentInMegabytes = valueInt;
        } else {
            goto unknown_option;
        }
    } else {
        // Some option types ('-button' and '-save') have no '=VALUE' part after the option name.
        // Put any such options here in the future.        

unknown_option:
        dprintf ("Ignoring unknown option name in '%s'\n", text);
    }
}


bool ExecuteCommand (const char *verb, const char *rest)        // returns true if 'quit' received
{
    int moveSource, moveDest, promotionPiece;

    if (CommandMayBeIgnored(verb)) {
        return false;   // do not end the program
    }

    if (0 == strcmp(verb,"quit")) {
        return true;    // end the program
    } else if (0 == strcmp(verb,"xboard")) {
        if (XboardVersion < 1) {
            XboardVersion = 1;      // now we know we are talking to at least xboard version 1
        }
    } else if (0 == strcmp(verb,"protover")) {
        XboardVersion = atoi(rest);
        if (XboardVersion < 2) {
            ChessFatal ("Failure to read protover properly.");
        }
        
        // Now is a good time for Chenard to say 'hi' to xboard and introduce himself...
        printf ("feature myname=\"Chenard %s\"\n", CHENARD_VERSION);
        
        // Send feature requests...
        printf ("feature sigint=0\n");
        printf ("feature sigterm=0\n");
        printf ("feature ping=1\n");
        printf ("feature usermove=1\n");
        printf ("feature variants=\"normal\"\n");
        printf ("feature colors=0\n");
        printf ("feature setboard=1\n");
        printf ("feature analyze=0\n");     // FIXFIXFIX:  Implement analyze command some day.
        printf ("feature debug=1\n");       // Not available in all WinBoard/xboard implementions: send debug prints only if we receive "accepted debug".
        printf ("feature memory=1\n");      // [16 September 2009]:  Adding support for the new "memory" command.
        printf ("feature option=\"%s -check %d\"\n", OPTION_OPENING_BOOK, (OpeningBookEnableState ? 1 : 0));      // [17 September 2009]:  Allow user to enable/disable internal opening book and external training file chenard.trx.
        printf ("feature done=1\n");        // ***** This must be the final feature sent (ends xboard timeout) *****
    } else if (0 == strcmp(verb,"accepted")) {
        if (0 == strcmp(rest,"debug")) {
            DebugPrintAllowed = true;
            dprintf ("Debug enabled.  This is a %d-bit build.\n", (int)(8*sizeof(void*)));
        } else if (0 == strcmp(verb,"usermove")) {
            UserMoveOptionGranted = true;
        }
    } else if (0 == strcmp(verb,"new")) {
        StartNewGame();
    } else if (0 == strcmp(verb, "variant")) {
        // No variants are currently accepted... say nite nite
        printf ("Error (variants not implemented): %s\n", verb);
        return true;
    } else if (0 == strcmp(verb,"usermove")) {
        if (LooksLikeMove(rest, moveSource, moveDest, promotionPiece)) {
            // This looks like a move, but we can't depend on it being legal.
            ReceiveMove (rest, moveSource, moveDest, promotionPiece);
        } else {
            // I don't think we can ever get here, unless there is a bug either in xboard or this code.
            SendMoveRejection (rest);
        }
    } else if (0 == strcmp(verb,"go")) {
        // xboard is telling us to play whoever's turn it is to move.
        // Note that a single xchenard process never plays both sides simultaneously.
        // To play xchenard against itself, xboard will launch two xchenard.exe processes.
        // This constraint simplifies our pondering algorithm and allows certain assumptions to be made.
        ComputerIsPlayingWhite = TheChessBoard.WhiteToMove();
        ComputerIsPlayingBlack = TheChessBoard.BlackToMove();
    } else if (0 == strcmp(verb,"force")) {
        // xboard is telling us to stop thinking, and play neither side.
        ReceiveForceCommand();
    } else if (0 == strcmp(verb,"st")) {
        SetTimeCommand (rest);
    } else if (0 == strcmp(verb,"sd")) {
        SetDepthCommand (rest);
    } else if (0 == strcmp(verb,"time")) {
        AcceptMyRemainingTime (atoi(rest));
    } else if (0 == strcmp(verb,"level")) {
        SetLevelCommand (rest);
    } else if (0 == strcmp(verb,"ping")) {
        printf ("pong %s\n", rest);
    } else if (0 == strcmp(verb,"undo")) {
        UndoMove();
    } else if (0 == strcmp(verb,"remove")) {
        UndoMove();
        UndoMove();
    } else if (0 == strcmp(verb,"setboard")) {
        SetBoard (rest);
    } else if (0 == strcmp(verb,"post")) {
        TheUserInterface.EnableThinkingDisplay (true);
    } else if (0 == strcmp(verb,"nopost")) {
        TheUserInterface.EnableThinkingDisplay (true);
    } else if (0 == strcmp(verb,"hard")) {
        PonderingAllowed = true;
    } else if (0 == strcmp(verb,"easy")) {
        PonderingAllowed = false;
    } else if (0 == strcmp(verb,"result")) {
        // The game is over.  
        // This is not an "ignorable" command, so that it has the side effect of stopping pondering.
    } else if (0 == strcmp(verb,"hint")) {
        // I am not sure we can ever get here, based on the way pondering works.
        // But just in case...
        SendHint();
    } else if (0 == strcmp(verb,"memory")) {
        // [16 September 2009]
        // http://www.open-aurec.com/wbforum/WinBoard/engine-intf.html  says:
        //      memory N
        //      This command informs the engine on how much memory it is allowed to use maximally, in MegaBytes. 
        //      On receipt of this command, the engine should adapt the size of its hash tables accordingly. 
        //      This command does only fix the total memory use, the engine has to decide for itself 
        //      (or be configured by the user by other means) how to divide up the available memory between 
        //      the various tables it wants to use (e.g. main hash, pawn hash, tablebase cache, bitbases). 
        //      This command will only be sent to engines that have requested it through the memory feature, 
        //      and only at the start of a game, as the first of the commands to relay engine option 
        //      settings just before each "new" command. 
        MemoryAllotmentInMegabytes = atoi (rest);   // it is OK if atoi() results in 0 (invalid integer): we will use defaults then
    } else if (0 == strcmp(verb,"option")) {
        // [17 September 2009]
        // WinBoard 4.4.xx allows Chenard to request custom options from it, and it will then reply
        // with values chosen by the user.  This is where we receive the choices.
        // Currently the only custom option we support is whether to enable/disable the opening book (and training file).
        // rest ==> "Use opening book=0"
        ParseOption (rest);
    } else {
        bool processed = false;
        if (!UserMoveOptionGranted) {
            // Not tested, but best effort at good behavior.
            // Since we could not specify the usermove feature, we have to expect
            // moves unprefaced with 'usermove' as a verb.
            if (LooksLikeMove (verb, moveSource, moveDest, promotionPiece)) {
                ReceiveMove (verb, moveSource, moveDest, promotionPiece);
                processed = true;
            }
        }
        
        if (!processed) {
            // Getting here means we have received a command we don't know about.
            // I have made every effort to include a proper response for every command we can receive.
            // It is a good idea to reject any commands we don't know about at this time.
            printf ("Error (unknown command): %s\n", verb);
        }
    }

    return false;       // do NOT quit!
}


void PlaySelf()
{
    INT32       timeSpent;
    Move        move;
    char        moveString [MAX_MOVE_STRLEN + 1];
    UnmoveInfo  unmove;

    TheComputerPlayer.SetTimeLimit (1500);
    while (
        !TheChessBoard.GameIsOver() && 
        (TheChessBoard.GetCurrentPlyNumber() < 80) && 
        TheComputerPlayer.GetMove (TheChessBoard, move, timeSpent)
    ) {
        FormatChessMove (TheChessBoard, move, moveString);
        printf ("%s:  [%6d]  %s\n", 
            (TheChessBoard.WhiteToMove() ? "White" : "Black"),
            move.score,
            moveString
        );
        TheChessBoard.MakeMove (move, unmove);
    }
}


int main (int argc, const char *argv[])
{
    char verb [MAX_XBOARD_LINE];

    TheUserInterface.SetComputerChessPlayer (&TheComputerPlayer);
    TheComputerPlayer.oppTime_SetEnableFlag();  // This just allows us to get predictions of opponent's move; whether we do pondering is a separate issue.

    // http://www.tim-mann.org/xboard/engine-intf.html
    setbuf (stdout, NULL);      // xboard requires unbuffered I/O
    setbuf (stdin, NULL);       // xboard requires unbuffered I/O

    if (argc == 2) {
        printf ("xchenard - by Don Cross - http://cosinekitty.com/chenard\n");
        if (0 == strcmp(argv[1],"-eg")) {
            TheUserInterface.EnableAdHocText();     // so we can see progress of the database generation
            GenerateEndgameDatabases (TheChessBoard, TheUserInterface);
        } else if (0 == strcmp(argv[1],"-constants")) {
            printf ("WPAWN    %9u  0x%08x      BPAWN    %9u  0x%08x\n", unsigned(WPAWN), unsigned(WPAWN), unsigned(BPAWN), unsigned(BPAWN));
            printf ("WKNIGHT  %9u  0x%08x      BKNIGHT  %9u  0x%08x\n", unsigned(WKNIGHT), unsigned(WKNIGHT), unsigned(BKNIGHT), unsigned(BKNIGHT));
            printf ("WBISHOP  %9u  0x%08x      BBISHOP  %9u  0x%08x\n", unsigned(WBISHOP), unsigned(WBISHOP), unsigned(BBISHOP), unsigned(BBISHOP));
            printf ("WROOK    %9u  0x%08x      BROOK    %9u  0x%08x\n", unsigned(WROOK), unsigned(WROOK), unsigned(BROOK), unsigned(BROOK));
            printf ("WQUEEN   %9u  0x%08x      BQUEEN   %9u  0x%08x\n", unsigned(WQUEEN), unsigned(WQUEEN), unsigned(BQUEEN), unsigned(BQUEEN));
            printf ("WKING    %9u  0x%08x      BKING    %9u  0x%08x\n", unsigned(WKING), unsigned(WKING), unsigned(BKING), unsigned(BKING));
            printf ("\n");
        } else if (0 == strcmp(argv[1],"-self")) {
            PlaySelf();     // I use this for Profile Guided Optimization
        } else {
            printf ("Unknown option on command line.");
        }
        return 0;
    }

    // UnitTest_IsInputReady();

    if (!InitializeInput()) {
        // error initializing input
        return 1;
    }

    MoveUndoStack = new MoveUndo [MAX_MOVE_UNDO];
    if (MoveUndoStack) {
        memset (MoveUndoStack, 0, MAX_MOVE_UNDO * sizeof(MoveUndo));
    }    

    // Look for an optional configuration file called xchenard.ini.
    // If it exists, load and execute xboard options from it.
    // This allows setting options in case using an older version of WinBoard/xboard
    // that do not support the "option" command.
    const char * const iniFileName = "xchenard.ini";
    FILE *iniFile = fopen (iniFileName, "rt");
    if (iniFile) {
        dprintf ("Reading options from configuration file '%s' ...\n", iniFileName);
        while (fgets (verb, sizeof(verb), iniFile)) {
            TrimString (verb);
            ParseOption (verb);
        }
        fclose (iniFile);
        iniFile = NULL;
    } else {
        dprintf ("Configuration file '%s' does not exist.", iniFileName);
    }
    
    // Enter the main task loop where we interact with WinBoard/xboard
    const char *rest;
    while (!GlobalAbortFlag && (NULL != (rest = ReadCommandFromXboard (verb)))) {
        if (ExecuteCommand (verb, rest)) {
            break;
        }

        // At this point, if input is quiet, check to see if it is our turn to move...
        if (!IsInputReady()) {
            ThinkIfChenardsTurn();
        }
    }

    XChenardCleanup();
    return 0;
}

void ChessFatal ( const char *message )
{
    printf ("tellusererror XCHENARD FATAL ERROR: %s\n", message);
    XChenardCleanup();
    exit(1);
}

const char *GetWhitePlayerString()
{
    return "White Player";
}

const char *GetBlackPlayerString()
{
    return "Black Player";
}

/*
    $Log: xchenard.cpp,v $
    Revision 1.29  2009/12/08 22:17:32  don.cross
    Fixed compiler errors about goto skipping over variable initializers.

    Revision 1.28  2009/12/08 21:59:22  Don.Cross
    Fixed embarrassing bugs in XChenard's timing code:
    I was using IsComputersTurn() to figure out whether to adjust the remaining moves in the current time period,
    but this was wrong because it in turn uses TheChessBoard to figure out whose turn it is; but TheChessBoard
    can be in a random state due to being called from the search!  Now the caller figures out whether or not the
    opponent's actual move has been received and recorded.
    Another problem was that I was finding out whose turn it was when I received the "time" command, which is received
    before the opponent's move has arrived!  A similar problem occurs at the beginning of the game, when the
    computer is playing neither White nor Black (yet).
    Also needed to reset the ply counter when the "new" command is received.

    Revision 1.27  2009/09/19 18:04:12  Don.Cross
    Fixed a timing bug that affected XChenard when the hosting software used external opening books:
    XChenard would get confused about how many moves remained in the current time period, and would
    typically lose by burning up all its time.  Instead of trying to keep special-purpose counters up to date,
    we just take a look at how many plies have been made, whether or not we have already received
    the opponent's move, and calculate where we are inside the current time period, if any is in effect.

    Revision 1.26  2009/09/17 17:48:58  Don.Cross
    Added support for the WinBoard 4.4.xx "option" command.
    Used this to implement a custom option for enabling/disabling opening book and training file (chenard.trx).
    On startup, do a one-time check for a file called xchenard.ini and process any options within it.

    Revision 1.25  2009/09/16 16:49:34  Don.Cross
    Added support for the WinBoard/xboard "memory" command to set hash table sizes.

    Revision 1.24  2008/12/10 19:00:32  Don.Cross
    Minor fixes to make Linux g++ warnings go away.

    Revision 1.23  2008/12/07 02:43:09  Don.Cross
    Learned how to use Profile Guided Optimization, thanks to a conversation with Jim Ablett on the WinBoard forum.
    Modified xchenard.exe to have a "-self" option to assist in the profiling step required by PGO.
    Also tweaked optimizations used in winchen.exe.
    I discovered that I had not enabled optimizations for xchenard.exe!
    Until I figure out how to automate PGO builds, webpack.bat will not auto-build the source code by default.

    Revision 1.22  2008/12/05 22:52:34  Don.Cross
    I received reports that people using Microsoft Visual C++ Express Edition were getting compiler errors in winchen.rc.
    It turns out that Express Edition does not ship all the header files that are available in the professional edition that I use.
    I fixed winchen.rc to remove dependency on these include files.

    Fixed a theoretical bug I discovered while reviewing the source code for XChenard:
    I was using centiseconds instead of seconds to budget time when there was less than 2 seconds left on the clock.
    This could have caused XChenard to let its flag drop, although I have never seen this happen.

    Revision 1.21  2008/11/23 21:25:26  Don.Cross
    I have noticed in playing xchenard vs xchenard, with pondering enabled, on my machine with 2 CPUs,
    WinBoard often has a hard time displaying the moves promptly because of all the CPU usage.
    I am trying out an idea: before pondering, sacrifice 0.35 seconds to sleep, so that WinBoard gets some time.

    Revision 1.20  2008/11/23 20:00:21  Don.Cross
    Fixed compiler error in Linux version of xchenard:  forgot to include stdarg.h for dprintf().
    I had a redundant include of stdlib.h instead of stdarg.h.
    I don't know why this compiled in Windows!

    Revision 1.19  2008/11/23 19:41:55  Don.Cross
    When we are pondering, and are about to display best path, check to see if we have already received
    the opponent's move that we were predicting.  If so, we should NOT include that move in the best path displayed,
    because it is no longer a hypothetical move.  In other words, from xboard's point of view, we are no longer pondering,
    but thinking about an actual position.
    Fixed bugs that caused MyRemainingMoves to not be in sync with the correct value.
    Worked on TimeBudgetForNextMove so that it does not let the time fall dangerously low.
    I have seen WinBoard display (!) after the time when it falls below one second, which just looks scary!

    Revision 1.18  2008/11/22 20:31:53  Don.Cross
    Tweaked TimeBudgetForNextMove to be more generous with the think time, and more so when pondering is enabled.

    Revision 1.17  2008/11/22 16:27:32  Don.Cross
    Massive overhaul to xchenard: now supports pondering.
    This required very complex changes, and will need a lot of testing before release to the public.
    Pondering support made it trivial to implement the "hint" command, so I added that too.
    Pondering required careful attention to every possible command we could receive from xboard,
    so now I have started explicitly sending back errors for any command we don't understand.
    I finally figured out how to get rid of the annoying /WP64 warning from the Microsoft C++ compiler.

    Revision 1.16  2008/11/21 03:16:45  Don.Cross
    I realized I could just call ComputerChessPlayer::getPredictedOpponentMove() instead of having to override a virtual method.
    It returns a move where dest==0 if there is no prediction.  This is much simpler!

    Revision 1.15  2008/11/21 03:05:16  Don.Cross
    First baby step toward pondering in xchenard: used a hack in the Chenard engine that supports multithreaded pondering in WinChen.
    We are not doing pondering the same way, but the hack allows us to share the part of WinChen that predicts the opponent's move.
    Did some other refactoring of code that may help finish pondering, but I'm still trying to figure out what I'm really going to do.

    Revision 1.14  2008/11/20 23:13:49  Don.Cross
    Now dprintf is a true function instead of a macro hack, and it prefixes its output with a "#" and time stamp.

    Revision 1.13  2008/11/20 02:55:53  Don.Cross
    Improved time management: use 'time' messages from xboard to know how much time it thinks we have left on our clock.
    Do some simple math on the value to move faster when time is running out.
    Also, be a little more careful with xboard protocol: check for accepted/rejected on feature requests that matter to us.
    Added a dprintf macro for debug prints when "feature debug=1" is accepted, but so far the WinBoard I am using doesn't accept it.

    Revision 1.12  2008/11/18 21:48:36  Don.Cross
    XChenard now supports the xboard commands post and nopost, to enable/disable display of thinking.

    Revision 1.11  2008/11/18 19:54:53  Don.Cross
    Fixed a really annoying bug: if the opponent played Black and tried to promote a pawn,
    XChenard would reject the move, saying it was illegal!  Now pawn promotion works for both White and Black opponents.

    Revision 1.10  2008/11/15 16:44:55  Don.Cross
    Fixed a bug I found in testing:  in the new Windows dual-threaded code, I was forgetting to trim the '\n' from the end of the input lines.

    Revision 1.9  2008/11/15 03:04:06  Don.Cross
    I think I have figured out how to make the Windows version of XChenard handle the "?" command
    for making a move immediately.  Needs more testing.  Basically I have conditional code for Linux or Windows.
    In Linux, I use the select() command as before, but now in Windows I start up a dedicated thread for
    processing lines of input from stdin and enqueuing them.  The main chess engine thread can enter a critical
    section and inspect the queue at any time to see if one or more lines of input is waiting.
    Needs more testing on Windows, and I also need to see if this will still compile and run correctly on Linux.
    Checking in tonight to back up my work before I go to bed.

    Revision 1.8  2008/11/06 22:25:07  Don.Cross
    Now ParseFancyMove accepts either PGN or longmove (e7e8q) style notation only.
    We tolerate of check '+' or checkmate '#' characters being absent or extraneous in the input.
    The former is especially important because I often see PGN notation where '+' is missing on checking moves.
    To allow for PGN input, I had to allow avoidance of the PromotePawn callback, so other code was changed as well.
    Version numbers are now always constently yyyy.mm.dd across all builds of Chenard!

    Revision 1.7  2008/11/02 22:23:02  Don.Cross
    Made tweaks so I can use xchenard to generate endgame databases.
    No longer disable SIGTERM and SIGINT: this had the effect of making it so I could not Ctrl+C to kill xchenard!
    These are no longer needed because I send back the feature commands to request xboard not doing that.

    Revision 1.6  2008/11/02 21:04:28  Don.Cross
    Building xchenard for Win32 console mode now, so I can debug some stuff.
    Tweaked code to make Microsoft compiler warnings go away: deprecated functions and implicit type conversions.
    Checking in with version set to 1.51 for WinChen.

    Revision 1.5  2008/11/02 20:04:13  Don.Cross
    xchenard: Implemented xboard command 'setboard'.
    This required a new function ChessBoard::SetForsythEdwardsNotation.
    Enhanced ChessBoard::PositionIsPossible to check for more things that are not possible.
    Fixed anachronisms in ChessBoard::operator== where I was returning 0 or 1 instead of false or true.

    Revision 1.4  2008/11/02 02:01:33  Don.Cross
    Added support for 'undo' and 'remove' commands.
    Figured out how to avoid the long delay during startup: xboard waits a while for more options,
    but if you specify "feature done=1" it immediately proceeds.
    Enabled ping command to avoid documented race conditions.

    Revision 1.3  2008/11/01 21:55:32  Don.Cross
    Xboard force move command '?' is working.
    Chenard can now play White or Black.
    Crude ability to obey the 'level' command for think time.

    Revision 1.2  2008/11/01 19:01:11  Don.Cross
    The Linux/xboard version of Chenard is starting to work!
    Right now it only knows how to play Black.
    There are a lot more xboard commands it needs to understand in order to be fully functional.

    Revision 1.1  2008/11/01 16:18:49  Don.Cross
    The beginning of an xboard version of Chenard for Linux.

*/
