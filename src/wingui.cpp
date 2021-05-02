/*=========================================================================

     wingui.cpp  -  Copyright (C) 1993-2005 by Don Cross

     Contains WinMain() for Win32 GUI version of Chenard.

=========================================================================*/

#include <string.h>
#include <stdio.h>
#include <process.h>
#include <io.h>
#include <assert.h>
#include <direct.h>

#include "chess.h"
#include "chenga.h"
#include "winchess.h"
#include "winguich.h"
#include "resource.h"
#include "profiler.h"

const char ChessProgramName[] = CHESS_PROGRAM_NAME;

char Global_GameFilename [256] = "*.PGN";

bool Global_ResetGameFlag    = false;
bool Global_ClearBoardFlag   = false;
int  Global_AnalysisType     = 0;   //0=none, 1=best path, 2=current path : FIXFIXFIX change to enum or define symbols
bool Global_GameOpenFlag     = false;
bool Global_GameSaveFlag     = false;
bool Global_HaveFilename     = false;
bool Global_GameModifiedFlag = false;
bool Global_TacticalBenchmarkFlag = false;
bool Global_GameOverFlag     = true;   // It's over before it begins!
bool Global_UndoMoveFlag     = false;
bool Global_RedoMoveFlag     = false;
bool Global_AllowResignFlag  = false;
bool Global_AnimateMoves     = true;
bool Global_EnableOppTime    = true;
bool Global_HiliteMoves      = true;
bool Global_AutoSingular     = false;
bool Global_ExtendedSearch   = false;
bool Global_EnableGA = false;
bool Global_EnableEndgameDatabaseGen = false;
bool Global_EnableBoardBreathe  = true;
bool Global_DialogBoxWhenLoadingPgn = true;
bool Global_PgnFileWasLoaded = false;
extern int      Global_EndgameDatabaseMode;
int      Global_GA_NumGenes = 200;
bool Global_SuggestThinkOnly = false;
bool Global_DingAfterSuggest = false;
bool Global_TransientCenteredMessage = false;

#if SUPPORT_INTERNET
    WSADATA  Global_WsaData;
    bool Global_NetworkInit   =  false;
    SOCKET   Global_MySocket      =  INVALID_SOCKET;
    char     Global_MyHostName [256];
    hostent  Global_MyHostEnt;
    bool Global_InternetPlayersReady = false;
    BOOL Global_ServerMode = FALSE;
#endif // SUPPORT_INTERNET

char DefaultServerIpAddress [256];

ChessUI_win32_gui *Global_UI = 0;
ChessBoard Global_Board;

BoardEditRequest Global_BoardEditRequest;

static char WhiteTimeLimit [64] = "10";
static char BlackTimeLimit [64] = "10";

char TitleBarText_WhitesTurn[128] = CHESS_PROGRAM_NAME " - White's turn";
char TitleBarText_BlacksTurn[128] = CHESS_PROGRAM_NAME " - Black's turn";
char TitleBarText_GameOver[128]   = CHESS_PROGRAM_NAME " - game over";

char GameSave_filename[] = "chess.gam";

LRESULT CALLBACK ChessWndProc ( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam );


bool Global_AbortFlag = false;
static bool globalImmedQuitFlag = false;
static int SoundEnableFlag = 1;
HWND   HwndMain;
HMENU  HmenuMain;
HINSTANCE global_hInstance = NULL;
DWORD global_windowFlags = WS_MINIMIZEBOX | WS_CAPTION | WS_SYSMENU | WS_OVERLAPPED;

bool PreDefinedPlayerSides = false;
bool PreDefinedWhiteHuman  = false;
bool PreDefinedBlackHuman  = false;
DefPlayerInfo DefPlayer;


static void EnableEditPieces ( HWND hwnd, bool enable )
{
    EnableWindow ( GetDlgItem(hwnd,IDC_EDIT_PAWN),   enable );
    EnableWindow ( GetDlgItem(hwnd,IDC_EDIT_KNIGHT), enable );
    EnableWindow ( GetDlgItem(hwnd,IDC_EDIT_BISHOP), enable );
    EnableWindow ( GetDlgItem(hwnd,IDC_EDIT_ROOK),   enable );
    EnableWindow ( GetDlgItem(hwnd,IDC_EDIT_QUEEN),  enable );
    EnableWindow ( GetDlgItem(hwnd,IDC_EDIT_KING),   enable );
}


BOOL CALLBACK About_DlgProc (
    HWND hwnd,
    UINT msg,
    WPARAM wparam,
    LPARAM lparam )
{
    BOOL result = FALSE;

    switch ( msg )
    {
    case WM_INITDIALOG:
        SetFocus ( GetDlgItem(hwnd,IDOK) );
        break;

    case WM_COMMAND:
        switch ( wparam )
        {
        case IDOK:
        case IDCANCEL:
            EndDialog ( hwnd, IDOK );
            result = TRUE;
            break;
        }
        break;
    }

    return result;
}


BOOL CALLBACK EnterFen_DlgProc (
    HWND    hwnd,
    UINT    msg,
    WPARAM  wparam,
    LPARAM  lparam )
{
    BOOL    result = FALSE;
    char    buffer [500];

    switch (msg)
    {
    case WM_INITDIALOG:
        SetFocus ( GetDlgItem(hwnd,TB_EDIT_FEN) );
        break;

    case WM_COMMAND:
        switch (wparam)
        {
        case IDOK:
            if (GetWindowText (GetDlgItem (hwnd,TB_EDIT_FEN), buffer, sizeof(buffer)))
            {
                Global_BoardEditRequest.sendFen (buffer);
            }
            EndDialog (hwnd, wparam);
            result = TRUE;
            break;

        case IDCANCEL:
            EndDialog (hwnd, wparam);
            result = TRUE;
            break;
        }
        break;
    }
    return result;
}


SCORE Global_BlunderAlertThreshold = static_cast<SCORE> (DEFAULT_BLUNDER_ALERT_THRESHOLD);

BOOL CALLBACK EditBlunderAlertThreshold_DlgProc(
    HWND    hwnd,
    UINT    msg,
    WPARAM  wparam,
    LPARAM  lparam )
{
    BOOL result = FALSE;
    char buffer [128];

    switch (msg)
    {
    case WM_INITDIALOG:
    {
        sprintf(buffer, "%d", static_cast<int>(Global_BlunderAlertThreshold));
        SetWindowText (GetDlgItem(hwnd,IDC_EDIT_BLUNDER_THRESHOLD), buffer);
        SetFocus(GetDlgItem(hwnd,IDC_EDIT_BLUNDER_THRESHOLD));
        SendDlgItemMessage(hwnd, IDC_EDIT_BLUNDER_THRESHOLD, EM_SETSEL, 0, -1);     // select the text
    }
    break;

    case WM_COMMAND:
    {
        switch ( wparam )
        {
        case IDOK:
        {
            result = TRUE;  // If we return FALSE after refusing to end the dialog, a weird state ensues where user can't type text!
            if (Global_UI)
            {
                if (GetWindowText(GetDlgItem(hwnd,IDC_EDIT_BLUNDER_THRESHOLD), buffer, sizeof(buffer)))
                {
                    // Parse and validate blunder alert threshold.
                    // Tacitly ignore the user's input if it is out of range.
                    const int thresh = atoi(buffer);
                    if ((thresh >= MIN_BLUNDER_ALERT_THRESHOLD) && (thresh <= MAX_BLUNDER_ALERT_THRESHOLD))
                    {
                        Global_BlunderAlertThreshold = static_cast<SCORE> (thresh);
                        EndDialog ( hwnd, IDOK );
                    }
                    else
                    {
                        // The value is outside the allowed range, so bark and balk.

                        // Play an exclamation sound.
                        PlaySound((LPCSTR)SND_ALIAS_SYSTEMEXCLAMATION, NULL, SND_ALIAS_ID | SND_ASYNC);

                        // For some reason, we are not supposed to use SetFocus here (only in WM_INITDIALOG).
                        // http://forums.codeguru.com/showthread.php?303711-Set-Focus-to-Edit-Box
                        SendDlgItemMessage(hwnd, IDC_EDIT_BLUNDER_THRESHOLD, WM_SETFOCUS, 0, 0);    // set focus on edit box
                        SendDlgItemMessage(hwnd, IDC_EDIT_BLUNDER_THRESHOLD, EM_SETSEL, 0, -1);     // select the text in the edit box
                    }
                }
            }
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


BOOL CALLBACK EditThinkTimes_DlgProc (
    HWND    hwnd,
    UINT    msg,
    WPARAM  wparam,
    LPARAM  lparam )
{
    BOOL result = FALSE;
    char buffer [128];

    switch ( msg )
    {
    case WM_INITDIALOG:
    {
        // Figure out which players are computer players.
        // Disable think time edit boxes for non-computer players.
        if ( Global_UI->queryWhitePlayerType() == DefPlayerInfo::computerPlayer )
        {
            INT32 timeLimit = Global_UI->queryWhiteThinkTime();
            sprintf ( buffer, "%0.1lf", double(timeLimit) / 100.0 );
            SetWindowText ( GetDlgItem(hwnd,IDC_EDIT_WHITE_TIME), buffer );
        }
        else
        {
            SetWindowText ( GetDlgItem(hwnd,IDC_EDIT_WHITE_TIME), "n/a" );
            EnableWindow ( GetDlgItem(hwnd,IDC_EDIT_WHITE_TIME), FALSE );
        }

        if ( Global_UI->queryBlackPlayerType() == DefPlayerInfo::computerPlayer )
        {
            INT32 timeLimit = Global_UI->queryBlackThinkTime();
            sprintf ( buffer, "%0.1lf", double(timeLimit) / 100.0 );
            SetWindowText ( GetDlgItem(hwnd,IDC_EDIT_BLACK_TIME), buffer );
        }
        else
        {
            SetWindowText ( GetDlgItem(hwnd,IDC_EDIT_BLACK_TIME), "n/a" );
            EnableWindow ( GetDlgItem(hwnd,IDC_EDIT_BLACK_TIME), FALSE );
        }
    }
    break;

    case WM_COMMAND:
    {
        switch ( wparam )
        {
        case IDOK:
        {
            if ( Global_UI )
            {
                double timeLimit = 0;
                if ( GetWindowText ( GetDlgItem(hwnd,IDC_EDIT_WHITE_TIME), buffer, sizeof(buffer) ) &&
                        (timeLimit = atof(buffer)) >= 0.1 )
                {
                    Global_UI->setWhiteThinkTime ( INT32(timeLimit * 100) );
                }

                if ( GetWindowText ( GetDlgItem(hwnd,IDC_EDIT_BLACK_TIME), buffer, sizeof(buffer) ) &&
                        (timeLimit = atof(buffer)) >= 0.1 )
                {
                    Global_UI->setBlackThinkTime ( INT32(timeLimit * 100) );
                }

                EndDialog ( hwnd, IDOK );
                result = TRUE;
            }
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


BOOL CALLBACK EditSquare_DlgProc (
    HWND hwnd,  // handle to dialog box
    UINT msg,           // message
    WPARAM wparam,  // first message parameter
    LPARAM lparam ) // second message parameter
{
    static SQUARE piece = EMPTY;
    static SQUARE side = EMPTY;
    BOOL result = TRUE;
    static SQUARE *squarePtr = 0;
    static WPARAM lastPiece = IDC_EDIT_PAWN;
    static WPARAM lastSide = IDC_EDIT_EMPTY;

    switch ( msg )
    {
    case WM_INITDIALOG:
    {
        squarePtr = (SQUARE *) (lparam);
        CheckRadioButton ( hwnd, IDC_EDIT_PAWN, IDC_EDIT_KING, (int)lastPiece );
        CheckRadioButton ( hwnd, IDC_EDIT_EMPTY, IDC_EDIT_BLACK, (int)lastSide );
        EnableEditPieces ( hwnd, lastSide != IDC_EDIT_EMPTY );
    }
    break;

    case WM_COMMAND:
    {
        switch ( wparam )
        {
        case IDOK:
        {
            if ( squarePtr )
            {
                if ( side == EMPTY )
                    *squarePtr = EMPTY;
                else
                    *squarePtr = PieceLookupTable.sval [ piece | side ];
            }

            EndDialog ( hwnd, IDOK );
        }
        break;

        case IDCANCEL:
        {
            EndDialog ( hwnd, IDCANCEL );
        }
        break;

        case IDC_EDIT_PAWN:    piece = P_INDEX;   lastPiece = wparam;   break;
        case IDC_EDIT_KNIGHT:  piece = N_INDEX;   lastPiece = wparam;   break;
        case IDC_EDIT_BISHOP:  piece = B_INDEX;   lastPiece = wparam;   break;
        case IDC_EDIT_ROOK:    piece = R_INDEX;   lastPiece = wparam;   break;
        case IDC_EDIT_QUEEN:   piece = Q_INDEX;   lastPiece = wparam;   break;
        case IDC_EDIT_KING:    piece = K_INDEX;   lastPiece = wparam;   break;

        case IDC_EDIT_EMPTY:
        {
            side = EMPTY;
            lastSide = wparam;
            EnableEditPieces ( hwnd, FALSE );
        }
        break;

        case IDC_EDIT_WHITE:
        {
            side = WHITE_IND;
            lastSide = wparam;
            EnableEditPieces ( hwnd, TRUE );
        }
        break;

        case IDC_EDIT_BLACK:
        {
            side = BLACK_IND;
            lastSide = wparam;
            EnableEditPieces ( hwnd, TRUE );
        }
        break;
        }
    }
    break;

    default:
        result = FALSE;
        break;
    }

    return result;
}


static bool EditSquareDialog ( HWND hwnd, SQUARE &square )
{
    INT_PTR result = DialogBoxParam (
                         global_hInstance,
                         MAKEINTRESOURCE(IDD_EDIT_SQUARE),
                         hwnd,
                         DLGPROC(EditSquare_DlgProc),
                         LPARAM(&square) );

    bool choseSomething = false;

    switch ( result )
    {
    case IDOK:
        choseSomething = true;
        break;
    }

    return choseSomething;
}


BOOL CALLBACK PromotePawn_DlgProc (
    HWND hwnd,          // handle to dialog box
    UINT msg,           // message
    WPARAM wparam,  // first message parameter
    LPARAM lparam ) // second message parameter
{
    BOOL result = FALSE;
    BOOL rook_rb, bishop_rb, knight_rb;
    static SQUARE *promPtr;

    switch ( msg )
    {
    case WM_INITDIALOG:
    {
        // Turn on the 'queen' radio button...
        CheckRadioButton ( hwnd, RB_QUEEN, RB_KNIGHT, RB_QUEEN );

        // Place to put promoted pawn data...
        promPtr = (SQUARE *) lparam;
        result = TRUE;
    }
    break;

    case WM_COMMAND:
    {
        switch ( wparam )
        {
        case IDOK:
        {
            rook_rb   = IsDlgButtonChecked ( hwnd, RB_ROOK );
            bishop_rb = IsDlgButtonChecked ( hwnd, RB_BISHOP );
            knight_rb = IsDlgButtonChecked ( hwnd, RB_KNIGHT );

            if ( rook_rb )
                *promPtr = R_INDEX;
            else if ( bishop_rb )
                *promPtr = B_INDEX;
            else if ( knight_rb )
                *promPtr = N_INDEX;
            else // assume the queen radio button is selected
                *promPtr = Q_INDEX;

            EndDialog ( hwnd, IDOK );
        }
        break;
        }
    }
    break;
    }

    return result;
}


#if SUPPORT_INTERNET


BOOL InitializeNetwork ( HWND hwnd )
{
    if ( !Global_NetworkInit )
    {
        int startRet = WSAStartup ( 0x202, &Global_WsaData );
        if ( startRet == 0 )
        {
            Global_NetworkInit = true;
            if ( gethostname ( Global_MyHostName, sizeof(Global_MyHostName) ) != 0 )
            {
                MessageBox ( hwnd, "Could not determine local host name!", "Network Error", MB_OK | MB_ICONERROR );
                return FALSE;
            }
            else
            {
                hostent *myHostInfo = gethostbyname (Global_MyHostName);
                if ( !myHostInfo )
                {
                    MessageBox ( hwnd, "Could not obtain local host information!", "Network Error", MB_OK | MB_ICONERROR );
                    return FALSE;
                }
                else
                {
                    Global_MyHostEnt = *myHostInfo;
                    Global_MySocket = socket ( AF_INET, SOCK_STREAM, 0 );
                    if ( Global_MySocket == INVALID_SOCKET )
                    {
                        MessageBox ( hwnd, "Could not create socket!", "Network Error", MB_OK | MB_ICONERROR );
                        return FALSE;
                    }
                }
            }
        }
        else
        {
            const char *errorString = "Unknown network error";
            switch ( startRet )
            {
            case WSASYSNOTREADY:
                errorString = "Network subsystem not ready";
                break;

            case WSAVERNOTSUPPORTED:
                errorString = "Unsupported version of WinSock";
                break;

            case WSAEINPROGRESS:
                errorString = "A blocking WinSock 1.1 operation is in progress";
                break;

            case WSAEPROCLIM:
                errorString = "Too many WinSock tasks!";
                break;
            }

            MessageBox ( hwnd, errorString, "Network Error", MB_OK | MB_ICONERROR );
            return FALSE;
        }
    }

    return TRUE;
}


BOOL ConnectToChenardServer (
    HWND defPlayerHwnd,
    DefPlayerInfo &playerInfo,
    const char *remoteIpAddressString )
{
    char tempString [256];

    if ( !InitializeNetwork(defPlayerHwnd) )
        return FALSE;

    unsigned remoteIp[4] = { 0, 0, 0, 0 };
    int numScanned = sscanf (
                         remoteIpAddressString,
                         "%u.%u.%u.%u",
                         &remoteIp[0], &remoteIp[1], &remoteIp[2], &remoteIp[3] );

    if ( numScanned != 4 ||
            remoteIp[0] > 0xff || remoteIp[1] > 0xff || remoteIp[2] > 0xff || remoteIp[3] > 0xff )
    {
        MessageBox ( defPlayerHwnd, "Invalid IP address entered!", "Error", MB_OK | MB_ICONERROR );
        SetFocus ( GetDlgItem(defPlayerHwnd,IDC_REMOTE_IP) );
        return FALSE;
    }

    sockaddr_in connectAddr;
    memset ( &connectAddr, 0, sizeof(connectAddr) );
    connectAddr.sin_family = AF_INET;
    connectAddr.sin_port = htons(InternetChessPlayer::QueryServerPortNumber());

    connectAddr.sin_addr.S_un.S_un_b.s_b1 = remoteIp[0];
    connectAddr.sin_addr.S_un.S_un_b.s_b2 = remoteIp[1];
    connectAddr.sin_addr.S_un.S_un_b.s_b3 = remoteIp[2];
    connectAddr.sin_addr.S_un.S_un_b.s_b4 = remoteIp[3];

    int result = connect ( Global_MySocket, (sockaddr *) &connectAddr, sizeof(connectAddr) );
    if ( result != 0 )
    {
        sprintf ( tempString, "Error %d connecting to remote Chenard", int(WSAGetLastError()) );
        MessageBox ( defPlayerHwnd, tempString, "Network Error", MB_OK | MB_ICONERROR );
        return FALSE;
    }

    // Now that we are connected to the remote Chenard,
    // receive from it our player definition:

    UINT32 packetSize = 0;
    int bytesReceived = recv ( Global_MySocket, (char *)&packetSize, 4, 0 );
    if ( bytesReceived != 4 )
    {
        MessageBox ( defPlayerHwnd,
                     "Error receiving player definition packet size!",
                     "Network Error",
                     MB_OK | MB_ICONERROR );

        return FALSE;
    }

    char inMessageType [16];
    memset ( inMessageType, 0, sizeof(inMessageType) );
    bytesReceived = recv ( Global_MySocket, inMessageType, 8, 0 );
    if ( bytesReceived != 8 || strncmp(inMessageType,"players ",8) != 0 )
    {
        MessageBox ( defPlayerHwnd,
                     "Error receiving player definition header!",
                     "Network Error",
                     MB_OK | MB_ICONERROR );

        return FALSE;
    }

    char playerDef [16];
    memset ( playerDef, 0, sizeof(playerDef) );
    bytesReceived = recv ( Global_MySocket, playerDef, 2, 0 );
    if ( bytesReceived != 2 )
    {
        MessageBox ( defPlayerHwnd,
                     "Error receiving player definition structure!",
                     "Network Error",
                     MB_OK | MB_ICONERROR );

        return FALSE;
    }

    if ( playerDef[0] == 'I' )
    {
        playerInfo.whiteType = DefPlayerInfo::internetPlayer;
        playerInfo.whiteInternetConnect.commSocket = Global_MySocket;
        playerInfo.whiteInternetConnect.waitForClient = 0;
    }
    else
        playerInfo.whiteType = DefPlayerInfo::humanPlayer;

    if ( playerDef[1] == 'I' )
    {
        playerInfo.blackType = DefPlayerInfo::internetPlayer;
        playerInfo.blackInternetConnect.commSocket = Global_MySocket;
        playerInfo.blackInternetConnect.waitForClient = 0;
    }
    else
        playerInfo.blackType = DefPlayerInfo::humanPlayer;

    Global_InternetPlayersReady = true;
    return TRUE;
}


static uintptr_t ServerConnectThreadId = 0;

void ServerConnectThreadFunc ( void * )
{
    while ( !Global_InternetPlayersReady )
    {
        int result = listen ( Global_MySocket, 16 );
        if ( result != 0 )
        {
            break;  // !!!
        }

        sockaddr_in connectAddr;
        int connectAddrSize = sizeof(connectAddr);
        SOCKET connectSocket = accept ( 
            Global_MySocket, 
            (sockaddr *) &connectAddr, 
            &connectAddrSize );

        if ( connectSocket == INVALID_SOCKET )
        {
            break;   // !!!
        }

        InternetConnectionInfo *connectInfo = 0;
        int clientIsWhite = 0;
        if ( DefPlayer.whiteInternetConnect.waitForClient )
        {
            clientIsWhite = 1;
            connectInfo = &DefPlayer.whiteInternetConnect;
        }
        else if ( DefPlayer.blackInternetConnect.waitForClient )
            connectInfo = &DefPlayer.blackInternetConnect;

        if ( connectInfo )
        {
            connectInfo->commSocket = connectSocket;
            connectInfo->waitForClient = 0;

            // Send player definition data to client.
            // We must express the data from *their* point of view.

            UINT32 packetSize = 10;
            result = send ( connectSocket, (const char *) &packetSize, 4, 0 );
            if ( result != 4 )
                break;

            result = send ( connectSocket, "players ", 8, 0 );
            if ( result != 8 )
                break;

            char playerDef [2];
            playerDef[0] = clientIsWhite ? 'H' : 'I';
            playerDef[1] = clientIsWhite ? 'I' : 'H';

            result = send ( connectSocket, playerDef, 2, 0 );
            if ( result != 2 )
                break;
        }

        Global_InternetPlayersReady =
            !DefPlayer.whiteInternetConnect.waitForClient &&
            !DefPlayer.blackInternetConnect.waitForClient;
    }

    ServerConnectThreadId = 0;
}

#endif // SUPPORT_INTERNET


BOOL CALLBACK DefineChessPlayers_DlgProc (
    HWND hwnd,          // handle to dialog box
    UINT msg,           // message
    WPARAM wparam,      // first message parameter
    LPARAM lparam )     // second message parameter
{
    BOOL result = TRUE;
    const char *defaultTimeLimit = "2";

    switch ( msg )
    {
    case WM_INITDIALOG:
    {
#if SUPPORT_INTERNET
        Global_ServerMode = TRUE;
        DefPlayer.whiteInternetConnect.waitForClient = 0;
        DefPlayer.blackInternetConnect.waitForClient = 0;
        SetWindowText ( GetDlgItem(hwnd,IDC_REMOTE_IP), DefaultServerIpAddress );
#endif

        SetWindowText ( GetDlgItem(hwnd,TB_WTIME), WhiteTimeLimit );
        SetWindowText ( GetDlgItem(hwnd,TB_BTIME), BlackTimeLimit );

        EnableWindow ( GetDlgItem(hwnd,TB_W_PIPE_NAME), FALSE );
        EnableWindow ( GetDlgItem(hwnd,TB_B_PIPE_NAME), FALSE );

        bool whiteHuman = true;
        bool blackHuman = true;

        if (PreDefinedPlayerSides)
        {
            // We already know what the probable chess player settings should be.
            // This can be because we have loaded a PGN file and found the player settings from it,
            // or because the user has told us to start a new game via File|New.
            // We will still show the dialog box so the user can override the settings.
            whiteHuman = PreDefinedWhiteHuman;
            blackHuman = PreDefinedBlackHuman;
        }
        else
        {
            // Choose randomly for the human player to play White or Black,
            // and the computer to play the opposite side.
            whiteHuman = (ChessRandom(2) != 0);
            blackHuman = !whiteHuman;
        }

        if (whiteHuman)
        {
            // Turn on the 'human' radio button for white player...
            CheckRadioButton ( hwnd, RB_WHUMAN, RB_WCOMPUTER, RB_WHUMAN );

            // Disable think time for White computer player:
            EnableWindow ( GetDlgItem(hwnd,TB_WTIME), FALSE );
        }
        else
        {
            // Turn on the 'computer' radio button for white player...
            CheckRadioButton ( hwnd, RB_WHUMAN, RB_WCOMPUTER, RB_WCOMPUTER );
            SetFocus ( GetDlgItem(hwnd,TB_WTIME) );

            // Disable blunder alert checkbox for white player...
            EnableWindow ( GetDlgItem(hwnd,IDC_CHECK_BLUNDER_ALERT_WHITE), FALSE );
        }

        if (blackHuman)
        {
            // Turn on the 'human' radio button for black player...
            CheckRadioButton ( hwnd, RB_BHUMAN, RB_BCOMPUTER, RB_BHUMAN );

            // Disable think time for Black computer player:
            EnableWindow ( GetDlgItem(hwnd,TB_BTIME), FALSE );
        }
        else
        {
            // Turn on the 'computer' radio button for black player...
            CheckRadioButton ( hwnd, RB_BHUMAN, RB_BCOMPUTER, RB_BCOMPUTER );
            SetFocus ( GetDlgItem(hwnd,TB_BTIME) );

            // Disable blunder alert checkbox for black player...
            EnableWindow ( GetDlgItem(hwnd,IDC_CHECK_BLUNDER_ALERT_BLACK), FALSE );
        }
    }
    break;

    case WM_COMMAND:
    {
        switch ( wparam )
        {
        case RB_WHUMAN:
            // Disable white's pipe name box
            EnableWindow ( GetDlgItem(hwnd,TB_W_PIPE_NAME), FALSE);

            // Disable the white computer player's think time box
            EnableWindow ( GetDlgItem(hwnd,TB_WTIME), FALSE );

            // Enable blunder alert checkbox for white player.
            EnableWindow ( GetDlgItem(hwnd,IDC_CHECK_BLUNDER_ALERT_WHITE), TRUE );
            break;

        case RB_WINTERNET:
            // Disable white's pipe name box
            EnableWindow ( GetDlgItem(hwnd,TB_W_PIPE_NAME), FALSE);

            // Disable the white computer player's think time box
            EnableWindow ( GetDlgItem(hwnd,TB_WTIME), FALSE );

            // Disable blunder alert checkbox for white player.
            EnableWindow ( GetDlgItem(hwnd,IDC_CHECK_BLUNDER_ALERT_WHITE), FALSE );
            break;

        case RB_W_NAMEDPIPE:
            // Enable white's pipe name box
            EnableWindow ( GetDlgItem(hwnd,TB_W_PIPE_NAME), TRUE);

            // Disable the white computer player's think time box
            EnableWindow ( GetDlgItem(hwnd,TB_WTIME), FALSE );

            // Disable blunder alert checkbox for white player.
            EnableWindow ( GetDlgItem(hwnd,IDC_CHECK_BLUNDER_ALERT_WHITE), FALSE );
            break;

        case RB_WCOMPUTER:
            // Disable white's pipe name box
            EnableWindow ( GetDlgItem(hwnd,TB_W_PIPE_NAME), FALSE);

            // Enable the white computer player's think time box
            EnableWindow ( GetDlgItem(hwnd,TB_WTIME), TRUE );
            SetFocus ( GetDlgItem(hwnd,TB_WTIME) );

            // Disable blunder alert checkbox for white player.
            EnableWindow ( GetDlgItem(hwnd,IDC_CHECK_BLUNDER_ALERT_WHITE), FALSE );
            break;

        case RB_BHUMAN:
            // Disable black's pipe name box
            EnableWindow ( GetDlgItem(hwnd,TB_B_PIPE_NAME), FALSE);

            // Disable the black computer player's think time box
            EnableWindow ( GetDlgItem(hwnd,TB_BTIME), FALSE );

            // Enable blunder alert checkbox for black player.
            EnableWindow ( GetDlgItem(hwnd,IDC_CHECK_BLUNDER_ALERT_BLACK), TRUE );
            break;

        case RB_BINTERNET:
            // Disable black's pipe name box
            EnableWindow ( GetDlgItem(hwnd,TB_B_PIPE_NAME), FALSE);

            // Disable the black computer player's think time box
            EnableWindow ( GetDlgItem(hwnd,TB_BTIME), FALSE );

            // Disable blunder alert checkbox for black player.
            EnableWindow ( GetDlgItem(hwnd,IDC_CHECK_BLUNDER_ALERT_BLACK), FALSE );
            break;

        case RB_B_NAMEDPIPE:
            // Enable black's pipe name box
            EnableWindow ( GetDlgItem(hwnd,TB_B_PIPE_NAME), TRUE);

            // Disable the black computer player's think time box
            EnableWindow ( GetDlgItem(hwnd,TB_BTIME), FALSE );

            // Disable blunder alert checkbox for black player.
            EnableWindow ( GetDlgItem(hwnd,IDC_CHECK_BLUNDER_ALERT_BLACK), FALSE );
            break;

        case RB_BCOMPUTER:
            // Disable black's pipe name box
            EnableWindow ( GetDlgItem(hwnd,TB_B_PIPE_NAME), FALSE);

            // Enable the black computer player's think time box
            EnableWindow ( GetDlgItem(hwnd,TB_BTIME), TRUE );
            SetFocus ( GetDlgItem(hwnd,TB_BTIME) );

            // Disable blunder alert checkbox for black player.
            EnableWindow ( GetDlgItem(hwnd,IDC_CHECK_BLUNDER_ALERT_BLACK), FALSE );
            break;

#if SUPPORT_INTERNET
        case IDC_CHESS_CLIENT:
        {
            BOOL remoteFlag = IsDlgButtonChecked ( hwnd, IDC_CHESS_CLIENT );
            Global_ServerMode = !remoteFlag;

            EnableWindow ( GetDlgItem(hwnd,RB_WHUMAN),      !remoteFlag );
            EnableWindow ( GetDlgItem(hwnd,RB_WCOMPUTER),   !remoteFlag );
            EnableWindow ( GetDlgItem(hwnd,RB_WINTERNET),   !remoteFlag );
            EnableWindow ( GetDlgItem(hwnd,RB_BHUMAN),      !remoteFlag );
            EnableWindow ( GetDlgItem(hwnd,RB_BCOMPUTER),   !remoteFlag );
            EnableWindow ( GetDlgItem(hwnd,RB_BINTERNET),   !remoteFlag );
            EnableWindow ( GetDlgItem(hwnd,RB_W_NAMEDPIPE), !remoteFlag );
            EnableWindow ( GetDlgItem(hwnd,RB_B_NAMEDPIPE), !remoteFlag );
            EnableWindow ( GetDlgItem(hwnd,IDC_CHECK_BLUNDER_ALERT_WHITE), !remoteFlag );
            EnableWindow ( GetDlgItem(hwnd,IDC_CHECK_BLUNDER_ALERT_BLACK), !remoteFlag );

            EnableWindow ( GetDlgItem(hwnd,IDC_REMOTE_IP), remoteFlag );
            if ( remoteFlag )
                SetFocus ( GetDlgItem(hwnd,IDC_REMOTE_IP) );

            BOOL enableTime = remoteFlag ? FALSE : IsDlgButtonChecked(hwnd,RB_WCOMPUTER);
            EnableWindow ( GetDlgItem(hwnd,TB_WTIME), enableTime );

            enableTime = remoteFlag ? FALSE : IsDlgButtonChecked(hwnd,RB_BCOMPUTER);
            EnableWindow ( GetDlgItem(hwnd,TB_BTIME), enableTime );
        }
        break;
#endif // SUPPORT_INTERNET


        case IDOK:
        {
            result = TRUE;
            SoundEnableFlag = TRUE;

#if SUPPORT_INTERNET
            if ( IsDlgButtonChecked ( hwnd, IDC_CHESS_CLIENT ) )
            {
                GetWindowText ( GetDlgItem(hwnd,IDC_REMOTE_IP), DefaultServerIpAddress, sizeof(DefaultServerIpAddress) );
                result = ConnectToChenardServer ( hwnd, DefPlayer, DefaultServerIpAddress );
                if ( result )
                    EndDialog ( hwnd, IDOK );

                return TRUE;
            }
#endif


            // Figure out whether white is human or computer
            if ( IsDlgButtonChecked ( hwnd, RB_WHUMAN ) )
            {
                DefPlayer.whiteType = DefPlayerInfo::humanPlayer;
            }
            else if ( IsDlgButtonChecked ( hwnd, RB_WCOMPUTER ) )
            {
                DefPlayer.whiteType = DefPlayerInfo::computerPlayer;
            }
            else if (IsDlgButtonChecked (hwnd, RB_W_NAMEDPIPE))
            {
                DefPlayer.whiteType = DefPlayerInfo::namedPipePlayer;
            }
            else
            {
                DefPlayer.whiteType = DefPlayerInfo::internetPlayer;
                DefPlayer.whiteInternetConnect.waitForClient = 1;
            }

            // Think time for white
            double value = 0;

            if ( !GetWindowText ( GetDlgItem(hwnd,TB_WTIME), WhiteTimeLimit, sizeof(WhiteTimeLimit) ) ||
                    (value = atof(WhiteTimeLimit)) <= 0 )
            {
                strcpy ( WhiteTimeLimit, defaultTimeLimit );
                value = atof(defaultTimeLimit);
            }

            if ( value < 0.1 )
                value = 0.1;

            DefPlayer.whiteThinkTime = long (100 * value);
            DefPlayer.whiteUseSeconds = 1;
            DefPlayer.whiteSearchBias = 1;

            // Figure out whether black is human or computer

            if ( IsDlgButtonChecked ( hwnd, RB_BHUMAN ) )
            {
                DefPlayer.blackType = DefPlayerInfo::humanPlayer;
            }
            else if ( IsDlgButtonChecked ( hwnd, RB_BCOMPUTER ) )
            {
                DefPlayer.blackType = DefPlayerInfo::computerPlayer;
            }
            else if (IsDlgButtonChecked (hwnd, RB_B_NAMEDPIPE))
            {
                DefPlayer.blackType = DefPlayerInfo::namedPipePlayer;
            }
            else
            {
                DefPlayer.blackType = DefPlayerInfo::internetPlayer;
                DefPlayer.blackInternetConnect.waitForClient = 1;
            }

            // Think time for black

            if ( !GetWindowText ( GetDlgItem(hwnd,TB_BTIME), BlackTimeLimit, sizeof(BlackTimeLimit) ) ||
                    (value = atof(BlackTimeLimit)) <= 0 )
            {
                strcpy ( BlackTimeLimit, defaultTimeLimit );
                value = atof(defaultTimeLimit);
            }

            if ( value < 0.1 )
                value = 0.1;

            DefPlayer.blackThinkTime = long(100 * value);
            DefPlayer.blackUseSeconds = 1;
            DefPlayer.blackSearchBias = 1;

            // Blunder alert
            DefPlayer.whiteBlunderAlert = IsDlgButtonChecked(hwnd, IDC_CHECK_BLUNDER_ALERT_WHITE) ? true : false;
            DefPlayer.blackBlunderAlert = IsDlgButtonChecked(hwnd, IDC_CHECK_BLUNDER_ALERT_BLACK) ? true : false;

            // Server names
            GetWindowText (GetDlgItem(hwnd,TB_W_PIPE_NAME), DefPlayer.whiteServerName, sizeof(DefPlayer.whiteServerName));
            GetWindowText (GetDlgItem(hwnd,TB_B_PIPE_NAME), DefPlayer.blackServerName, sizeof(DefPlayer.blackServerName));

            EndDialog ( hwnd, IDOK );
        }
        break;

        case IDCANCEL:
        {
            EndDialog ( hwnd, IDCANCEL );
        }
        break;
        }
    }
    break;

    default:
        result = FALSE;
        break;
    }

    return result;
}


bool DefinePlayerDialog ( HWND hwnd )
{
    if ( Global_EnableGA || Global_EnableEndgameDatabaseGen )
        return true;

    if (Global_PgnFileWasLoaded && !Global_DialogBoxWhenLoadingPgn)
    {
        DefPlayer.whiteType = DefPlayerInfo::humanPlayer;
        DefPlayer.blackType = DefPlayerInfo::humanPlayer;
        return true;    // skip dialog box
    }

#if SUPPORT_INTERNET
    int netFlag = 0;
    FILE *netFile = fopen ( "chenard.net", "r" );
    if ( netFile )
    {
        fclose (netFile);
        netFlag = 1;
    }

    LPTSTR restag = netFlag ?
                    MAKEINTRESOURCE(IDD_DEFINE_PLAYERS2) :
                    MAKEINTRESOURCE(IDD_DEFINE_PLAYERS);
#else
    LPTSTR restag = MAKEINTRESOURCE(IDD_DEFINE_PLAYERS);
#endif

    INT_PTR fred = DialogBox (
                       global_hInstance,
                       restag,
                       hwnd,
                       DLGPROC(DefineChessPlayers_DlgProc) );

#if SUPPORT_INTERNET
    char tempString [256];

    if ( fred == IDOK )
    {
        int iAmServer = Global_ServerMode &&
                        (DefPlayer.whiteType == DefPlayerInfo::internetPlayer ||
                         DefPlayer.blackType == DefPlayerInfo::internetPlayer);

        if ( iAmServer )
        {
            if ( !InitializeNetwork(hwnd) )
                return false;

            sockaddr_in localAddress;
            memset ( &localAddress, 0, sizeof(localAddress) );
            localAddress.sin_family = AF_INET;
            localAddress.sin_addr.s_addr = INADDR_ANY;
            localAddress.sin_port = htons(InternetChessPlayer::QueryServerPortNumber());

            if ( bind ( Global_MySocket, (sockaddr *)&localAddress, sizeof(localAddress) ) != 0 )
            {
                MessageBox ( hwnd, "Could not bind socket!", "Network Error", MB_OK | MB_ICONERROR );
                return FALSE;
            }

            ServerConnectThreadId = _beginthread ( ServerConnectThreadFunc, 0, NULL );
            if ( long(ServerConnectThreadId) == -1 )
            {
                MessageBox ( hwnd, "Cannot create server thread!", "Bad Problem!", MB_OK | MB_ICONERROR );
                return false;
            }

            sprintf ( tempString, "Inform opponent(s) that server address is:\n\n%u.%u.%u.%u",
                      unsigned ( Global_MyHostEnt.h_addr[0] & 0xff ),
                      unsigned ( Global_MyHostEnt.h_addr[1] & 0xff ),
                      unsigned ( Global_MyHostEnt.h_addr[2] & 0xff ),
                      unsigned ( Global_MyHostEnt.h_addr[3] & 0xff ) );

            MessageBox ( hwnd, tempString, "", MB_OK | MB_ICONINFORMATION );
        }
    }
#endif

    return (fred == IDOK) ? true : false;
}

bool Global_PrevBlunderAlertState = false;     // was window resized to include blunder alert area?

static BOOL SetChessWindowSize ( HWND hwnd, bool showDebugInfo )
{
    RECT rectDesktop;
    GetWindowRect ( GetDesktopWindow(), &rectDesktop );

    int newDX = CHESS_WINDOW_DX;
    int newDY = CHESS_WINDOW_DY;

    if (showDebugInfo)
    {
        newDX += DEBUG_WINDOW_DX;
    }

    // We cache whether we resized the window for later determining
    // whether a change is warranted.
    // (Consider refactoring this to just remember previous DX, DY.)
    Global_PrevBlunderAlertState = Global_UI && Global_UI->blunderAlert_EnabledForEitherPlayer();
    if (Global_PrevBlunderAlertState)
    {
        newDY += BLUNDER_WINDOW_DY;
    }

    RECT newFrameRect;
    newFrameRect.left   = (rectDesktop.right - newDX) / 2;
    newFrameRect.right  = newFrameRect.left + newDX;
    newFrameRect.top    = (rectDesktop.bottom - newDY) / 2;
    newFrameRect.bottom = newFrameRect.top + newDY;

    if ( AdjustWindowRect ( &newFrameRect, global_windowFlags, TRUE ) )
    {
        if ( newFrameRect.left < 0 )
        {
            newFrameRect.right -= newFrameRect.left;
            newFrameRect.left = 0;
        }

        if ( newFrameRect.top < 0 )
        {
            newFrameRect.bottom -= newFrameRect.top;
            newFrameRect.top = 0;
        }

        return SetWindowPos (
            hwnd, HWND_TOP,
            newFrameRect.left,
            newFrameRect.top,
            newFrameRect.right - newFrameRect.left,
            newFrameRect.bottom - newFrameRect.top,
            SWP_SHOWWINDOW );
    }

    return FALSE;   // some error occurred in AdjustWindowRect
}


const char Profile_ViewDebugInfo[]      = "ViewDebugInfo";
const char Profile_WhiteTimeLimit[]     = "WhiteTimeLimit";
const char Profile_BlackTimeLimit[]     = "BlackTimeLimit";
const char Profile_SpeakMovesFlag[]     = "SpeakMovesFlag";
const char Profile_ResignFlag[]         = "ResignFlag";
const char Profile_AnimateMoves[]       = "AnimateMoves";
const char Profile_HiliteMoves[]        = "HiliteMoves";
const char Profile_AutoSingular[]       = "AutoSingular";
const char Profile_ExtendedSearch[]     = "ExtendedSearch";
const char Profile_ServerIpAddress[]    = "ServerIpAddress";
const char Profile_EnableOppTime[]      = "ThinkOnOpponentTime";
const char Profile_EnableBlunderAlert[] = "BlunderAlert";
const char Profile_AnnounceMate[]       = "AnnounceMate";
const char Profile_SuggestThinkTime[]   = "SuggestThinkTime";
const char Profile_SuggestThinkOnly[]   = "SuggestThinkOnly";
const char Profile_DingAfterSuggest[]   = "DingAfterSuggest";
const char Profile_PgnDialogBox[]       = "PgnDialogBox";
const char Profile_BlunderThreshold[]   = "BlunderAlertThreshold";

const char * const CHENARD_REGISTRY_KEY_NAME = "Chenard";

void ReadChenardSetting (const char *key, const char *defaultValue, char *opt, size_t optSize)
{
    bool readValue = false;     // if remains false, it means we failed to read data and will use defaultValue

    HKEY softwareKeyHandle;
    LSTATUS status = RegOpenKeyEx (
        HKEY_CURRENT_USER,
        "Software",
        0,
        KEY_READ,
        &softwareKeyHandle
    );

    if (status == ERROR_SUCCESS)
    {
        HKEY chenardKey;

        status = RegOpenKeyEx (
            softwareKeyHandle,
            CHENARD_REGISTRY_KEY_NAME,
            0,
            KEY_READ,
            &chenardKey
        );

        if (status == ERROR_SUCCESS)
        {
            DWORD dataType;
            DWORD dataSize = (DWORD) optSize;
            status = RegQueryValueEx (chenardKey, key, 0, &dataType, (LPBYTE) opt, &dataSize);
            if (status == ERROR_SUCCESS)
            {
                if (dataType == REG_SZ)
                {
                    opt[dataSize] = '\0';   // registry functions aren't guaranteed to null-terminate strings for us
                    readValue = true;       // we have read a valid value; do not fall back on the default value passed in to this function
                }
            }

            RegCloseKey (chenardKey);
        }

        RegCloseKey (softwareKeyHandle);
    }


    if (!readValue)
    {
        strcpy_s (opt, optSize, defaultValue);
    }
}


LSTATUS WriteChenardSetting (const char *key, const char *value)
{
    HKEY softwareKeyHandle;
    LSTATUS status = RegOpenKeyEx (
        HKEY_CURRENT_USER,
        "Software",
        0,
        KEY_ALL_ACCESS,
        &softwareKeyHandle
    );

    if (status == ERROR_SUCCESS)
    {
        HKEY chenardKey;

        status = RegCreateKeyEx (
            softwareKeyHandle, 
            CHENARD_REGISTRY_KEY_NAME, 
            0, 
            NULL, 
            0, 
            KEY_ALL_ACCESS, 
            NULL, 
            &chenardKey, 
            NULL
        );

        if (status == ERROR_SUCCESS)
        {
            status = RegSetValueEx (chenardKey, key, 0, REG_SZ, (const BYTE *)value, (DWORD) (1 + strlen(value)));
            RegCloseKey (chenardKey);
        }

        RegCloseKey (softwareKeyHandle);
    }

    return status;
}


static void LoadChessOptions()
{
    char opt[128];

    ReadChenardSetting ( Profile_ViewDebugInfo, "0", opt, sizeof(opt) );
    Global_AnalysisType = atoi(opt);

    ReadChenardSetting ( Profile_WhiteTimeLimit, "2", WhiteTimeLimit, sizeof(WhiteTimeLimit) );
    ReadChenardSetting ( Profile_BlackTimeLimit, "2", BlackTimeLimit, sizeof(BlackTimeLimit) );

    ReadChenardSetting ( Profile_SpeakMovesFlag, "0", opt, sizeof(opt) );
    Global_SpeakMovesFlag = (atoi(opt) != 0);

    ReadChenardSetting ( Profile_ResignFlag, "0", opt, sizeof(opt) );
    Global_AllowResignFlag = (atoi(opt) != 0);

    ReadChenardSetting ( Profile_AnimateMoves, "1", opt, sizeof(opt) );
    Global_AnimateMoves = (atoi(opt) != 0);

    ReadChenardSetting ( Profile_EnableOppTime, "0", opt, sizeof(opt) );
    Global_EnableOppTime = (atoi(opt) != 0);

    ReadChenardSetting ( Profile_HiliteMoves, "1", opt, sizeof(opt) );
    Global_HiliteMoves = (atoi(opt) != 0);

    ReadChenardSetting ( Profile_AutoSingular, "0", opt, sizeof(opt) );
    Global_AutoSingular = (atoi(opt) != 0);

    ReadChenardSetting ( Profile_ExtendedSearch, "0", opt, sizeof(opt) );
    Global_ExtendedSearch = (atoi(opt) != 0);

    ReadChenardSetting ( Profile_ServerIpAddress, "", DefaultServerIpAddress, sizeof(DefaultServerIpAddress) );

    ReadChenardSetting ( Profile_AnnounceMate, "1", opt, sizeof(opt) );
    Global_EnableMateAnnounce = (atoi(opt) != 0);

    ReadChenardSetting ( Profile_SuggestThinkTime, "500", opt, sizeof(opt) );
    SuggestThinkTime = (INT32) atoi(opt);

    ReadChenardSetting ( Profile_SuggestThinkOnly, "0", opt, sizeof(opt) );
    Global_SuggestThinkOnly = (atoi(opt) != 0);

    ReadChenardSetting ( Profile_DingAfterSuggest, "0", opt, sizeof(opt) );
    Global_DingAfterSuggest = (atoi(opt) != 0);

    ReadChenardSetting ( Profile_PgnDialogBox, "1", opt, sizeof(opt) );
    Global_DialogBoxWhenLoadingPgn = (atoi(opt) != 0);

    ReadChenardSetting ( Profile_BlunderThreshold, "0", opt, sizeof(opt) );
    {
        // Enforce sane limits for blunder alert threshold.
        const int thresh = atoi(opt);
        if ((thresh >= MIN_BLUNDER_ALERT_THRESHOLD) && (thresh <= MAX_BLUNDER_ALERT_THRESHOLD))
        {
            Global_BlunderAlertThreshold = static_cast<SCORE> (thresh);
        }
        else
        {
            Global_BlunderAlertThreshold = static_cast<SCORE> (DEFAULT_BLUNDER_ALERT_THRESHOLD);
        }
    }
}


static void ApplyChessOptions()
{
    // Set the analysis checkmarks
    UINT uCheck = (Global_AnalysisType==0) ? MF_CHECKED : MF_UNCHECKED;
    CheckMenuItem ( HmenuMain, ID_VIEW_ANALYSIS_NONE, uCheck );
    uCheck = (Global_AnalysisType==1) ? MF_CHECKED : MF_UNCHECKED;
    CheckMenuItem ( HmenuMain, ID_VIEW_ANALYSIS_BESTPATH, uCheck );
    uCheck = (Global_AnalysisType==2) ? MF_CHECKED : MF_UNCHECKED;
    CheckMenuItem ( HmenuMain, ID_VIEW_ANALYSIS_CURRENTPATH, uCheck );

    // Set the checkmark for speaking moves...
    uCheck = Global_SpeakMovesFlag ? MF_CHECKED : MF_UNCHECKED;
    CheckMenuItem ( HmenuMain, ID_VIEW_SPEAKMOVESTHROUGHSOUNDCARD, uCheck );

    // set checkmark for resign flag
    uCheck = Global_AllowResignFlag ? MF_CHECKED : MF_UNCHECKED;
    CheckMenuItem ( HmenuMain, ID_GAME_ALLOWCOMPUTERTORESIGN, uCheck );

    uCheck = Global_AnimateMoves ? MF_CHECKED : MF_UNCHECKED;
    CheckMenuItem ( HmenuMain, ID_ANIMATE, uCheck );

    uCheck = Global_HiliteMoves ? MF_CHECKED : MF_UNCHECKED;
    CheckMenuItem ( HmenuMain, ID_HIGHLIGHT_MOVE, uCheck );

    uCheck = Global_AutoSingular ? MF_CHECKED : MF_UNCHECKED;
    CheckMenuItem ( HmenuMain, ID_GAME_AUTO_SINGULAR, uCheck );

    uCheck = Global_ExtendedSearch ? MF_CHECKED : MF_UNCHECKED;
    CheckMenuItem ( HmenuMain, ID_GAME_ALLOWEXTENDEDSEARCH, uCheck );

    uCheck = Global_EnableOppTime ? MF_CHECKED : MF_UNCHECKED;
    CheckMenuItem ( HmenuMain, ID_GAME_OPPTIME, uCheck );

    uCheck = Global_EnableMateAnnounce ? MF_CHECKED : MF_UNCHECKED;
    CheckMenuItem (HmenuMain, ID_VIEW_ANNOUNCEMATE, uCheck);
}


static void SaveChessOptions()
{
    char opt [64];

    sprintf ( opt, "%d", Global_AnalysisType );
    WriteChenardSetting (Profile_ViewDebugInfo, opt);

    WriteChenardSetting (Profile_WhiteTimeLimit, WhiteTimeLimit);
    WriteChenardSetting (Profile_BlackTimeLimit, BlackTimeLimit);

    sprintf ( opt, "%d", int(Global_SpeakMovesFlag) );
    WriteChenardSetting (Profile_SpeakMovesFlag, opt);

    sprintf ( opt, "%d", int(Global_AllowResignFlag) );
    WriteChenardSetting (Profile_ResignFlag, opt);

    sprintf ( opt, "%d", int(Global_AnimateMoves) );
    WriteChenardSetting (Profile_AnimateMoves, opt);

    sprintf ( opt, "%d", int(Global_HiliteMoves) );
    WriteChenardSetting (Profile_HiliteMoves, opt);

    sprintf ( opt, "%d", int(Global_AutoSingular) );
    WriteChenardSetting (Profile_AutoSingular, opt);

    sprintf ( opt, "%d", int(Global_ExtendedSearch) );
    WriteChenardSetting (Profile_ExtendedSearch, opt);

    sprintf ( opt, "%d", int(Global_EnableOppTime) );
    WriteChenardSetting (Profile_EnableOppTime, opt);

    if (DefaultServerIpAddress[0] != '\0')
    {
        WriteChenardSetting (Profile_ServerIpAddress, DefaultServerIpAddress);
    }

    sprintf (opt, "%d", int(Global_EnableMateAnnounce));
    WriteChenardSetting (Profile_AnnounceMate, opt);

    sprintf (opt, "%d", int(SuggestThinkTime));
    WriteChenardSetting (Profile_SuggestThinkTime, opt);

    sprintf (opt, "%d", int(Global_SuggestThinkOnly));
    WriteChenardSetting (Profile_SuggestThinkOnly, opt);

    sprintf (opt, "%d", int(Global_DingAfterSuggest));
    WriteChenardSetting (Profile_DingAfterSuggest, opt);

    sprintf (opt, "%d", int(Global_DialogBoxWhenLoadingPgn));
    WriteChenardSetting (Profile_PgnDialogBox, opt);

    sprintf (opt, "%d", int(Global_BlunderAlertThreshold));
    WriteChenardSetting (Profile_BlunderThreshold, opt);
}

bool Global_StartNewGameConfirmed = true;   // used by File|New command

void ChessThreadFunc ( void * )
{
    // Note that we DO NOT reset the board state - might have been loaded from PGN file.

    ChessUI_win32_gui  theUserInterface ( HwndMain );
    Global_UI = &theUserInterface;
    TheBoardDisplayBuffer.update ( Global_Board );

    if ( Global_EnableGA )
    {
        // Run the genetic algorithm to evolve eval
        // and move ordering heuristics...

        theUserInterface.allowMateAnnounce ( false );
        Global_AnimateMoves = false;

        ChessGA ga ( theUserInterface, 0 );
        if ( !ga.load() )
        {
            ga.createRandomPopulation ( Global_GA_NumGenes );
            ga.save();
        }

        ga.run();
        return;
    }

    if ( Global_EnableEndgameDatabaseGen )
    {
        // Run the endgame database generator.

        GenerateEndgameDatabases ( Global_Board, theUserInterface );
        return;
    }

    // Loop forever.  The only way out is if the program is closed.
    for(;;)
    {
        // Each time around the loop we create new player objects
        // and play the game, but we CANNOT assume that the board
        // is at the beginning of a new game; it could have been
        // loaded from a PGN file, or the user may have undone
        // moves after having been checkmated, etc.

        // Clear the flag so as to prevent the game from being restarted.
        // Only if the user says to start a new game do we loop again.
        // Each loop causes 'theGame' to be constructed/destructed,
        // which in turn refreshes the player definitions based on
        // global DefPlayer struct.
        Global_StartNewGameConfirmed = false;

        // Constructing the ChessGame object causes
        // CreatePlayer calls to the UI object for White and Black.
        // If this isn't the first time they have been called, we
        // need to tell them to explicitly forget anything they have
        // cached about what kind of players White and Black are
        // (i.e. Human, Computer, other).  This way logic inside
        // CreatePlayer can execute only after both players have
        // been re-created.
        theUserInterface.ResetPlayers();

        // Create the game object, which as a side-effect causes
        // player creation via the user interface.
        ChessGame theGame ( Global_Board, theUserInterface );

        // Creating the players also has the side-effect of setting
        // blunder alert flags inside theUserInterface.
        // This is the ideal time to resize the chess window if needed.
        // Avoid repositioning the window unless we really do need to resize it...
        if (Global_PrevBlunderAlertState != theUserInterface.blunderAlert_EnabledForEitherPlayer())
        {
            PostMessage(HwndMain, UINT(WM_UPDATE_CHESS_WINDOW_SIZE), 0, 0);
        }

        UpdateGameStateText(Global_Board);

        // Apply global options to chess player objects, depending
        // on whether the player is human/computer.

        if ( theUserInterface.queryWhitePlayerType() == DefPlayerInfo::humanPlayer )
        {
            HumanChessPlayer *human = (HumanChessPlayer *) theUserInterface.queryWhitePlayer();
            if ( human )
            {
                human->setAutoSingular (Global_AutoSingular);
            }
        }

        if ( theUserInterface.queryBlackPlayerType() == DefPlayerInfo::humanPlayer )
        {
            HumanChessPlayer *human = (HumanChessPlayer *) theUserInterface.queryBlackPlayer();
            if ( human )
            {
                human->setAutoSingular (Global_AutoSingular);
            }
        }

        if ( theUserInterface.queryWhitePlayerType() == DefPlayerInfo::computerPlayer )
        {
            ComputerChessPlayer *puter = (ComputerChessPlayer *) theUserInterface.queryWhitePlayer();
            if ( puter )
            {
                puter->setExtendedSearchFlag (Global_ExtendedSearch);
            }
        }

        if ( theUserInterface.queryBlackPlayerType() == DefPlayerInfo::computerPlayer )
        {
            ComputerChessPlayer *puter = (ComputerChessPlayer *) theUserInterface.queryBlackPlayer();
            if ( puter )
            {
                puter->setExtendedSearchFlag (Global_ExtendedSearch);
            }
        }

        Global_GameOverFlag = false;
        theGame.Play();

        // Getting here means the game ended, one way or another.
        PostMessage ( HwndMain, UINT(WM_DDC_GAME_OVER), WPARAM(0), LPARAM(0) );

        while (!Global_StartNewGameConfirmed)
        {
            // The user did NOT confirm starting a new game, so
            // keep processing commands while displaying the board
            // in the state it was at the end of the game.
            Sleep(100);
            int dummySource=0, dummyDest=0;
            if ( ProcessChessCommands ( Global_Board, dummySource, dummyDest ) )
            {
                // It is time to re-create players and fire the game back up
                // using whatever state the board is in right now.
                break;
            }
        }
    }

    // !!! Unreachable code !!!
    globalImmedQuitFlag = true;
    Global_UI = 0;
}

PGN_FILE_STATE LoadGameFromPgnFile (FILE *f, ChessBoard &board);     // loads only the first game in the PGN file


void SetDirectoryFromFilePath (const char *filepath)
{
    const char *backslash = strrchr (filepath, '\\');
    if (backslash)
    {
        int length = (int) (backslash - filepath);
        char *path = new char [length + 1];
        memcpy (path, filepath, length);
        path[length] = '\0';

        _chdir (path);

        delete[] path;
    }
}


bool LoadGameFile (const char *arg)
{
    if (arg == NULL)
    {
        return false;   // should never happen, but avoid crash
    }

    const char *firstQuote = strchr (arg, '"');
    const char *secondQuote = firstQuote ? strchr(firstQuote+1,'"') : NULL;
    int firstIndex;
    int secondIndex;
    if (firstQuote && secondQuote)
    {
        // Assume the filename is the first group of characters that
        // are surrounded by double quote marks...
        firstIndex  = (int) (firstQuote - arg) + 1;     // skip over quote
        secondIndex = (int) (secondQuote - arg);        // point to quote (will be excluded)
    }
    else
    {
        // Assume the filename is the first group of non-whitespace characters...
        firstIndex = 0;
        while (arg[firstIndex] && isspace(arg[firstIndex]))
        {
            ++firstIndex;
        }

        if (arg[firstIndex])
        {
            secondIndex = firstIndex + 1;
            while (arg[secondIndex] && !isspace(arg[secondIndex]))
            {
                ++secondIndex;
            }
        }
        else
        {
            secondIndex = firstIndex;   // empty string
        }
    }

    int length = secondIndex - firstIndex;
    if (length <= 0)
    {
        return false;
    }

    // Extract the filename from the command line.
    // Make a dynamically allocated substring...
    char *filename = new char [1 + length];
    memcpy (filename, arg + firstIndex, length);
    filename[length] = '\0';

    bool loaded = false;

    char msg [256];
    const char *ext = strrchr (filename, '.');
    if (ext)
    {
        if (0 == stricmp (ext, ".pgn"))
        {
            // This appears to be a PGN file.
            // Use it to load the game.
            FILE *infile = fopen (filename, "rt");
            if (infile)
            {
                PGN_FILE_STATE state = LoadGameFromPgnFile (infile, Global_Board);
                if (state == PGN_FILE_STATE_FINISHED)
                {
                    // We successfully loaded the game state into the global chess board.
                    // Now we assist the user by trying to restore the player definitions.
                    // The dialog box will still show, but will be filled in with computer/human
                    // playing White/Black.
                    // If Chenard created the file, it will contain lines like these:
                    // [White "Human Player"]
                    // [Black "Computer Player (Chenard)"]
                    rewind (infile);
                    char line [128];
                    bool foundWhite = false;
                    bool foundBlack = false;
                    while (fgets (line, sizeof(line), infile))
                    {
                        if (0 == strcmp(line,"[White \"Human Player\"]\n"))
                        {
                            foundWhite = true;
                            PreDefinedWhiteHuman = true;
                        }
                        else if (0 == strcmp(line, "[Black \"Human Player\"]\n"))
                        {
                            foundBlack = true;
                            PreDefinedBlackHuman = true;
                        }
                        else if (0 == strcmp(line, "[White \"Computer Player (Chenard)\"]\n"))
                        {
                            foundWhite = true;
                            PreDefinedWhiteHuman = false;
                        }
                        else if (0 == strcmp(line, "[Black \"Computer Player (Chenard)\"]\n"))
                        {
                            foundBlack = true;
                            PreDefinedBlackHuman = false;
                        }
                    }
                    PreDefinedPlayerSides = (foundWhite && foundBlack);

                    // Remember this game filename so the user can save to it again...
                    strcpy_s (Global_GameFilename, sizeof(Global_GameFilename), filename);
                    Global_HaveFilename = true;
                    loaded = true;

                    // Set the current directory to the location of the executable.
                    // We assume this is where endgame, training, wav, etc files are...
                    SetDirectoryFromFilePath (__argv[0]);
                }
                else
                {
                    // Something is wrong with this PGN file.
                    // Let the user know we tried, but could not load the file...
                    const char *error = GetPgnFileStateString (state);
                    sprintf (msg, "Problem loading pgn file:\n%s", error);
                    MessageBox (HwndMain, msg, "PGN Load Error", MB_OK | MB_ICONERROR);

                    // Reset the board...
                    Global_Board.Init();
                }
                fclose (infile);
            }
            else
            {
                MessageBox (HwndMain, "Could not open the specified PGN file.", "PGN Load Error", MB_OK | MB_ICONERROR);
            }
        }
    }

    delete[] filename;
    return loaded;
}


int WINAPI WinMain(
    HINSTANCE   hInstance,          // handle to current instance
    HINSTANCE   hPrevInstance,      // handle to previous instance
    LPSTR       lpCmdLine,          // pointer to command line
    int         nCmdShow )          // show state of window
{
    global_hInstance = hInstance;
    LoadChessOptions();

    extern CRITICAL_SECTION SuggestMoveCriticalSection;
    InitializeCriticalSection (&SuggestMoveCriticalSection);

    static CHAR chessWindowClass[] = "DDC_Chenard";
    WNDCLASS wndclass;
    if ( !hPrevInstance )
    {
        wndclass.style       =  CS_HREDRAW | CS_VREDRAW;
        wndclass.lpfnWndProc =  ChessWndProc;
        wndclass.cbClsExtra  =  0;
        wndclass.cbWndExtra  =  0;
        wndclass.hInstance   =  hInstance;
        wndclass.hIcon       =  LoadIcon ( NULL, IDI_APPLICATION );
        wndclass.hCursor     =  LoadCursor ( NULL, IDC_ARROW );
        wndclass.hbrBackground   =  (HBRUSH) GetStockObject ( WHITE_BRUSH );
        wndclass.lpszMenuName    =  NULL;
        wndclass.lpszClassName   =  chessWindowClass;
        RegisterClass ( &wndclass );
    }

    if (lpCmdLine[0] != '\0')
    {
        if (LoadGameFile (lpCmdLine))
        {
            Global_PgnFileWasLoaded = true;
            // No need to examine the command line further: we have already loaded the game.
        }
        else if ( lpCmdLine[0] == '-' )
        {
            if ( lpCmdLine[1] == 'g' )
            {
                Global_EnableGA = true;
                if ( lpCmdLine[2] )
                {
                    Global_GA_NumGenes = atoi(&lpCmdLine[2]);
                }
            }
            else if ( lpCmdLine[1]=='d' && lpCmdLine[2]=='b' )        // -db : generate endgame database
            {
                Global_EnableEndgameDatabaseGen = true;
                Global_EnableBoardBreathe = false;

                switch (lpCmdLine[3])
                {
                case 'd':   // -dbd : dump info about existing databases
                    Global_EndgameDatabaseMode = 1;
                    break;

                case 's':   // -dbs : analyze symmetries of databases
                    Global_EndgameDatabaseMode = 2;
                    break;

                default:
                    Global_EndgameDatabaseMode = 0;     // -db : actually generate the databases
                    break;
                }
            }
        }
    }

    HmenuMain = LoadMenu ( hInstance, MAKEINTRESOURCE(999) );

    HwndMain = CreateWindow (
                   chessWindowClass,
                   TitleBarText_WhitesTurn,
                   global_windowFlags,
                   CW_USEDEFAULT,
                   CW_USEDEFAULT,
                   CW_USEDEFAULT,
                   CW_USEDEFAULT,
                   NULL,
                   HmenuMain,
                   hInstance,
                   NULL
               );

    ApplyChessOptions();
    SetChessWindowSize ( HwndMain, (Global_AnalysisType != 0) );

    HACCEL hAccel = LoadAccelerators ( hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR1) );

    MSG msg;
    msg.wParam = 0;

    int DisplayTimerEnabled = (SetTimer(HwndMain,2,500,NULL) != NULL);

    uintptr_t chessThreadID = _beginthread ( ChessThreadFunc, 0, NULL );
    if ( chessThreadID != (unsigned long)(-1) )
    {
        while ( GetMessage ( &msg, NULL, 0, 0 ) && !Global_AbortFlag )
        {
            if ( !TranslateAccelerator(HwndMain,hAccel,&msg) )
            {
                TranslateMessage (&msg);
                DispatchMessage (&msg);
            }
        }
    }
    else
    {
        // Put error code here!
        ChessBeep ( 880, 200 );
    }

    if ( DisplayTimerEnabled )
    {
        KillTimer ( HwndMain, 2 );
    }

    SaveChessOptions();

    ChessDisplayTextBuffer::DeleteAll();
    ExitProcess(0);   // This is needed to make sure the chess thread is killed
    return 0;
}


static void CheckHumanMoveState ( bool insideHumanMove )
{
    UINT uEnableHuman  = insideHumanMove ? (MF_BYCOMMAND | MF_ENABLED) : (MF_BYCOMMAND | MF_GRAYED);

    // Things to enable only when it is a human player's move...
    EnableMenuItem ( HmenuMain, ID_CHENARD_FILE_NEW, uEnableHuman );
    EnableMenuItem ( HmenuMain, ID_CHENARD_FILE_OPEN, uEnableHuman );
    EnableMenuItem ( HmenuMain, ID_CHENARD_FILE_SAVE, uEnableHuman );
    EnableMenuItem ( HmenuMain, ID_CHENARD_FILE_SAVEAS, uEnableHuman );
    EnableMenuItem ( HmenuMain, ID_EDIT_CLEARBOARD, uEnableHuman );
    EnableMenuItem ( HmenuMain, ID_EDIT_EDITMODE, uEnableHuman );
    EnableMenuItem ( HmenuMain, ID_EDIT_UNDOMOVE, uEnableHuman );
    EnableMenuItem ( HmenuMain, ID_EDIT_REDOMOVE, uEnableHuman );
    EnableMenuItem ( HmenuMain, ID_GAME_SUGGEST, uEnableHuman );
    EnableMenuItem ( HmenuMain, ID_GAME_SUGGESTAGAIN, uEnableHuman );
    EnableMenuItem ( HmenuMain, ID_ENTER_FEN, uEnableHuman );

    // Things to enable only when it is a computer player's move...
    // Allow user to abort computer thinking when it is the computer's turn, or when the suggest-move thinker is active...
    extern ComputerChessPlayer *Global_SuggestMoveThinker;
    UINT uEnableComputer = (insideHumanMove && (Global_SuggestMoveThinker == NULL)) ? (MF_BYCOMMAND | MF_GRAYED) : (MF_BYCOMMAND | MF_ENABLED);
    EnableMenuItem ( HmenuMain, ID_GAME_FORCEMOVE, uEnableComputer );
}


static void SendEditRequest ( int x, int y, SQUARE s )
{
    // If the following flag is set, it means that an edit
    // is still pending.  We don't want to get confused, so
    // we ignore mouse clicks until then.

    if ( Global_BoardEditRequest.getState() == BER_NOTHING )
    {
        Global_BoardEditRequest.sendSingleSquareEdit (x, y, s);
    }
}


static bool CopyForsythEdwardsNotation()
{
    const char *copyError = "Copy Forsyth-Edwards Notation Error";

    if (!OpenClipboard(HwndMain))
    {
        MessageBox (
            HwndMain,
            "Cannot open the Windows clipboard!",
            copyError,
            MB_ICONERROR | MB_OK
        );
        return false;
    }

    const unsigned bufSize = 256;
    char buffer [bufSize];

    if (!Global_Board.GetForsythEdwardsNotation (buffer, bufSize))
    {
        MessageBox (
            HwndMain,
            "Error getting FEN string from board!",
            copyError,
            MB_ICONERROR | MB_OK
        );
        return false;
    }

    bool result = true;
    HGLOBAL hGlobalMemory = GlobalAlloc ( GHND, strlen(buffer)+1 );
    if (hGlobalMemory)
    {
        char far *globalPtr = (char far *) GlobalLock (hGlobalMemory);
        if (globalPtr)
        {
            char far *a = globalPtr;
            const char far *b = buffer;
            do
            {
                *a++ = *b;
            }
            while ( *b++ );
            GlobalUnlock (hGlobalMemory);
            EmptyClipboard();
            SetClipboardData ( CF_TEXT, hGlobalMemory );
        }
        else
        {
            GlobalDiscard ( hGlobalMemory );
            MessageBox (
                HwndMain,
                "Error locking global memory handle!",
                copyError,
                MB_ICONERROR | MB_OK
            );

            result = false;
        }
    }
    else
    {
        MessageBox (
            HwndMain,
            "Error allocating global memory handle!",
            copyError,
            MB_ICONERROR | MB_OK
        );

        result = false;
    }

    CloseClipboard();
    return result;
}


static bool CopyGameListing()
{
    const char *copyError = "Copy Game Listing Error";
    if ( Global_Board.HasBeenEdited() )
    {
        MessageBox (
            HwndMain,
            "Cannot get game listing because board has been edited",
            copyError,
            MB_ICONERROR | MB_OK );
        return false;
    }

    if ( !OpenClipboard(HwndMain) )
    {
        MessageBox (
            HwndMain,
            "Cannot open the Windows clipboard!",
            copyError,
            MB_ICONERROR | MB_OK );
        return false;
    }

    const unsigned bufSize = 64 * 1024;
    char *buffer = new char [bufSize];
    if ( !buffer )
    {
        MessageBox (
            HwndMain,
            "Out of memory allocating buffer for game listing!",
            copyError,
            MB_ICONERROR | MB_OK );

        CloseClipboard();
        return false;
    }

    GetGameListing ( Global_Board, buffer, bufSize );

    bool result = true;
    HGLOBAL hGlobalMemory = GlobalAlloc ( GHND, strlen(buffer)+1 );
    if ( hGlobalMemory )
    {
        char far *globalPtr = (char far *) GlobalLock (hGlobalMemory);
        if ( globalPtr )
        {
            char far *a = globalPtr;
            const char far *b = buffer;
            do
            {
                *a++ = *b;
            }
            while ( *b++ );
            GlobalUnlock (hGlobalMemory);
            EmptyClipboard();
            SetClipboardData ( CF_TEXT, hGlobalMemory );
        }
        else
        {
            GlobalDiscard ( hGlobalMemory );
            MessageBox (
                HwndMain,
                "Error locking global memory handle!",
                copyError,
                MB_ICONERROR | MB_OK );

            result = false;
        }
    }
    else
    {
        MessageBox (
            HwndMain,
            "Error allocating global memory handle!",
            copyError,
            MB_ICONERROR | MB_OK );

        result = false;
    }

    CloseClipboard();
    delete[] buffer;
    return result;
}

void Chenard_FileSaveAs()
{
    static char filenameBuffer [256];
    strcpy ( filenameBuffer, "*.pgn" );

    OPENFILENAME ofn;
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = HwndMain;
    ofn.hInstance = NULL;
    ofn.lpstrFilter = "Portable Game Notation (*.pgn)\0.pgn\0Chenard Chess Games (*.gam)\0.gam\0";
    ofn.lpstrCustomFilter = NULL;
    ofn.nMaxCustFilter = 0;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = filenameBuffer;
    ofn.nMaxFile = sizeof(filenameBuffer);
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.lpstrTitle = "Save Game As";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.nFileOffset = 0;
    ofn.nFileExtension = 0;
    ofn.lpstrDefExt = "pgn";
    ofn.lCustData = 0;
    ofn.lpfnHook = NULL;
    ofn.lpTemplateName = NULL;

    if ( GetSaveFileName(&ofn) )
    {
        strcpy ( Global_GameFilename, filenameBuffer );
        Global_GameSaveFlag = true;
    }
}


void Chenard_FileSave()
{
    if ( Global_HaveFilename )
        Global_GameSaveFlag = true;
    else
        Chenard_FileSaveAs();
}


void Chenard_FileOpen()
{
    static char filenameBuffer [256];
    strcpy ( filenameBuffer, "*.pgn;*.gam" );

    OPENFILENAME ofn;
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = HwndMain;
    ofn.hInstance = NULL;
    ofn.lpstrFilter = "Chess Games\0*.pgn;*.gam\0";
    ofn.lpstrCustomFilter = NULL;
    ofn.nMaxCustFilter = 0;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = filenameBuffer;
    ofn.nMaxFile = sizeof(filenameBuffer);
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.lpstrTitle = "Open Game";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.nFileOffset = 0;
    ofn.nFileExtension = 0;
    ofn.lpstrDefExt = "pgn";
    ofn.lCustData = 0;
    ofn.lpfnHook = NULL;
    ofn.lpTemplateName = NULL;

    if ( GetOpenFileName(&ofn) )
    {
        strcpy ( Global_GameFilename, filenameBuffer );
        Global_GameOpenFlag = true;
    }
}


static bool WaveFilesExist()
{
    char c;
    char filename [256];

    // See if 1.WAV through 8.WAV exist...

    for ( c='1'; c<='8'; c++ )
    {
        sprintf ( filename, "%c.wav", c );
        if ( _access ( filename, 4 ) != 0 )  return false;
    }

    // See if A.WAV through H.WAV exist...
    for ( c='a'; c<='h'; c++ )
    {
        sprintf ( filename, "%c.wav", c );
        if ( _access ( filename, 4 ) != 0 )  return false;
    }

    // Check on the WAV files with the piece names

    if ( _access ( "knight.wav", 4 ) != 0 )     return false;
    if ( _access ( "bishop.wav", 4 ) != 0 )     return false;
    if ( _access ( "rook.wav", 4 ) != 0 )       return false;
    if ( _access ( "queen.wav", 4 ) != 0 )      return false;
    if ( _access ( "king.wav", 4 ) != 0 )       return false;

    // Other WAV files...

    if ( _access ( "check.wav", 4 ) != 0 )      return false;
    if ( _access ( "ep.wav", 4 ) != 0 )         return false;
    if ( _access ( "mate.wav", 4 ) != 0 )       return false;
    if ( _access ( "movesto.wav", 4 ) != 0 )    return false;
    if ( _access ( "oo.wav", 4 ) != 0 )         return false;
    if ( _access ( "ooo.wav", 4 ) != 0 )        return false;
    if ( _access ( "prom.wav", 4 ) != 0 )       return false;
    if ( _access ( "stale.wav", 4 ) != 0 )      return false;
    if ( _access ( "takes.wav", 4 ) != 0 )      return false;

    return true;
}


static void UpdateAnalysisDisplay ()
{
    // update nodes evaluated...

    extern ComputerChessPlayer *ComputerPlayerSearching;
    ComputerChessPlayer *cp = ComputerPlayerSearching;
    if ( Global_UI && cp )
    {
        extern INT32 TimeSearchStarted;

        INT32 currentTime = ChessTime();
        Global_UI->ReportComputerStats (
            currentTime - TimeSearchStarted,
            0,
            cp->queryNodesEvaluated(),
            0, 0, 0, 0 );
    }
}


static void DisplayGameReportText(
    HWND hwnd,
    const ChessUI_win32_gui::gameReport& report)
{
    const char *message = CHESS_PROGRAM_NAME;
    const char *brief = "";

    switch (report.winner)
    {
    case SIDE_WHITE:
        if (report.resignation)
        {
            switch (report.quitReason)
            {
            case qgr_lostConnection:
                message = CHESS_PROGRAM_NAME " - Lost connection to Black";
                brief = "Lost connection!";
                break;

            case qgr_startNewGame:
                // Leave messages blank
                break;

            case qgr_resign:
            default:
                message = CHESS_PROGRAM_NAME " - Black resigns";
                brief = "Black resigns";
                break;
            }
        }
        else
        {
            message = CHESS_PROGRAM_NAME " - White wins";
            brief = "1-0";
        }
        break;

    case SIDE_BLACK:
        if (report.resignation)
        {
            switch (report.quitReason)
            {
            case qgr_lostConnection:
                message = CHESS_PROGRAM_NAME " - Lost connection to White";
                brief = "Lost connection!";
                break;

            case qgr_startNewGame:
                // Leave messages blank
                break;

            case qgr_resign:
            default:
                message = CHESS_PROGRAM_NAME " - White resigns";
                brief = "White resigns";
                break;
            }
        }
        else
        {
            message = CHESS_PROGRAM_NAME " - Black wins";
            brief = "0-1";
        }
        break;

    case SIDE_NEITHER:
        message = CHESS_PROGRAM_NAME " - This game is a draw";
        brief = "\xbd-\xbd";    // 1/2 - 1/2
        break;

    default:
        message = CHESS_PROGRAM_NAME " - !!! I'm confused !!!";
        brief = "? ? ?";
        break;
    }

    if ( !Global_EnableGA && !Global_EnableEndgameDatabaseGen )
    {
        SetWindowText(hwnd, message);
        ChessDisplayTextBuffer::SetText(STATIC_ID_GAME_RESULT, brief);
    }
}


void UpdateGameStateText(ChessBoard& board)
{
    if (board.CurrentPlayerCanMove())
    {
        if (board.IsDefiniteDraw())
        {
            if (!Global_EnableGA && !Global_EnableEndgameDatabaseGen)
            {
                SetWindowText(HwndMain, CHESS_PROGRAM_NAME " - Drawn game");
                ChessDisplayTextBuffer::SetText(STATIC_ID_GAME_RESULT, "Drawn game");
            }
        }
        else
        {
            // Game is still in progress, so update to reflect current player's turn.
            UpdateGameStateText(board.WhiteToMove());
        }
    }
    else
    {
        // Game is over... figure out why.
        const char* message = "";
        const char* brief = "";

        if (board.CurrentPlayerInCheck())
        {
            if (board.WhiteToMove())
            {
                message = CHESS_PROGRAM_NAME " - Black wins";
                brief = "Black wins";
            }
            else
            {
                message = CHESS_PROGRAM_NAME " - White wins";
                brief = "White wins";
            }
        }
        else
        {
            message = CHESS_PROGRAM_NAME " - Stalemate";
            brief = "Stalemate";
        }

        if (!Global_EnableGA && !Global_EnableEndgameDatabaseGen)
        {
            SetWindowText(HwndMain, message);
            ChessDisplayTextBuffer::SetText(STATIC_ID_GAME_RESULT, brief);
        }
    }
}

void UpdateGameStateText(bool whiteToMove)
{
    // Update the title bar to reflect whose turn it is.
    SetWindowText (
        HwndMain,
        whiteToMove ? TitleBarText_WhitesTurn : TitleBarText_BlacksTurn
    );

    // The game is still going, so clear any prior end-of-game text
    // that is overlapping the center of the board.
    ChessDisplayTextBuffer::SetText(STATIC_ID_GAME_RESULT, "");
}


LRESULT CALLBACK ChessWndProc (
    HWND hwnd,
    UINT msg,
    WPARAM wparam,
    LPARAM lparam )
{
    BOOL result = FALSE;
    char buffer [128];
    static bool insideHumanMove = false;

    switch ( msg )
    {
    case WM_INITMENU:
        CheckHumanMoveState ( insideHumanMove );
        break;

    case WM_TIMER:
    {
#ifdef CHENARD_PROFILER
        if ( wparam == 1 )   // make sure it's really our timer
            ++ProfilerHitCount[ProfilerIndex];
#endif
        if ( wparam == 2 )   // analysis display update timer
            UpdateAnalysisDisplay ();
    }
    break;

    case WM_CLOSE:
    {
#if 0
        if ( Global_GameModifiedFlag )
        {
            int choice = MessageBox (
                             hwnd,
                             "Changes to game will be lost - do you really want to quit?",
                             ChessProgramName,
                             MB_ICONQUESTION | MB_YESNO );

            if ( choice == IDNO )
                break;   // Don't exit the program after all.
        }
#endif

#if SUPPORT_INTERNET
        if ( Global_NetworkInit )
        {
            WSACleanup();
            Global_NetworkInit = false;
        }
#endif

        PostMessage ( hwnd, WM_QUIT, 0, 0 );
    }
    break;

    case WM_CREATE:
    {
        if ( !DefinePlayerDialog(hwnd) )
        {
            Global_AbortFlag = true;
        }
        else
        {
            const int textSep = 2;
            const int textHeight = 16;
            const int textWidth = 8;

            ChessDisplayTextBuffer *tb = new ChessDisplayTextBuffer (
                hwnd, STATIC_ID_WHITES_MOVE, CHESS_BOARD_BORDER_DX, 0,
                ChessDisplayTextBuffer::margin_bottom, CHENARD_PROPORTIONAL_FONT );

            tb = new ChessDisplayTextBuffer (
                hwnd, STATIC_ID_BLACKS_MOVE, 170, 0,
                ChessDisplayTextBuffer::margin_bottom, CHENARD_PROPORTIONAL_FONT);

            tb = new ChessDisplayTextBuffer (
                hwnd, STATIC_ID_THINKING, SQUARE_SCREENX1(8) - 8*textWidth, 0,
                ChessDisplayTextBuffer::margin_bottom, CHENARD_PROPORTIONAL_FONT);

            int textY = 8;
            int textID;
            int depth = 0;
            for (textID = FIRST_RIGHT_TEXTID; textID <= LAST_RIGHT_TEXTID; textID++, depth++)
            {
                // Creating the object puts it in the class's list of instances
                tb = new ChessDisplayTextBuffer (
                    hwnd, textID, 0, textY,
                    ChessDisplayTextBuffer::margin_right, CHENARD_MONOSPACE_FONT,
                    (textID>=STATIC_ID_BESTPATH(0)) ? (depth & 1) : 0 );

                textY += (textHeight + textSep);
            }

            for (textID = STATIC_ID_ADHOC1; textID <= STATIC_ID_ADHOC2; ++textID)
            {
                tb = new ChessDisplayTextBuffer (
                    hwnd, textID, 0, textY,
                    ChessDisplayTextBuffer::margin_right, CHENARD_MONOSPACE_FONT, 0 );

                textY += (textHeight + textSep);
            }

            char coordString [2] = {0, 0};
            for (int coord = 0; coord < 8; ++coord)
            {
                tb = new ChessDisplayTextBuffer (
                    hwnd, STATIC_ID_COORD_RANK_BASE + coord,
                    0,  // x-coord will be calculated later by RepositionAll()
                    0,  // same for y-coord
                    ChessDisplayTextBuffer::margin_rank,
                    CHENARD_PROPORTIONAL_FONT, 
                    1 );

                coordString[0] = '1' + coord;
                tb->setText ( coordString );

                tb = new ChessDisplayTextBuffer (
                    hwnd, STATIC_ID_COORD_FILE_BASE + coord,
                    0,  // x-coord will be calculated later by RepositionAll()
                    0,  // same for y-coord
                    ChessDisplayTextBuffer::margin_file,
                    CHENARD_PROPORTIONAL_FONT,
                    1 );

                coordString[0] = 'a' + coord;
                tb->setText ( coordString );
            }

            // Create text buffer for displaying the result of the game,
            // e.g. "White Wins", "Black Wins", "Draw".
            // Most of the time it will contain an empty string.
            tb = new ChessDisplayTextBuffer (
                hwnd,
                STATIC_ID_GAME_RESULT,
                0,  // x-coord will be calculated later by RepositionAll()
                0,  // same for y-coord
                ChessDisplayTextBuffer::margin_center,
                CHENARD_GAMERESULT_FONT,
                1,
                false   // transparent background
            );

            // Use a special color for game result text...
            tb->setColorOverride(RGB(0xf0, 0x20, 0x20));

            // Create text buffer for displaying blunder analysis
            tb = new ChessDisplayTextBuffer(
                hwnd,
                STATIC_ID_BLUNDER_ANALYSIS,
                0,  // x-coord calculated by RepositionAll()
                0,  // y-coord calculated by RepositionAll()
                ChessDisplayTextBuffer::margin_blunderAnalysis,
                CHENARD_PROPORTIONAL_FONT
            );

            // Make blunder line somewhat reddish color...
            tb->setColorOverride(RGB(0xff, 0xb0, 0x00));

            // Create text buffer for display "better move" analysis when blunder is detected.
            tb = new ChessDisplayTextBuffer(
                hwnd,
                STATIC_ID_BETTER_ANALYSIS,
                0,  // x-coord calculated by RepositionAll()
                0,  // y-coord calculated by RepositionAll()
                ChessDisplayTextBuffer::margin_betterAnalysis,
                CHENARD_PROPORTIONAL_FONT,
                1   // highlight yellow
            );

            ChessDisplayTextBuffer::RepositionAll();
        }
    }
    break;

    case WM_UPDATE_CHESS_WINDOW_SIZE:
        SetChessWindowSize(hwnd, (Global_AnalysisType != 0));
        break;

    case WM_COMMAND:   // Mostly handles program's menus
    {
        switch ( LOWORD(wparam) )
        {
        case ID_CHENARD_FILE_NEW:
            Global_ResetGameFlag = true;
            break;

        case ID_CHENARD_FILE_OPEN:
            Chenard_FileOpen();
            break;

        case ID_CHENARD_FILE_SAVE:
            Chenard_FileSave();
            break;

        case ID_CHENARD_FILE_SAVEAS:
            Chenard_FileSaveAs();
            break;

        case ID_CHENARD_FILE_EXIT:
            SendMessage ( hwnd, WM_CLOSE, 0, 0 );
            break;

        case ID_EDIT_CLEARBOARD:
            Global_ClearBoardFlag = true;
            break;

        case ID_EDIT_REDOMOVE:
            Global_RedoMoveFlag = true;
            break;

        case ID_EDIT_UNDOMOVE:
            Global_UndoMoveFlag = true;
            break;

        case ID_EDIT_EDITMODE:
        {
            int editMode = GetMenuState ( HmenuMain, ID_EDIT_EDITMODE, MF_BYCOMMAND );
            editMode = (editMode ^ MF_CHECKED) & MF_CHECKED;
            CheckMenuItem ( HmenuMain, ID_EDIT_EDITMODE, editMode | MF_BYCOMMAND );
        }
        break;

        case ID_EDIT_COPYGAMELISTING:
        {
            if ( CopyGameListing() )
            {
                MessageBox (
                    hwnd,
                    "A text listing of the game has been copied to the clipboard.\n"
                    "You can now paste the listing into a text editor, word processor, etc.",
                    CHESS_PROGRAM_NAME,
                    MB_ICONINFORMATION | MB_OK );
            }
        }
        break;

        case ID_HELP_VISITCHENARDWEBPAGE:
        {
            ShellExecute (NULL, "open", "http://cosinekitty.com/chenard/", NULL, NULL, SW_SHOWNORMAL);
        }
        break;

        case ID_EDIT_COPY_FEN:
        {
            if (CopyForsythEdwardsNotation())
            {
                MessageBox (
                    hwnd,
                    "The Forsyth-Edwards Notation (FEN) for the current "
                    "board position has been copied to the clipboard.",
                    CHESS_PROGRAM_NAME,
                    MB_ICONINFORMATION | MB_OK
                );
            }
        }
        break;

        case ID_VIEW_ROTATEBOARD:
            TheBoardDisplayBuffer.toggleView();
            TheBoardDisplayBuffer.updateAlgebraicCoords();
            TheBoardDisplayBuffer.freshenBoard();
            break;

        case ID_DUMP_XPOS_TABLE:
        {
            if (ComputerChessPlayer::XposTable)
            {
                const char *dumpFilename = "xposdump.txt";
                ComputerChessPlayer::XposTable->debugDump ( dumpFilename );
                sprintf ( buffer,
                          "Transposition table has been dumped to file '%s'",
                          dumpFilename );
            }
            else
            {
                strcpy(buffer, "Transposition table is NULL.");
            }

            MessageBox ( hwnd, buffer, CHESS_PROGRAM_NAME, MB_ICONINFORMATION | MB_OK );
        }
        break;

        case ID_VIEW_ANALYSIS_NONE:
        {
            CheckMenuItem ( HmenuMain, ID_VIEW_ANALYSIS_NONE, MF_CHECKED | MF_BYCOMMAND );
            CheckMenuItem ( HmenuMain, ID_VIEW_ANALYSIS_BESTPATH, MF_UNCHECKED | MF_BYCOMMAND );
            CheckMenuItem ( HmenuMain, ID_VIEW_ANALYSIS_CURRENTPATH, MF_UNCHECKED | MF_BYCOMMAND );
            Global_AnalysisType = 0;
            ChessDisplayTextBuffer::RepositionAll();
            SetChessWindowSize ( hwnd, (Global_AnalysisType != 0) );
        }
        break;

        case ID_VIEW_ANALYSIS_BESTPATH:
        {
            CheckMenuItem ( HmenuMain, ID_VIEW_ANALYSIS_NONE, MF_UNCHECKED | MF_BYCOMMAND );
            CheckMenuItem ( HmenuMain, ID_VIEW_ANALYSIS_BESTPATH, MF_CHECKED | MF_BYCOMMAND );
            CheckMenuItem ( HmenuMain, ID_VIEW_ANALYSIS_CURRENTPATH, MF_UNCHECKED | MF_BYCOMMAND );
            Global_AnalysisType = 1;
            ChessDisplayTextBuffer::RepositionAll();
            SetChessWindowSize ( hwnd, (Global_AnalysisType != 0) );
        }
        break;

        case ID_VIEW_ANALYSIS_CURRENTPATH:
        {
            CheckMenuItem ( HmenuMain, ID_VIEW_ANALYSIS_NONE, MF_UNCHECKED | MF_BYCOMMAND );
            CheckMenuItem ( HmenuMain, ID_VIEW_ANALYSIS_BESTPATH, MF_UNCHECKED | MF_BYCOMMAND );
            CheckMenuItem ( HmenuMain, ID_VIEW_ANALYSIS_CURRENTPATH, MF_CHECKED | MF_BYCOMMAND );
            Global_AnalysisType = 2;
            ChessDisplayTextBuffer::RepositionAll();
            SetChessWindowSize ( hwnd, (Global_AnalysisType != 0) );
        }
        break;

        case ID_ANIMATE:
        {
            int animateFlag = GetMenuState ( HmenuMain, ID_ANIMATE, MF_BYCOMMAND );
            animateFlag = (animateFlag ^ MF_CHECKED) & MF_CHECKED;
            Global_AnimateMoves = animateFlag ? true : false;
            CheckMenuItem ( HmenuMain, ID_ANIMATE, animateFlag | MF_BYCOMMAND );
        }
        break;

        case ID_GAME_OPPTIME:
        {
            int oppTimeFlag = GetMenuState ( HmenuMain, ID_GAME_OPPTIME, MF_BYCOMMAND );
            oppTimeFlag = (oppTimeFlag ^ MF_CHECKED) & MF_CHECKED;
            Global_EnableOppTime = oppTimeFlag ? true : false;
            CheckMenuItem ( HmenuMain, ID_GAME_OPPTIME, oppTimeFlag | MF_BYCOMMAND );

            if (!Global_EnableOppTime)
            {
                // Immediately abort any pondering that might be in progress.
                if (Global_UI)
                {
                    Global_UI->oppTime_abortSearch();
                }
            }
        }
        break;

        case ID_HIGHLIGHT_MOVE:
        {
            int hiliteFlag = GetMenuState ( HmenuMain, ID_HIGHLIGHT_MOVE, MF_BYCOMMAND );
            hiliteFlag = (hiliteFlag ^ MF_CHECKED) & MF_CHECKED;
            Global_HiliteMoves = hiliteFlag ? true : false;
            CheckMenuItem ( HmenuMain, ID_HIGHLIGHT_MOVE, hiliteFlag | MF_BYCOMMAND );
        }
        break;

        case ID_VIEW_SPEAKMOVESTHROUGHSOUNDCARD:
        {
            int speak = GetMenuState ( HmenuMain, ID_VIEW_SPEAKMOVESTHROUGHSOUNDCARD, MF_BYCOMMAND );
            speak = (speak ^ MF_CHECKED) & MF_CHECKED;
            if ( speak )
            {
                if ( WaveFilesExist() )
                {
                    Global_SpeakMovesFlag = true;
                }
                else
                {
                    Global_SpeakMovesFlag = false;
                    speak = 0;

                    MessageBox (
                        HwndMain,
                        "Chenard is unable to speak because "
                        "one or more WAV files (audio recordings) are missing. "
                        "Note that these files are distributed separately from "
                        "this program due to their size and the fact that some people "
                        "will not want to use this feature.\n\n"
                        "Under the Help menu, choose \"Visit Chenard web page\", "
                        "download chenwav.zip, and unzip its contents into the same directory "
                        "where you saved this program (winchen.exe or winchen64.exe)."
                        ,
                        "Chenard: Cannot speak!",   // address of title of message box
                        MB_ICONERROR | MB_OK );
                }
            }
            else
            {
                Global_SpeakMovesFlag = false;
            }
            CheckMenuItem ( HmenuMain, ID_VIEW_SPEAKMOVESTHROUGHSOUNDCARD, speak | MF_BYCOMMAND );
        }
        break;

        case ID_VIEW_ANNOUNCEMATE:
        {
            int announce = GetMenuState (HmenuMain, ID_VIEW_ANNOUNCEMATE, MF_BYCOMMAND);
            announce = (announce ^ MF_CHECKED) & MF_CHECKED;    // toggle
            Global_EnableMateAnnounce = (announce != 0);
            if (Global_UI)
            {
                Global_UI->allowMateAnnounce (Global_EnableMateAnnounce);
            }
            CheckMenuItem (HmenuMain, ID_VIEW_ANNOUNCEMATE, announce | MF_BYCOMMAND);
        }
        break;

        case ID_GAME_FORCEMOVE:
        {
            if ( Global_UI )
                Global_UI->forceMove();
        }
        break;

        case ID_GAME_SUGGEST:
        {
            extern bool Global_SuggestMoveFlag;
            Global_SuggestMoveFlag = true;
        }
        break;

        case ID_GAME_SUGGESTAGAIN:
        {
            extern bool Global_RepeatSuggestMoveFlag;
            Global_RepeatSuggestMoveFlag = true;
        }
        break;

        case ID_GAME_TACTICALBENCHMARK:
        {
            Global_TacticalBenchmarkFlag = true;
        }
        break;

        case ID_GAME_ALLOWEXTENDEDSEARCH:
        {
            if ( Global_UI )
            {
                int flag = GetMenuState ( HmenuMain, ID_GAME_ALLOWEXTENDEDSEARCH, MF_BYCOMMAND );
                flag = (flag ^ MF_CHECKED) & MF_CHECKED;
                Global_ExtendedSearch = flag ? true : false;
                CheckMenuItem ( HmenuMain, ID_GAME_ALLOWEXTENDEDSEARCH, flag | MF_BYCOMMAND );
                MessageBox ( HwndMain,
                             Global_ExtendedSearch ?
                             "The computer will now think longer when it thinks it was about to make a mistake." :
                             "The computer will now use no more than its allowed think time.",
                             CHESS_PROGRAM_NAME,
                             MB_OK | MB_ICONINFORMATION );

                if ( Global_UI->queryWhitePlayerType() == DefPlayerInfo::computerPlayer )
                {
                    ComputerChessPlayer *puter =
                        (ComputerChessPlayer *)Global_UI->queryWhitePlayer();

                    if ( puter )
                        puter->setExtendedSearchFlag ( Global_ExtendedSearch );
                }

                if ( Global_UI->queryBlackPlayerType() == DefPlayerInfo::computerPlayer )
                {
                    ComputerChessPlayer *puter =
                        (ComputerChessPlayer *)Global_UI->queryBlackPlayer();

                    if ( puter )
                        puter->setExtendedSearchFlag ( Global_ExtendedSearch );
                }
            }
        }
        break;

        case ID_GAME_AUTO_SINGULAR:
        {
            if ( Global_UI )
            {
                int flag = GetMenuState ( HmenuMain, ID_GAME_AUTO_SINGULAR, MF_BYCOMMAND );
                flag = (flag ^ MF_CHECKED) & MF_CHECKED;
                Global_AutoSingular = flag ? true : false;
                CheckMenuItem ( HmenuMain, ID_GAME_AUTO_SINGULAR, flag | MF_BYCOMMAND );
                MessageBox ( HwndMain,
                             Global_AutoSingular ?
                             "When you have only one legal move, it will be made for you." :
                             "When you have only one legal move, you must manually make it.",
                             CHESS_PROGRAM_NAME,
                             MB_OK | MB_ICONINFORMATION );

                if ( Global_UI->queryWhitePlayerType() == DefPlayerInfo::humanPlayer )
                {
                    HumanChessPlayer *human =
                        (HumanChessPlayer *)Global_UI->queryWhitePlayer();

                    if ( human )
                        human->setAutoSingular ( Global_AutoSingular );
                }

                if ( Global_UI->queryBlackPlayerType() == DefPlayerInfo::humanPlayer )
                {
                    HumanChessPlayer *human =
                        (HumanChessPlayer *)Global_UI->queryBlackPlayer();

                    if ( human )
                        human->setAutoSingular ( Global_AutoSingular );
                }
            }
        }
        break;

        case ID_GAME_ALLOWCOMPUTERTORESIGN:
        {
            int flag = GetMenuState ( HmenuMain, ID_GAME_ALLOWCOMPUTERTORESIGN, MF_BYCOMMAND );
            flag = (flag ^ MF_CHECKED) & MF_CHECKED;
            Global_AllowResignFlag = flag ? true : false;
            CheckMenuItem ( HmenuMain, ID_GAME_ALLOWCOMPUTERTORESIGN, flag | MF_BYCOMMAND );
        }
        break;

        case ID_GAME_RESIGN:
        {
            if (Global_GameOverFlag)
            {
                MessageBox(HwndMain, "Cannot resign because game is already over.", CHESS_PROGRAM_NAME, MB_OK);
            }
            else
            {
                if (MessageBox (
                            HwndMain,
                            "Do you really want to resign?",
                            CHESS_PROGRAM_NAME,
                            MB_ICONQUESTION | MB_YESNO ) == IDYES )
                {
                    extern bool Global_UserResign;
                    Global_UserResign = true;
                }
            }
        }
        break;

        case ID_EDIT_THINK_TIMES:
        {
            if ( Global_UI )
            {
                DialogBox (
                    global_hInstance,
                    MAKEINTRESOURCE(IDD_EDIT_THINK_TIMES),
                    hwnd,
                    DLGPROC(EditThinkTimes_DlgProc) );
            }
        }
        break;

        case ID_EDIT_BLUNDER_THRESHOLD:
        {
            if (Global_UI)
            {
                DialogBox(
                    global_hInstance,
                    MAKEINTRESOURCE(IDD_EDIT_BLUNDER_THRESHOLD),
                    hwnd,
                    DLGPROC(EditBlunderAlertThreshold_DlgProc));
            }
        }
        break;

        case ID_ENTER_FEN:
            DialogBox (
                global_hInstance,
                MAKEINTRESOURCE(IDD_ENTER_FEN),
                hwnd,
                DLGPROC(EnterFen_DlgProc)
            );
            break;

        case ID_HELP_ABOUT:
            DialogBox (
                global_hInstance,
                MAKEINTRESOURCE(IDD_ABOUT),
                hwnd,
                DLGPROC(About_DlgProc) );
            break;

        case ID_HELP_HOWDOICASTLE:
            MessageBox (
                HwndMain,
                "To castle, click on your king, then click the square "
                "where the king will land (i.e., two squares to the right or left)."
                "The computer will automatically move the rook around the king for you.",
                CHESS_PROGRAM_NAME,
                MB_ICONINFORMATION | MB_OK );
            break;

        case ID_KEY_N:      TheBoardDisplayBuffer.keyMove ( 0,  1);   break;
        case ID_KEY_NE:     TheBoardDisplayBuffer.keyMove ( 1,  1);   break;
        case ID_KEY_E:      TheBoardDisplayBuffer.keyMove ( 1,  0);   break;
        case ID_KEY_SE:     TheBoardDisplayBuffer.keyMove ( 1, -1);   break;
        case ID_KEY_S:      TheBoardDisplayBuffer.keyMove ( 0, -1);   break;
        case ID_KEY_SW:     TheBoardDisplayBuffer.keyMove (-1, -1);   break;
        case ID_KEY_W:      TheBoardDisplayBuffer.keyMove (-1,  0);   break;
        case ID_KEY_NW:     TheBoardDisplayBuffer.keyMove (-1,  1);   break;

        case ID_KEY_CHOOSE:
            if ( insideHumanMove )
            {
                int x = TheBoardDisplayBuffer.getKeySelectX();
                int y = TheBoardDisplayBuffer.getKeySelectY();

                bool editMode = 0 != (MF_CHECKED & GetMenuState(HmenuMain, ID_EDIT_EDITMODE, MF_BYCOMMAND));

                if ( editMode )
                {
                    SQUARE square = EMPTY;

                    if ( Global_GameOverFlag || EditSquareDialog ( hwnd, square ) )
                        SendEditRequest ( x, y, square );
                }
                else
                {
                    TheBoardDisplayBuffer.squareSelectedNotify ( x, y );
                }
            }
            break;

            // Allow the user to enter moves by typing algebraic coordinates.
            // For example, "g1f3" will move the piece at g1 to f3.
        case ID_ALGEBRAIC_A:
        case ID_ALGEBRAIC_B:
        case ID_ALGEBRAIC_C:
        case ID_ALGEBRAIC_D:
        case ID_ALGEBRAIC_E:
        case ID_ALGEBRAIC_F:
        case ID_ALGEBRAIC_G:
        case ID_ALGEBRAIC_H:
        case ID_ALGEBRAIC_1:
        case ID_ALGEBRAIC_2:
        case ID_ALGEBRAIC_3:
        case ID_ALGEBRAIC_4:
        case ID_ALGEBRAIC_5:
        case ID_ALGEBRAIC_6:
        case ID_ALGEBRAIC_7:
        case ID_ALGEBRAIC_8:
        {
            bool editMode = 0 != (MF_CHECKED & GetMenuState(HmenuMain, ID_EDIT_EDITMODE, MF_BYCOMMAND));
            if (insideHumanMove && !editMode)
            {
                // Convert the command code to the equivalent character 'a'..'h' or '1'..'8'.
                const WORD id = LOWORD(wparam);
                char c;
                if ((id >= ID_ALGEBRAIC_A) && (id <= ID_ALGEBRAIC_H))
                {
                    c = static_cast<char>((id - ID_ALGEBRAIC_A) + 'a');
                }
                else
                {
                    c = static_cast<char>((id - ID_ALGEBRAIC_1) + '1');
                }
                TheBoardDisplayBuffer.sendAlgebraicChar(c);
            }
        }
        break;
        }
    }
    break;

    case WM_ERASEBKGND:
    {
        if ( !Global_AbortFlag )
        {
            RECT crect;
            GetClientRect ( hwnd, &crect );
            // Do not erase the background behind the board because the bitmaps
            // are opaque anyway.  Break the remaining area into four rectangles.
            HBRUSH hbrush = CreateSolidBrush ( CHESS_BACKGROUND_COLOR );

            // Erase above the chess board
            RECT rect;
            rect.left = crect.left;
            rect.top = crect.top;
            rect.right = SQUARE_SCREENX2(7)+1;
            rect.bottom = SQUARE_SCREENY1(7)-1;
            FillRect ( HDC(wparam), &rect, hbrush );

            // Erase below the chess board
            rect.top = SQUARE_SCREENY2(0)+1;
            rect.bottom = crect.bottom;
            FillRect ( HDC(wparam), &rect, hbrush );

            // Erase to the left of the chess board
            rect.left = crect.left;
            rect.right = SQUARE_SCREENX1(0)-1;
            rect.top = SQUARE_SCREENY1(7)-1;
            rect.bottom = SQUARE_SCREENY2(0)+1;
            FillRect ( HDC(wparam), &rect, hbrush );

            // Erase to the right of the chess board
            rect.left = SQUARE_SCREENX2(7)+1;
            rect.right = crect.right;
            rect.top = crect.top;
            rect.bottom = crect.bottom;
            FillRect ( HDC(wparam), &rect, hbrush );
            DeleteObject ( hbrush );
            result = TRUE;
        }
    }
    break;

    case WM_PAINT:
    {
        if ( !Global_AbortFlag )
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint ( hwnd, &ps );
            int x1 = SQUARE_CHESSX (ps.rcPaint.left);
            int x2 = SQUARE_CHESSX (ps.rcPaint.right);
            int y1 = SQUARE_CHESSY (ps.rcPaint.bottom);
            int y2 = SQUARE_CHESSY (ps.rcPaint.top);
            TheBoardDisplayBuffer.draw ( hdc, x1, x2, y1, y2 );
            ChessDisplayTextBuffer::DrawAll ( hdc );
            EndPaint ( hwnd, &ps );
            result = TRUE;
        }
    }
    break;

    case WM_LBUTTONUP:
    {
        if ( insideHumanMove )
        {
            int mouseX = LOWORD(lparam);
            int mouseY = HIWORD(lparam);

            int x = SQUARE_CHESSX ( mouseX );
            int y = SQUARE_CHESSY ( mouseY );
            if ( !TheBoardDisplayBuffer.queryWhiteView() )
            {
                // Need to rotate the coords...

                x = 7 - x;
                y = 7 - y;
            }

            int editMode = MF_CHECKED &
                           GetMenuState ( HmenuMain, ID_EDIT_EDITMODE, MF_BYCOMMAND );

            if ( editMode )
            {
                SQUARE square = EMPTY;

                if ( Global_GameOverFlag || EditSquareDialog ( hwnd, square ) )
                    SendEditRequest ( x, y, square );
            }
            else
            {
                TheBoardDisplayBuffer.squareSelectedNotify ( x, y );
            }

            if (Global_TransientCenteredMessage)
            {
                // There is text superimposed on the board that
                // should go away as soon as any mouse clicks are received.
                Global_TransientCenteredMessage = false;
                ChessDisplayTextBuffer::SetText(STATIC_ID_GAME_RESULT, "");
            }
        }
    }
    break;

    case WM_RBUTTONUP:
    {
        if ( insideHumanMove )
        {
            int mouseX = LOWORD(lparam);
            int mouseY = HIWORD(lparam);

            int editMode = MF_CHECKED &
                           GetMenuState ( HmenuMain, ID_EDIT_EDITMODE, MF_BYCOMMAND );

            if ( editMode )
            {
                int x = SQUARE_CHESSX ( mouseX );
                int y = SQUARE_CHESSY ( mouseY );

                if ( !TheBoardDisplayBuffer.queryWhiteView() )
                {
                    // Need to rotate the coords...

                    x = 7 - x;
                    y = 7 - y;
                }

                SendEditRequest ( x, y, Global_BoardEditRequest.getMostRecentSquare() );
            }
        }
    }
    break;

    case WM_DDC_ENTER_HUMAN_MOVE:
    {
        // Getting here means that we have just begun reading
        // a human player's move.  There are certain menu options
        // whose enable state should be changed now.

        insideHumanMove = true;
        CheckHumanMoveState ( insideHumanMove );
    }
    break;

    case WM_DDC_LEAVE_HUMAN_MOVE:
    {
        // Getting here means we are just now finishing with
        // a human player's move.

        insideHumanMove = false;
        CheckHumanMoveState ( insideHumanMove );
    }
    break;

    case WM_DDC_FATAL:
    {
        char *errorText = (char *)(lparam);

        MessageBox (
            hwnd,
            errorText,
            CHESS_PROGRAM_NAME " fatal error",
            MB_ICONEXCLAMATION | MB_OK );

        PostMessage ( hwnd, WM_QUIT, ULONG(0), ULONG(0) );
    }
    break;

    case WM_DDC_PREDICT_MATE:
        if ( !Global_SpeakMovesFlag )
        {
            sprintf ( buffer, "Mate in %d", int(wparam) );
            MessageBox ( hwnd,
                         buffer,
                         ChessProgramName,
                         MB_OK );
        }
        *(int *)(lparam) = 1;   // signals that message box is finished
        break;

    case WM_DDC_PROMOTE_PAWN:
    {
        SQUARE *promPtr = (SQUARE *)lparam;
        SQUARE localProm = EMPTY;

        DialogBoxParam (
            global_hInstance,
            MAKEINTRESOURCE(IDD_PROMOTE_PAWN),
            hwnd,
            DLGPROC(PromotePawn_DlgProc),
            LPARAM(&localProm) );

        if ( localProm != R_INDEX &&
             localProm != B_INDEX &&
             localProm != N_INDEX &&
             localProm != Q_INDEX )
        {
            localProm = Q_INDEX;
        }

        *promPtr = localProm;
    }
    break;

    case WM_DDC_GAME_RESULT:
    {
        const ChessUI_win32_gui::gameReport& report = *(ChessUI_win32_gui::gameReport *)(lparam);
        DisplayGameReportText(hwnd, report);
    }
    break;

    case WM_DDC_GAME_OVER:
    {
        insideHumanMove = true;
        Global_GameOverFlag = true;
        CheckHumanMoveState ( insideHumanMove );
    }
    break;

    case WM_DDC_DRAW_VECTOR:
    {
        int vofs1 = int(wparam);
        int vofs2 = int(lparam);
        int x1 = SQUARE_MIDX ( XPART(vofs1) - 2 );
        int y1 = SQUARE_MIDY ( YPART(vofs1) - 2 );
        int x2 = SQUARE_MIDX ( XPART(vofs2) - 2 );
        int y2 = SQUARE_MIDY ( YPART(vofs2) - 2 );
        HDC hdc = GetDC ( hwnd );
        HGDIOBJ oldpen = SelectObject ( hdc, GetStockObject(WHITE_PEN) );
        int prev = SetROP2 ( hdc, R2_XORPEN );
        MoveToEx ( hdc, x1, y1, NULL );
        LineTo ( hdc, x2, y2 );
        SetROP2 ( hdc, prev );
        SelectObject ( hdc, oldpen );
        ReleaseDC ( hwnd, hdc );
    }
    break;

    default:
        result = (BOOL) DefWindowProc ( hwnd, msg, wparam, lparam );
        break;
    }

    return result;
}


void ChessFatal ( const char *message )
{
    static int firstTime = 1;
    if ( firstTime )
    {
        firstTime = 0;
        BREAKPOINT();
        assert (false);
        PostMessage ( HwndMain, UINT(WM_DDC_FATAL), WPARAM(0), LPARAM(message) );
    }
}


void ChessBeep ( int freq, int duration )
{
//    if ( SoundEnableFlag )
//    {
//        Beep ( freq, duration );
//    }
}


//------------------------------------------------------------------


ChessDisplayTextBuffer *ChessDisplayTextBuffer::All = nullptr;


ChessDisplayTextBuffer::ChessDisplayTextBuffer (
    HWND _hwnd, int _id, int _x, int _y, marginType _margin, int _textFont,
    int _highlight, bool _isBackgroundOpaque ):
    id ( _id ),
    hwnd ( _hwnd ),
    text ( new char [MAX_CHESS_TEXT+1] ),
    next ( ChessDisplayTextBuffer::All ),
    x ( _x ),
    y ( _y ),
    margin ( _margin ),
    textFont ( _textFont ),
    highlight ( _highlight ),
    isBackgroundOpaque(_isBackgroundOpaque),
    colorOverride(RGB(0, 0, 0))
{
    text[0] = '\0';

    // Make sure the id is unique (not already in the All list.)
    ChessDisplayTextBuffer *duplicate = Find(id);
    assert(0 == duplicate);

    All = this;
}


ChessDisplayTextBuffer::~ChessDisplayTextBuffer()
{
    if ( text )
    {
        delete[] text;
        text = 0;
    }
}


void ChessDisplayTextBuffer::reposition()
{
    const int textWidth = 8;
    const int textHeight = 12;

    switch ( margin )
    {
    case margin_right:
    {
        x = CHESS_BITMAP_DX*8 + CHESS_LEFT_MARGIN + 2*CHESS_BOARD_BORDER_DX + 4;
    }
    break;

    case margin_bottom:
    {
        y = CHESS_BITMAP_DY*8 + 2*CHESS_BOARD_BORDER_DY + CHESS_BOTTOM_MARGIN - 2;

        // Special case for "thinking" tag:

        if ( id == STATIC_ID_THINKING )
            x = SQUARE_SCREENX1(8) - 8*textWidth;
    }
    break;

    case margin_rank:
    {
        const int coord = id - STATIC_ID_COORD_RANK_BASE;
        x = CHESS_BOARD_BORDER_DX - 1;
        y = SQUARE_MIDY(coord) - textHeight/2;
    }
    break;

    case margin_file:
    {
        const int coord = id - STATIC_ID_COORD_FILE_BASE;
        x = SQUARE_MIDX(coord) - textWidth/2;
        y = SQUARE_SCREENY1(-1) + 2;
    }
    break;

    case margin_center:
    {
        x = 0;
        y = 0;
        RECT rect;
        calcRegion(rect);

        x = SQUARE_SCREENX2(3) - rect.right/2;
        y = SQUARE_SCREENY1(3) - rect.bottom/2;
    }
    break;

    case margin_blunderAnalysis:
    {
        x = SQUARE_SCREENX1(0);

        // Same calculation as margin_bottom, but then shifted down.
        y = CHESS_BITMAP_DY*8 + 2*CHESS_BOARD_BORDER_DY + CHESS_BOTTOM_MARGIN - 2;
        y += MOVE_TEXT_DY + 8 + 0*20;
    }
    break;

    case margin_betterAnalysis:
    {
        x = SQUARE_SCREENX1(0);

        // Same calculation as margin_bottom, but then shifted down.
        y = CHESS_BITMAP_DY*8 + 2*CHESS_BOARD_BORDER_DY + CHESS_BOTTOM_MARGIN - 2;
        y += MOVE_TEXT_DY + 8 + 1*20;
    }
    break;
    }
}


void ChessDisplayTextBuffer::calcRegion ( RECT &rect ) const
{
    rect.left = x;
    rect.top = y;

    SIZE size;
    size.cx = 0;
    size.cy = 0;

    if (text != nullptr)
    {
        HDC hdc = GetDC(hwnd);
        HFONT font = createFont();
        HFONT oldFont = (HFONT)SelectObject(hdc, font);
        GetTextExtentPoint32(hdc, text, (int)strlen(text), &size);
        SelectObject(hdc, oldFont);
        deleteFont(font);
        ReleaseDC(hwnd, hdc);
    }

    rect.right = (LONG)(x + size.cx);
    rect.bottom = (LONG)(y + size.cy);
}


HFONT ChessDisplayTextBuffer::createFont() const
{
    HFONT font;

    switch (textFont)
    {
    case CHENARD_GAMERESULT_FONT:
        font = CreateFont(
            80, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0,
            CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, VARIABLE_PITCH, "Arial");
        break;

    case CHENARD_PROPORTIONAL_FONT:
        font = CreateFont(
            16, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0,
            CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, VARIABLE_PITCH, "Arial");
        break;

    case CHENARD_MONOSPACE_FONT:
    default:
        font = CreateFont(
            18, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0,
            CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, VARIABLE_PITCH, "Courier New");
        break;
    }

    return font;
}

void ChessDisplayTextBuffer::deleteFont(HFONT font) const
{
    if (font != NULL)
        DeleteObject(font);
}


void ChessDisplayTextBuffer::draw ( HDC hdc ) const
{
    HFONT font = createFont();
    HFONT oldFont = (HFONT) SelectObject(hdc, font);
    char *p = text ? text : "";

    COLORREF color;
    switch (highlight)
    {
    case 0:
        color = CHESS_TEXT_COLOR;
        break;

    case 1:
        color = CHESS_TEXT_COLOR2;
        break;

    default:
        // hack: highlight==2 means use override color.
        color = colorOverride;
        break;
    }

    COLORREF oldTextColor = SetTextColor ( hdc, color );
    COLORREF oldBkColor   = SetBkColor ( hdc, CHESS_BACKGROUND_COLOR );
    SetBkMode(hdc, isBackgroundOpaque ? OPAQUE : TRANSPARENT);
    TextOut ( hdc, x, y, p, (int)strlen(p) );
    SetBkMode(hdc, OPAQUE);
    if ( oldBkColor != CLR_INVALID )
        SetBkColor ( hdc, oldBkColor );
    if ( oldTextColor != CLR_INVALID )
        SetTextColor ( hdc, oldTextColor );
    SelectObject ( hdc, oldFont );

    deleteFont(font);
}


void ChessDisplayTextBuffer::invalidate()
{
    RECT rect;
    calcRegion ( rect );
    InvalidateRect ( hwnd, &rect, TRUE );
}


void ChessDisplayTextBuffer::setText ( const char *newText )
{
    if (0 == strcmp(text, newText))
    {
        return;     // avoid "blinking" the display with spurious InvalidateRect calls
    }

    RECT before;
    calcRegion(before);

    strncpy(text, newText, MAX_CHESS_TEXT);
    if (margin == margin_center)
    {
        reposition();   // margin_center requires reposition after every text change.
    }

    if (textFont == 0)
    {
        // I'm having trouble calculating bounds correctly for special-case fonts.
        // Just invalidate the entire chess display.
        InvalidateRect(hwnd, NULL, TRUE);
    }
    else
    {
        RECT after;
        calcRegion(after);
        InvalidateRect(hwnd, &before, TRUE);
        InvalidateRect(hwnd, &after,  TRUE);
    }
}


ChessDisplayTextBuffer *ChessDisplayTextBuffer::Find ( int _id )
{
    for ( ChessDisplayTextBuffer *p=All; p; p = p->next )
    {
        if ( p->id == _id )
            return p;
    }

    return 0;
}


void ChessDisplayTextBuffer::DrawAll ( HDC hdc )
{
    for ( ChessDisplayTextBuffer *p=All; p; p=p->next )
        p->draw ( hdc );
}


void ChessDisplayTextBuffer::RepositionAll ()
{
    for ( ChessDisplayTextBuffer *p=All; p; p=p->next )
        p->reposition();
}


void ChessDisplayTextBuffer::DeleteAll()
{
    while (All)
    {
        ChessDisplayTextBuffer *next = All->next;
        delete All;
        All = next;
    }
}


void ChessDisplayTextBuffer::SetText ( int _id, const char *newText )
{
    ChessDisplayTextBuffer *p = Find(_id);
    if (p)
    {
        p->setText(newText);
    }
}


/*
    $Log: wingui.cpp,v $
    Revision 1.21  2009/09/16 16:49:34  Don.Cross
    Added support for the WinBoard/xboard "memory" command to set hash table sizes.

    Revision 1.20  2008/11/17 18:47:07  Don.Cross
    WinChen.exe now has an Edit/Enter FEN function for setting a board position based on a FEN string.
    Tweaked ChessBoard::SetForsythEdwardsNotation to skip over any leading white space.
    Fixed incorrect Chenard web site URL in the about box.
    Now that computers are so much faster, reduced the

    Revision 1.19  2008/11/12 23:06:07  Don.Cross
    Massive refactoring of the concept of an 'editedFlag' in ChessBoard.
    Now instead of just saying the board was edited, we will always remember the most recent position
    after which there are no more pseudo-moves representing edit actions.
    This allows us to save/load PGN files based on edited positions.
    More work needs to be done:
    - Need to allow move undo/redo to work in edited positions.
    - Because the game history is stored now inside the ChessBoard, it seems silly to have an external copy also!
    - Move numbers should be based on FEN when PGN is saved.
    In general, this is a scary change, so it needs more careful testing before being published.

    Revision 1.18  2008/05/03 15:26:19  Don.Cross
    Updated code for Visual Studio 2005.

    Revision 1.17  2006/02/25 22:22:19  dcross
    Added support for connecting to Flywheel named pipe server as a Chenard chess player.
    This allows me to use my new Flywheel engine via the Chenard GUI.

    Revision 1.16  2006/02/24 20:55:39  dcross
    Added menu option for copying Forsyth Edwards Notation (FEN)
    for current board position into clipboard.

    Revision 1.15  2006/01/22 20:53:15  dcross
    Now when the game is over, just set title bar text instead of popping up a dialog box.

    Revision 1.14  2006/01/22 20:27:29  dcross
    Tweaked default settings:
    1. Default think time is now 2 seconds instead of 10 seconds.
    2. Thinking on opponent time is now disabled by default.

    Revision 1.13  2006/01/22 20:20:22  dcross
    Added an option to enable/disable announcing forced mates via dialog box.

    Revision 1.12  2006/01/18 19:58:17  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.11  2006/01/07 13:45:09  dcross
    Now ChessFatal will execute a breakpoint instruction in debug builds.
    This allows me to see exactly what's going on inside the debugger.

    Revision 1.10  2006/01/07 13:19:54  dcross
    1. I found and fixed a serious bug in the bn.edb generator:
       It was not generating optimal checkmates.  I discovered this by running
       a symmetry validator algorithm, which found inconsistencies among equivalent positions.
       I fixed this by making sure that in each pass, we find only checkmates in that number of moves.
       This requires a lot more passes, but at least it is correct!
    2. Because the same bug exists in all the other block-copied versions of the algorithm,
       and because I want to get away from the complexities of requiring bishops to be on a
       given color, I am starting all over and replacing egdb.cpp with a new file egdbase.cpp.
       I will keep the existing code around for reference, but I have put a #error in it to prevent
       me from accidentally trying to compile it in the future.

    Revision 1.9  2006/01/06 20:11:07  dcross
    Fixed linker error in portable.exe:  moved  Global_EndgameDatabaseMode from wingui.cpp to egdb.cpp.

    Revision 1.8  2006/01/06 17:16:55  dcross
    Now "winchen.exe -dbd" will generate a text listing of bn.edb.

    Revision 1.7  2006/01/03 02:08:23  dcross
    1. Got rid of non-PGN move generator code... I'm committed to the standard now!
    2. Made *.pgn the default filename pattern for File/Open.
    3. GetGameListing now formats more tightly, knowing that maximum possible PGN move is 7 characters.

    Revision 1.6  2006/01/02 19:40:29  dcross
    Added ability to use File/Open dialog to load PGN files into Win32 version of Chenard.

    Revision 1.5  2005/12/31 23:26:38  dcross
    Now Windows GUI version of Chenard can save game in PGN format.

    Revision 1.4  2005/11/26 19:16:26  dcross
    Added support for using the keyboard to make moves.

    Revision 1.3  2005/11/23 21:30:33  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:45  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!


         Revision history:

    2001 January 14 [Don Cross]
         Turned off question "do you really want to quit?"...
         after all, it's just a chess game, not your PhD thesis.

    2001 January 5 [Don Cross]
         Adding support for generation of endgame databases,
         specifically for K+R vs K, K+Q vs K, K+P vs K.
         When there are three pieces on the board, there is
         an upper limit of 64*63*62 = 249984 possible positions.
         We can pre-compute a file of optimal moves for these
         situations.

    1999 April 2 [Don Cross]
         Added a temporary ComputerChessPlayer pointer in
         UpdateAnalysisDisplay() because I found a rare case where
         the pointer is set to NULL from the chess thread after it
         has been tested for not being NULL.  This results in an
         access violation.  Hopefully, this fix should prevent that
         from happening again.

    1999 March 13 [Don Cross]
         Added a call to SetFocus() in About_DlgProc() so that user
         can just hit Enter to dismiss the dialog.  Previously, the
         OK button did not have focus for some reason.

    1999 February 19 [Don Cross]
         Adding support for genetic algorithm trainer to GUI version of
         Chenard.

    1999 January 21 [Don Cross]
         Adding "extended search" flag, which when enabled allows computer
         to think longer when its minmax score drops surprisingly.

    1999 January 16 [Don Cross]
         Adding "Automatic Singular Move" option.

    1999 January 12 [Don Cross]
         Made "copy game listing" buffer be 64K instead of just 4K,
         because extremely long games were getting truncated.

    1999 January 7 [Don Cross]
         In "Define Players" dialog, now set focus on think time when
         radio button for a computer player is selected.

    1999 January 5 [Don Cross]
         Started working on InternetChessPlayer support.

    1999 January 3 [Don Cross]
         Now displays algebraic coords to the left and bottom of the board.
         Display "thinking" when the computer performing search.

    1998 November 15 [Don Cross]
         Fixed minor UI bug: "Game | Allow Computer to Resign" option
         was working, but not reflecting changes in state.  Added one
         line of code to fix this.
         Now user can enter non-integer number of seconds as search times.
         Changed default piece style to "Skak".

    1997 June 10 [Don Cross]
         Adding an execution profiler so I can figure out where my
         code needs to be optimized.

    1997 May 10 [Don Cross]
         Making Edit|Copy Game Listing work when it is the computer's
         turn to move.

    1997 March 8 [Don Cross]
         Adding Game|Allow computer to resign, Game|Resign.

    1997 March 3 [Don Cross]
         Changed bool Global_ViewDebugFlag into int Global_AnalysisType.
         Now the allowed values are 0=none, 1=bestpath, 2=currentpath.

    1997 February 6 [Don Cross]
         I have decided to move the WAV files in the WinChenard
         distribution to their own zip file which can be downloaded
         independently of the executable and readme file.  Therefore,
         I am making Chenard check for the existence of the necessary
         *.WAV files before going into View|Speak mode.  If they are not
         present, I will present a dialog box which tells the user how
         to get them from the Chenard web page.

    1997 January 25 [Don Cross]
         Made Edit|Copy Game Listing self-explanatory by displaying
         an informational dialog box after the operation completes
         successfully.  This was in response to an email I received
         from someone who couldn't figure out what the command was doing.
         Also added better error checking for anomalous conditions.

    1997 January 30 [Don Cross]
         Adding support for Edit|Undo Move and Edit|Redo Move.
         Adding new Skak font.

    1997 January 3 [Don Cross]
         Added support for editing the think time limit for computer
         players while the game is in progress.
         Also, put the View|Freshen menu command back in, because
         Lyle says he saw the display get screwed up once.

    1996 August 24 [Don Cross]
         Replaced memory-based learning tree with disk-based tree.

    1996 July 29 [Don Cross]
         Copied code from OS/2 Presentation Manager version of Chenard
         as baseline for Win32 GUI version.

    1994 February 9 [Don Cross]
         Adding visual feedback for selecting squares.

    1994 January 31 [Don Cross]
         Fixed RestoreBoardFromFile so that it calls
         ChessBoard::SaveSpecialMove when it finds an edit command
         in the game file.

    1994 February 4 [Don Cross]
         Fixed bug with editing board when viewed from Black's side.
         Fixed bug with Empty side not disabling pieces in edit dialog.

    1994 January 12 [Don Cross]:
         Started adding application menu for Game, Edit, View.
         Here are the menu commands I intend to support:

         Game:
            New:   start over at the beginning of the game.
            Open:  read a game from a file and continue it.
            Save, Save As:  save the game to a file.

         Edit:  (most/all options will be disabled if not human's turn)
            Clear board:  make the board blank???
            Set up pieces:  go into "edit mode"???

    1993 December 31 [Don Cross]:
         Fixed bug: Title bar text used to say it was White's turn
         when we restored a game from a file and it was either side's
         turn to move.

*/

