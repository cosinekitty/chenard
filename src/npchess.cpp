/*
    npchess.h  -  Copyright (C) 2006 Don Cross.  All Rights Reserved.
    
    class NamedPipeChessPlayer.
*/

#include <windows.h>
#include <stdio.h>

#include "chess.h"
#include "ichess.h"
#include "winguich.h"


#if SUPPORT_NAMED_PIPE


NamedPipeChessPlayer::NamedPipeChessPlayer (ChessUI &_ui, const char *_MachineName):
    ChessPlayer (_ui),
    NamedPipe (INVALID_HANDLE_VALUE)    // indicates there is no connection (yet)
{
    char PipeName [512];

    SetQuitReason(qgr_lostConnection);

    if (_MachineName && strlen(_MachineName) < sizeof(MachineName)) {
        if (_MachineName[0]) {
            strcpy (MachineName, _MachineName);
        } else {
            strcpy (MachineName, ".");
        }
        sprintf (PipeName, "\\\\%s\\pipe\\flywheel_chess_thinker", MachineName);
        NamedPipe = CreateFileA (
            PipeName,
            GENERIC_READ | GENERIC_WRITE,
            0,      // no sharing
            NULL,   // default security
            OPEN_EXISTING,
            0,      // default attributes
            NULL    // no template
        );
    }
}


NamedPipeChessPlayer::~NamedPipeChessPlayer()
{
    if (NamedPipe != INVALID_HANDLE_VALUE) {
        CloseHandle (NamedPipe);
        NamedPipe = INVALID_HANDLE_VALUE;
    }
}


//-------------------------------------------------------------
// The following function should store the player's move
// and return true, or return false to quit the game.
// This function will not be called if it is already the
// end of the game.
//-------------------------------------------------------------
bool NamedPipeChessPlayer::GetMove (ChessBoard &board, Move &move, INT32 &timeSpent)
{
    if (NamedPipe != INVALID_HANDLE_VALUE) {
        INT32 StartTime = ChessTime();

        char request [256];
        char fen [128];
        if (board.GetForsythEdwardsNotation (fen, sizeof(fen))) {
            sprintf (request, "5:%s", fen);     // 5 second think time hardcoded for now

            DWORD num_bytes_written;
            DWORD request_size = (DWORD) (1 + strlen(request));

            BOOL happy = WriteFile (
                NamedPipe,
                request,
                request_size,
                &num_bytes_written,
                NULL
            );

            if (happy) {
                if (num_bytes_written == request_size) {
                    // wait for response...
                    char reply [16];
                    DWORD num_bytes_read;
                    happy = ReadFile (
                        NamedPipe,
                        reply,
                        sizeof(reply),
                        &num_bytes_read,
                        NULL
                    );

                    timeSpent = ChessTime() - StartTime;

                    if (happy) {
                        if (num_bytes_read >= 5) {
                            if (reply[0] >= 'a' && reply[0] <= 'h') {
                                if (reply[1] >= '1' && reply[1] <= '8') {
                                    if (reply[2] >= 'a' && reply[2] <= 'h') {
                                        if (reply[3] >= '1' && reply[3] <= '8') {
                                            int rsource = OFFSET (reply[0] - 'a' + 2, reply[1] - '1' + 2);
                                            int rdest   = OFFSET (reply[2] - 'a' + 2, reply[3] - '1' + 2);

                                            SQUARE rprom;
                                            switch (reply[4]) {
                                                case '\0': rprom = EMPTY;  break;
                                                case 'Q':  rprom = board.WhiteToMove() ? WQUEEN  : BQUEEN;  break;
                                                case 'R':  rprom = board.WhiteToMove() ? WROOK   : BROOK;   break;
                                                case 'B':  rprom = board.WhiteToMove() ? WBISHOP : BBISHOP; break;
                                                case 'N':  rprom = board.WhiteToMove() ? WKNIGHT : BKNIGHT; break;
                                                default:    return false;
                                            }

                                            int xsource, xdest;
                                            SQUARE xprom;

                                            MoveList    LegalMoves;
                                            board.GenMoves (LegalMoves);

                                            for (int i=0; i < LegalMoves.num; ++i) {
                                                xprom = LegalMoves.m[i].actualOffsets (board, xsource, xdest);
                                                if (xsource==rsource && xdest==rdest && xprom==rprom) {
                                                    move = LegalMoves.m[i];
                                                    userInterface.DisplayMove (board, move);
                                                    return true;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return false;
}

//-------------------------------------------------------------
//  The following member function was added to support
//  class InternetChessPlayer.  When a player resigns,
//  this member function is called for the other player.
//  This way, a remote player will immediately know that
//  the local player has resigned.
//-------------------------------------------------------------
void NamedPipeChessPlayer::InformResignation()
{
}


//-------------------------------------------------------------
//  The following member function is called whenever the
//  game has ended due to the opponent's move.  This was
//  added so that InternetChessPlayer objects (representing
//  a remote player) have a way to receive the final move.
//-------------------------------------------------------------
void NamedPipeChessPlayer::InformGameOver (const ChessBoard &)
{
}
 

#endif // SUPPORT_NAMED_PIPE

/*
    $Log: npchess.cpp,v $
    Revision 1.2  2006/03/04 17:24:12  dcross
    Improvements to named pipe interface:
    1. No longer need double backslashes before machine name in player creation dialog box.
    2. If machine name is left blank, "." is assumed.
    3. Now animate moves sent back from named pipe server.

    Revision 1.1  2006/02/25 22:22:19  dcross
    Added support for connecting to Flywheel named pipe server as a Chenard chess player.
    This allows me to use my new Flywheel engine via the Chenard GUI.

*/

