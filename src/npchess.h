/*
    npchess.h  -  Copyright (C) 2006 Don Cross.  All Rights Reserved.

    class NamedPipeChessPlayer.
*/

#if SUPPORT_NAMED_PIPE

class NamedPipeChessPlayer: public ChessPlayer
{
public:
    NamedPipeChessPlayer (ChessUI &_ui, const char *_MachineName);
    virtual ~NamedPipeChessPlayer();

    //-------------------------------------------------------------
    // The following function should store the player's move
    // and return true, or return false to quit the game.
    // This function will not be called if it is already the
    // end of the game.
    //-------------------------------------------------------------
    virtual bool GetMove (ChessBoard &, Move &, INT32 &timeSpent);

    //-------------------------------------------------------------
    //  The following member function was added to support
    //  class InternetChessPlayer.  When a player resigns,
    //  this member function is called for the other player.
    //  This way, a remote player will immediately know that
    //  the local player has resigned.
    //-------------------------------------------------------------
    virtual void InformResignation();

    //-------------------------------------------------------------
    //  The following member function is called whenever the
    //  game has ended due to the opponent's move.  This was
    //  added so that InternetChessPlayer objects (representing
    //  a remote player) have a way to receive the final move.
    //-------------------------------------------------------------
    virtual void InformGameOver (const ChessBoard &);

private:
    char    MachineName [256];    // name of server, e.g. "\\computer"
    HANDLE  NamedPipe;
};


#endif // SUPPORT_NAMED_PIPE

/*
    $Log: npchess.h,v $
    Revision 1.1  2006/02/25 22:22:19  dcross
    Added support for connecting to Flywheel named pipe server as a Chenard chess player.
    This allows me to use my new Flywheel engine via the Chenard GUI.

*/
