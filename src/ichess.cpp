/*======================================================================================

    ichess.cpp  -  Copyright (C) 1999-2005 by Don Cross

    Win32 version of InternetChessPlayer.
    Allows two instances of Chenard to play chess over TCP/IP using WinSock.

======================================================================================*/
#include <windows.h>
#include <stdio.h>

#include "chess.h"
#include "ichess.h"
#include "winguich.h"


unsigned InternetChessPlayer::ServerPortNumber = DEFAULT_CHENARD_PORT;


#if SUPPORT_INTERNET


void InternetChessPlayer::SetServerPortNumber ( unsigned newPortNumber )
{
    ServerPortNumber = newPortNumber;
}


unsigned InternetChessPlayer::QueryServerPortNumber()
{
    return ServerPortNumber;
}


InternetChessPlayer::InternetChessPlayer ( 
    ChessUI &ui, 
    const InternetConnectionInfo &_connectInfo ):
        ChessPlayer (ui),
        connectInfo ( _connectInfo )
{
    SetQuitReason(qgr_lostConnection);
}


InternetChessPlayer::~InternetChessPlayer()
{
    if ( connectInfo.commSocket != INVALID_SOCKET )
    {
        closesocket ( connectInfo.commSocket );
        connectInfo.commSocket = INVALID_SOCKET;
    }
}


bool InternetChessPlayer::GetMove ( 
    ChessBoard &board, 
    Move &move, 
    INT32 &timeSpent )
{
    timeSpent = 0;
    const INT32 startTime = ChessTime();

    if ( !send(board) ) {
        return false;
    }

    if ( !receive(board,move) ) {
        return false;
    }

    timeSpent = ChessTime() - startTime;

    userInterface.DisplayMove ( board, move );
    return true;
}


bool InternetChessPlayer::send ( const ChessBoard &board )
{
    char tempString [256];

    UINT32 numPlies = board.GetCurrentPlyNumber();
    if ( numPlies > 0 )
    {
        // Before receiving the move from the remote opponent, we must
        // send him the complete state of the game as it exists locally.

        // Send an 8-byte string to specify what kind of message this is.
        // In this case, it is "history", because we are sending the entire history
        // of moves in the game history so far...

        UINT32 plyBytes = numPlies * sizeof(Move);
        UINT32 packetSize = 8 + sizeof(numPlies) + plyBytes;
        int result = ::send ( connectInfo.commSocket, (const char *)&packetSize, 4, 0 );
        if ( result != 4 )
        {
            sprintf ( tempString, "send psize: %d", WSAGetLastError() );
            userInterface.ReportSpecial (tempString);
            return false;
        }

        result = ::send ( connectInfo.commSocket, "history ", 8, 0 );
        if ( result != 8 )
        {
            sprintf ( tempString, "send 'history': %d", WSAGetLastError() );
            userInterface.ReportSpecial (tempString);
            return false;
        }

        result = ::send ( connectInfo.commSocket, (const char *)&numPlies, 4, 0 );
        if ( result != 4 )
            return false;

        Move *history = new Move [numPlies];
        if ( !history )
        {
            userInterface.ReportSpecial ( "out of memory!" );
            return false;
        }

        for ( UINT32 ply = 0; ply < numPlies; ++ply )
            history[ply] = board.GetPastMove (ply);

        result = ::send ( connectInfo.commSocket, (const char *)history, plyBytes, 0 );
        delete[] history;
        history = NULL;

        if ( UINT32(result) != plyBytes )
        {
            sprintf ( tempString, "send: %d", WSAGetLastError() );
            userInterface.ReportSpecial (tempString);
            return false;
        }
    }

    return true;
}


bool InternetChessPlayer::receive ( 
    ChessBoard &board, 
    Move &move )
{
    char tempString [256];
    for(;;)
    {
        UINT32 packetSize = 0;
        int result = ::recv ( connectInfo.commSocket, (char *)&packetSize, 4, 0 );
        if ( result != 4 )
        {
            sprintf ( tempString, "recv psize: %d", WSAGetLastError() );
            userInterface.ReportSpecial (tempString);
            return false;
        }

        char inMessageType [16];
        memset ( inMessageType, 0, sizeof(inMessageType) );
        result = ::recv ( connectInfo.commSocket, inMessageType, 8, 0 );
        if ( result != 8 )
        {
            sprintf ( tempString, "recv(message): %d", WSAGetLastError() );
            userInterface.ReportSpecial (tempString);
            return false;
        }

        if ( strcmp ( inMessageType, "history " ) == 0 )
        {
            // Receive the number of plies from the other side...

            int numPlies = 0;
            result = ::recv ( connectInfo.commSocket, (char *)&numPlies, sizeof(int), 0 );
            if ( result != sizeof(int) )
            {
                sprintf ( tempString, "recv(numPlies): size=%d err=%d", result, WSAGetLastError() );
                userInterface.ReportSpecial (tempString);
                return false;
            }

            if ( numPlies > 0 )
            {
                if ( numPlies > 1024 )
                {
                    sprintf ( tempString, "numPlies = %d", numPlies );
                    userInterface.ReportSpecial (tempString);
                    return false;
                }

                Move *history = new Move [numPlies];
                if ( !history )
                {
                    userInterface.ReportSpecial ( "out of memory!" );
                    return false;
                }

                int plyBytes = numPlies * sizeof(Move);
                result = ::recv ( connectInfo.commSocket, (char *)history, plyBytes, 0 );
                if ( result != plyBytes )
                {
                    sprintf ( tempString, "recv: size=%d err=%d", result, WSAGetLastError() );
                    userInterface.ReportSpecial (tempString);
                    return false;
                }

                // Now that we safely have the game history from the opponent,
                // we can reset the board and apply all but the final ply to the board.
                // The final ply is returned as the move made by the InternetChessPlayer
                // so that it can be animated on the board display.

                UnmoveInfo unmove;
                board.Init();
                for ( int ply = 0; ply < numPlies-1; ++ply )
                {
                    Move hm = history[ply];
                    if ( (hm.dest & SPECIAL_MOVE_MASK) == SPECIAL_MOVE_EDIT )
                    {
                        board.EditCommand ( hm );
                        board.SaveSpecialMove ( hm );
                    }
                    else
                        board.MakeMove ( hm, unmove );
                }

                move = history [numPlies - 1];
                delete[] history;
            }

            break;
        }
        else if ( strcmp ( inMessageType, "resign  " ) == 0 )
        {
            SetQuitReason(qgr_resign);
            return false;   // remote player has resigned
        }
        else  // unknown message, perhaps from a later version of Chenard
        {
            // eat all the bytes in the message and ignore them

            int bytesLeft = int(packetSize) - 8;
            char trash;
            while ( bytesLeft-- > 0 )
                ::recv ( connectInfo.commSocket, &trash, 1, 0 );
        }
    }

    return true;
}


void InternetChessPlayer::InformResignation()
{
    UINT32 packetSize = 8;
    ::send ( connectInfo.commSocket, (const char *)&packetSize, 4, 0 );
    ::send ( connectInfo.commSocket, "resign  ", 8, 0 );
}


void InternetChessPlayer::InformGameOver ( const ChessBoard &board )
{
    send (board);
}


#endif  // SUPPORT_INTERNET

/*
    $Log: ichess.cpp,v $
    Revision 1.5  2006/01/27 18:02:01  dcross
    During code review, I noticed that memory leakage happened in InternetChessPlayer::send
    whenever an error occurred sending the move history.  This has been fixed.

    Revision 1.4  2006/01/18 19:58:11  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.3  2005/11/23 21:30:30  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:40  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!

    
    
        Revision history:
    
    1999 January 5 [Don Cross]
        Started writing.
    
*/

