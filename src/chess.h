/*===========================================================================

     chess.h  -  Copyright (C) 1993-2005 by Don Cross

===========================================================================*/
#ifndef __cplusplus
#error Must compile as C++
#endif

//#define CHENARD_PROFILER 1

#ifndef __DDC_CHESS_32_H
#define __DDC_CHESS_32_H

#if 1
    // __asm keyword not allowed in x64 Windows builds
    #define BREAKPOINT()    ((void)0)
#else
    #if defined(_WIN32) && defined(_DEBUG)
        #define BREAKPOINT()    __asm int 3
    #else
        #define BREAKPOINT()    ((void)0)
    #endif
#endif

// The Mac OS X build is basically the same as the Linux build,
// so I define CHENARD_LINUX as a helper for conditional compilation.
#define CHENARD_LINUX   (defined(__linux__) || defined(__APPLE__))    

#if CHENARD_LINUX
	#define stricmp strcasecmp
#endif

#ifdef _MSC_VER
	// Microsoft C++ compiler.
	// Disable warnings about deprecated functions like sprintf(), sscanf(), etc.
	// I would like to fix these, but no equivalent exists in Linux that I can tell.
	// Maybe some day I will make macros that expand to sprintf_s in Microsoft, or sprintf in Linux.
	#pragma warning (disable: 4996)
#endif // _MSC_VER

class ChessBoard;
class ChessPlayer;
class ComputerChessPlayer;
class ChessGame;
class ChessUI;

#define NODES_ARRAY_SIZE    100

// Define portable integer types...

#ifdef __BORLANDC__

      #if sizeof(int) == 4
          typedef unsigned int    UINT32;
          typedef          int    INT32;
      #elif sizeof(long) == 4
          typedef unsigned long   UINT32;
          typedef          long   INT32;
      #else
          #error Error finding a 32-bit integer type!
      #endif

      #if sizeof(int) == 2
          typedef int       INT16;
          typedef unsigned  UINT16;
      #elif sizeof(short) == 2
          typedef short            INT16;
          typedef unsigned short   UINT16;
      #else
          #error Error finding a 16-bit integer type!
      #endif

#else  // __BORLANDC__ is not defined (assume 2-pass compiler)

     // Just fake it...if it's not right, it will be caught at runtime.
     typedef unsigned int     UINT32;
     typedef int              INT32;
     typedef unsigned short   UINT16;
     typedef short            INT16;

#endif  // __BORLANDC__

typedef UINT32          SQUARE;    //holds contents of square on chess board
typedef unsigned char   BYTE;
typedef signed char     INT8;
typedef INT16           SCORE;     //holds move-ordering and eval scores


/*-------------------------------------------------------------------------

    This is a map of the bits used in a SQUARE:

                bit7 bit6 bit5 bit4 bit3 bit2 bit1 bit0
    BYTE 0:        0    0   WK   WQ   WR   WB   WN   WP
    BYTE 1:     offb    0   BK   BQ   BR   BB   BN   BP
    BYTE 2:        0    0    0 Bbit Wbit  pc2  pc1  pc0
    BYTE 3:        0 ind6 ind5 ind4 ind3 ind2 ind1 ind0

-------------------------------------------------------------------------*/



//--------------------------- BYTE 0, 1 piece masks...

#define  WP_MASK   (1uL <<  0)
#define  WN_MASK   (1uL <<  1)
#define  WB_MASK   (1uL <<  2)
#define  WR_MASK   (1uL <<  3)
#define  WQ_MASK   (1uL <<  4)
#define  WK_MASK   (1uL <<  5)

#define  BP_MASK   (1uL <<  8)
#define  BN_MASK   (1uL <<  9)
#define  BB_MASK   (1uL << 10)
#define  BR_MASK   (1uL << 11)
#define  BQ_MASK   (1uL << 12)
#define  BK_MASK   (1uL << 13)

#define  P_MASK    (WP_MASK | BP_MASK)
#define  N_MASK    (WN_MASK | BN_MASK)
#define  B_MASK    (WB_MASK | BB_MASK)
#define  R_MASK    (WR_MASK | BR_MASK)
#define  Q_MASK    (WQ_MASK | BQ_MASK)
#define  K_MASK    (WK_MASK | BK_MASK)

#define  OFFBOARD  (1uL << 15)

#define  WHITE_MASK  (WP_MASK|WN_MASK|WB_MASK|WR_MASK|WQ_MASK|WK_MASK)
#define  BLACK_MASK  (BP_MASK|BN_MASK|BB_MASK|BR_MASK|BQ_MASK|BK_MASK)
#define  ALL_MASK    (WHITE_MASK | BLACK_MASK)

#define  SAME_PIECE(square1,square2)   (((square1)&(square2)&(ALL_MASK))!=0)

//--------------------------- BYTE 2 piece indices and side masks
#define  P_INDEX   0
#define  N_INDEX   1
#define  B_INDEX   2
#define  R_INDEX   3
#define  Q_INDEX   4
#define  K_INDEX   5

#define  WHITE_IND    (1 << 3)
#define  BLACK_IND    (1 << 4)

#define  WP_INDEX  (P_INDEX | WHITE_IND)
#define  WN_INDEX  (N_INDEX | WHITE_IND)
#define  WB_INDEX  (B_INDEX | WHITE_IND)
#define  WR_INDEX  (R_INDEX | WHITE_IND)
#define  WQ_INDEX  (Q_INDEX | WHITE_IND)
#define  WK_INDEX  (K_INDEX | WHITE_IND)

#define  BP_INDEX  (P_INDEX | BLACK_IND)
#define  BN_INDEX  (N_INDEX | BLACK_IND)
#define  BB_INDEX  (B_INDEX | BLACK_IND)
#define  BR_INDEX  (R_INDEX | BLACK_IND)
#define  BQ_INDEX  (Q_INDEX | BLACK_IND)
#define  BK_INDEX  (K_INDEX | BLACK_IND)

#define  PIECE_MASK   7
#define  SIDE_MASK    (WHITE_IND | BLACK_IND)
#define  INDEX_MASK   (SIDE_MASK | PIECE_MASK)

#define  PIECE_ARRAY_SIZE     (BLACK_IND + K_INDEX + 1)

#define  SPIECE_INDEX(square)   (((square) >> 16) & INDEX_MASK)
#define  UPIECE_INDEX(square)   (((square) >> 16) & PIECE_MASK)

//------------------------------------------------------------------

// Raw piece values...

#define   PAWN_VAL      10
#define   KNIGHT_VAL    31
#define   BISHOP_VAL    33
#define   ROOK_VAL      50
#define   QUEEN_VAL     90
#define   KING_VAL      35

// Lookup table for piece values...
extern SCORE  RawPieceValues[];


SCORE MaterialEval ( SCORE wmaterial, SCORE bmaterial );


struct PieceLookup
{
    SQUARE   sval [PIECE_ARRAY_SIZE];

    PieceLookup();
};

extern PieceLookup PieceLookupTable;

#define PROM_PIECE(fakedest,side_index)  \
   (PieceLookupTable.sval[((fakedest) & PIECE_MASK) | (side_index)])

#define  RAW_PIECE_VALUE(square)  \
   RawPieceValues [ UPIECE_INDEX(square) ]

//---------- The following piece definitions are used
// to allow a board edit command to fit inside a Move.

#define EMPTY_NYBBLE  0      // 0000
#define WP_NYBBLE     1      // 0001
#define WN_NYBBLE     2      // 0010
#define WB_NYBBLE     3      // 0011
#define WR_NYBBLE     4      // 0100
#define WQ_NYBBLE     5      // 0101
#define WK_NYBBLE     6      // 0110
#define BP_NYBBLE     7      // 0111
#define BN_NYBBLE     8      // 1000
#define BB_NYBBLE     9      // 1001
#define BR_NYBBLE    10      // 1010
#define BQ_NYBBLE    11      // 1011
#define BK_NYBBLE    12      // 1100

SQUARE ConvertNybbleToSquare ( int nybble );
int    ConvertSquareToNybble ( SQUARE );

//--------------------------- definitions for the 32-bit pieces

#define  WPAWN     (WP_MASK | (SQUARE(WP_INDEX) << 16))
#define  WKNIGHT   (WN_MASK | (SQUARE(WN_INDEX) << 16))
#define  WBISHOP   (WB_MASK | (SQUARE(WB_INDEX) << 16))
#define  WROOK     (WR_MASK | (SQUARE(WR_INDEX) << 16))
#define  WQUEEN    (WQ_MASK | (SQUARE(WQ_INDEX) << 16))
#define  WKING     (WK_MASK | (SQUARE(WK_INDEX) << 16))

#define  BPAWN     (BP_MASK | (SQUARE(BP_INDEX) << 16))
#define  BKNIGHT   (BN_MASK | (SQUARE(BN_INDEX) << 16))
#define  BBISHOP   (BB_MASK | (SQUARE(BB_INDEX) << 16))
#define  BROOK     (BR_MASK | (SQUARE(BR_INDEX) << 16))
#define  BQUEEN    (BQ_MASK | (SQUARE(BQ_INDEX) << 16))
#define  BKING     (BK_MASK | (SQUARE(BK_INDEX) << 16))

#define  EMPTY     SQUARE(0)

//------------------------------------------------------------------


#define  OFFSET(x,y)     ((x) + 12*(y))    // offset in board
#define  XPART(ofs)      ((ofs) % 12)      // x part of offset
#define  YPART(ofs)      ((ofs) / 12)      // y part of offset

#define  NORTH         OFFSET(0,1)
#define  NORTHEAST     OFFSET(1,1)
#define  EAST          OFFSET(1,0)
#define  SOUTHEAST     OFFSET(1,-1)
#define  SOUTH         OFFSET(0,-1)
#define  SOUTHWEST     OFFSET(-1,-1)
#define  WEST          OFFSET(-1,0)
#define  NORTHWEST     OFFSET(-1,1)


#define  WON_FOR_BLACK  (-29000)   // position is winnable for black
#define  WON_FOR_WHITE   (29000)   // position is winnable for white

#define  BLACK_WINS     (-30000)   // black has checkmate
#define  WHITE_WINS      (30000)   // white has checkmate
#define  DRAW                (0)   // (yawn)

#define  WIN_DELAY_PENALTY    1
#define  WIN_POSTPONEMENT(depth)   (WIN_DELAY_PENALTY*(depth))

#define  MIN_WINDOW     (-31000)   // alpha-beta search window
#define  MAX_WINDOW      (31000)

#define  NEGINF         (-32000)   // negative pseudo-infinity
#define  POSINF          (32000)   // positive pseudo-infinity


/*-------------------------------------------------------------------
     The 'Move' structure contains the source offset in the board,
     the destination offset in the board, and a place to put the
     score of this move (for move sorting, etc).

     *** NOTE 1 ***
     For "special" moves (casting, en passant, and pawn promotion),
     'dest' will contain one of the SPECIAL_MOVE_xxx indicators,
     all of which are invalid offsets (because they are bigger than
     143).  In this case, the source is still valid.

     SPECIAL_MOVE_PROMOTE_xxx use the upper nybble as a
     flag that the move is a pawn promotion.  In this case, the
     lower nybble should be checked for one of the following
     piece indices, to know which piece we are promoting to:

            N_INDEX   to promote to a knight
            B_INDEX   to promote to a bishop
            R_INDEX   to promote to a rook
            Q_INDEX   to promote to a queen

     *** NOTE 2 ***
     Move::source will have its MSB set if the move has already
     been discovered to cause check to the opponent.
     If a Move is used after having called ChessBoard::MakeWhiteMove()
     or ChessBoard::MakeBlackMove() with it, you should always
     mask the source with BOARD_OFFSET_MASK to make sure the
     high order bit is cleared.  To test for check, test against
     the CAUSES_CHECK_BIT.

-------------------------------------------------------------------*/

#define   CAUSES_CHECK_BIT     0x80
#define   BOARD_OFFSET_MASK    0x7F

#define   SPECIAL_MOVE_PROMOTE_NORM          (0x80)
#define   SPECIAL_MOVE_PROMOTE_CAP_EAST      (0x90)
#define   SPECIAL_MOVE_PROMOTE_CAP_WEST      (0xA0)
#define   SPECIAL_MOVE_KCASTLE               (0xB0)
#define   SPECIAL_MOVE_QCASTLE               (0xC0)
#define   SPECIAL_MOVE_EP_EAST               (0xD0)
#define   SPECIAL_MOVE_EP_WEST               (0xE0)
#define   SPECIAL_MOVE_EDIT                  (0xF0)
#define   SPECIAL_MOVE_NULL                  (0x00)

#define   SPECIAL_MOVE_MASK       0xF0


struct Move   // A nice 32-bit structure!
{
    BYTE   source;   // NOTE: High bit of source used as a special flag!
    BYTE   dest;     // Sometimes this isn't a dest...see comments above.
    SCORE  score;    // Used for move-ordering

    bool operator== ( const Move &other ) const
    {
        return ((source ^ other.source) & BOARD_OFFSET_MASK) == 0 &&
                dest == other.dest;
    }

    bool operator!= ( const Move &other ) const
    {
        return !(*this == other);
    }

    void Fix ( unsigned long openBookData, ChessBoard & );
    bool Fix ( const ChessBoard &, int Source, int Dest, SQUARE OptionalProm, ChessUI & );

    bool ShouldIgnore() const
    {
        return dest == SPECIAL_MOVE_NULL ||
                (dest & SPECIAL_MOVE_MASK) == SPECIAL_MOVE_EDIT;
    }

    bool isPawnPromotion() const
    {
        return
            (dest == SPECIAL_MOVE_PROMOTE_NORM)     ||
            (dest == SPECIAL_MOVE_PROMOTE_CAP_EAST) ||
            (dest == SPECIAL_MOVE_PROMOTE_CAP_WEST)
        ;
    }

    // actualOffsets returns promotion piece if move is a promotion, or EMPTY if not.
    SQUARE actualOffsets ( const ChessBoard &board, int &ofs1, int &ofs2 ) const;
    SQUARE actualOffsets ( bool white_to_move, int &ofs1, int &ofs2 ) const;

    int blackHash() const;
    int whiteHash() const;

    void whiteOffsets ( int &ofs1, int &ofs2 ) const;
    void blackOffsets ( int &ofs1, int &ofs2 ) const;
};


/*-----------------------------------------------------------------
      The following flags are defined for
      ChessBoard::flags and related fields in UnmoveInfo.
-----------------------------------------------------------------*/
#define   SF_WKMOVED         0x0001      // has the white king moved?
#define   SF_WKRMOVED        0x0002      // has the white king rook moved?
#define   SF_WQRMOVED        0x0004      // has the white queen rook moved?
#define   SF_WCHECK          0x0008      // is white in check?

#define   SF_BKMOVED         0x0010      // has the black king moved?
#define   SF_BKRMOVED        0x0020      // has the black king rook moved?
#define   SF_BQRMOVED        0x0040      // has the black queen rook moved?
#define   SF_BCHECK          0x0080      // is black in check?


struct UnmoveInfo
{
    SQUARE    capture;          //whatever was captured, or EMPTY
    UINT16    flags;
    SCORE     wmaterial;
    SCORE     bmaterial;
    Move      prev_move;        //prev ChessBoard::prev_move (for e.p.)
    INT16     lastCapOrPawn;
    UINT32    cachedHash;
};

// http://www.stmintz.com/ccc/index.php?id=424966
// http://www.chess.com/forum/view/fun-with-chess/what-chess-position-has-the-most-number-of-possible-moves
#define  MAX_MOVES  220        // maximum number of moves in a MoveList

struct MoveList
{
    UINT16  num;                   // number of moves in list
    Move    m [MAX_MOVES];         // moves themselves

    bool IsLegal ( Move ) const;   // does a simple check for inclusion

    void WhiteSort ();
    void BlackSort ();

    void AddMove ( int source, int dest );
    void AddMove ( Move );

    void SendToFront ( Move );

    void Shuffle();
};


#define MAX_BESTPATH_DEPTH  50

#define DEBUG_BEST_PATH  0

struct BestPath
{
    int    depth;
    Move   m [MAX_BESTPATH_DEPTH + 8];

#if DEBUG_BEST_PATH
    long   counter [MAX_BESTPATH_DEPTH + 8];
#endif

    BestPath():  depth(0)   {}
};



//------------------------------------------------------------------

class InternetChessPlayer;

enum ChessSide { SIDE_NEITHER, SIDE_WHITE, SIDE_BLACK };

inline ChessSide OppositeSide(ChessSide side)
{
    switch (side)
    {
    case SIDE_WHITE:    return SIDE_BLACK;
    case SIDE_BLACK:    return SIDE_WHITE;
    default:            return SIDE_NEITHER;
    }
}

enum QuitGameReason
{
    qgr_resign,
    qgr_lostConnection,
    qgr_startNewGame,       // lets player start a new game without the humiliating red resignation text!
};


class ChessUI
{
public:
    ChessUI();
    virtual ~ChessUI();

    virtual bool Activate()  {return true;}

    virtual ChessPlayer *CreatePlayer ( ChessSide ) = 0;

    // NOTE:  The following is called only when there is a
    //        checkmate, stalemate, or draw by attrition.
    //        The function is NOT called if a player resigns.
    virtual void ReportEndOfGame ( ChessSide winner ) = 0;

    virtual void Resign ( ChessSide iGiveUp, QuitGameReason ) = 0;

    //--------------------------------------------------------------
    //  The following function should return true if the Move
    //  was read successfully, or false to abort the game.
    //  The move does not have to be legal; if an illegal move
    //  is returned, the caller will repeatedly call until a
    //  legal move is obtained.
    //  
    //--------------------------------------------------------------
    virtual bool ReadMove ( 
        ChessBoard  &board, 
        int         &source, 
        int         &dest, 
        SQUARE      &promPieceIndex     // set to 0 (P_INDEX) if no promotion, or call to PromotePawn is needed; set to N_INDEX..Q_INDEX to specify promotion.
    ) = 0;

    //--------------------------------------------------------------
    //  Called whenever a player needs to be asked which
    //  piece a pawn should be promoted to.
    //  Must return one of the following values:
    //      Q_INDEX, R_INDEX, B_INDEX, N_INDEX
    //--------------------------------------------------------------
    virtual SQUARE PromotePawn ( int PawnDest, ChessSide ) = 0;

    //--------------------------------------------------------------
    //  The following member function is called bracketing
    //  when a ComputerChessPlayer object is deciding upon a move.
    //  The parameter 'entering' is true when the computer is
    //  starting to think, and false when the thinking is done.
    //--------------------------------------------------------------
    virtual void ComputerIsThinking ( 
        bool /*entering*/,
        ComputerChessPlayer & ) {}

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
    virtual void DisplayMove ( ChessBoard &, Move ) = 0;

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
    virtual void RecordMove ( ChessBoard &, Move, INT32 thinkTime ) = 0;

    //----------------------------------------------------------------
    //   The following function is called when a method other than
    //   searching has been used to choose a move, e.g.
    //   opening book or experience database.
    //----------------------------------------------------------------
    virtual void ReportSpecial ( const char * /*message*/ )  {}

    virtual void ReportComputerStats (
        INT32   thinkTime,
        UINT32  nodesVisited,
        UINT32  nodesEvaluated,
        UINT32  nodesGenerated,
        int     fwSearchDepth,
        UINT32  vis [NODES_ARRAY_SIZE],
        UINT32  gen [NODES_ARRAY_SIZE] );

    //--------------------------------------------------------------//
    //  The following function should display the given ChessBoard  //
    //  to the user.                                                //
    //  In a GUI, this means updating the board display to the      //
    //  given board's contents.                                     //
    //  In TTY-style UI, this means printing out a new board.       //
    //--------------------------------------------------------------//
    virtual void DrawBoard ( const ChessBoard & ) = 0;

    //----------------------------------------------------------------
    //  The following member function displays a prompt and waits
    //  for acknowledgement from the user.
    //----------------------------------------------------------------
    virtual void NotifyUser ( const char *message ) = 0;

    virtual void DisplayBestMoveSoFar (
        const ChessBoard &,
        Move,
        int level );

    virtual void DisplayCurrentMove (
        const ChessBoard &,
        Move,
        int level );

    virtual void DisplayBestPath (
        const ChessBoard &,
        const BestPath & )  {}

    // The ComputerChessPlayer calls this to announce that it
    // has found a forced mate.

    virtual void PredictMate ( int numMovesFromNow );

    // The following function is a hook for debugging every move in a
    // search.  The board is given before the move is made in it, so
    // that correct move notation can be calculated.

    virtual void DebugPly ( int /*depth*/, ChessBoard &, Move ) {}

    // The following function is called whenever a level is exited.
    virtual void DebugExit ( int /*depth*/, ChessBoard &, SCORE ) {}

    virtual void SetAdHocText ( int /*index*/, const char *, ... ) {}
    virtual bool allowMateAnnounce ( bool ) 
    {
        return false;   // returns prior state of mate announce.  since default is to do nothing, return false.
    }

    //----------------- thinking on opponent's time ------------------------

    virtual bool oppTime_startThinking (
        const ChessBoard & /*currentPosition*/,
        Move               /*myMove*/,
        Move               /*predictedOpponentMove*/ )
    {
        return false;  // not implemented in most ChessUIs
    }

    virtual bool oppTime_finishThinking (
        const ChessBoard &  /*newPosition*/,
        INT32               /*timeToThink*/,
        INT32            &  /*timeSpent*/,
        Move             &  /*myResponse*/,
        Move             &  /*predictedOpponentMove*/ )
    {
        return false;
    }

    virtual void oppTime_abortSearch()
    {
    }

    //----------------------------------------------------------------------
};



//------------------------------------------------------------------

class ChessPlayer
{
public:
    ChessPlayer ( ChessUI & );
    virtual ~ChessPlayer();

    //-------------------------------------------------------------
    // The following function should store the player's move
    // and return true, or return false to quit the game.
    // This function will not be called if it is already the
    // end of the game.
    //-------------------------------------------------------------
    virtual bool GetMove ( ChessBoard &, Move &, INT32 &timeSpent ) = 0;

    //-------------------------------------------------------------
    //  The following member function was added to support
    //  class InternetChessPlayer.  When a player resigns, 
    //  this member function is called for the other player.
    //  This way, a remote player will immediately know that
    //  the local player has resigned.
    //-------------------------------------------------------------
    virtual void InformResignation() {}

    //-------------------------------------------------------------
    //  The following member function is called whenever the 
    //  game has ended due to the opponent's move.  This was
    //  added so that InternetChessPlayer objects (representing
    //  a remote player) have a way to receive the final move.
    //-------------------------------------------------------------
    virtual void InformGameOver ( const ChessBoard & ) {}

    QuitGameReason QueryQuitReason() const { return quitReason; }
    void SetQuitReason(QuitGameReason reason) { quitReason = reason; }

protected:
    ChessUI &userInterface;

private:
    QuitGameReason quitReason;
};


class HumanChessPlayer: public ChessPlayer
{
public:
    HumanChessPlayer ( ChessUI & );  // needed for parent
    ~HumanChessPlayer();

    bool GetMove ( ChessBoard &, Move &, INT32 &timeSpent );

    bool setAutoSingular ( bool newValue )
    {
        return automaticSingularMove = newValue;
    }

    bool queryAutoSingular() const { return automaticSingularMove; }

private:
    bool automaticSingularMove;   // make move for user when only 1 exists?
};


// Jan 1999: We're gonna try the transposition table thing again!

#define  XF_STALE     0x01     // entry is (slightly) out of date

struct TranspositionEntry
{
    UINT32  boardHash;       // hash code of board producing this node
    BYTE    searchedDepth;   // how deep full-width search is beneath this node
    BYTE    future;          // how deep in the tree we found the node
    BYTE    flags;           // see XF_... above
    Move    bestReply;       // source, dest, and score all valid
    SCORE   alpha;
    SCORE   beta;

// Utility functions...

    bool scoreIsInsideWindow() const 
    { 
        return alpha <= bestReply.score && bestReply.score <= beta;
    }

    bool compatibleWindow ( SCORE callerAlpha, SCORE callerBeta) const
    {
        // caller's window must be inside entry's window to be compatible
        return alpha <= callerAlpha && callerBeta <= beta;
    }
};

class TranspositionTable
{
public:
    TranspositionTable (int memorySizeInMegabytes);
    ~TranspositionTable();

    void reset();
    void startNewSearch();

    void rememberWhiteMove ( 
        ChessBoard &board,
        int level,
        int depth,
        Move bestReply,
        SCORE alpha,
        SCORE beta );

    void rememberBlackMove (
        ChessBoard &board,
        int level,
        int depth,
        Move bestReply,
        SCORE alpha,
        SCORE beta );

    const TranspositionEntry *locateWhiteMove ( ChessBoard &board );
    const TranspositionEntry *locateBlackMove ( ChessBoard &board );

    void debugDump ( const char *filename ) const;

protected:
    void init ();

    void rememberMove (
        ChessBoard &board,
        int searchedDepth,
        int future,
        Move bestReply,
        SCORE alpha,
        SCORE beta,
        TranspositionEntry *table,
        bool whiteToMove );

    const TranspositionEntry *locateMove (
        ChessBoard &board,
        TranspositionEntry *table );

    bool findEntry ( 
        UINT32 hashCode, 
        TranspositionEntry *table,
        unsigned &index );

private:
    unsigned numTableEntries;
    TranspositionEntry *whiteTable;
    TranspositionEntry *blackTable;
    unsigned numTries;   // total number of attempts to find old entry
    unsigned numHits;    // number of times table entry was retrieved

    unsigned numStores;    // total number of attempted stores into hash table
    unsigned numStomps;    // number of times previous entry is overwritten
    unsigned numFresh;     // number of fresh entries stored (most desirable)
    unsigned numStales;    // number of stale entries overwritten
    unsigned numFailures;  // number of entries which could not be stored
    unsigned numInferior;  // number of nodes that match existing, but less deep
};


// The following struct is used to define each heuristic constant
// in a ChessGene.


struct ChessGeneDefinition
{
    const char *name;
    SCORE  defaultValue;
    SCORE  minValue;
    SCORE  maxValue;

    SCORE  random() const;
};


// The following constant specifies how many array slots 
// there are in ChessGene::v[].  This value must exactly
// match the number of items defined in ChessGene::DefTable[]
// (excluding terminator element) or a ChessFatal() will occur
// at run time.

#define  NUM_CHESS_GENES     82


// A ChessGene is a vector of heuristic constants that affects
// evaluation and move ordering for a ComputerChessPlayer.
// It is called a gene because it can be evolved by a genetic
// algorithm.

class ChessGene
{
    friend class ComputerChessPlayer;

public:
    ChessGene();

    void reset();        // sets all heuristics to their default values
    void randomize();    // creates a randomized gene
    void blend ( const ChessGene &a, const ChessGene &b );

    int load ( const char *filename );
    int save ( const char *filename ) const;

protected:
    static int Locate ( const char *name, int expectedIndex );

private:
    SCORE v [NUM_CHESS_GENES];

    static ChessGeneDefinition DefTable[];
};


enum CCP_SEARCH_TYPE    // defines what kind of search done by ComputerChessPlayer object
{
    CCPST_UNDEFINED,        // search type is not defined
    CCPST_TIMED_SEARCH,     // search will last a specified maximum time
    CCPST_DEPTH_SEARCH,     // search will iteratively examine to specified max depth
    CCPST_MAXEVAL_SEARCH    // search will continue until certain number of nodes evaluated
};


class ComputerChessPlayer: public ChessPlayer
{
public:
    ComputerChessPlayer ( ChessUI & );
    virtual ~ComputerChessPlayer();

    static void LazyInitXposTable();

    bool GetMove ( ChessBoard &, Move &, INT32 &timeSpent );

    void SetSearchDepth ( int NewSearchDepth );       // cancels time limit
    void SetTimeLimit ( INT32 hundredthsOfSeconds );  // cancels search depth
    void SetMaxNodesEvaluated ( UINT32 _maxNodesEvaluated );  // cancels timed search/depth search

    void SetMinSearchDepth (int _minSearchDepth);

    INT32 QueryTimeLimit() const { return timeLimit; }

    // The following method is used to abort a search in progress
    // in multi-threaded environments.  There is no proper use of this
    // function in a single-threaded program (at least none that I can
    // think of!)
    void AbortSearch ();
    bool IsSearchAborted() const { return searchAborted; }

    virtual void InformResignation();

    bool IsSearchRandomized() const
    {
        return searchBias != 0;
    }

    void SetSearchBias  ( SCORE NewSearchBias );    //0=deterministic search, 1=randomized search
    void SetOpeningBookEnable ( bool newEnable )
    {
        openingBookSearchEnabled = newEnable;
    }

    void SetTrainingEnable ( bool newTrainingEnable )
    {
        trainingEnabled = newTrainingEnable;
    }

    void ResetHistoryBuffers();

    bool queryResignFlag() const { return allowResignation; }
    void setResignFlag ( bool x ) { allowResignation = x; }

    bool queryExtendedSearchFlag() const { return extendSearchFlag; }
    void setExtendedSearchFlag ( bool x ) { extendSearchFlag = x; }

    bool queryEnableMoveDisplay() const { return enableMoveDisplayFlag; }
    void setEnableMoveDisplay ( bool x ) { enableMoveDisplayFlag = x; }

    UINT32 queryNodesEvaluated() const { return evaluated; }

    void loadGene ( const ChessGene &newGene ) { gene = newGene; }

    SCORE queryResignThreshold() const { return resignThreshold; }
    void setResignThreshold ( unsigned _resignThreshold );

    void oppTime_SetEnableFlag() { oppTimeEnable = true; }
    void oppTime_SetInstanceFlag() { oppTimeInstance = true; }
    bool oppTime_QueryInstanceFlag() const { return oppTimeInstance; }

    void blunderAlert_SetInstanceFlag() { blunderAlertInstance = true; }
    bool blunderAlert_QueryInstanceFlag() const { return blunderAlertInstance; }

    bool isBackgroundThinker() const { return oppTimeInstance || blunderAlertInstance; }

    Move getPredictedOpponentMove() const { return predictedOppMove; }
    void cancelPredictedOpponentMove()      // can be called when board has been edited, new game started, etc., to prevent pondering
    {
        predictedOppMove.source = 0;
        predictedOppMove.dest   = 0;
        predictedOppMove.score  = 0;
    }

    const BestPath& getBestPath() const { return currentBestPath; }

protected:
    typedef SCORE (ComputerChessPlayer::*EvalFunction)
        ( ChessBoard &,
          int depth,
          SCORE alpha,
          SCORE beta );

    void GetWhiteMove ( ChessBoard &, Move & );
    void GetBlackMove ( ChessBoard &, Move & );

    bool  WhiteOpening ( ChessBoard &, Move & );
    bool  BlackOpening ( ChessBoard &, Move & );

    bool  FindEndgameDatabaseMove ( ChessBoard &, Move & );

    int WhiteKingFreedom ( ChessBoard & );
    int BlackKingFreedom ( ChessBoard & );

    SCORE WhiteSearchRoot ( ChessBoard &, Move & );
    SCORE BlackSearchRoot ( ChessBoard &, Move & );

    SCORE WhiteSearch (
        ChessBoard &,
        int depth,
        SCORE alpha,
        SCORE beta,
        bool bestPathFlag );

    SCORE BlackSearch (
        ChessBoard &,
        int depth,
        SCORE alpha,
        SCORE beta,
        bool bestPathFlag );

    SCORE WhiteQSearch (
        ChessBoard &,
        int depth,
        SCORE alpha,
        SCORE beta,
        bool bestPathFlag );

    SCORE BlackQSearch (
        ChessBoard &,
        int depth,
        SCORE alpha,
        SCORE beta,
        bool bestPathFlag );

    void GenWhiteCapturesAndChecks ( ChessBoard &board, MoveList &ml );
    void GenBlackCapturesAndChecks ( ChessBoard &board, MoveList &ml );

    SCORE  CommonMidgameEval ( ChessBoard &board );
    SCORE  WhiteMidgameEval ( ChessBoard &board, int depth, SCORE alpha, SCORE beta );
    SCORE  BlackMidgameEval ( ChessBoard &board, int depth, SCORE alpha, SCORE beta );

    // The following eval functions are for endgames in which
    // the ComputerChessPlayer has a mating net advantage against
    // a lone enemy king.

    SCORE  EndgameEval1 ( ChessBoard &board, int depth, SCORE, SCORE );
    SCORE  ProximityBonus ( ChessBoard &, int ofs, int mask );

    SCORE  WhitePawnBonus ( const ChessBoard &, const int ofs, const int x, const int ybase ) const;
    SCORE  BlackPawnBonus ( const ChessBoard &, const int ofs, const int x, const int ybase ) const;

    void  WhiteMoveOrdering (
        const ChessBoard &,
        Move &,
        const UnmoveInfo &,
        int depth,
        bool bestPathFlag );

    void  BlackMoveOrdering (
        const ChessBoard &,
        Move &,
        const UnmoveInfo &,
        int depth,
        bool bestPathFlag );

    bool CheckTimeLimit();

    void ChooseEvalFunctions ( ChessBoard &board );

    void FindPrevBestPath ( Move );
    void FoundBestMove ( Move, int depth );
    void HitBottom ( int depth )   { nextBestPath[depth].depth = depth - 1; }
    BestPath *SaveTLMBestPath ( Move );

    SCORE WhiteRookBonus ( const SQUARE *b, int ofs, int bk_offset );
    SCORE BlackRookBonus ( const SQUARE *b, int ofs, int wk_offset );

    SCORE WhiteBishopBonus ( const SQUARE *b, int ofs, int wk_offset );
    SCORE BlackBishopBonus ( const SQUARE *b, int ofs, int bk_offset );

    SCORE WhiteKnightFork ( const SQUARE *p );
    SCORE BlackKnightFork ( const SQUARE *p );

    SCORE WhiteKnightBonus ( const SQUARE *inBoard, int ofs, int wk_offset );
    SCORE BlackKnightBonus ( const SQUARE *inBoard, int ofs, int bk_offset );

    SCORE WhiteQueenBonus ( const SQUARE *b, int ofs, int bk_offset );
    SCORE BlackQueenBonus ( const SQUARE *b, int ofs, int wk_offset );

    SCORE CastleHoleDanger ( const SQUARE *p, int dir, SQUARE mask );


private:
    MoveList   rootml;              // MoveList at the root of the search
    ChessGene  gene;
    int        minlevel;
    int        maxlevel;
    int        level;
    int        maxCheckDepth;     // used in quiescence search to limit check search
    UINT32     visited;
    UINT32     evaluated; 
    UINT32     generated;
    UINT32     visnodes [NODES_ARRAY_SIZE];
    UINT32     gennodes [NODES_ARRAY_SIZE];
    SCORE      searchBias;    // 0=deterministic search, 1=randomized search

    // The following members are used to assist in automatically extending the search...
    bool       extendSearchFlag;      // enabling allows computer to search longer sometimes
    bool       computerPlayingWhite;
    SCORE      expectedScorePrev;
    SCORE      expectedScoreNow;
    INT32      revertTimeLimit;    // 0 means search was not extended

    CCP_SEARCH_TYPE  searchType;     // added 1999-Mar-09

    UINT32     maxNodesEvaluated;   // used when searchType==CCPST_MAXEVAL_SEARCH

    bool       searchAborted;  // gets set when search is aborted
    INT32      stopTime;       // interval marker to stop thinking at (0.01 sec)
    INT32      timeLimit;      // holds time limit from search to search

    INT32      timeCheckCounter;
    INT32      timeCheckLimit;
    INT32      prevTime;

    EvalFunction   whiteEval;   // eval used for White's turn nodes
    EvalFunction   blackEval;   // eval used for Black's turn nodes

    // Best path stuff...

    int      eachBestPathCount;                     // number of paths in eachBestPath[]
    BestPath eachBestPath [MAX_MOVES];              // array of BestPath for each Top Level Move
    BestPath currentBestPath;                       // BestPath from prev search of current TLM
    BestPath nextBestPath [MAX_BESTPATH_DEPTH+8];   // BestPath for next search
    UINT32   expectedNextBoardHash;                 // hash value of board if opponent makes expected move
    int      prevCompletedLevel;                    // highest full-width level completed in previous search
    UINT32   hashPath [MAX_BESTPATH_DEPTH+8];

    // The following variables sneak information to move ordering

    int       moveOrder_depth;
    bool      moveOrder_bestPathFlag;
    Move      moveOrder_xposBestMove;

    bool hitMaxHistory;
    SCORE *whiteHist;
    SCORE *blackHist;

    bool openingBookSearchEnabled;
    bool trainingEnabled;
    bool allowResignation;
    bool enableMoveDisplayFlag;

    SCORE resignThreshold;

    friend class ChessBoard;    // needed for move ordering
    bool firstTimeChooseEval;

    // the KingPosTable... arrays are used only by EndgameEval1 functions.
    // Depending on which piece will deliver mate, they figure out which
    // corners the enemy king should be forced toward.
    static const int KingPosTableQR [144];   // queen or rook: any corner
    static const int KingPosTableBW [144];  // bishop on white: white corners only
    static const int KingPosTableBB [144];  // bishop on black: black corners only

    const int *KingPosTable;     // pointer to chosen king position table

    bool oppTimeInstance;   // is this an opponent time thinker?
    bool oppTimeEnable;     // is this instance allowed to do opp time thinking?
    Move predictedOppMove;

    bool blunderAlertInstance;  // is this a blunder alert thinker?

public:
    // Since it uses so much memory, all ComputerChessPlayer 
    // objects will share a single TranspositionTable object.

    static TranspositionTable *XposTable;
};


//----------------------------------------------------------------------
// The following symbol is the max number of plies that can be stored
// in ChessBoard::gameHistory.  This ought to be enough!
//----------------------------------------------------------------------

#define MAX_GAME_HISTORY   1500
#define MAX_MOVE_STRLEN      20

// The following prime number is used as the size of the tables ChessBoard::whiteRepeatHash and ChessBoard::blackRepeatHash.
#define REPEAT_HASH_SIZE   70001u


#define PACKEDFLAG_WHITE_TO_MOVE    0x01
#define PACKEDFLAG_WKMOVED          0x02
#define PACKEDFLAG_WKRMOVED         0x04
#define PACKEDFLAG_WQRMOVED         0x08
#define PACKEDFLAG_BKMOVED          0x10
#define PACKEDFLAG_BKRMOVED         0x20
#define PACKEDFLAG_BQRMOVED         0x40


class PackedChessBoard
{
public:
    PackedChessBoard();
    PackedChessBoard ( const ChessBoard & );
    PackedChessBoard & operator= ( const ChessBoard & );
    bool operator== ( const ChessBoard &board ) const;
    bool operator== ( const PackedChessBoard &other ) const;

    bool operator!= ( const ChessBoard &board ) const { return !(*this == board); }
    bool operator!= ( const PackedChessBoard &other ) const { return !(*this == other); }

private:
    UINT32  hash;
    BYTE    square [64];
    BYTE    flags;
    Move    prevMove;
};


class ChessBoard
{
public:
    ChessBoard();
    ~ChessBoard();

    ChessBoard ( const ChessBoard & );
    ChessBoard & operator= ( const ChessBoard & );

    bool operator== ( const ChessBoard &other ) const;

    void   Init();        // resets the chess board to beginning-of-game
    UINT32 Hash() const { return cachedHash; }

    //********************************************************************
    //****
    //****   SetSquareContents is "safe": it calls ChessBoard::Update().
    //****
    //********************************************************************
    void SetSquareContents ( SQUARE, int x, int y );

    //********************************************************************
    //****
    //****   SetOffsetContents is "dangerous": you NEED to call Update
    //****   after calling it one or more times before doing anything
    //****   significant with the board (but only if it returns true,
    //****   meaning that it didn't ignore your commands).
    //****   This dangerousness is provided for performance reasons.
    //****   If 'force' is passed true, putting something where a
    //****   king is will force that king to move to the first empty
    //****   square found on the board.  Otherwise, trying to put
    //****   something where a king already is will be ignored.
    //****
    //********************************************************************
    bool SetOffsetContents (
        SQUARE     newSquareContents,
        int        offset,
        bool   force = false );

    void Update();          // For refreshing internal stuff after editing the board

    void ClearEverythingButKings();
    void EditCommand ( Move );   // pass a move with a SPECIAL_MOVE_EDIT 'dest'

    bool HasBeenEdited() const { return initialFen != 0; }
    int  InitialPlyNumber() const { return initialPlyNumber; }

    SQUARE GetSquareContents ( int x, int y ) const;  // (x,y) 0-based on board
    SQUARE GetSquareContents ( int offset ) const;    // standard offset

    int  GenWhiteMoves ( MoveList &, ComputerChessPlayer *player = 0 );
    int  GenBlackMoves ( MoveList &, ComputerChessPlayer *player = 0 );
    int  GenMoves ( MoveList &ml )
    {
        int rc = 0;
        if ( WhiteToMove() )
            rc = GenWhiteMoves ( ml );
        else
            rc = GenBlackMoves ( ml );
        return rc;
    }

    int  GenWhiteCaptures ( MoveList &, ComputerChessPlayer *player = 0 );
    int  GenBlackCaptures ( MoveList &, ComputerChessPlayer *player = 0 );

    int  NumWhiteMoves() const;
    int  NumBlackMoves() const;

    bool  WhiteCanMove();
    bool  BlackCanMove();
    bool  CurrentPlayerCanMove()
    {
        return WhiteToMove() ? WhiteCanMove() : BlackCanMove();
    }

    bool  GameIsOver();

    bool  WhiteToMove()  const;
    bool  BlackToMove()  const;

    bool  WhiteInCheck()  const;
    bool  BlackInCheck()  const;

    bool  CurrentPlayerInCheck() const
    {
        return WhiteToMove() ? WhiteInCheck() : BlackInCheck();
    }

    bool  IsDefiniteDraw ( int *numReps = 0 );      // Does NOT find stalemate!
    int NumberOfRepetitions();

    void   SaveSpecialMove ( Move );

    void   MakeWhiteMove (
        Move       &move,     // can have side-effects!
        UnmoveInfo &unmove,
        bool    look_for_self_check,
        bool    look_for_enemy_check );

    void   MakeBlackMove (
        Move       &move,     // can have side-effects!
        UnmoveInfo &unmove,
        bool    look_for_self_check,
        bool    look_for_enemy_check );

    void MakeMove ( Move &move, UnmoveInfo &unmove )
    {
       if ( WhiteToMove() )
           MakeWhiteMove ( move, unmove, true, true );
       else
           MakeBlackMove ( move, unmove, true, true );
    }

    void   UnmakeWhiteMove ( Move move, UnmoveInfo &unmove );
    void   UnmakeBlackMove ( Move move, UnmoveInfo &unmove );

    void UnmakeMove ( Move move, UnmoveInfo &unmove )
    {
       if ( WhiteToMove() )
           UnmakeBlackMove ( move, unmove );
       else
           UnmakeWhiteMove ( move, unmove );
    }

    bool ScanMove (const char *pgn, Move &move);

    bool  IsAttackedByWhite ( int offset ) const;
    bool  IsAttackedByBlack ( int offset ) const;

    int    GetCurrentPlyNumber() const;
    Move   GetPastMove ( int plyNumber ) const;

    const SQUARE *queryBoardPointer() const { return board; }
    const INT16 *queryInventoryPointer() const { return inventory; }

    bool isLegal ( Move );  // generates legal move list to test legality

    UINT16  GetWhiteKingOffset() const { return wk_offset; }
    UINT16  GetBlackKingOffset() const { return bk_offset; }

    //-------------------------------------------------------------------------
    //   Stores FEN for the current position into 'buffer'.
    //-------------------------------------------------------------------------
    char *GetForsythEdwardsNotation (char *buffer, int buffersize) const;

    //-------------------------------------------------------------------------
    //   If the game did not start with the standard opening position, 
    //   (e.g. the board was edited), this function returns a dynamically-allocated 
    //   string copy of the position from which subsequent moves are recorded.  
    //   The caller must use delete[] to free the allocated memory when 
    //   finished using the FEN string.
    //   If the board has not been edited, and all moves proceed from the
    //   standard opening position, this function returns NULL.
    //-------------------------------------------------------------------------
    char *GetInitialForsythEdwardsNotation() const;

    //-------------------------------------------------------------------------
    //   If 'fen' is valid Forsyth-Edwards notation, and correpsonds
    //   to a position endorsed by PositionIsPossible(), the board
    //   is modified to match the given notation and the method returns true.
    //-------------------------------------------------------------------------
    bool SetForsythEdwardsNotation (const char *fen);


    //-------------------------------------------------------------------------
    //   The following method checks a board that has just been edited
    //   for certain impossible situations, such as having the wrong
    //   number of kings on the board, pawns in rediculous positions,
    //   or the side who just moved still in check.
    //   I have expanded what this function looks for because of the
    //   new SetForsythEdwardsNotation function.
    //-------------------------------------------------------------------------
    bool PositionIsPossible() const;

protected:
    SQUARE      board [144];     // The board's squares

    UINT16      flags;           // See SF_xxx above

    SCORE       wmaterial;       // raw total of white's material
    SCORE       bmaterial;       // raw total of black's material

    UINT16      wk_offset;       // location of white king in the board
    UINT16      bk_offset;       // location of black king in the board

    bool        white_to_move;    // Is it white's turn to move?
    INT16       inventory [PIECE_ARRAY_SIZE];   // how many of each piece
    Move        prev_move;        // Need for en passant
    INT16       ply_number;       // What ply are we on?  0=beginning of game
    Move       *gameHistory;      // array using ply_number as index
    char       *initialFen;       // if board has been edited, holds initial position in FEN notation.  otherwise, is NULL.
    int         initialPlyNumber;   // the ply number beyond which are zero board edits in the gameHistory[] array.

    INT16       lastCapOrPawn;   // ply number of last capture or pawn advance

    // The following are important for detecting draws by repetition
    UINT32      cachedHash;

    // The following arrays store the number of times the given (hash%size)
    // value has been seen, as a way to detect repeated board positions.
    // The size of both arrays is the prime number REPEAT_HASH_SIZE defined above.
    int        *whiteRepeatHash;
    int        *blackRepeatHash;

private:
    bool pgnCloseMatch (const char *pgn, Move move) const;

    UINT32 CalcHash() const;  // calculates 32-bit hash code of board

    void  GenMoves_WP ( MoveList &, int source, int ybase );
    void  GenMoves_WN ( MoveList &, int source );
    void  GenMoves_WB ( MoveList &, int source );
    void  GenMoves_WR ( MoveList &, int source );
    void  GenMoves_WQ ( MoveList &, int source );
    void  GenMoves_WK ( MoveList &, int source );

    void  GenMoves_BP ( MoveList &, int source, int ybase );
    void  GenMoves_BN ( MoveList &, int source );
    void  GenMoves_BB ( MoveList &, int source );
    void  GenMoves_BR ( MoveList &, int source );
    void  GenMoves_BQ ( MoveList &, int source );
    void  GenMoves_BK ( MoveList &, int source );

    void  GenCaps_WP ( MoveList &, int source, int ybase );
    void  GenCaps_WN ( MoveList &, int source );
    void  GenCaps_WB ( MoveList &, int source );
    void  GenCaps_WR ( MoveList &, int source );
    void  GenCaps_WQ ( MoveList &, int source );
    void  GenCaps_WK ( MoveList &, int source );

    void  GenCaps_BP ( MoveList &, int source, int ybase );
    void  GenCaps_BN ( MoveList &, int source );
    void  GenCaps_BB ( MoveList &, int source );
    void  GenCaps_BR ( MoveList &, int source );
    void  GenCaps_BQ ( MoveList &, int source );
    void  GenCaps_BK ( MoveList &, int source );

    bool  WP_CanMove ( int ofs, int ybase );
    bool  WN_CanMove ( int ofs );
    bool  WB_CanMove ( int ofs );
    bool  WR_CanMove ( int ofs );
    bool  WQ_CanMove ( int ofs );
    bool  WK_CanMove ( int ofs );

    bool  BP_CanMove ( int ofs, int ybase );
    bool  BN_CanMove ( int ofs );
    bool  BB_CanMove ( int ofs );
    bool  BR_CanMove ( int ofs );
    bool  BQ_CanMove ( int ofs );
    bool  BK_CanMove ( int ofs );

    void  RemoveIllegalWhite (
        MoveList &ml,
        ComputerChessPlayer *myPlayer = 0 );

    void  RemoveIllegalBlack (
        MoveList &ml,
        ComputerChessPlayer *myPlayer = 0 );


    friend class ComputerChessPlayer;    // for efficiency reasons!
    friend class ChessUI_dos_cga;
    friend class ChessUI_dos_vga;
    friend class PackedChessBoard;

    friend void FormatChessMove (
        const ChessBoard &,
        Move,
        char movestr [MAX_MOVE_STRLEN + 1] );
};


class ChessGame
{
public:
    // The following constructor automatically creates players.
    ChessGame ( ChessBoard &, ChessUI & );

    // The following constructor refrains from creating players,
    // but is told what kind of player objects to expect.
    // Each of the bool parameters should be set to true iff
    // its respective player is an instance of the C++ class HumanPlayer.
    // Otherwise, the value should be false.
    ChessGame ( 
        ChessBoard &, 
        ChessUI &, 
        bool whiteIsHuman,
        bool blackIsHuman,
        ChessPlayer *_whitePlayer,
        ChessPlayer *_blackPlayer );

    virtual ~ChessGame();

    void Play();

    // The following function tells the ChessGame to save the state
    // of the game in the given file after each move.
    // If filename == NULL, any existing auto-save is canceled.
    // The function returns false if any errors occur (e.g. out of memory).
    bool AutoSaveToFile ( const char *filename );

    void SetPlayers ( ChessPlayer * _white,  ChessPlayer * _black )
    {
        whitePlayer = _white;
        blackPlayer = _black;
    }

protected:
    void DetectAutoSaveFile();

protected:
    ChessBoard   &board;
    ChessPlayer  *whitePlayer;
    ChessPlayer  *blackPlayer;
    ChessUI      &ui;

    char *autoSave_Filename;
};

bool SaveGame ( const ChessBoard &board, const char *filename );
bool LoadGame ( ChessBoard &board, const char *filename );


//-------------------------------------------------------
//   Global functions that must be defined on a
//   per-project basis...
//-------------------------------------------------------
void  ChessFatal ( const char *message );   // displays message and aborts
INT32 ChessTime();   // returns interval marker in hundredths of a second


//-------------------------------------------------------
//   Helpful utility functions that are available...
//-------------------------------------------------------
int ChessRandom ( int n );   // returns random value between 0 and n-1
char PieceRepresentation ( SQUARE square );

// The Distance function returns minimum number of squares that a king
// would have to travel to get from ofs1 to ofs2.
inline int Distance ( int ofs1, int ofs2 )
{
    int xdist = XPART(ofs1) - XPART(ofs2);
    if ( xdist < 0 ) xdist = -xdist;
    int ydist = YPART(ofs1) - YPART(ofs2);
    if ( ydist < 0 ) ydist = -ydist;
    return (xdist > ydist) ? xdist : ydist;
}


void FormatChessMove (
    const ChessBoard &,
    Move,
    char movestr [MAX_MOVE_STRLEN + 1] );

void FormatChessMove (      // This version is much more efficient if you already have a legal move list!
    const ChessBoard &,
    const MoveList &LegalMoves,
    Move,
    char movestr [MAX_MOVE_STRLEN + 1] );

void GetGameListing (
    const ChessBoard &,
    char *listing,
    unsigned maxListingLength );


bool ParseMove (
    const char  *notation,
    ChessBoard  &board,
    int         &source,
    int         &dest,
    SQUARE      &promIndex,
    Move        &move
);

inline bool ParseFancyMove (
    const char  *notation,
    ChessBoard  &board,
    Move        &move )
{
    int     ignoredSource;
    int     ignoredDest;
    SQUARE  ignoredPromIndex;
    return ParseMove(notation, board, ignoredSource, ignoredDest, ignoredPromIndex, move);
}

inline bool ParseFancyMove(
    const char *notation,
    ChessBoard &board,
    int        &source,
    int        &dest,
    SQUARE     &promIndex)
{
    Move ignoredMove;
    return ParseMove(notation, board, source, dest, promIndex, ignoredMove);
}

extern int Learn_Output;


void GenerateEndgameDatabases (
    ChessBoard &board,
    ChessUI &ui );


enum PGN_FILE_STATE {
    PGN_FILE_STATE_UNDEFINED,
    PGN_FILE_STATE_NEWGAME,
    PGN_FILE_STATE_SAMEGAME,
    PGN_FILE_STATE_GAMEOVER,
    PGN_FILE_STATE_EOF,
    PGN_FILE_STATE_SYNTAX_ERROR,
    PGN_FILE_STATE_INVALID_PARAMETER,
    PGN_FILE_STATE_ILLEGAL_MOVE
};

const char *GetPgnFileStateString (PGN_FILE_STATE);

const char *ConvertDateToVersion (const char *compileDateString);   // pass in __DATE__ to get a string like "2008.11.06".

char *CopyString (const char *string);
void  FreeString (char *&string);
void  ReplaceString (char *&target, const char *source);

#endif  // __DDC_CHESS_32_H

/*
    $Log: chess.h,v $
    Revision 1.27  2009/09/16 16:49:34  Don.Cross
    Added support for the WinBoard/xboard "memory" command to set hash table sizes.

    Revision 1.26  2008/11/24 11:21:58  Don.Cross
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

    Revision 1.25  2008/11/22 16:27:32  Don.Cross
    Massive overhaul to xchenard: now supports pondering.
    This required very complex changes, and will need a lot of testing before release to the public.
    Pondering support made it trivial to implement the "hint" command, so I added that too.
    Pondering required careful attention to every possible command we could receive from xboard,
    so now I have started explicitly sending back errors for any command we don't understand.
    I finally figured out how to get rid of the annoying /WP64 warning from the Microsoft C++ compiler.

    Revision 1.24  2008/11/12 23:06:07  Don.Cross
    Massive refactoring of the concept of an 'editedFlag' in ChessBoard.
    Now instead of just saying the board was edited, we will always remember the most recent position
    after which there are no more pseudo-moves representing edit actions.
    This allows us to save/load PGN files based on edited positions.
    More work needs to be done:
    - Need to allow move undo/redo to work in edited positions.
    - Because the game history is stored now inside the ChessBoard, it seems silly to have an external copy also!
    - Move numbers should be based on FEN when PGN is saved.
    In general, this is a scary change, so it needs more careful testing before being published.

    Revision 1.23  2008/11/12 17:44:44  Don.Cross
    Replaced ComputerChessPlayer::WhiteDbEndgame and ComputerChessPlayer::BlackDbEndgame
    with a single method ComputerChessPlayer::FindEndgameDatabaseMove.
    Replaced complicated ad hoc logic for captures and promotions changing the situation in endgame searches
    with generic solution that uses feedback from prior endgame database calculations.
    This allows us to accurately calculate optimal moves and accurately report how long it will take to
    checkmate the opponent.  The extra file I/O required in the search does not appear to slow things down too badly.

    Revision 1.22  2008/11/08 16:54:39  Don.Cross
    Fixed a bug in WinChen.exe:  Pondering continued even when the opponent resigned.
    Now we abort the pondering search when informed of an opponent's resignation.

    Revision 1.21  2008/11/06 22:25:06  Don.Cross
    Now ParseFancyMove accepts either PGN or longmove (e7e8q) style notation only.
    We tolerate of check '+' or checkmate '#' characters being absent or extraneous in the input.
    The former is especially important because I often see PGN notation where '+' is missing on checking moves.
    To allow for PGN input, I had to allow avoidance of the PromotePawn callback, so other code was changed as well.
    Version numbers are now always constently yyyy.mm.dd across all builds of Chenard!

    Revision 1.20  2008/11/02 21:04:27  Don.Cross
    Building xchenard for Win32 console mode now, so I can debug some stuff.
    Tweaked code to make Microsoft compiler warnings go away: deprecated functions and implicit type conversions.
    Checking in with version set to 1.51 for WinChen.

    Revision 1.19  2008/11/02 20:04:13  Don.Cross
    xchenard: Implemented xboard command 'setboard'.
    This required a new function ChessBoard::SetForsythEdwardsNotation.
    Enhanced ChessBoard::PositionIsPossible to check for more things that are not possible.
    Fixed anachronisms in ChessBoard::operator== where I was returning 0 or 1 instead of false or true.

    Revision 1.18  2008/10/27 21:47:07  Don.Cross
    Made fixes to help people build text-only version of Chenard in Linux:
    1. The function 'stricmp' is called 'strcasecmp' in Linux Land.
    2. Found a fossil 'FALSE' that should have been 'false'.
    3. Conditionally remove some relatively unimportant conio.h code that does not translate nicely in the Linux/Unix keyboard model.
    4. Added a script for building using g++ in Linux.

    Revision 1.17  2006/02/19 23:25:06  dcross
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

    Revision 1.16  2006/01/28 21:44:16  dcross
    1. Updated auto-save feature to use PGN format instead of old GAM format.
       This means we are sacrificing the ability to auto-save edited games, but I don't care.
    2. Added a feature in class ChessGame: if the file 'game.counter' exists, auto-save
       to file based on integer counter it contains, and increment that counter in the file.
       This is so I no longer have to manually make up unique filenames and remember to
       save files when Chenard is in "combat mode".
    3. Reworked PGN save functions so they can be shared by Win32 and portable versions.
       This requires all versions of Chenard to define functions GetWhitePlayerString and
       GetBlackPlayerString, so the players can be recorded in the PGN header.

    Revision 1.15  2006/01/25 19:09:12  dcross
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

    Revision 1.14  2006/01/18 19:58:11  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.13  2006/01/10 22:08:40  dcross
    Fixed a bug that caused Chenard to bomb out with a ChessFatal:
    Go into edit mode with White to move, and place a White piece where it
    attacks Black's king.  Chenard would immediately put a dialog box saying
    that a Black king had been captured, and exit.  Now you just get a dialog
    box saying "doing that would create an impossible board position", but
    Chenard keeps running.

    Revision 1.12  2006/01/08 15:11:26  dcross
    1. Getting ready to add support for wp.egm endgame database.  I know I am going to need
       an easy way to detect pawn promotions, so I have modified the venerable Move::actualOffsets
       functions to detect that case for me and return EMPTY if a move is not a pawn promotion,
       or the actual piece value if it is a promotion.
    2. While doing this I noticed and corrected an oddity with the PROM_PIECE macro:
        the parameter 'piece' was unused, so I removed it.

    Revision 1.11  2006/01/08 02:11:08  dcross
    Initial tests of wq.egm are very promising!

    1. I was having a fit because I needed to get a move's actual offsets without having the correct board position.
       When I looked at the code for Move::actualOffsets, I suddenly realized it only needs to know whether
       it is White's or Black's turn.  So I overloaded the function to send in either a ChessBoard & (for compatibility)
       or a boolean telling whether it is White's move.

    2. Coded generic ConsultDatabase, WhiteDbEndgame, BlackDbEndgame.
       Still needs work for pawns to have any possibility of working right.

    Revision 1.10  2006/01/07 13:45:09  dcross
    Now ChessFatal will execute a breakpoint instruction in debug builds.
    This allows me to see exactly what's going on inside the debugger.

    Revision 1.9  2006/01/06 19:17:34  dcross
    Because I discovered that there are positions that require 54 moves to mate
    (king + bishop + knight, vs king), I have changed WIN_DELAY_PENALTY from 10 to 1.
    Without this change, 30000 - 54*2*10 = 28920, which is too small to be considered a forced mate.
    This change makes it possible to have up to 500 moves in a forced mate.
    One possible problem with this is incorrect mate announcement for moves stored in chenard.tre.

    Revision 1.8  2006/01/03 02:08:23  dcross
    1. Got rid of non-PGN move generator code... I'm committed to the standard now!
    2. Made *.pgn the default filename pattern for File/Open.
    3. GetGameListing now formats more tightly, knowing that maximum possible PGN move is 7 characters.

    Revision 1.7  2006/01/02 19:40:29  dcross
    Added ability to use File/Open dialog to load PGN files into Win32 version of Chenard.

    Revision 1.6  2006/01/01 19:19:53  dcross
    Starting to add support for loading PGN data files into Chenard.
    First I am starting with the ability to read in an opening library into the training tree.
    Not finished yet.

    Revision 1.5  2005/12/31 21:37:02  dcross
    1. Added the ability to generate Forsyth-Edwards Notation (FEN) string, representing the state of the game.
    2. In stdio version of chess UI, display FEN string every time the board is printed.

    Revision 1.4  2005/12/31 17:30:40  dcross
    Ported PGN notation generator from Chess.java to Chenard.
    I left in the goofy old code just in case I ever need to use it again (see #define PGN_FORMAT).

    Revision 1.3  2005/11/23 21:30:29  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:38  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!


     Revision history:

    1993 April ?? [Don Cross]
         Started writing
    
    1993 August 30 [Don Cross]
         Changing pointers to references in the interfaces where
         appropriate.
    
    1993 October 22 [Don Cross]
         Moving all operating system specific stuff into separate
         header files.
    
    1994 January 15 [Don Cross]
         Added ComputerChessPlayer::AbortSearch().  I did this to
         facilitate certain features of the OS/2 Presentation Manager
         version of this chess program.
    
    1994 January 22 [Don Cross]
         Going to change the search so that it uses a pointer to
         member function to call the eval function.  The eval function
         will be changed according to the situation.  This will replace
         my older endgame heuristics.
    
    1994 February 2 [Don Cross]
         Added BestPath stuff.
    
    1994 February 10 [Don Cross]
         Starting implementing keeping of piece offsets.
         This should speed eval greatly!
    
    1995 March 26 [Don Cross]
         Added new constructor ChessGame::ChessGame ( bool, bool ).
         This constructor refrains from creating the players, but is told what
         kind of players to expect.
    
         Added simple learning algorithm.
         The ComputerChessPlayer class remembers every move it makes in
         every game it plays.  The scores assigned to each position is
         based on the time the number of seconds evaluating it was maximized.
         An entire "game tree" is saved to disk in binary form after each
         move generated by a timed search.
    
    1995 July 13 [Don Cross]
         Removed UI dependencies from global functions LoadGame and SaveGame.
         Moved these functions out of UISTDIO into the portable code space
         (game.cpp).
         Added ChessGame::AutoSaveToFile.
    
    1995 December 25 [Don Cross]
         Fixed a bug!  ChessBoard needed a copy constructor and
         an assignment operator.
    
    1996 January 1 [Don Cross]
         Adding the function ParseFancyMove.
         This function recognizes more natural move notation like
         B-c6, PxP, etc.
    
    1996 January 23 [Don Cross]
         Starting to add code to experiment with transposition tables.
    
    1997 January 3 [Don Cross]
         Added ComputerChessPlayer::QueryTimeLimit().
    
    1998 December 25 [Don Cross]
         Started porting to Linux.
    
    1999 January 6 [Don Cross]
         Adding support for InternetChessPlayer.
    
    1999 January 15 [Don Cross]
         Removing '#pragma pack(1)' because it was causing problems
         under SunOS build.  It really turns out to be unnecessary,
         because the main reason I wanted it in the first place was
         to make sure that the Move struct was 4 bytes.  Discovered
         that the structure is still 4 bytes without the pragma,
         so it's history!
    
    1999 January 19 [Don Cross]
         Trying one more time to write code that correctly detects
         draw by repetition!  If it doesn't work this time, I'm 
         gonna go break something.
    
    1999 January 21 [Don Cross]
         Adding an "oops" mode for the search, where if it sees the
         score drop, the search gets extended to find a way out of
         the mistake instead of just making it because search time elapsed.
    
    1999 February 11 [Don Cross]
         Added 'hashPath[]' array to ComputerChessPlayer, to detect
         a single repetition within a path as being equivalent to a draw.
         This should prevent time-wasting in timed games which could 
         otherwise delay avoidance or forcing of a draw by repetition.
    
    1999 February 17 [Don Cross]
         Creating a 'gene' for Chenard's evaluation and move ordering
         heuristics that can be read/written to a file, to replace
         static #defined constants.  This will allow a genetic algorithm
         to automatically search for better heuristics.
    
    1999 February 19 [Don Cross]
         Adding ChessUI::SetAdHocText() as a way to set special text
         in graphical environments.
    
    1999 March 9 [Don Cross]
         Adding a new search type: max nodes evaluated.
         This required changing boolean variable to
         a new enumerated type.
    
    1999 August 4 [Don Cross]
         Adding ChessBoard::isLegal(Move) to stomp out a bug!
    
    1999 August 5 [Don Cross]
         Added utility function ChessBoard::GameIsOver().
    
    2001 January 4 [Don Cross]
         Making KingPosTable... arrays member variables, so that
         the correct one can be chosen when using EndgameEval1.
    
    2001 January 5 [Don Cross]
         Adding support for generation of endgame databases,
         specifically for K+R vs K, K+Q vs K, K+P vs K.
         When there are three pieces on the board, there is
         an upper limit of 64*63*62 = 249984 possible positions.
         We can pre-compute a file of optimal moves for these
         situations.
    
    2001 January 6 [Don Cross]
         Adding virtual ChessUI::allowMateAnnounce hook for pre-existing
         allowMateAnnounce in the win32 version of the ChessUI.
    
    2001 January 13 [Don Cross]
         Adding support for thinking on opponent's turn.
         This will be handled by the ChessUI abstract class, because
         some UIs will support multi-threading and some won't, and
         the ones that do will do so in OS-specific ways.
         
    2002 November 21 [Don Cross]
         Drinking beer due to loss of version 1.044 source code.
         I still have the 1.044 binary in \\zaphod\bin\winchen.exe.
         Thinking about WinDiff-ing that binary with this source
         code's binary.
    
*/

