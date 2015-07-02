/*==========================================================================

    uiwin32.cpp  -  Copyright (C) 1993-2005 by Don Cross

    Contains Chenard user interface code specialized
    for 32-bit Windows.

==========================================================================*/

#include <stdio.h>
#include <process.h>
#include <assert.h>
#include <time.h>

#include "chess.h"
#include "gamefile.h"
#include "winchess.h"
#include "ichess.h"
#include "winguich.h"
#include "resource.h"
#include "profiler.h"

#define  REQUEST_BENCHMARK_DATA   0

bool Global_SuggestMoveFlag = false;
bool Global_RepeatSuggestMoveFlag = false;
ComputerChessPlayer *Global_SuggestMoveThinker = 0;
CRITICAL_SECTION SuggestMoveCriticalSection;        // initialized in WinMain()
bool Global_UserResign = false;
bool Global_SpeakMovesFlag = true;
bool Global_EnableMateAnnounce = true;
extern bool Global_EnableOppTime;
extern int Global_AnalysisType;

const uintptr_t INVALID_THREAD = static_cast<uintptr_t>(-1L);

void BlunderAlertThreadFunc(void *data);


ChessUI_win32_gui::ChessUI_win32_gui ( HWND _hwnd ):
    hwnd ( _hwnd ),
    whitePlayerType (DefPlayerInfo::unknownPlayer),
    blackPlayerType (DefPlayerInfo::unknownPlayer),
    whitePlayer (0),
    blackPlayer (0),
    tacticalBenchmarkMode (false),
    benchmarkPlayer (0),
    enableMateAnnounce (Global_EnableMateAnnounce),
    enableOppTime (true),
    blunderAlertEnabled_White(false),
    blunderAlertEnabled_Black(false)
{
}


ChessUI_win32_gui::~ChessUI_win32_gui()
{
    ResetPlayers();
}


void ChessUI_win32_gui::ResetPlayers()
{
    whitePlayer = 0;
    blackPlayer = 0;
    whitePlayerType = DefPlayerInfo::unknownPlayer;
    blackPlayerType = DefPlayerInfo::unknownPlayer;
}


ChessPlayer *ChessUI_win32_gui::CreatePlayer ( ChessSide side )
{
    ChessPlayer *player = 0;
    int searchDepth;
    int searchBias;
    int useSeconds;

    DefPlayerInfo::Type playerType;

    if (side == SIDE_WHITE)
    {
        playerType = DefPlayer.whiteType;
        searchDepth = DefPlayer.whiteThinkTime;
        searchBias = DefPlayer.whiteSearchBias;
        useSeconds = DefPlayer.whiteUseSeconds;
    }
    else
    {
        playerType = DefPlayer.blackType;
        searchDepth = DefPlayer.blackThinkTime;
        searchBias = DefPlayer.blackSearchBias;
        useSeconds = DefPlayer.blackUseSeconds;
    }

    switch (playerType)
    {
    case DefPlayerInfo::computerPlayer:
    {
        ComputerChessPlayer *puter = new ComputerChessPlayer (*this);
        if (puter)
        {
            player = puter;
            if (useSeconds)
            {
                puter->SetTimeLimit(searchDepth);
            }
            else
            {
                puter->SetSearchDepth(searchDepth);
            }

            puter->SetSearchBias(searchBias);
            extern bool Global_AllowResignFlag;
            puter->setResignFlag(Global_AllowResignFlag);
        }
    }
    break;

    case DefPlayerInfo::humanPlayer:
    {
        player = new HumanChessPlayer (*this);
    }
    break;

#if SUPPORT_INTERNET
    case DefPlayerInfo::internetPlayer:
    {
        const InternetConnectionInfo &cinfo =
            (side == SIDE_WHITE) ?
            DefPlayer.whiteInternetConnect :
            DefPlayer.blackInternetConnect;

        extern bool Global_InternetPlayersReady;
        while ( !Global_InternetPlayersReady )
        {
            Sleep(500);
        }

        MessageBox ( hwnd, "Remote connection established...\nready to play!", "", MB_OK | MB_ICONINFORMATION );
        player = new InternetChessPlayer (*this, cinfo);
    }
    break;
#endif  // SUPPORT_INTERNET

#if SUPPORT_NAMED_PIPE
    case DefPlayerInfo::namedPipePlayer:
    {
        const char *serverMachineName =
            (side == SIDE_WHITE) ?
            DefPlayer.whiteServerName :
            DefPlayer.blackServerName;

        player = new NamedPipeChessPlayer (*this, serverMachineName);
    }
    break;
#endif // SUPPORT_NAMED_PIPE
    }

    if (side == SIDE_WHITE)
    {
        whitePlayerType = playerType;
        whitePlayer = player;
        blunderAlertEnabled_White = DefPlayer.whiteBlunderAlert;
    }
    else
    {
        blackPlayerType = playerType;
        blackPlayer = player;
        blunderAlertEnabled_Black = DefPlayer.blackBlunderAlert;
    }

    if (whitePlayer && blackPlayer)
    {
        // Both players have been defined.
        // Set options that depend on them.

        if ( whitePlayerType != DefPlayerInfo::humanPlayer &&
             blackPlayerType == DefPlayerInfo::humanPlayer )
        {
            // When a human is playing Black and
            // something other than human is playing White,
            // show the chess board from Black's point of view.
            TheBoardDisplayBuffer.setView(false);
        }
        else
        {
            // In all other cases, show the board from White's point of view.
            TheBoardDisplayBuffer.setView(true);
        }

        // Thinking on opponent's time is disabled by default when
        // a computer player is created.  Leave things that way
        // unless we know for sure that the opponent isn't also
        // a computer.

        if ( whitePlayerType == DefPlayerInfo::computerPlayer &&
             blackPlayerType != DefPlayerInfo::computerPlayer )
        {
            ((ComputerChessPlayer *)whitePlayer)->oppTime_SetEnableFlag();
        }

        if ( blackPlayerType == DefPlayerInfo::computerPlayer &&
             whitePlayerType != DefPlayerInfo::computerPlayer )
        {
            ((ComputerChessPlayer *)blackPlayer)->oppTime_SetEnableFlag();
        }
    }

    return player;
}


static void LoadGameFromFile ( FILE *f, ChessBoard &board )
{
    board.Init();

    Move move;
    UnmoveInfo unmove;

    for(;;)
    {
        int c1 = fgetc(f);
        int c2 = fgetc(f);
        int c3 = fgetc(f);
        int c4 = fgetc(f);
        if ( c4 == EOF )
            break;

        move.source  =  BYTE(c1);
        move.dest    =  BYTE(c2);
        move.score   =  (c4 << 8) | c3;

        if ( sizeof(move.score)>2 && (c4&0x80) )
            move.score |= ((-1) ^ 0xffff);      // perform sign-extend if necessary

        if ( (move.dest & SPECIAL_MOVE_MASK) == SPECIAL_MOVE_EDIT )
        {
            board.EditCommand ( move );
            board.SaveSpecialMove ( move );
        }
        else
            board.MakeMove ( move, unmove );
    }

    Global_MoveUndoPath.resetFromBoard ( board );
}



const char *GetPgnFileStateString (PGN_FILE_STATE state);



PGN_FILE_STATE LoadGameFromPgnFile (FILE *f, ChessBoard &board)     // loads only the first game in the PGN file
{
    PGN_FILE_STATE  state;
    char            movestr [1+MAX_MOVE_STRLEN];
    Move            move;
    UnmoveInfo      unmove;
    PgnExtraInfo    info;

    board.Init();

    while (GetNextPgnMove (f, movestr, state, info))
    {
        if (info.fen[0])
        {
            bool success = board.SetForsythEdwardsNotation (info.fen);
            if (!success)
            {
                state = PGN_FILE_STATE_SYNTAX_ERROR;
                break;
            }
        }

        if (board.ScanMove (movestr, move))
        {
            board.MakeMove (move, unmove);
        }
        else
        {
            state = PGN_FILE_STATE_ILLEGAL_MOVE;
            break;
        }
    }

    if (info.fen[0])
    {
        // Special case:  there may have been an edited position, but no moves after that.
        bool success = board.SetForsythEdwardsNotation (info.fen);
        if (!success)
        {
            state = PGN_FILE_STATE_SYNTAX_ERROR;
        }
    }

    Global_MoveUndoPath.resetFromBoard (board);
    return state;
}


static void LoadGameFromHardcode (
    unsigned long *h,
    int n,
    ChessBoard &board )
{
    board.Init();
    Move move;

    for ( int i=0; i<n; i++ )
    {
        move.source = BYTE ( h[i] & 0xff );
        move.dest   = BYTE ( (h[i] >> 8) & 0xff );
        move.score  = SCORE ( (h[i] >> 16) & 0xffff );
        if ( sizeof(move.score)>2 && (move.score & 0x8000) )
            move.score |= ((-1) ^ 0xffff);      // perform sign-extend if necessary

        if ( (move.dest & SPECIAL_MOVE_MASK) == SPECIAL_MOVE_EDIT )
        {
            board.EditCommand ( move );
            board.SaveSpecialMove ( move );
        }
        else
        {
            UnmoveInfo unmove;
            board.MakeMove ( move, unmove );
        }
    }
}


static void SaveGameToFile (
    FILE *outfile,
    const ChessBoard  &board )
{
    int ply = board.GetCurrentPlyNumber();

    for ( int p=0; p < ply; p++ )
    {
        Move move = board.GetPastMove(p);
        fputc ( move.source, outfile );
        fputc ( move.dest, outfile );
        fputc ( move.score & 0xff, outfile );
        fputc ( (move.score >> 8) & 0xff, outfile );
    }
}



INT32 SuggestThinkTime = 500;   // expressed in centiseconds


BOOL CALLBACK SuggestMove_DlgProc (
    HWND hwnd,
    UINT msg,
    WPARAM wparam,
    LPARAM lparam )
{
    BOOL result = FALSE;
    char buffer [128];
    HWND timeEditBox = GetDlgItem (hwnd, IDC_EDIT_SUGGEST_TIME);

    switch ( msg )
    {
    case WM_INITDIALOG:
    {
        // Set check box to match previous "ding after suggestion" option.
        CheckDlgButton(
            hwnd,
            IDC_CHECK_DING_AFTER_SUGGEST,
            Global_DingAfterSuggest ? BST_CHECKED : BST_UNCHECKED);

        // Remember think time setting from previous suggested moves...

        int length = sprintf ( buffer, "%lg", double(SuggestThinkTime)/100.0 );
        SetWindowText (timeEditBox, buffer);
        SetFocus (timeEditBox);

        // Select the text inside the text box, so the user does not need to hit TAB key to edit it.
        // http://msdn.microsoft.com/en-us/library/bb761661(VS.85).aspx
        SendMessage (timeEditBox, EM_SETSEL, 0 /*starting position*/, length /*one past end position*/);
    }
    break;

    case WM_COMMAND:
    {
        switch ( wparam )
        {
        case IDOK:
        {
            buffer[0] = '\0';
            GetWindowText ( timeEditBox, buffer, sizeof(buffer) );
            double thinkTime = atof(buffer);
            if ( thinkTime < 0.1 )
                thinkTime = 0.1;

            SuggestThinkTime = INT32 ( thinkTime * 100 );
            Global_DingAfterSuggest = IsDlgButtonChecked(hwnd, IDC_CHECK_DING_AFTER_SUGGEST) ? true : false;
            EndDialog ( hwnd, IDOK );
            result = TRUE;
        }
        break;

        case IDCANCEL:
        {
            EndDialog ( hwnd, IDCANCEL );
            result = TRUE;
        }
        break;
        }
    }
    break;
    }

    return result;
}


static bool SuggestMove (
    ChessBoard  &board,
    int         &source,
    int         &dest,
    bool        promptForSettings )
{
    if (Global_UI)
    {
        if (Global_GameOverFlag)
        {
            MessageBox(HwndMain, "Cannot suggest a move because the game is over.", "Game is over.", MB_OK);
            return false;
        }

        INT_PTR choice = IDOK;

        if (promptForSettings)
        {
            choice = DialogBox (
                global_hInstance,
                MAKEINTRESOURCE(IDD_REQUEST_SUGGESTION),
                HwndMain,
                DLGPROC(SuggestMove_DlgProc));
        }

        if (choice == IDOK)
        {
            // Cancel any pondering that might be in progress.
            // We don't want two computer players thinking at the same time!
            Global_UI->oppTime_abortSearch();

            INT32 timeSpent = 0;
            Move move;
            ComputerChessPlayer thinker (*Global_UI);
            thinker.SetTimeLimit ( SuggestThinkTime );
            thinker.setEnableMoveDisplay (false);
            bool saveMateAnnounce = Global_UI->allowMateAnnounce (false);

            Global_SuggestMoveThinker = &thinker;           // so user can force the thinker to stop thinking early
            thinker.GetMove ( board, move, timeSpent );

            // We use a critical section to avoid a race condition where
            // the user forces the thinker to stop thinking, but the pointer changes to NULL
            // right before the call to Global_SuggestMoveThinker->AbortSearch().
            EnterCriticalSection (&SuggestMoveCriticalSection);
            Global_SuggestMoveThinker = 0;                  // do not let the global code access this local variable any more
            LeaveCriticalSection (&SuggestMoveCriticalSection);

            Global_UI->allowMateAnnounce (saveMateAnnounce);

            char moveString [1 + MAX_MOVE_STRLEN];
            FormatChessMove ( board, move, moveString );

            if (Global_DingAfterSuggest)
            {
                // Make an attention-getting sound.
                // Play it asynchronously so that it doesn't pause the thread.
                // http://msdn.microsoft.com/en-us/library/ms712879.aspx
                PlaySound((LPCSTR)SND_ALIAS_SYSTEMASTERISK, NULL, SND_ALIAS_ID | SND_ASYNC);
            }

            if (Global_SuggestThinkOnly)
            {
                return false;
            }

            char forced_mate [64];
            int mate_in_moves = 0;

            if (board.WhiteToMove())
            {
                if (move.score >= WON_FOR_WHITE)
                {
                    if (move.score < WHITE_WINS - WIN_DELAY_PENALTY)
                    {
                        mate_in_moves = 1 + ((WHITE_WINS - int(move.score)) / WIN_DELAY_PENALTY) / 2;
                    }
                }
            }
            else
            {
                if (move.score <= WON_FOR_BLACK)
                {
                    if (move.score > BLACK_WINS + WIN_DELAY_PENALTY)
                    {
                        mate_in_moves = 1 + ((int(move.score) - BLACK_WINS) / WIN_DELAY_PENALTY) / 2;
                    }
                }
            }

            if (mate_in_moves)
            {
                sprintf (forced_mate, "  [Mate in %d]", mate_in_moves);
            }
            else
            {
                forced_mate[0] = '\0';
            }

            char buffer [256];

            sprintf (
                buffer,
                "After thinking for %0.2lf seconds,\n"
                "the computer suggests the following move:\n\n"
                "     %s%s\n\n"
                "Do you want the computer to make this move for you?",
                (double(timeSpent) / 100.0),
                moveString,
                forced_mate
            );

            int makeMoveChoice = MessageBox (
                HwndMain,
                buffer,
                CHESS_PROGRAM_NAME,
                MB_ICONQUESTION | MB_YESNO
            );

            if (makeMoveChoice == IDYES)
            {
                Global_UI->DisplayMove ( board, move );
                source = move.source;
                dest = move.dest;
                return true;
            }
        }
    }

    return false;
}


const char *GetPlayerString (DefPlayerInfo::Type type)
{
    switch (type)
    {
    case DefPlayerInfo::computerPlayer:     return "Computer Player (Chenard)";
    case DefPlayerInfo::humanPlayer:        return "Human Player";
    case DefPlayerInfo::internetPlayer:     return "Remote/Internet Player";
    case DefPlayerInfo::namedPipePlayer:    return "Named Pipe Server";
    default:                                return "?";
    }
}


const char *GetWhitePlayerString ()
{
    const char *WhitePlayerString;
    if (Global_UI)
    {
        WhitePlayerString = GetPlayerString (Global_UI->queryWhitePlayerType());
    }
    else
    {
        WhitePlayerString = "?";
    }
    return WhitePlayerString;
}


const char *GetBlackPlayerString()
{
    const char *BlackPlayerString;
    if (Global_UI)
    {
        BlackPlayerString = GetPlayerString (Global_UI->queryBlackPlayerType());
    }
    else
    {
        BlackPlayerString = "?";
    }
    return BlackPlayerString;
}


bool ProcessChessCommands ( ChessBoard &board, int &source, int &dest )
{
    char msg [1024];
    bool whiteMove = board.WhiteToMove();
    Move edit;
    edit.score = 0;

    bool itIsTimeToCruise = false;
    if ( Global_TacticalBenchmarkFlag && Global_UI )
    {
        const char *explainText =
            "The tactical benchmark will run a series of pre-programmed chess positions "
            "to determine how quickly your computer can perform chess calculations. "
            "After the benchmark is finished, the current chess position will be restored "
            "and you will be able to resume your game. "
            "However, this test may take a long time (4 minutes on a 100 MHz Pentium).\n\n"
            "Do you really want to run the tactical benchmark?";

        Global_TacticalBenchmarkFlag = false;
        int choice = MessageBox ( HwndMain,
                                  explainText,
                                  CHESS_PROGRAM_NAME,
                                  MB_ICONQUESTION | MB_YESNO );

        if ( choice == IDYES )
        {
            double timeElapsed = Global_UI->runTacticalBenchmark();
            if ( timeElapsed > 0.0 )
            {
                const char *resultFormat =
                    "Benchmark completed in %0.2lf seconds\n\n"
                    "Your computer is %0.2lf times as fast as the author's 100 MHz Pentium."
#ifdef CHENARD_PROFILER
                    "\n\ntime data: {%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d}\n"
                    "call data: {%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d}\n\n"
#endif

#if REQUEST_BENCHMARK_DATA
                    "\n\nIf you feel like it, send email to me <cosinekitty@hotmail.com> with your computer's "
                    "benchmark time and your processor type (e.g. 486, Pentium) and speed (MHz). "
                    "I want to start collecting these statistics to publish them on the Chenard web page:\n\n"
                    "http://www.cosinekitty.com/chenard.html"
#endif
                    ;

                sprintf ( msg, resultFormat,
                          timeElapsed,
                          290.0 / timeElapsed
#ifdef CHENARD_PROFILER
                          , ProfilerHitCount[0],
                          ProfilerHitCount[1],
                          ProfilerHitCount[2],
                          ProfilerHitCount[3],
                          ProfilerHitCount[4],
                          ProfilerHitCount[5],
                          ProfilerHitCount[6],
                          ProfilerHitCount[7],
                          ProfilerHitCount[8],
                          ProfilerHitCount[9],

                          ProfilerCallCount[0],
                          ProfilerCallCount[1],
                          ProfilerCallCount[2],
                          ProfilerCallCount[3],
                          ProfilerCallCount[4],
                          ProfilerCallCount[5],
                          ProfilerCallCount[6],
                          ProfilerCallCount[7],
                          ProfilerCallCount[8],
                          ProfilerCallCount[9]
#endif
                        );

                MessageBox ( HwndMain, msg, CHESS_PROGRAM_NAME, MB_OK );
                TheBoardDisplayBuffer.update (board);
                TheBoardDisplayBuffer.freshenBoard();
            }
        }
    }
    else if ( Global_SuggestMoveFlag )
    {
        Global_SuggestMoveFlag = false;
        itIsTimeToCruise = SuggestMove ( board, source, dest, true );
    }
    else if ( Global_RepeatSuggestMoveFlag )
    {
        Global_RepeatSuggestMoveFlag = false;
        itIsTimeToCruise = SuggestMove ( board, source, dest, false );
    }
    else if ( Global_BoardEditRequest.getState() != BER_NOTHING )
    {
        if ( Global_GameOverFlag )
        {
            MessageBox ( HwndMain,
                         "Please start a new game (File | New) before editing the board",
                         CHESS_PROGRAM_NAME,
                         MB_ICONEXCLAMATION | MB_OK );
        }
        else
        {
            if (Global_BoardEditRequest.getState() == BER_FEN)
            {
                // Set entire board to the specified Forsyth Edwards Notation.

                const char *fen = Global_BoardEditRequest.getFen();
                if (board.SetForsythEdwardsNotation (fen))
                {
                    TheBoardDisplayBuffer.update (board);
                    // We need to make a NULL move so that legal move list gets regenerated,
                    // *and* the correct player will move,
                    // *and* we will detect end-of-game properly.

                    source = 0;
                    dest = SPECIAL_MOVE_NULL;
                    Global_GameModifiedFlag = true;
                    itIsTimeToCruise = true;
                }
                else
                {
                    // There was something wrong with the FEN string we were given.
                    MessageBox (
                        HwndMain,
                        "The Forsyth Edwards Notation was not valid.",
                        CHESS_PROGRAM_NAME,
                        MB_ICONEXCLAMATION | MB_OK
                    );
                }
            }
            else if (Global_BoardEditRequest.getState() == BER_SQUARE)
            {
                // Change the contents of a single square.

                SQUARE square;
                int    x;
                int    y;

                Global_BoardEditRequest.getSingleSquareEdit (x, y, square);

                SQUARE prev = board.GetSquareContents (x, y);
                board.SetSquareContents ( square, x, y );
                if (board.PositionIsPossible())
                {
                    edit.dest = dest = SPECIAL_MOVE_EDIT | ConvertSquareToNybble(square);
                    edit.source = source = OFFSET(x+2,y+2);
                    board.SaveSpecialMove ( edit );
                    Global_GameModifiedFlag = true;
                    itIsTimeToCruise = true;
                }
                else
                {
                    board.SetSquareContents (prev, x, y);

                    MessageBox (
                        HwndMain,
                        "Doing that would create an impossible position.",
                        CHESS_PROGRAM_NAME,
                        MB_ICONEXCLAMATION | MB_OK
                    );
                }
            }
        }

        Global_BoardEditRequest.reset();
    }
    else if ( Global_ClearBoardFlag )
    {
        if ( Global_GameOverFlag )
        {
            MessageBox ( HwndMain,
                         "Please start a new game (File | New) before clearing the board",
                         CHESS_PROGRAM_NAME,
                         MB_ICONEXCLAMATION | MB_OK );
        }
        else
        {
            int choice = MessageBox
                         ( HwndMain,
                           "Do you really want to clear the board?",
                           CHESS_PROGRAM_NAME,
                           MB_YESNO | MB_ICONQUESTION | MB_APPLMODAL );

            if ( choice == IDYES )
            {
                board.ClearEverythingButKings();
                itIsTimeToCruise = true;
                edit.source = source = 0;        // signal to clear everything!
                edit.dest = dest = SPECIAL_MOVE_EDIT;
                board.SaveSpecialMove ( edit );
            }
        }

        Global_ClearBoardFlag = false;
    }
    else if ( Global_ResetGameFlag )
    {
        Global_ResetGameFlag = false;

        // Recycle the current side settings (human/computer as White/Black).
        extern bool PreDefinedPlayerSides;
        extern bool PreDefinedWhiteHuman;
        extern bool PreDefinedBlackHuman;

        PreDefinedPlayerSides = true;
        PreDefinedWhiteHuman = (DefPlayer.whiteType == DefPlayerInfo::humanPlayer);
        PreDefinedBlackHuman = (DefPlayer.blackType == DefPlayerInfo::humanPlayer);

        // Present the Create Players dialog box again.
        if (DefinePlayerDialog(HwndMain))
        {
            board.Init();
            itIsTimeToCruise = true;
            dest = SPECIAL_MOVE_NULL;
            source = 0;
            Global_HaveFilename = false;
            Global_GameModifiedFlag = false;
            Global_StartNewGameConfirmed = true;    // tell ChessThreadFunc to loop back and create new game

            if ( Global_UI )
            {
                Global_UI->allowMateAnnounce (Global_EnableMateAnnounce);
            }
        }
    }
    else if ( Global_RedoMoveFlag )
    {
        Global_RedoMoveFlag = false;
        Global_MoveUndoPath.redo ( board );
        TheBoardDisplayBuffer.update ( board );
        Global_GameModifiedFlag = true;

        if ( Global_UI )
        {
            Global_UI->allowMateAnnounce (Global_EnableMateAnnounce);
            Global_UI->DrawBoard ( board );
        }

        if ( board.GameIsOver() )
        {
            dest = SPECIAL_MOVE_NULL;
            source = 0;
            itIsTimeToCruise = true;
        }
    }
    else if ( Global_UndoMoveFlag )
    {
        Global_UndoMoveFlag = false;
        Global_MoveUndoPath.undo ( board );
        TheBoardDisplayBuffer.update ( board );
        Global_GameModifiedFlag = true;

        if ( Global_UI )
        {
            Global_UI->allowMateAnnounce (Global_EnableMateAnnounce);
            Global_UI->DrawBoard ( board );
        }

        // The following lines were added to handle the special case
        // of undoing a move at end-of-game.
        itIsTimeToCruise = true;
        Global_GameOverFlag = false;
    }
    else if ( Global_GameSaveFlag )
    {
        Global_GameSaveFlag = false;
        bool known = false;
        sprintf ( msg, "Cannot save file '%s'", Global_GameFilename );     // get error message ready, just in case

        // Look at the filename extension to determine which kind of file to save:
        const char *ext = strrchr (Global_GameFilename, '.');
        if (ext)
        {
            FILE *outfile = NULL;
            if (0 == stricmp(ext,".pgn"))
            {
                known = true;
                outfile = fopen ( Global_GameFilename, "wt" );
                if (outfile != NULL)
                {
                    SavePortableGameNotation (outfile, board);
                }
            }
            else if (0 == stricmp(ext,".gam"))
            {
                known = true;
                outfile = fopen ( Global_GameFilename, "wb" );
                if (outfile != NULL)
                {
                    SaveGameToFile ( outfile, board );
                }
            }
            if (known)
            {
                if (outfile == NULL)
                {
                    MessageBox ( HwndMain, msg, "File Open Error", MB_OK | MB_ICONERROR );
                }
                else
                {
                    fclose (outfile);
                    outfile = NULL;
                    Global_HaveFilename = true;
                    Global_GameModifiedFlag = false;
                }
            }
            assert (outfile == NULL);
        }
        if (!known)
        {
            sprintf ( msg, "File '%s' has unknown extension", Global_GameFilename );
            MessageBox ( HwndMain, msg, "File Open Error", MB_OK | MB_ICONERROR );
        }
    }
    else if ( Global_GameOpenFlag )
    {
        Global_GameOpenFlag = false;
        FILE *infile = fopen ( Global_GameFilename, "rb" );
        if ( infile )
        {
            // Before going any further, cancel any pondering that might be in progress.
            if (Global_UI)
            {
                Global_UI->oppTime_abortSearch();
            }

            const char *ext = strrchr (Global_GameFilename, '.');
            if (0 == stricmp(ext,".pgn"))
            {
                PGN_FILE_STATE state = LoadGameFromPgnFile (infile, board);
                if (state != PGN_FILE_STATE_GAMEOVER)
                {
                    const char *error = GetPgnFileStateString (state);
                    sprintf (msg, "Problem loading pgn file '%s':\n%s", Global_GameFilename, error);
                    MessageBox (HwndMain, msg, "PGN Load Error", MB_OK | MB_ICONERROR);
                }
            }
            else
            {
                LoadGameFromFile ( infile, board );
            }
            Global_HaveFilename = true;

            UpdateGameStateText(board);     // updates title bar and clears game-over text in center of board.

            TheBoardDisplayBuffer.update (board);
            TheBoardDisplayBuffer.freshenBoard();
            Global_GameModifiedFlag = false;

            if ( Global_UI )
                Global_UI->allowMateAnnounce (Global_EnableMateAnnounce);

            // We need to make a NULL move so that legal move list gets regenerated,
            // *and* the correct player will move,
            // *and* we will detect end-of-game properly.

            source = 0;
            dest = SPECIAL_MOVE_NULL;
            itIsTimeToCruise = true;
        }
        else
        {
            sprintf ( msg, "Cannot open file '%s'", Global_GameFilename );
            MessageBox ( HwndMain, msg, "File Open Error", MB_OK | MB_ICONERROR );
        }
    }

    return itIsTimeToCruise;
}


BlunderAlertParms* ChessUI_win32_gui::blunderAlert_Initialize (const ChessBoard& board)
{
    BlunderAlertParms* blunderAlertParms = NULL;
    if (blunderAlert_EnabledForCurrentPlayer(board))
    {
        // Create a computer chess player that the blunder alert thread
        // will use to think about all the moves the human might make.
        ComputerChessPlayer* blunderAlertThinker = new ComputerChessPlayer(*this);

        if (blunderAlertThinker != NULL)
        {
            // Make the thinker appropriate for a background task.
            blunderAlertThinker->blunderAlert_SetInstanceFlag();    // avoids mate prediction announcements
            blunderAlertThinker->setEnableMoveDisplay(false);       // do not display the move the computer selects
            blunderAlertThinker->SetSearchBias(0);                  // fastest possible analysis, no randomness
            blunderAlertThinker->SetOpeningBookEnable(false);       // force tactical analysis every time (no opening book)
            blunderAlertThinker->SetTrainingEnable(false);          // force tactical analysis every time (no experience)
            blunderAlertThinker->setExtendedSearchFlag(false);      // always search exactly as long as told
            blunderAlertThinker->setResignFlag(false);              // do not let the thinker resign

            blunderAlertParms = new BlunderAlertParms(&board, blunderAlertThinker, Global_BlunderAlertThreshold);

            if (blunderAlertParms != NULL)
            {
                // Create a new thread to process the blunder alert analysis.
                // IMPORTANT: _beginthread may return an invalid handle even
                // though the thread was started properly, because blunder alert
                // thread might exit very quickly.
                uintptr_t threadHandle = _beginthread(BlunderAlertThreadFunc, 0, blunderAlertParms);
                if (threadHandle == INVALID_THREAD)
                {
                    // If the thread actually did run, we can tell for sure by looking at
                    // blunderAlertParms for the flag saying it started and finished.
                    if (!blunderAlertParms->isStarted)
                    {
                        // Assume there really was a failure to start the thread.
                        delete blunderAlertParms;
                        blunderAlertParms = NULL;
                    }
                }

                if (blunderAlertParms != NULL)
                {
                    // Getting here means we believe we have started the thread successfully.
                    // Wait until it tells us that we can safely proceed.
                    // Otherwise we risk corrupting the chess board before
                    // the child thread has finished making a safe copy of it.
                    while (!blunderAlertParms->isStarted)
                    {
                        Sleep(10);
                    }
                }
            }
            else
            {
                // Memory failure allocating parameters!
                // Avoid leaking the thinker...
                delete blunderAlertThinker;
            }
        }
    }

    return blunderAlertParms;
}


void ChessUI_win32_gui::blunderAlert_StopThinking (BlunderAlertParms* parms)
{
    if (parms)      // will be NULL if no blunder alert was started in the first place
    {
        // Stop the search immediately; there are too many special cases
        // where we need to respond very quickly to user input
        // to risk any noticeable delay here.
        parms->hurry = true;        // must set BEFORE aborting search!

        // Now we must sit back and wait for the child thread to finish.
        while (!parms->isFinished)
        {
            // There are theoretical cases where a race condition
            // can cause AbortSearch to be ignored: we can abort
            // right before ComputerChessPlayer::GetMove clears the abort flag,
            // so the child thread never sees the abort.
            // So we keep aborting every time around the loop.
            // (knock knock knock PENNY, knock knock knock PENNY, knock knock knock PENNY...)
            parms->thinker->AbortSearch();

            // Avoid burning up CPU time...
            Sleep(10);
        }
    }
}


bool ChessUI_win32_gui::blunderAlert_IsAnalysisSufficient(const BlunderAlertParms* parms) const
{
    if (parms != NULL)   // make sure blunder alert analysis actually happened
    {
        if (parms->levelCompleted >= parms->minLevel)
        {
            // Sanity check: make sure each legal move has been evaluated.
            for (int i=0; i < parms->numLegalMoves; ++i)
            {
                assert(parms->path[i].depth > 0);
            }

            return true;
        }
    }

    return false;
}


bool ChessUI_win32_gui::blunderAlert_IsLegalMove(
    const BlunderAlertParms* parms,
    const ChessBoard& board,
    int source,
    int dest)
{
    // The goal here is to return false only when we are SURE the move is illegal.
    // Return true if it looks legal or we can't tell what's going on because the board has changed.

    if (parms != NULL)
    {
        if (parms->packedBoard == board)     // make sure analysis corresponds to actual position
        {
            // Iterate through all the known legal moves.
            // If we find the same source/dest pair, we know the move is OK.
            for (int i=0; i < parms->numLegalMoves; ++i)
            {
                Move move = parms->path[i].m[0];
                int moveSource;
                int moveDest;
                move.actualOffsets(board, moveSource, moveDest);
                if ((source == moveSource) && (dest == moveDest))
                {
                    return true;     // valid pair of squares was clicked
                }
            }

            return false;   // the user definitely attempted an illegal move
        }
    }

    return true;
}


bool ChessUI_win32_gui::blunderAlert_LooksLikeBlunder(
    BlunderAlertParms* parms,
    ChessBoard& board,
    int source,
    int dest)
{
    if (blunderAlert_IsAnalysisSufficient(parms))
    {
        if (parms->packedBoard == board)     // make sure analysis corresponds to actual position
        {
            if (parms->numLegalMoves < 2)
            {
                // I don't think this can happen, but just in case,
                // there is no such thing as a blunder when there are not at least 2 choices.
                return false;
            }

            // Check to see if this move is significantly below the ideal score.
            // Annoying special case: if the selected move is
            // a pawn promotion, we don't know yet what piece the user wants to promote to,
            // because that UI callback hasn't happened yet.
            // So we do something a little cheesy: find the best score of any
            // move whose (source,dest) pair matches.

            int bestMoveIndex = 0;      // first move is best move (so far)
            int matchIndex = -1;        // no match at all (yet)

            for (int i=0; i < parms->numLegalMoves; ++i)
            {
                assert(parms->path[i].depth > 0);   // otherwise we didn't actually expore this move!

                // Each time through this loop, we do two things:
                // 1. Update our concept of the best move, based on scores.

                Move move = parms->path[i].m[0];
                SCORE excess = move.score - parms->path[bestMoveIndex].m[0].score;
                if (board.BlackToMove())
                {
                    // Toggle polarity for Black: negative is good!
                    excess = -excess;
                }
                if (excess > 0)
                {
                    bestMoveIndex = i;
                }

                // 2. Search for moves that matched the squares the user clicked on.
                int moveSource;
                int moveDest;
                move.actualOffsets(board, moveSource, moveDest);
                if ((source == moveSource) && (dest == moveDest))
                {
                    // We found the (or one of the) matching moves.
                    if (matchIndex < 0)
                    {
                        // This is the first matching move we have found.
                        matchIndex = i;
                    }
                    else
                    {
                        // This must be a pawn promotion, because how else
                        // could there be more than one matching legal move?
                        assert(move.isPawnPromotion());

                        excess = move.score - parms->path[matchIndex].m[0].score;
                        if (board.BlackToMove())
                        {
                            // Toggle polarity for Black: negative is good!
                            excess = -excess;
                        }
                        if (excess > 0)
                        {
                            matchIndex = i;
                        }
                    }
                }
            }  // for each legal move

            // When we get here, we know the best move (according to analysis)
            // and we know which move the user chose.
            if (matchIndex < 0)
            {
                assert(false);  // we could not find the user's chosen move!
            }
            else
            {
                Move bestMove  = parms->path[bestMoveIndex].m[0];
                Move humanMove = parms->path[matchIndex].m[0];
                SCORE deficit = bestMove.score - humanMove.score;
                if (board.BlackToMove())
                {
                    // Toggle polarity for Black's moves.
                    deficit = -deficit;
                }
                assert(deficit >= 0);   // how can human move be better than the "best" move?
                if (deficit >= parms->blunderScoreThreshold)
                {
                    // This move looks like a blunder!
                    blunderAlert_WarnUser(parms, board, matchIndex, bestMoveIndex);
                    return true;
                }
            }
        }
    }

    return false;
}

void ChessUI_win32_gui::blunderAlert_FormatContinuation(
    const BlunderAlertParms* parms,
    ChessBoard& board,
    int moveIndex,
    const char* prefixText,
    char* text,
    int maxTextLength)
{
    memset(text, '\0', 1+maxTextLength);
    int length = 0;

    if (!parms || moveIndex < 0 || moveIndex >= parms->numLegalMoves)
    {
        assert(false);      // this function should not have been called!
        return;
    }

    const BestPath& path = parms->path[moveIndex];
    if (path.depth < 1)
    {
        assert(false);      // it does not look like analysis was completed here!
        return;
    }

#define SafeAppendToken(s)                          \
    do {                                             \
        const char *string = (s);                     \
        const int tokenLength = (int)strlen(string);   \
        if (tokenLength + length > maxTextLength)       \
        {                                                \
            goto rewind_all_moves;                        \
        }                                                  \
        strcpy (&text[length], string);                     \
        length += tokenLength;                               \
    } while(false)

    // Prefix the continuation with some explanatory text (blunder, better).
    SafeAppendToken(prefixText);

    // Isolate local variables in scope block...
    {
        // Include the evaluation score so the user
        // has a clue how to tune the blunder threshold.
        int score = path.m[0].score;
        if (board.BlackToMove())
        {
            // Toggle dispalyed score to be relative to current player.
            score = -score;
        }
        char scoreText[20];
        sprintf(scoreText, " (%d): ", score);
        SafeAppendToken(scoreText);
    }

    // Loop for each move in the path.
    UnmoveInfo unmove[MAX_BESTPATH_DEPTH];
    int d = 0;
    while (d < path.depth)
    {
        Move move = path.m[d];  // make safe copy so MakeMove can modify it

        // FormatChessMoves will have to generate legal move list anyway
        // if we call the overloaded version of it that has no legal move
        // list parameter.  We might as well do so here and be paranoid
        // about verifying the contents of 'path'.
        MoveList legalMoves;
        board.GenMoves(legalMoves);

        // Avoid any chance of crashing due to an illegal move.
        // For example, putting one's own king in check can
        // lead to a ChessFatal when that king gets captured.
        // (There are also other ways the board could get corrupted.)
        if (!legalMoves.IsLegal(move))
        {
            assert(false);              // how did an illegal move get in here?
            goto rewind_all_moves;      // bail out - better to have a truncated string than crash
        }

        char number[20];
        sprintf(number, "%d. ", board.GetCurrentPlyNumber()/2 + 1);

        if (board.WhiteToMove())
        {
            if (d > 0)
            {
                SafeAppendToken(" ");
            }
            SafeAppendToken(number);
        }
        else
        {
            // Black's turn.
            if (d == 0)
            {
                // Prefix with move number only if analysis starts with Black's move.
                SafeAppendToken(number);
                SafeAppendToken("... ");
            }
            else
            {
                // Separate White and Black moves with a space.
                SafeAppendToken(" ");
            }
        }

        // Append the PGN notation for the move itself
        char movestr[MAX_MOVE_STRLEN + 1];
        FormatChessMove(board, legalMoves, move, movestr);
        SafeAppendToken(movestr);

        // Make the move on the board so we can format the next move correctly.
        board.MakeMove(move, unmove[d++]);
    }

rewind_all_moves:
    // Before leaving, we must put the chess board back in the same state it was.
    // Unmake all the moves we made formatting the continuation.
    while(d > 0)
    {
        --d;
        board.UnmakeMove(path.m[d], unmove[d]);
    }

#undef SafeAppendToken
}


void ChessUI_win32_gui::blunderAlert_WarnUser(
    BlunderAlertParms* parms,
    ChessBoard& board,
    int humanMoveIndex,
    int bestMoveIndex)
{
    // Write text at the bottom of the chess window:
    // First, make very conspicuous text that this looks like a blunder.
    ChessDisplayTextBuffer::SetText(STATIC_ID_GAME_RESULT, "Blunder?");

    // Set a flag that indicates that the next mouse click to come along
    // should clear the big red "Blunder?" in the middle of the board.
    Global_TransientCenteredMessage = true;

    // Show analysis of user's selected move.
    const int TEXT_LENGTH = 80;
    char text[1+TEXT_LENGTH];

    blunderAlert_FormatContinuation(parms, board, humanMoveIndex, "Blunder", text, TEXT_LENGTH);
    ChessDisplayTextBuffer::SetText(STATIC_ID_BLUNDER_ANALYSIS, text);

    // Show analysis of "better" move.
    blunderAlert_FormatContinuation(parms, board, bestMoveIndex, "Better", text, TEXT_LENGTH);
    ChessDisplayTextBuffer::SetText(STATIC_ID_BETTER_ANALYSIS, text);

    // Make an attention-getting sound.
    // Play it asynchronously so that it doesn't pause the thread.
    // http://msdn.microsoft.com/en-us/library/ms712879.aspx
    PlaySound((LPCSTR)SND_ALIAS_SYSTEMEXCLAMATION, NULL, SND_ALIAS_ID | SND_ASYNC);
}


void ChessUI_win32_gui::blunderAlert_ClearWarning()
{
    // Erase any prior warning text related to blunder alert.
    ChessDisplayTextBuffer::SetText(STATIC_ID_GAME_RESULT, "");
    ChessDisplayTextBuffer::SetText(STATIC_ID_BLUNDER_ANALYSIS, "");
    ChessDisplayTextBuffer::SetText(STATIC_ID_BETTER_ANALYSIS, "");
}



bool ChessUI_win32_gui::ReadMove (
    ChessBoard &board,
    int        &source,
    int        &dest,
    SQUARE     &promIndex )
{
    promIndex = 0;      // indicates that this ChessUI implementation does NOT send back promoted piece, but requires a separate callback

    // Let the client window know we are starting to read
    // a human player's move...
    SendMessage(hwnd, WM_DDC_ENTER_HUMAN_MOVE, 0, 0);

    BlunderAlertParms* blunderAlertParms = blunderAlert_Initialize(board);

try_another_move:
    TheBoardDisplayBuffer.startReadingMove(board.WhiteToMove());
    bool moveWasRead = false;
    while (TheBoardDisplayBuffer.isReadingMove())
    {
        Sleep(100);

        if (Global_AbortFlag)
        {
            break;   // simulate resignation to abort
        }

        if (Global_UserResign)
        {
            Global_UserResign = false;
            break;
        }

        if (ProcessChessCommands(board, source, dest))
        {
            SendMessage(hwnd, WM_DDC_LEAVE_HUMAN_MOVE, 0, 0);
            if (Global_StartNewGameConfirmed)
            {
                // Special case: the user is starting a new game, so simulate
                // a resignation to kick ChessGame::Play out of the current game.
                ChessPlayer* currentPlayer = board.WhiteToMove() ? whitePlayer : blackPlayer;
                if (currentPlayer)
                {
                    // But set the quit reason for the current player to a special code
                    // to indicate that there should not be a big red "[White|Black] Resigned"
                    // in the middle of the board!
                    currentPlayer->SetQuitReason(qgr_startNewGame);
                }
                break;
            }
            moveWasRead = true;     // special case: a "pseudo-move" has been read (edit command, etc.)
            break;
        }
    }

    // Did the user click a pair of squares?
    // Even if the pair of squares is an illegal move,
    // the following will be set to true.
    // It will be false if the user action was a pseudo-move,
    // e.g. load PGN file, reset game, edit square, etc.
    const bool attemptedPieceMove = !moveWasRead & !TheBoardDisplayBuffer.isReadingMove();

    if (attemptedPieceMove)
    {
        // Avoid canceling blunder alert if the user didn't even make a valid move.
        // Note that in the case of pawn promotion, we don't know the promotion piece yet.
        // (That is handled by a separate callback later.)
        // So really we are asking: are source and dest squares consistent with any legal move?
        TheBoardDisplayBuffer.copyMove(source, dest);
        if (!blunderAlert_IsLegalMove(blunderAlertParms, board, source, dest))
        {
            goto try_another_move;
        }
    }

    // Force blunder alert analysis (if in progress) to stop IMMEDIATELY!
    blunderAlert_StopThinking(blunderAlertParms);

    if (attemptedPieceMove)
    {
        // Getting here means the user actually clicked a pair of squares.
        // It does NOT mean the move is legal!
        // Also, if this is a pawn promotion, the user has not yet selected
        // which piece to promote it to.

        // Warn the user at most once per turn.
        if (blunderAlertParms && !blunderAlertParms->wasUserWarned)
        {
            if (blunderAlert_LooksLikeBlunder(blunderAlertParms, board, source, dest))
            {
                blunderAlertParms->wasUserWarned = true;
                goto try_another_move;
            }
        }

        blunderAlert_ClearWarning();

        moveWasRead = true;
        SendMessage(hwnd, WM_DDC_LEAVE_HUMAN_MOVE, 0, 0);
    }

    if (blunderAlertParms != NULL)
    {
        delete blunderAlertParms;
        blunderAlertParms = NULL;
    }

    return moveWasRead;
}


SQUARE ChessUI_win32_gui::PromotePawn ( int, ChessSide side )
{
    static volatile SQUARE prom;
    prom = EMPTY;
    PostMessage ( hwnd, WM_DDC_PROMOTE_PAWN, WPARAM(side), LPARAM(&prom) );

    while ( prom == EMPTY )
        Sleep ( 100 );

    return prom;
}


void ChessUI_win32_gui::DrawBoard ( const ChessBoard &board )
{
    TheBoardDisplayBuffer.update ( board );
    TheBoardDisplayBuffer.deselectSquare();
    TheBoardDisplayBuffer.freshenBoard();

    if ( !Global_GameOverFlag )
    {
        ::SetWindowText ( hwnd,
                          board.WhiteToMove() ?
                          TitleBarText_WhitesTurn :
                          TitleBarText_BlacksTurn );
    }

    extern bool Global_EnableBoardBreathe;
    if ( Global_EnableBoardBreathe )
        Sleep ( 100 );   // Helps the board to "breathe"
}


void ChessUI_win32_gui::Resign ( ChessSide iGiveUp, QuitGameReason reason )
{
    static gameReport report;
    report.winner = (iGiveUp == SIDE_WHITE) ? SIDE_BLACK : SIDE_WHITE;
    report.resignation = true;
    report.quitReason = reason;
    PostMessage ( hwnd, WM_DDC_GAME_RESULT, WPARAM(0), LPARAM(&report) );
}


void ChessUI_win32_gui::ReportEndOfGame ( ChessSide winner )
{
    static gameReport report;
    report.winner = winner;
    report.resignation = false;
    PostMessage ( hwnd, WM_DDC_GAME_RESULT, WPARAM(0), LPARAM(&report) );
}


void ChessUI_win32_gui::PredictMate ( int n )
{
    if ( !enableMateAnnounce )
        return;

    Sleep ( 200 );
    static volatile int messageProcessed;
    messageProcessed = 0;

    PostMessage (
        hwnd,
        WM_DDC_PREDICT_MATE,
        WPARAM(n),
        LPARAM(&messageProcessed) );

    while ( !messageProcessed )
        Sleep ( 200 );

    enableMateAnnounce = false;   // don't annoy the user further (once is enough!)
}


static void FlashSquare ( int ofs, SQUARE before, SQUARE after )
{
    const int x = XPART(ofs) - 2;
    const int y = YPART(ofs) - 2;

    TheBoardDisplayBuffer.setSquareContents ( x, y, after );
    TheBoardDisplayBuffer.freshenSquare ( x, y );

    for ( int i=0; i < 2; i++ )
    {
        Sleep ( 125 );
        TheBoardDisplayBuffer.setSquareContents ( x, y, before );
        TheBoardDisplayBuffer.freshenSquare ( x, y );
        Sleep ( 125 );
        TheBoardDisplayBuffer.setSquareContents ( x, y, after );
        TheBoardDisplayBuffer.freshenSquare ( x, y );
    }
}


void ChessUI_win32_gui::DisplayMove ( ChessBoard &board, Move move )
{
    if ( tacticalBenchmarkMode )
        return;     // keep move display from messing up our timing

    ChessBeep ( 1400, 50 );

    extern bool Global_AnimateMoves;
    if ( Global_AnimateMoves )
    {
        int ofs1, ofs2;
        move.actualOffsets ( board, ofs1, ofs2 );

        // Show it being picked up...
        FlashSquare ( ofs1, board.GetSquareContents(ofs1), EMPTY );

        // Temporarily make move in board to show it being put down...
        UnmoveInfo unmove;
        board.MakeMove ( move, unmove );
        TheBoardDisplayBuffer.update ( board );
        TheBoardDisplayBuffer.freshenSquare ( XPART(ofs1)-2, YPART(ofs1)-2 );
        FlashSquare ( ofs2, EMPTY, board.GetSquareContents(ofs2) );

        // Undo the move because it's done officially by the ChessGame object.
        board.UnmakeMove ( move, unmove );
    }
}


const unsigned MaxVectorDepth = 3;


void ChessUI_win32_gui::DebugPly ( int depth, ChessBoard &board, Move move )
{
    static int counter = 0;
    static ChessBoard TopBoard, TempBoard;              // FIXFIXFIX:  NOT THREAD SAFE!!!
    static Move CurrentPath [MAX_BESTPATH_DISPLAY];     // FIXFIXFIX:  NOT THREAD SAFE!!!

    if ( depth >= MAX_BESTPATH_DISPLAY )
        return;

    CurrentPath[depth] = move;
    if ( depth == 0 )
    {
        // The parameter 'board' contains the top-level position in the search.
        // Copy it into TempBoard so we have it whenever we need to display
        // move notation for the current path.
        TopBoard = board;
    }

    if ( Global_AnalysisType == 2 )
    {
        const int limit = 15000;    // FIXFIXFIX: CPU speed dependent!
        if ( ++counter > limit )
        {
            // Now it's time to refresh the current path display.

            TempBoard = TopBoard;

            int i;
            for ( i=0; i<=depth; i++ )
            {
                char moveString [64];
                UnmoveInfo unmove;
                FormatChessMove ( TempBoard, CurrentPath[i], moveString );
                TempBoard.MakeMove ( CurrentPath[i], unmove );
                ChessDisplayTextBuffer::SetText ( STATIC_ID_BESTPATH(i), moveString );
            }

            for ( ; i < MAX_BESTPATH_DISPLAY; i++ )
            {
                ChessDisplayTextBuffer::SetText ( STATIC_ID_BESTPATH(i), "" );
            }

            counter = 0;
        }
    }
}


void ChessUI_win32_gui::DebugExit ( int depth, ChessBoard &board, SCORE score )
{
}


void ChessUI_win32_gui::DisplayBestPath (
    const ChessBoard &_board,
    const BestPath &path )
{
    if ( Global_AnalysisType != 1 )
        return;

    // It should not be possible to get here without having
    // initialized the transposition tables, but why risk a crash?
    ComputerChessPlayer::LazyInitXposTable();

    // We change the board here, but we put it back the way we find it!
    ChessBoard &board = (ChessBoard &) _board;

    int n = path.depth;
    if ( n > MAX_BESTPATH_DISPLAY )
        n = MAX_BESTPATH_DISPLAY;

    UnmoveInfo unmove [MAX_BESTPATH_DISPLAY];
    bool foundBestMove = true;
    char moveString [64];
    int i, numMovesFound=0;
    Move move [MAX_BESTPATH_DISPLAY];
    for ( i=0; foundBestMove && i < MAX_BESTPATH_DISPLAY; )
    {
        bool xposFlag = false;
        if ( i > n )
        {
            // Try to find best move from transposition table
            const TranspositionEntry *xpos;
            if ( board.WhiteToMove() )
                xpos = ComputerChessPlayer::XposTable->locateWhiteMove(board);
            else
                xpos = ComputerChessPlayer::XposTable->locateBlackMove(board);

            if ( xpos )
            {
                if ( board.isLegal ( xpos->bestReply ) )
                {
                    move[i] = xpos->bestReply;
                    xposFlag = true;
                }
                else
                    foundBestMove = false;
            }
            else
                foundBestMove = false;
        }
        else
        {
            if ( board.isLegal(path.m[i]) )
                move[i] = path.m[i];
            else
                foundBestMove = false;
        }

        if ( foundBestMove )
        {
            FormatChessMove ( board, move[i], moveString );
            if ( i == 0 )
            {
                char temp [256];
                sprintf ( temp, "%-12s %6d", moveString, int(move[i].score) );
                strcpy ( moveString, temp );
            }

            if ( xposFlag )
                strcat ( moveString, "." );

            int numReps = 0;
            bool drawFlag = board.IsDefiniteDraw (&numReps);
            if ( drawFlag )
            {
                if ( numReps >= 3 )
                    strcat ( moveString, " rep" );
                else
                    strcat ( moveString, " draw" );
            }

            board.MakeMove ( move[i], unmove[i] );
            ChessDisplayTextBuffer::SetText ( STATIC_ID_BESTPATH(i), moveString );
            ++i;

            if ( drawFlag )
                break;
        }
    }

    for ( numMovesFound = i--; i >= 0; --i )
        board.UnmakeMove ( move[i], unmove[i] );

    for ( i = numMovesFound; i < MAX_BESTPATH_DISPLAY; ++i )
        ChessDisplayTextBuffer::SetText ( STATIC_ID_BESTPATH(i), "" );
}


void ChessUI_win32_gui::RecordMove (
    ChessBoard  &board,
    Move         move,
    INT32        thinkTime )
{
    int sourceOfs=0, destOfs=0;
    move.actualOffsets ( board, sourceOfs, destOfs );
    TheBoardDisplayBuffer.informMoveCoords (
        XPART(sourceOfs)-2, YPART(sourceOfs)-2,
        XPART(destOfs)-2,   YPART(destOfs)-2 );

    char moveString [MAX_MOVE_STRLEN + 1];
    char displayString [30 + MAX_MOVE_STRLEN + 1];

    FormatChessMove ( board, move, moveString );

    UnmoveInfo unmove;
    board.MakeMove ( move, unmove );
    Global_MoveUndoPath.resetFromBoard ( board );
    board.UnmakeMove ( move, unmove );

    if ( Global_SpeakMovesFlag )
    {
        Speak (moveString);
    }

    if (board.WhiteToMove())
    {
        sprintf ( displayString, "%d. %s: %s (%0.2lf)",
                  1 + board.GetCurrentPlyNumber()/2,
                  "White",
                  moveString,
                  double(thinkTime) / 100.0 );

        ChessDisplayTextBuffer::SetText(STATIC_ID_WHITES_MOVE, displayString);
    }
    else
    {
        sprintf ( displayString,
                  "%s: %s (%0.2lf)",
                  "Black",
                  moveString,
                  double(thinkTime) / 100.0 );

        ChessDisplayTextBuffer::SetText(STATIC_ID_BLACKS_MOVE, displayString);
    }

    // It will be the opposite side's turn to move very soon...
    UpdateGameStateText(!board.WhiteToMove());

    if ( Global_EnableOppTime != enableOppTime )
        enableOppTime = Global_EnableOppTime;

    Global_GameModifiedFlag = true;
}


void ChessUI_win32_gui::DisplayBestMoveSoFar (
    const ChessBoard &board,
    Move bestMove,
    int )
{
    if ( Global_AnalysisType == 2 )
    {
        char moveString [MAX_MOVE_STRLEN + 1];
        char displayString [20 + MAX_MOVE_STRLEN + 1];

        FormatChessMove ( board, bestMove, moveString );
        sprintf ( displayString,
                  "%6d [%s]",
                  int(bestMove.score),
                  moveString );

        ChessDisplayTextBuffer::SetText ( STATIC_ID_CURRENT_MOVE, displayString );
    }

    if ( tacticalBenchmarkMode && bestMove == benchmarkExpectedMove )
        benchmarkPlayer->AbortSearch();
}


void ChessUI_win32_gui::DisplayCurrentMove (
    const ChessBoard &board,
    Move move,
    int level )
{
    if ( Global_AnalysisType != 0 )
    {
        char moveString [MAX_MOVE_STRLEN + 1];
        char displayString [20 + MAX_MOVE_STRLEN + 1];

        FormatChessMove ( board, move, moveString );

        sprintf (
            displayString,
            "%2d: %6d [%s]",
            level,
            int ( move.score ),
            moveString );

        if ( Global_AnalysisType == 1 )
            ChessDisplayTextBuffer::SetText ( STATIC_ID_CURRENT_MOVE, displayString );
    }
}


void ChessUI_win32_gui::ReportSpecial ( const char *msg )
{
    ChessDisplayTextBuffer::SetText (
        STATIC_ID_CURRENT_MOVE,
        msg );
}


void ChessUI_win32_gui::ComputerIsThinking (
    bool                  entering,
    ComputerChessPlayer & thinker )
{
    if (thinker.blunderAlert_QueryInstanceFlag())
    {
        // Ignore blunder alert analysis entirely... (blinking down there distracts)
        return;
    }

    const char *displayString = "";
    if ( entering )
    {
        if ( thinker.oppTime_QueryInstanceFlag() )
        {
            displayString = "< pondering >";
        }
        else
        {
            displayString = "< thinking >";
        }
    }

    ChessDisplayTextBuffer::SetText(STATIC_ID_THINKING, displayString);
}


static void FormatDisplayedInteger (unsigned long x, const char *prefix, char *output)
{
    output += sprintf (output, "%s", prefix);

    if (x == 0)
    {
        sprintf (output, "0");
    }
    else
    {
        char r [80];
        int i = 0;
        int n = 0;
        while (x > 0)
        {
            ++n;
            r[i++] = '0' + (x % 10);
            x /= 10;
            if (n % 3 == 0 && x > 0)
            {
                r[i++] = ',';
            }
        }
        r[i] = '\0';
        strcpy (output, strrev(r));
    }
}


void ChessUI_win32_gui::ReportComputerStats (
    INT32   thinkTime,
    UINT32  /*nodesVisited*/,
    UINT32  nodesEvaluated,
    UINT32  /*nodesGenerated*/,
    int     /*fwSearchDepth*/,
    UINT32    nvis   [NODES_ARRAY_SIZE],
    UINT32  /*ngen*/ [NODES_ARRAY_SIZE] )
{
    char displayString [80];

    FormatDisplayedInteger (nodesEvaluated, "evaluated=", displayString);
    ChessDisplayTextBuffer::SetText ( STATIC_ID_NODES_EVALUATED, displayString );

    displayString[0] = '\0';     // fallback in case eval/sec cannot be displayed
    if (thinkTime > 5)
    {
        double nodesPerSecond = double(nodesEvaluated) * 100.0 / double(thinkTime);
        if (nodesPerSecond >= 0.0 && nodesPerSecond < 1.0e+12)
        {
            FormatDisplayedInteger ((unsigned long) nodesPerSecond, "eval/sec=", displayString);
        }
    }
    ChessDisplayTextBuffer::SetText ( STATIC_ID_NODES_EVALPERSEC, displayString );

    unsigned long elapsedSeconds = thinkTime / 100;
    unsigned long elapsedMinutes = elapsedSeconds / 60;
    elapsedSeconds %= 60;
    unsigned long elapsedHours = elapsedMinutes / 60;
    elapsedMinutes %= 60;
    sprintf (displayString, "time=%lu:%02lu:%02lu", elapsedHours, elapsedMinutes, elapsedSeconds);
    ChessDisplayTextBuffer::SetText (STATIC_ID_ELAPSED_TIME, displayString);

    if ( nvis )
    {
        int limit = MAX_BESTPATH_DISPLAY;
        if ( limit > NODES_ARRAY_SIZE )
            limit = NODES_ARRAY_SIZE;

        for ( int i=1; i < limit; ++i )
        {
            ChessDisplayTextBuffer *tb = ChessDisplayTextBuffer::Find (STATIC_ID_BESTPATH(i));

            if ( tb )
            {
                const char *moveString = tb->queryText();
                char buffer [64];
                int k;
                for ( k=0; moveString[k] && k<12; ++k )
                    buffer[k] = moveString[k];

                for ( ; k<13; ++k )
                    buffer[k] = ' ';

                sprintf ( buffer+k, "%9lu", (unsigned long)(nvis[i]) );
                tb->setText ( buffer );
            }
        }
    }

    extern bool Global_AllowResignFlag;
    if ( whitePlayerType == DefPlayerInfo::computerPlayer && whitePlayer )
        ((ComputerChessPlayer *)whitePlayer)->setResignFlag ( Global_AllowResignFlag );

    if ( blackPlayerType == DefPlayerInfo::computerPlayer && blackPlayer )
        ((ComputerChessPlayer *)blackPlayer)->setResignFlag ( Global_AllowResignFlag );
}


void ChessUI_win32_gui::forceMove()
{
    if ( whitePlayer && whitePlayerType == DefPlayerInfo::computerPlayer )
    {
        ((ComputerChessPlayer *)whitePlayer)->AbortSearch();
    }

    if ( blackPlayer && blackPlayerType == DefPlayerInfo::computerPlayer )
    {
        ((ComputerChessPlayer *)blackPlayer)->AbortSearch();
    }

    EnterCriticalSection (&SuggestMoveCriticalSection);     // critical section prevents Global_SuggestMoveThinker changing to NULL in other thread while we are using it.
    if (Global_SuggestMoveThinker)
    {
        Global_SuggestMoveThinker->AbortSearch();
    }
    LeaveCriticalSection (&SuggestMoveCriticalSection);
}


void ChessUI_win32_gui::setWhiteThinkTime ( INT32 hundredthsOfSeconds )
{
    if ( whitePlayerType == DefPlayerInfo::computerPlayer &&
            whitePlayer && hundredthsOfSeconds >= 10 )
    {
        ComputerChessPlayer *puter = (ComputerChessPlayer *)whitePlayer;
        puter->SetTimeLimit ( hundredthsOfSeconds );
    }
}


void ChessUI_win32_gui::setBlackThinkTime ( INT32 hundredthsOfSeconds )
{
    if ( blackPlayerType == DefPlayerInfo::computerPlayer &&
            blackPlayer && hundredthsOfSeconds >= 10 )
    {
        ComputerChessPlayer *puter = (ComputerChessPlayer *)blackPlayer;
        puter->SetTimeLimit ( hundredthsOfSeconds );
    }
}


INT32 ChessUI_win32_gui::queryWhiteThinkTime() const
{
    if ( whitePlayerType == DefPlayerInfo::computerPlayer && whitePlayer )
    {
        const ComputerChessPlayer *puter = (const ComputerChessPlayer *)whitePlayer;
        return puter->QueryTimeLimit();
    }

    return 0;
}


INT32 ChessUI_win32_gui::queryBlackThinkTime() const
{
    if ( blackPlayerType == DefPlayerInfo::computerPlayer && blackPlayer )
    {
        const ComputerChessPlayer *puter = (const ComputerChessPlayer *)blackPlayer;
        return puter->QueryTimeLimit();
    }

    return 0;
}


void ChessUI_win32_gui::NotifyUser ( const char *message )
{
    MessageBox ( HwndMain, message, CHESS_PROGRAM_NAME, MB_OK | MB_ICONINFORMATION );
}


//-------------------------- benchmark stuff ------------------------------


/*
   hardcode varok.gam varok.cpp VarokGame -U -x
*/

static unsigned long VarokGame[] =
{
    0x4129L,0x5B74L,0x3720L,0xFFFD586FL,0x3226L,0x244D65L,0x362AL,0xFFFD5C68L,
    0x291BL,0xFFED6570L,0x3F27L,0xFFE74A62L,0x4B3FL,0xFFED6258L,0x3E32L,
    0xFFD86873L,0x4028L,0xFFD5B072L,0x321CL,0xFFDD5864L,0x584BL,0xFFC75862L,
    0x331DL,0xFFCF7065L,0x4D40L,0xFFF54D5BL,0x4B1FL,0xFFFB5A70L,0x5037L,
    0xC6571L,0xB01EL,0x147173L,0x1B1AL,0x156F6EL
};


double ChessUI_win32_gui::runTacticalBenchmark()
{
    StartProfiler();
    tacticalBenchmarkMode = true;
    bool saveMateAnnounce = enableMateAnnounce;
    enableMateAnnounce = false;

    // Create a temporary chess board.
    ChessBoard tboard;

    // Create a temporary chess player to think about the position
    benchmarkPlayer = new ComputerChessPlayer (*this);

    INT32 timeSpent;
    double totalTime = 0.0;
    Move move;
    UnmoveInfo unmove;

    LoadGameFromHardcode (
        VarokGame, sizeof(VarokGame)/sizeof(VarokGame[0]), tboard );
    benchmarkExpectedMove.source = 0;
    benchmarkExpectedMove.dest = 0;
    TheBoardDisplayBuffer.update (tboard);
    TheBoardDisplayBuffer.freshenBoard();
    benchmarkPlayer->SetSearchDepth (4);
    benchmarkPlayer->ResetHistoryBuffers();
    benchmarkPlayer->GetMove ( tboard, move, timeSpent );
    totalTime += (0.01 * timeSpent);

    // Now just search a neutral but very complex midgame position to a fixed depth.
    // We do not expect a particular move; we just want to see how long a fixed depth takes.
    tboard.Init();
    tboard.ClearEverythingButKings();
    tboard.SetSquareContents ( WKING, 6, 0 );
    tboard.SetSquareContents ( BKING, 6, 7 );
    tboard.SetSquareContents ( WROOK, 0, 0 );
    tboard.SetSquareContents ( WROOK, 5, 0 );
    tboard.SetSquareContents ( WPAWN, 0, 1 );
    tboard.SetSquareContents ( WPAWN, 1, 1 );
    tboard.SetSquareContents ( WPAWN, 2, 1 );
    tboard.SetSquareContents ( WQUEEN, 4, 1 );
    tboard.SetSquareContents ( WPAWN, 5, 1 );
    tboard.SetSquareContents ( WPAWN, 6, 1 );
    tboard.SetSquareContents ( WPAWN, 7, 1 );
    tboard.SetSquareContents ( WBISHOP, 1, 2 );
    tboard.SetSquareContents ( WKNIGHT, 2, 2 );
    tboard.SetSquareContents ( WPAWN, 3, 2 );
    tboard.SetSquareContents ( WKNIGHT, 5, 2 );
    tboard.SetSquareContents ( WPAWN, 4, 3 );
    tboard.SetSquareContents ( BBISHOP, 2, 4 );
    tboard.SetSquareContents ( BPAWN, 4, 4 );
    tboard.SetSquareContents ( WBISHOP, 6, 4 );
    tboard.SetSquareContents ( BPAWN, 0, 5 );
    tboard.SetSquareContents ( BKNIGHT, 2, 5 );
    tboard.SetSquareContents ( BPAWN, 3, 5 );
    tboard.SetSquareContents ( BKNIGHT, 5, 5 );
    tboard.SetSquareContents ( BPAWN, 1, 6 );
    tboard.SetSquareContents ( BPAWN, 2, 6 );
    tboard.SetSquareContents ( BBISHOP, 3, 6 );
    tboard.SetSquareContents ( BPAWN, 5, 6 );
    tboard.SetSquareContents ( BPAWN, 6, 6 );
    tboard.SetSquareContents ( BPAWN, 7, 6 );
    tboard.SetSquareContents ( BROOK, 0, 7 );
    tboard.SetSquareContents ( BQUEEN, 3, 7 );
    tboard.SetSquareContents ( BROOK, 4, 7 );
    benchmarkExpectedMove.source = 0;
    benchmarkExpectedMove.dest = 0;
    TheBoardDisplayBuffer.update (tboard);
    TheBoardDisplayBuffer.freshenBoard();
    benchmarkPlayer->SetSearchDepth (4);
    benchmarkPlayer->ResetHistoryBuffers();
    benchmarkPlayer->GetMove ( tboard, move, timeSpent );
    totalTime += (0.01 * timeSpent);

    // Now do a little endgame position
    tboard.Init();
    tboard.ClearEverythingButKings();
    tboard.SetSquareContents ( WPAWN, 0, 1 );
    tboard.SetSquareContents ( WPAWN, 1, 1 );
    tboard.SetSquareContents ( WKING, 5, 3 );
    tboard.SetSquareContents ( WKNIGHT, 6, 3 );
    tboard.SetSquareContents ( BPAWN, 3, 4 );
    tboard.SetSquareContents ( WPAWN, 4, 4 );
    tboard.SetSquareContents ( BKING, 4, 5 );
    tboard.SetSquareContents ( BPAWN, 0, 6 );
    tboard.SetSquareContents ( BPAWN, 1, 6 );
    tboard.SetSquareContents ( BBISHOP, 3, 6 );
    TheBoardDisplayBuffer.update (tboard);
    TheBoardDisplayBuffer.freshenBoard();
    benchmarkExpectedMove.source = 0;
    benchmarkExpectedMove.dest = 0;
    benchmarkPlayer->SetSearchDepth (7);
    benchmarkPlayer->ResetHistoryBuffers();
    benchmarkPlayer->GetMove ( tboard, move, timeSpent );
    totalTime += (0.01 * timeSpent);

    // zugzwang test:  Smyslov v Kasparov, Vilnius 1984, game 12.
    tboard.Init();
    tboard.ClearEverythingButKings();
    tboard.SetSquareContents ( WKING, 7, 1 );
    tboard.SetSquareContents ( BKING, 6, 6 );
    tboard.SetSquareContents ( WBISHOP, 4, 1 );
    tboard.SetSquareContents ( WPAWN, 0, 2 );
    tboard.SetSquareContents ( WKNIGHT, 3, 2 );
    tboard.SetSquareContents ( BQUEEN, 4, 2 );
    tboard.SetSquareContents ( WPAWN, 6, 2 );
    tboard.SetSquareContents ( WPAWN, 1, 3 );
    tboard.SetSquareContents ( WPAWN, 4, 3 );
    tboard.SetSquareContents ( WQUEEN, 6, 3 );
    tboard.SetSquareContents ( BPAWN, 1, 4 );
    tboard.SetSquareContents ( BPAWN, 6, 4 );
    tboard.SetSquareContents ( WPAWN, 7, 4 );
    tboard.SetSquareContents ( BPAWN, 0, 5 );
    tboard.SetSquareContents ( WPAWN, 4, 5 );
    tboard.SetSquareContents ( BPAWN, 7, 5 );
    tboard.SetSquareContents ( BKNIGHT, 4, 6 );
    tboard.SetSquareContents ( BROOK, 5, 7 );
    move.source = OFFSET(9,3);
    move.dest = OFFSET(8,3);
    tboard.MakeMove ( move, unmove );
    benchmarkExpectedMove.source = 0;
    benchmarkExpectedMove.dest = 0;
    benchmarkPlayer->SetSearchDepth (4);
    TheBoardDisplayBuffer.update (tboard);
    TheBoardDisplayBuffer.freshenBoard();
    benchmarkPlayer->ResetHistoryBuffers();
    benchmarkPlayer->GetMove ( tboard, move, timeSpent );
    totalTime += (0.01 * timeSpent);

    // Late midgame, with highly mobile pieces
    tboard.Init();
    tboard.ClearEverythingButKings();
    tboard.SetSquareContents ( WKING, 6, 0 );
    tboard.SetSquareContents ( WROOK, 3, 0 );
    tboard.SetSquareContents ( WROOK, 4, 0 );
    tboard.SetSquareContents ( WPAWN, 0, 1 );
    tboard.SetSquareContents ( WPAWN, 1, 1 );
    tboard.SetSquareContents ( WBISHOP, 2, 1 );
    tboard.SetSquareContents ( WPAWN, 5, 1 );
    tboard.SetSquareContents ( WPAWN, 6, 1 );
    tboard.SetSquareContents ( WPAWN, 2, 2 );
    tboard.SetSquareContents ( WQUEEN, 6, 2 );
    tboard.SetSquareContents ( WPAWN, 7, 2 );
    tboard.SetSquareContents ( WKNIGHT, 3, 3 );
    tboard.SetSquareContents ( BPAWN, 1, 5 );
    tboard.SetSquareContents ( BPAWN, 4, 5 );
    tboard.SetSquareContents ( BQUEEN, 5, 5 );
    tboard.SetSquareContents ( BPAWN, 6, 5 );
    tboard.SetSquareContents ( BPAWN, 0, 6 );
    tboard.SetSquareContents ( BBISHOP, 1, 6 );
    tboard.SetSquareContents ( BPAWN, 5, 6 );
    tboard.SetSquareContents ( BBISHOP, 6, 6 );
    tboard.SetSquareContents ( BKING, 0, 7 );
    tboard.SetSquareContents ( BROOK, 2, 7 );
    tboard.SetSquareContents ( BROOK, 4, 7 );
    benchmarkExpectedMove.source = 0;
    benchmarkExpectedMove.dest = 0;
    benchmarkPlayer->SetSearchDepth (4);
    TheBoardDisplayBuffer.update (tboard);
    TheBoardDisplayBuffer.freshenBoard();
    benchmarkPlayer->ResetHistoryBuffers();
    benchmarkPlayer->GetMove ( tboard, move, timeSpent );
    totalTime += (0.01 * timeSpent);

    // Complex queen's gambit declined opening position
    tboard.Init();
    tboard.ClearEverythingButKings();
    tboard.SetSquareContents ( WKING, 6, 0 );
    tboard.SetSquareContents ( WROOK, 0, 0 );
    tboard.SetSquareContents ( WQUEEN, 3, 0 );
    tboard.SetSquareContents ( WROOK, 5, 0 );
    tboard.SetSquareContents ( WPAWN, 1, 1 );
    tboard.SetSquareContents ( WPAWN, 5, 1 );
    tboard.SetSquareContents ( WPAWN, 6, 1 );
    tboard.SetSquareContents ( WPAWN, 7, 1 );
    tboard.SetSquareContents ( WPAWN, 0, 2 );
    tboard.SetSquareContents ( WKNIGHT, 2, 2 );
    tboard.SetSquareContents ( WBISHOP, 3, 2 );
    tboard.SetSquareContents ( WPAWN, 4, 2 );
    tboard.SetSquareContents ( WKNIGHT, 5, 2 );
    tboard.SetSquareContents ( WPAWN, 2, 3 );
    tboard.SetSquareContents ( WPAWN, 3, 3 );
    tboard.SetSquareContents ( BPAWN, 3, 4 );
    tboard.SetSquareContents ( WBISHOP, 6, 4 );
    tboard.SetSquareContents ( BPAWN, 1, 5 );
    tboard.SetSquareContents ( BKNIGHT, 2, 5 );
    tboard.SetSquareContents ( BPAWN, 4, 5 );
    tboard.SetSquareContents ( BKNIGHT, 5, 5 );
    tboard.SetSquareContents ( BPAWN, 0, 6 );
    tboard.SetSquareContents ( BBISHOP, 1, 6 );
    tboard.SetSquareContents ( BPAWN, 2, 6 );
    tboard.SetSquareContents ( BBISHOP, 4, 6 );
    tboard.SetSquareContents ( BPAWN, 5, 6 );
    tboard.SetSquareContents ( BPAWN, 6, 6 );
    tboard.SetSquareContents ( BPAWN, 7, 6 );
    tboard.SetSquareContents ( BKING, 6, 7 );
    tboard.SetSquareContents ( BROOK, 4, 7 );
    tboard.SetSquareContents ( BQUEEN, 3, 7 );
    tboard.SetSquareContents ( BROOK, 0, 7 );
    benchmarkExpectedMove.source = 0;
    benchmarkExpectedMove.dest = 0;
    benchmarkPlayer->SetSearchDepth (4);
    TheBoardDisplayBuffer.update (tboard);
    TheBoardDisplayBuffer.freshenBoard();
    benchmarkPlayer->ResetHistoryBuffers();
    benchmarkPlayer->GetMove ( tboard, move, timeSpent );
    totalTime += (0.01 * timeSpent);

    tacticalBenchmarkMode = false;
    enableMateAnnounce = saveMateAnnounce;
    delete benchmarkPlayer;
    benchmarkPlayer = 0;

    StopProfiler();

    return totalTime;
}


void ChessUI_win32_gui::SetAdHocText (
    int index,
    const char *format,
    ... )
{
    if ( index < 0 || index > 3 )
        return;

    char buffer [512];
    va_list argptr;
    va_start ( argptr, format );
    int numChars = vsprintf ( buffer, format, argptr );
    va_end ( argptr );

    if ( index < 2 )
    {
        ChessDisplayTextBuffer::SetText ( STATIC_ID_ADHOC1 + index, buffer );
    }
    else if ( index == 2 )
    {
        strcpy ( TitleBarText_WhitesTurn, "White's Turn - " );
        strcat ( TitleBarText_WhitesTurn, buffer );

        strcpy ( TitleBarText_BlacksTurn, "Black's Turn - " );
        strcat ( TitleBarText_BlacksTurn, buffer );

        strcpy ( TitleBarText_GameOver, buffer );
    }
    else if ( index == 3 )
    {
        ::SetWindowText ( hwnd, buffer );
    }
}


#if ENABLE_OPP_TIME
//----------------- thinking on opponent's time ------------------------

static uintptr_t OppTimeThreadID = INVALID_THREAD;


void OppTimeThreadFunc ( void *data )
{
    ChessBoard board;
    OppTimeParms *parms = (OppTimeParms *)data;
    for(;;)
    {
        // Wait state...
        while ( !parms->wakeUpSignal )
            Sleep(50);

        board = *(parms->inBoard);

        parms->finished     = false;
        parms->wakeUpSignal = false;   // signal caller that board is safely copied
        parms->foundMove    = false;

        UnmoveInfo unmove;
        if ( board.isLegal(parms->myMove_current) )
        {
            board.MakeMove ( parms->myMove_current, unmove );
            if ( board.isLegal(parms->oppPredicted_current) )
            {
                board.MakeMove ( parms->oppPredicted_current, unmove );
                parms->packedNextBoard = board;
                parms->oppThinker->SetTimeLimit (360000);   // one hour

                parms->foundMove = parms->oppThinker->GetMove (
                    board,
                    parms->myMove_next,
                    parms->timeSpent );

                parms->oppPredicted_next = parms->oppThinker->getPredictedOpponentMove();
            }
        }

        parms->finished = true;    // do this LAST!!!
    }
}

bool ChessUI_win32_gui::oppTime_startThinking (
    const ChessBoard    &currentPosition,
    Move                myMove,
    Move                predictedOpponentMove )
{
    if ( !enableOppTime )
        return false;

    if ( !oppTimeParms.oppThinker )
    {
        oppTimeParms.oppThinker = new ComputerChessPlayer (*this);
        if ( !oppTimeParms.oppThinker )
            return false;

        oppTimeParms.oppThinker->oppTime_SetInstanceFlag();
        oppTimeParms.oppThinker->setEnableMoveDisplay (false);
        oppTimeParms.oppThinker->SetSearchBias(1);      // allow randomized search
    }

    // If the thread needs to be started, try to start it...
    if ( OppTimeThreadID == INVALID_THREAD )
    {
        oppTimeParms.wakeUpSignal = false;     // tell thread to wait for work
        OppTimeThreadID = _beginthread ( OppTimeThreadFunc, 0, &oppTimeParms );
    }

    // If the thread is up and running, use it...
    if ( OppTimeThreadID != INVALID_THREAD )
    {
        oppTimeParms.inBoard                =   &currentPosition;
        oppTimeParms.myMove_current         =   myMove;
        oppTimeParms.oppPredicted_current   =   predictedOpponentMove;
        oppTimeParms.finished               =   false;
        oppTimeParms.timeStartedThinking    =   ChessTime();
        oppTimeParms.wakeUpSignal           =   true;  // must do this LAST!!!

        // Wait for OppTimeThreadFunc to get a safe copy of the board before we return...
        while ( oppTimeParms.wakeUpSignal )
            Sleep(50);

        return true;
    }

    return false;
}


bool ChessUI_win32_gui::oppTime_finishThinking (
    const ChessBoard    &newPosition,
    INT32               timeToThink,
    INT32               &timeSpent,
    Move                &myResponse,
    Move                &predictedOpponentMove )
{
    if (OppTimeThreadID != INVALID_THREAD && oppTimeParms.oppThinker)
    {
        if (enableOppTime)
        {
            bool predictedCorrectly = (oppTimeParms.packedNextBoard == newPosition);
            if (predictedCorrectly)
            {
                INT32 timeAlreadySpent = ChessTime() - oppTimeParms.timeStartedThinking;
                INT32 extraTime = timeToThink - timeAlreadySpent;
                if (extraTime < 20)
                {
                    oppTimeParms.oppThinker->AbortSearch();
                }
                else
                {
                    oppTimeParms.oppThinker->SetTimeLimit (extraTime);
                }
            }
            else
            {
                oppTimeParms.oppThinker->AbortSearch();
            }

            while (!oppTimeParms.finished)
            {
                Sleep(50);
            }

            if (predictedCorrectly && oppTimeParms.foundMove)
            {
                myResponse              =   oppTimeParms.myMove_next;
                predictedOpponentMove   =   oppTimeParms.oppPredicted_next;
                if (((ChessBoard &)newPosition).isLegal(myResponse))
                {
                    // We're on a roll!  Keep thinking...
                    timeSpent = ChessTime() - oppTimeParms.timeStartedThinking;
                    oppTime_startThinking (newPosition, myResponse, predictedOpponentMove);
                    return true;
                }
            }
        }
        else
        {
            // Getting here means opp time was enabled, but then turned off later.
            // Cancel any pondering that may be taking place to avoid burning up CPU time needlessly.
            oppTime_abortSearch();
        }
    }

    return false;
}


void ChessUI_win32_gui::oppTime_abortSearch()
{
    if (OppTimeThreadID != INVALID_THREAD && oppTimeParms.oppThinker && !oppTimeParms.finished)
    {
        while (!oppTimeParms.finished)
        {
            // Abort each time around the loop, because there
            // is a theoretical race condition where a single
            // abort could be missed: ComputerChessPlayer::GetMove
            // clears the abort flag.
            oppTimeParms.oppThinker->AbortSearch();

            // Avoid burning up CPU time.
            Sleep(50);
        }
    }
}


//----------------------------------------------------------------------
#endif  /* ENABLE_OPP_TIME */


void VerifyBestPath(ChessBoard& board, const BestPath& path)
{
    // For each move along the path, make sure the move is legal.

    if (path.depth < 0 || path.depth > MAX_BESTPATH_DEPTH)
    {
        assert(false);      // something is wrong with path.depth
    }
    else
    {
        // We must keep a stack of unmove info in order to put the board
        // back in the state it was when this function was called.
        UnmoveInfo unmove[MAX_BESTPATH_DEPTH];
        int d = 0;
        while (d < path.depth)
        {
            Move move = path.m[d];

            MoveList legalMoves;
            board.GenMoves(legalMoves);

            if (!legalMoves.IsLegal(move))
            {
                assert(false);
                goto rewind_moves;
            }

            board.MakeMove(move, unmove[d++]);
        }

rewind_moves:
        while (d > 0)
        {
            --d;
            board.UnmakeMove(path.m[d], unmove[d]);
        }
    }
}


//----------------------------------------------------------------------
// Blunder Alert feature:
// Optionally explore every possible move the human player could make,
// figuring out their scores and what the human's opponent could do in reply.
// Note that there is no requirement that the human's opponent is the computer;
// could be another human, an internet player, etc.

void BlunderAlertThreadFunc(void *data)
{
    BlunderAlertParms* parms = static_cast<BlunderAlertParms*>(data);

    // Make a safe copy of the chess board passed in.
    ChessBoard board(*parms->inBoard);
    parms->inBoard = NULL;      // slam the door on any chance of accessing the input board again!
    parms->levelCompleted = 0;

    // Keep track of the exact state of the board,
    // so the parent thread can match it at some point in the future.
    parms->packedBoard = board;

    // Generate all legal moves for the current player.
    MoveList humanLegalMoves;
    board.GenMoves(humanLegalMoves);

    // Copy legal move list into array of BestPath.
    for (unsigned i=0; i < humanLegalMoves.num; ++i)
    {
        // Remember the human's move at the top of each path.
        parms->path[i].m[0] = humanLegalMoves.m[i];

        // Indicate that no analysis at all has been done for this move.
        // Later, even if we discover that the human move ends the game,
        // the depth will increase to 1 to indicate that it was analyzed.
        parms->path[i].depth = 0;
    }

    parms->numLegalMoves = humanLegalMoves.num;

    // Notify the parent thread that we have safely copied the board,
    // and that it may proceed to modify the board.
    // The parent thread waits until this flag is true before going any further.
    parms->isStarted = true;

    // There is no point thinking unless there are 2 or more moves available.
    if (parms->numLegalMoves > 1)
    {
        for (int level = parms->minLevel; level <= parms->maxLevel; ++level)
        {
            // FIXFIXFIX:  Improve pruning by recycling best path info from prior search for this move.
            parms->thinker->SetMinSearchDepth(level);
            parms->thinker->SetSearchDepth(level);

            for (int i=0; i < parms->numLegalMoves; ++i)
            {
                if (parms->hurry)
                {
                    goto thread_exit;
                }

                Move& humanMove = parms->path[i].m[0];
                Move bestReply;
                bestReply.source = bestReply.dest = 0;
                bestReply.score = NEGINF;

                UnmoveInfo unmove;
                board.MakeMove(humanMove, unmove);

                if (board.CurrentPlayerCanMove())
                {
                    if (board.IsDefiniteDraw())
                    {
                        // Game is over: it is a draw.
                        humanMove.score = DRAW;
                        parms->path[i].depth = 1;   // analysis is final!
                    }
                    else
                    {
                        // Use computer player to search for best move and its score.
                        INT32 timeSpent = 0;
                        const bool madeMove = parms->thinker->GetMove(board, bestReply, timeSpent);

                        // If the search was aborted, we can't use anything from this search.
                        // The parent thread will always set the 'hurry' flag before aborting.
                        if (!parms->hurry)
                        {
                            assert(madeMove);   // happens if computer resigns, but that should have been disabled.
                            // Even if the computer resigned, bestReply and best path should be valid.

                            humanMove.score = bestReply.score;      // TODO: should we adjust for extra ply?

                            // Copy best path analysis for parent thread to use.
                            // We need to be careful to remember that the source best path
                            // begins with the opponent's reply in slot [0], but the target
                            // best path should hold it at slot [1], since [0] there is for
                            // the human's move, which has already been made.
                            const BestPath& bestPath = parms->thinker->getBestPath();

                            // Special case: if the opponent can immediately checkmate,
                            // there is no best-path info, just a single reply.
                            // In general, assume immediate game-ender has no bestpath.
                            if (bestPath.depth == 0)
                            {
                                parms->path[i].m[1] = bestReply;
                                parms->path[i].depth = 2;
                            }
                            else
                            {
                                assert((bestPath.depth > 0) && (bestReply == bestPath.m[0]));

                                // When we include the human move, the total path is one deeper.
                                int targetDepth = bestPath.depth + 1;

                                // But truncate the target if it is deeper than the array can hold.
                                if (targetDepth > MAX_BESTPATH_DEPTH)
                                {
                                    targetDepth = MAX_BESTPATH_DEPTH;
                                }

                                // Copy the entire bestPath to one level below the human move in parms->path.
                                for (int d=1; d < targetDepth; ++d)
                                {
                                    parms->path[i].m[d] = bestPath.m[d-1];
                                }

                                parms->path[i].depth = targetDepth;
                            }
                        }
                    }
                }
                else
                {
                    // Game is over: either stalemate or checkmate.
                    parms->path[i].depth = 1;   // analysis is final!
                    if (board.CurrentPlayerInCheck())
                    {
                        // checkmate
                        if (board.WhiteToMove())
                        {
                            // Black has checkmated White.
                            humanMove.score = (BLACK_WINS + WIN_POSTPONEMENT(0));
                        }
                        else
                        {
                            // White has checkmated Black.
                            humanMove.score = (WHITE_WINS - WIN_POSTPONEMENT(0));
                        }
                    }
                    else
                    {
                        // stalemate
                        humanMove.score = DRAW;
                    }
                }

                board.UnmakeMove(humanMove, unmove);
                VerifyBestPath(board, parms->path[i]);
            }

            if (!parms->hurry)
            {
                parms->levelCompleted = level;
            }
        } // for loop for each search level
    }

thread_exit:
    // Set the finished flag LAST... otherwise nasty race conditions are possible.
    parms->isFinished = true;
}

//----------------------------------------------------------------------


/*
    $Log: uiwin32.cpp,v $
    Revision 1.27  2009/09/16 16:49:34  Don.Cross
    Added support for the WinBoard/xboard "memory" command to set hash table sizes.

    Revision 1.26  2008/12/15 02:15:57  Don.Cross
    Made WinChen pondering algorithm move quicker when the opponent's move has been predicted correctly.
    This is what XChenard already does.

    Revision 1.25  2008/12/01 23:08:53  Don.Cross
    Norm Pollock announced on the WinBoard forum that he has updated his PGN database:
        http://www.open-aurec.com/wbforum/viewtopic.php?t=49522
        http://www.hoflink.com/~npollock/chess.html
    This is just what I was looking for, so that I could beef up chenard.trx (training file).
    I modified the PGN scanner to recognize WhiteElo, BlackElo, and Result tags.
    I am using WhiteElo and BlackElo to decide whether a game is trustworthy enough to pretend like Chenard thought
    of the move itself, so that it will play that move in actual games, instead of just as a reference point for opponent moves.
    I am currently in the process of slurping an enormous amount of openings into chenard.trx.
    I decided to move GetNextPgnMove from board.cpp to gamefile.cpp, where it really belongs.

    Revision 1.24  2008/11/24 11:21:58  Don.Cross
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

    Revision 1.23  2008/11/17 18:47:07  Don.Cross
    WinChen.exe now has an Edit/Enter FEN function for setting a board position based on a FEN string.
    Tweaked ChessBoard::SetForsythEdwardsNotation to skip over any leading white space.
    Fixed incorrect Chenard web site URL in the about box.
    Now that computers are so much faster, reduced the

    Revision 1.22  2008/11/13 15:29:05  Don.Cross
    Fixed bug: user can now Undo/Redo moves after loading a game from a PGN file.

    Revision 1.21  2008/11/12 23:06:07  Don.Cross
    Massive refactoring of the concept of an 'editedFlag' in ChessBoard.
    Now instead of just saying the board was edited, we will always remember the most recent position
    after which there are no more pseudo-moves representing edit actions.
    This allows us to save/load PGN files based on edited positions.
    More work needs to be done:
    - Need to allow move undo/redo to work in edited positions.
    - Because the game history is stored now inside the ChessBoard, it seems silly to have an external copy also!
    - Move numbers should be based on FEN when PGN is saved.
    In general, this is a scary change, so it needs more careful testing before being published.

    Revision 1.20  2008/11/08 16:54:39  Don.Cross
    Fixed a bug in WinChen.exe:  Pondering continued even when the opponent resigned.
    Now we abort the pondering search when informed of an opponent's resignation.

    Revision 1.19  2008/11/08 15:47:00  Don.Cross
    Fixed a bug in the GUI display where eval/sec was showing rediculous numbers in very large searches.
    The expression long(nodesEvaluated)*100L was overflowing 32 bits.
    Now I do the arithmetic using floating point.

    Revision 1.18  2008/11/06 22:25:06  Don.Cross
    Now ParseFancyMove accepts either PGN or longmove (e7e8q) style notation only.
    We tolerate of check '+' or checkmate '#' characters being absent or extraneous in the input.
    The former is especially important because I often see PGN notation where '+' is missing on checking moves.
    To allow for PGN input, I had to allow avoidance of the PromotePawn callback, so other code was changed as well.
    Version numbers are now always constently yyyy.mm.dd across all builds of Chenard!

    Revision 1.17  2008/05/03 15:26:19  Don.Cross
    Updated code for Visual Studio 2005.

    Revision 1.16  2006/02/25 22:22:19  dcross
    Added support for connecting to Flywheel named pipe server as a Chenard chess player.
    This allows me to use my new Flywheel engine via the Chenard GUI.

    Revision 1.15  2006/02/19 23:25:06  dcross
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

    Revision 1.14  2006/01/28 21:44:16  dcross
    1. Updated auto-save feature to use PGN format instead of old GAM format.
       This means we are sacrificing the ability to auto-save edited games, but I don't care.
    2. Added a feature in class ChessGame: if the file 'game.counter' exists, auto-save
       to file based on integer counter it contains, and increment that counter in the file.
       This is so I no longer have to manually make up unique filenames and remember to
       save files when Chenard is in "combat mode".
    3. Reworked PGN save functions so they can be shared by Win32 and portable versions.
       This requires all versions of Chenard to define functions GetWhitePlayerString and
       GetBlackPlayerString, so the players can be recorded in the PGN header.

    Revision 1.13  2006/01/25 19:09:13  dcross
    Today I noticed a case where Chenard correctly predicted the opponent's next move,
    but ran out of time while doing the deep search.  It then proceeded to pick a horrible
    move randomly from the prior list.  Because of this, and the fact that I knew the
    search would be faster, I have adopted the approach I took in my Java chess applet:
    Instead of using a pruning searchBias of 1, and randomly picking from the set of moves
    with maximum value after the fact, just shuffle the movelist right before doing move
    ordering at the top level of the search.  This also has the advantage of always making
    the move shown at the top of the best path.  So now "searchBias" is really a boolean
    flag that specifies whether to pre-shuffle the move list or not.  It has been removed
    from the pruning logic, so the search is slightly more efficient now.

    Revision 1.12  2006/01/22 20:53:15  dcross
    Now when the game is over, just set title bar text instead of popping up a dialog box.

    Revision 1.11  2006/01/22 20:20:22  dcross
    Added an option to enable/disable announcing forced mates via dialog box.

    Revision 1.10  2006/01/18 20:50:08  dcross
    The ability to speak move notation through the sound card was broken when
    I went from goofy move notation to PGN.  This is a quickie hack to make it
    work, though it's not perfect because it should say "moves to", and "at", but it doesn't.
    Also removed the board and move parameters to Speak(), because they were never used.

    Revision 1.9  2006/01/18 19:58:16  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.8  2006/01/13 17:17:30  dcross
    Fixed bug in opponent time thinking:
    If it was enabled, then later turned off, the opponent time thinker thread
    would keep burning up CPU indefinitely.  Now we catch this case
    and abort the opp time search.

    Revision 1.7  2006/01/10 22:08:40  dcross
    Fixed a bug that caused Chenard to bomb out with a ChessFatal:
    Go into edit mode with White to move, and place a White piece where it
    attacks Black's king.  Chenard would immediately put a dialog box saying
    that a Black king had been captured, and exit.  Now you just get a dialog
    box saying "doing that would create an impossible board position", but
    Chenard keeps running.

    Revision 1.6  2006/01/03 02:08:23  dcross
    1. Got rid of non-PGN move generator code... I'm committed to the standard now!
    2. Made *.pgn the default filename pattern for File/Open.
    3. GetGameListing now formats more tightly, knowing that maximum possible PGN move is 7 characters.

    Revision 1.5  2006/01/02 19:40:29  dcross
    Added ability to use File/Open dialog to load PGN files into Win32 version of Chenard.

    Revision 1.4  2005/12/31 23:26:38  dcross
    Now Windows GUI version of Chenard can save game in PGN format.

    Revision 1.3  2005/11/23 21:30:32  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:44  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!


        Revision history:

    1993 July 14 [Don Cross]
         Started writing!  (originally OS/2 Presentation Manager version.)

    1993 December 31 [Don Cross]
         Added code to save the whole game to the file 'chess.gam'
         each time the board is displayed in ChessUI_win32_gui::DrawBoard().
         Changed the special "save game" command to write to 'save.gam'.
         This way, it is easier to quit from a game, then restore its
         last position some other time.

    1994 January 15 [Don Cross]
         Added support for allowing the user to re-start the game.

    1994 January 30 [Don Cross]
         Giving back SPECIAL_MOVE_EDIT signals now when board edit commands
         are processed.

    1996 July 29 [Don Cross]
         Starting porting from uios2pm.cpp to Win32 environment.

    1996 August 28 [Don Cross]
         Added automatic benchmark facility.

    1997 January 30 [Don Cross]
         Adding support for undo move and redo move.

    1997 March 8 [Don Cross]
         Added option to allow computer to resign.
         Added command for human to resign.

    1999 January 3 [Don Cross]
         Now "mate in N" is announced only once per game.
         Updated coding style.

    1999 January 5 [Don Cross]
         Adding support for Internet player.

    1999 January 18 [Don Cross]
         Made game save/load code portable to machines with
         different endianness and word size.

    1999 January 31 [Don Cross]
         Adding 'suggest move' command.

    1999 February 10 [Don Cross]
         Modified display of best path to include information from
         transposition table if not in the best path info per se.

    1999 February 12 [Don Cross]
         Now display the visited nodes array after the search completes.
         Made FlashSquare blink a little bit faster.

    1999 August 5 [Don Cross]
         Apparently, sometimes illegal moves are ending up in best path.
         Now I am sanity checking the moves before they are made on
         the board in DisplayBestPath.

    1999 August 6 [Don Cross]
         Fixed bugs in move undo/redo:
         (1) Now you can undo moves after loading a game file.
             Before, you had to make a move, then undo would start working.
         (2) Now you can undo moves when the game has ended.
         (3) When you redo a move that causes game to end, it is now
             handled properly.  Because of item #2, this situation
             never occurred, and thus was untested.

    2001 January 13 [Don Cross]
         Adding support for thinking on opponent's time.

*/

