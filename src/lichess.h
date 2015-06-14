/*===========================================================================

    lichess.h  -  Copyright (C) 1999 by Don Cross <dcross@intersrv.com>

    Win32 version of InternetChessPlayer.
    Allows two instances of Chenard to play chess over TCP/IP using WinSock.

===========================================================================*/
#ifndef __ddc_chenard_lichess_h
#define __ddc_chenard_lichess_h


#define  DEFAULT_CHENARD_PORT   5387


class InternetChessPlayer: public ChessPlayer
{
public:
    InternetChessPlayer ( ChessUI & );
    ~InternetChessPlayer();

    int serverConnect ( ChessSide remoteSide );

    static void      SetServerPortNumber ( unsigned newPortNumber );
    static unsigned  QueryServerPortNumber();

    virtual bool GetMove ( ChessBoard &, Move &, INT32 &timeSpent );
    virtual void InformResignation();
    virtual void InformGameOver ( const ChessBoard & );

protected:
    bool send ( const ChessBoard & );
    bool receive ( ChessBoard &board, Move &move );

private:
    static unsigned ServerPortNumber;

private:
    int commSocket;
    int listenSocket;
};


#endif // __ddc_chenard_lichess_h

/*
    $Log: lichess.h,v $
    Revision 1.2  2006/01/18 19:58:12  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.1  2005/11/25 19:47:27  dcross
    Recovered lots of old chess source files from the dead!
    Found these on CDROM marked "14 September 1999".
    Added cvs log tag and moved old revision history after that.


Revision history:

1999 January 23 [Don Cross]
    Ported from WinSock version (ichess.h) to Linux environment.

*/

