/*
    uiserver.h  -  Don Cross  -  http://cosinekitty.com

    Derived user interface class for chenserver.
*/

class ChessUI_Server : public ChessUI
{
public:
    ChessUI_Server();
    virtual ~ChessUI_Server();

    virtual ChessPlayer *CreatePlayer(ChessSide);

    // NOTE:  The following is called only when there is a
    //        checkmate, stalemate, or draw by attrition.
    //        The function is NOT called if a player resigns.
    virtual void ReportEndOfGame(ChessSide winner);

    virtual void Resign(ChessSide iGiveUp, QuitGameReason);

    //--------------------------------------------------------------
    //  The following function should return true if the Move
    //  was read successfully, or false to abort the game.
    //  The move does not have to be legal; if an illegal move
    //  is returned, the caller will repeatedly call until a
    //  legal move is obtained.
    //--------------------------------------------------------------
    virtual bool ReadMove(
        ChessBoard  &board,
        int         &source,
        int         &dest,
        SQUARE      &promPieceIndex     // set to 0 (P_INDEX) if no promotion, or call to PromotePawn is needed; set to N_INDEX..Q_INDEX to specify promotion.
        );

    //--------------------------------------------------------------
    //  Called whenever a player needs to be asked which
    //  piece a pawn should be promoted to.
    //  Must return one of the following values:
    //      Q_INDEX, R_INDEX, B_INDEX, N_INDEX
    //--------------------------------------------------------------
    virtual SQUARE PromotePawn(int PawnSource, int PawnDest, ChessSide);

    //--------------------------------------------------------------
    //  The following function should display the given Move
    //  in the context of the given ChessBoard.  The Move
    //  passed to this function will always be legal for the
    //  given ChessBoard.
    //
    //  The purpose of this function is for a non-human ChessPlayer
    //  to have a chance to animate the chess board.
    //  The ChessGame does not call this function; it is up
    //  to a particular kind of ChessPlayer to do so if needed.
    //--------------------------------------------------------------
    virtual void DisplayMove(ChessBoard &, Move);

    //----------------------------------------------------------------
    //  The following function is called by the ChessGame after
    //  each player moves, so that the move can be displayed
    //  in standard chess notation, if the UI desires.
    //  It is helpful to use the ::FormatChessMove() function
    //  for this purpose.
    //  The parameter 'thinkTime' is how long the player thought
    //  about the move, expressed in hundredths of seconds.
    //
    //  NOTE:  This function is called BEFORE the move
    //  has been made on the given ChessBoard.
    //  This is necessary for ::FormatChessMove() to work.
    //----------------------------------------------------------------
    virtual void RecordMove(ChessBoard &, Move, INT32 thinkTime);

    virtual void ReportComputerStats(
        INT32   thinkTime,
        UINT32  nodesVisited,
        UINT32  nodesEvaluated,
        UINT32  nodesGenerated,
        int     fwSearchDepth,
        UINT32  vis[NODES_ARRAY_SIZE],
        UINT32  gen[NODES_ARRAY_SIZE]);

    //--------------------------------------------------------------//
    //  The following function should display the given ChessBoard  //
    //  to the user.                                                //
    //  In a GUI, this means updating the board display to the      //
    //  given board's contents.                                     //
    //  In TTY-style UI, this means printing out a new board.       //
    //--------------------------------------------------------------//
    virtual void DrawBoard(const ChessBoard &);

    //----------------------------------------------------------------
    //  The following member function displays a prompt and waits
    //  for acknowledgement from the user.
    //----------------------------------------------------------------
    virtual void NotifyUser(const char *message);

    virtual void DisplayBestMoveSoFar(
        const ChessBoard &,
        Move,
        int level);

    virtual void DisplayCurrentMove(
        const ChessBoard &,
        Move,
        int level);

    // The ComputerChessPlayer calls this to announce that it
    // has found a forced mate.

    virtual void PredictMate(int numMovesFromNow);
};
