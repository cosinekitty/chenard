/*======================================================================================

    ichess.h  -  Copyright (C) 1999-2005 by Don Cross

    Win32 version of InternetChessPlayer.
    Allows two instances of Chenard to play chess over TCP/IP using WinSock.

======================================================================================*/
#ifndef __ddc_chenard_ichess_h
#define __ddc_chenard_ichess_h


#define  DEFAULT_CHENARD_PORT   5387


struct InternetConnectionInfo
{
    SOCKET commSocket;
    int waitForClient;
};


class InternetChessPlayer: public ChessPlayer
{
public:
    InternetChessPlayer ( ChessUI &, const InternetConnectionInfo & );
    ~InternetChessPlayer();

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
    InternetConnectionInfo connectInfo;
};


#endif // __ddc_chenard_ichess_h

/*
    $Log: ichess.h,v $
    Revision 1.4  2006/01/18 19:58:11  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.3  2005/11/23 21:30:30  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:41  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!

*/

