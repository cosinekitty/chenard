/*=======================================================================

       uivga.h  -  Copyright (C) 1993-1996 by Don Cross
       email: dcross@intersrv.com
       WWW:   http://www.intersrv.com/~dcross/

       Mutated from uicga.h by Don Cross, 24 April 1992.

=======================================================================*/

// This is a special UI for MS-DOS, re-using the UI code from
// Chessola, Chester, and LanChess...


class ChessUI_dos_vga: public ChessUI
{
public:
   ChessUI_dos_vga();
   ~ChessUI_dos_vga();

#ifdef NO_SEARCH
   bool Activate();
#endif

   ChessPlayer *CreatePlayer ( ChessSide );
   bool ReadMove ( ChessBoard &, int &source, int &dest );
   SQUARE PromotePawn ( int PawnDest, ChessSide );
   void DisplayMove ( ChessBoard &, Move );
   void RecordMove ( ChessBoard &, Move, INT32 thinkTime );
   void DrawBoard ( const ChessBoard & );
   void ReportEndOfGame ( ChessSide winner );
   void DisplayBestMoveSoFar ( const ChessBoard &, Move bestSoFar, int level );
   void DisplayCurrentMove   ( const ChessBoard &, Move move, int  level );
   void PredictMate ( int numMoves );

   void ReportComputerStats ( INT32    thinkTime,
                              UINT32   nodesVisited,
                              UINT32   nodesEvaluated,
                              UINT32   nodesGenerated,
                              int      fwSearchDepth,
                              UINT32   vis [NODES_ARRAY_SIZE],
                              UINT32   gen [NODES_ARRAY_SIZE] );

   void Resign ( ChessSide, QuitGameReason )  {}

   void DebugPly ( int depth, ChessBoard &, Move );
   void DebugExit ( int depth, ChessBoard &, SCORE );
   void NotifyUser ( const char *message );
   void ReportSpecial ( const char *message );

private:
   bool  graphics_initialized;
   bool  debugSearchFlag;

   Move FixMove ( const ChessBoard &, int source, int dest );
};


void sqrborder ( int x, int y, int grcolor );
void drawsqr ( SQUARE p, int x, int y );
void initscreen ();
void drawgrid ();
void drawboard ( const SQUARE board[144] );
void refreshboard ( const SQUARE board[144] );
void flashsqr ( int x, int y, SQUARE oldpiece, SQUARE newpiece, int keywait );
void drawmove ( const SQUARE b[144], int ofs, int disp );
int  getkey (void);
int  select ( char *options );
void displaynotation ( char *notation, unsigned long thinkTime, int side );


/*
    $Log: uivga.h,v $
    Revision 1.2  2006/01/18 19:58:16  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.1  2005/11/25 19:47:28  dcross
    Recovered lots of old chess source files from the dead!
    Found these on CDROM marked "14 September 1999".
    Added cvs log tag and moved old revision history after that.


Revision history:

1999 February 14 [Don Cross]
     Bringing up to date with 32-bit code base.
     Adding ChessUI_dos_vga::ReportSpecial and
     ChessUI_dos_vga::NotifyUser.

*/
