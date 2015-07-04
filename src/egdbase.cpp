/*
    egdbase.cpp  -  Don Cross, 7 January 2006.

    Second-generation endgame database generator/fetcher.
    I am starting over, replacing egdb.cpp completely.
    Improvements:

    1. Rely on 8-fold symmetry to reduce database size and generation time.

        Bishops will be allowed on all 64 squares, unlike in egdb.cpp.
        Pawns will be allowed to have left-right symmetry only, and
        are allowed on only 8*6 = 48 squares.
        All other pieces will have full 8-fold symmetry and be in any of 64 squares.
        For the set of pieces given, all allowed symmetries will be applied, and
        the table index with the smallest value will be calculated and kept.
        Some symmetries reduce the search space (and table size) by a factor of 2.
        Others are not quite 2 because of the diagonal pivot problem:  the
        FlipSlash symmetry leaves all the squares on the pivotal diagonal
        in their same respective locations.  If all the pieces are on this
        diagonal, the symmetric position is identical, and so it has no
        space-saving equivalent.

        Another symmetry:  in the case of "wbwb.egm", swapping the bishops
        will put each on the opposite color square, but the search space
        is identical.  In other words, the two pieces are interchangeable.

        To exploit symmetry of interchangeable pieces, we will pick whichever
        table index has the least value by permuting those pieces.

        Because the eventual size of each database is a bit tricky to calculate,
        I am going to have the computer do it for me automatically in each special case.
        This means I will not be able to use a hard-coded constant size to determine
        whether a database is complete or not.  Therefore I will have a file prefix
        for special information, including file signature and self-reporting size.

    2. Try to use more generic algorithms to reduce code bloat.

        A single function will generate any kind of desired database.
        It will be sent an array like this:  {BKING, WKING, WBISHOP, WKNIGHT}.
        The first 2 slots will always be {BKING,WKING}.

        The name of the database file will always be based on the non-king pieces
        in array order, and will have an extension of "egm".  For the example
        array listed above, it will be:  "wbwn.egm".  Note that I am reserving
        the ability to generate databases where Black still has non-king piece(s).
*/

#ifdef _WIN32
// needed for setting process priority
#include <windows.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chess.h"

//----------------------------------------------------------------------------------------------

struct tDatabasePrefix
{
    char        signature [4];      // "egdb"
    short       prefixSize;         // sizeof(tDatabasePrefix)
    short       entrySize;          // 2 or 4, depending on compression level
    unsigned    numTableEntries;    // how many entries are stored in the table
    unsigned    reserved [5];       // pad out so hex dump is easy to read
};

//----------------------------------------------------------------------------------------------

int Global_EndgameDatabaseMode = 0;

FILE *dblog = NULL;     // File where we log a text listing while calculating the endgame database

//----------------------------------------------------------------------------------------------

inline unsigned CalcPieceOffset (unsigned pieceIndex)
{
    unsigned x = pieceIndex % 8;
    unsigned y = pieceIndex / 8;
    assert (y <= 7);
    return (x+2) + 12*(y+2);
}

inline unsigned CalcPieceIndex (unsigned pieceOffset)
{
    int x = pieceOffset % 12;
    int y = pieceOffset / 12;
    assert ((x >= 2) && (x <= 9));
    assert ((y >= 2) && (y <= 9));
    return (x-2) + 8*(y-2);
}

inline bool OffsetIsValid (unsigned pieceOffset)
{
    int x = pieceOffset % 12;
    int y = pieceOffset / 12;
    return (x >= 2) && (x <= 9) && (y >= 2) && (y <= 9);
}


inline bool SquareIsWhite (unsigned offset)
{
    int x = XPART(offset);
    int y = YPART(offset);
    assert ((x >= 2) && (x <= 9) && (y >= 2) && (y <= 9));
    return ((x+y) & 1) != 0;
}


inline bool SquareIsBlack (unsigned offset)
{
    return !SquareIsWhite(offset);
}

//----------------------------------------------------------------------------------------------


int Symmetry (int n, int ofs)
{
    // The value of n is treated as 0..7.  Only the 3 lowest bits are used.
    // All other bits in n are ignored.
    // The lowest  bit (1) selects whether x is replaced with 11-x.
    // The middle  bit (2) selects whether y is replaced with 11-y.
    // The highest bit (4) selects whether the coordinates <x,y> are swapped to <y,x>.

    int x = XPART(ofs);
    int y = YPART(ofs);

    if (n & 1)
    {
        x = 11 - x;
    }

    if (n & 2)
    {
        y = 11 - y;
    }

    if (n & 4)
    {
        int t = x;
        x = y;
        y = t;
    }

    return OFFSET(x,y);
}


// The following are symbolic names for values of 'n'
// that you can pass to Symmetry():

#define SYMMETRY_IDENTITY           0
#define SYMMETRY_LEFT_RIGHT_MIRROR  1
#define SYMMETRY_TOP_BOTTOM_MIRROR  2
#define SYMMETRY_ROTATE_180         3
#define SYMMETRY_FLIP_SLASH         4
#define SYMMETRY_CLOCKWISE          5
#define SYMMETRY_COUNTERCLOCKWISE   6
#define SYMMETRY_FLIP_BACKSLASH     7

// Use the following table to undo symmetry.
// Note CLOCKWISE and COUNTERCLOCKWISE are mutual inverses; all others are self inverses.
// Example:  INVERSE_SYMMETRY[SYMMETRY_CLOCKWISE] == SYMMETRY_COUNTERCLOCKWISE.

const int INVERSE_SYMMETRY[] =
{
    SYMMETRY_IDENTITY,
    SYMMETRY_LEFT_RIGHT_MIRROR,
    SYMMETRY_TOP_BOTTOM_MIRROR,
    SYMMETRY_ROTATE_180,
    SYMMETRY_FLIP_SLASH,
    SYMMETRY_COUNTERCLOCKWISE,
    SYMMETRY_CLOCKWISE,
    SYMMETRY_FLIP_BACKSLASH
};

//-----------------------------------------------------------------------------------------------------

Move RotateMove (Move move, bool white_to_move, int sym)
{
    int msource, mdest;
    SQUARE promotion = move.actualOffsets (white_to_move, msource, mdest);
    move.source = Symmetry (sym, msource);
    if (promotion)
    {
        // We need to use a special destination code for pawn promotion.
        // We can be rotating for one of 2 reasons:  either left/right
        // symmetry optimizations, or rotation to toggle White/Black.
        switch (sym)
        {
        case SYMMETRY_IDENTITY:
            // Do nothing:  move.dest is already correct
            break;

        case SYMMETRY_LEFT_RIGHT_MIRROR:
        case SYMMETRY_ROTATE_180:
            // Need to toggle east/west...
            assert (move.dest > OFFSET(9,9));   // otherwise, this couldn't really be a promotion!
            switch (move.dest & SPECIAL_MOVE_MASK)
            {
            case SPECIAL_MOVE_PROMOTE_NORM:
                // leave normal promotion alone
                break;

            case SPECIAL_MOVE_PROMOTE_CAP_EAST:
                move.dest = (move.dest & PIECE_MASK) | SPECIAL_MOVE_PROMOTE_CAP_WEST;   // change east to west
                break;

            case SPECIAL_MOVE_PROMOTE_CAP_WEST:
                move.dest = (move.dest & PIECE_MASK) | SPECIAL_MOVE_PROMOTE_CAP_EAST;   // change west to east
                break;

            default:
                assert(false);      // not pawn promotion, but we thought it was!
                break;
            }
            break;

        default:
            assert(false);  // invalid symmetry request for a pawn move
            break;
        }
    }
    else
    {
        move.dest = Symmetry (sym, mdest);
    }
    return move;
}

//-----------------------------------------------------------------------------------------------------

SQUARE AdjustPiece (SQUARE piece, bool WinnerIsWhite)
{
    if (!WinnerIsWhite)
    {
        switch (piece)
        {
        case EMPTY:     return EMPTY;

        case WPAWN:     return BPAWN;
        case WKNIGHT:   return BKNIGHT;
        case WBISHOP:   return BBISHOP;
        case WROOK:     return BROOK;
        case WQUEEN:    return BQUEEN;
        case WKING:     return BKING;

        case BPAWN:     return WPAWN;
        case BKNIGHT:   return WKNIGHT;
        case BBISHOP:   return WBISHOP;
        case BROOK:     return WROOK;
        case BQUEEN:    return WQUEEN;
        case BKING:     return WKING;

        default:
            assert (false);
        }
    }

    return piece;
}

//-----------------------------------------------------------------------------------------------------


const int FlipSlashOffsets[64] =
{
    26, 27, 28, 29,
    39, 40, 41,
    52, 53,
    65
};

const int FlipSlashTable [64] =         // abcdefgh
{
     0,  1,  2,  3, 10, 11, 12, 13,     // 0123.... 1
    14,  4,  5,  6, 15, 16, 17, 18,     // .456.... 2
    19, 20,  7,  8, 21, 22, 23, 24,     // ..78.... 3
    25, 26, 27,  9, 28, 29, 30, 31,     // ...9.... 4
    32, 33, 34, 35, 36, 37, 38, 39,     // ........ 5
    40, 41, 42, 43, 44, 45, 46, 47,     // ........ 6
    48, 49, 50, 51, 52, 53, 54, 55,     // ........ 7
    56, 57, 58, 59, 60, 61, 62, 63      // ........ 8
};

//----------------------------------------------------------------------------------------------

const int LeftRightOffsets[64] =
{
     26,  27,  28,  29,
     38,  39,  40,  41,
     50,  51,  52,  53,
     62,  63,  64,  65,
     74,  75,  76,  77,
     86,  87,  88,  89,
     98,  99, 100, 101,
    110, 111, 112, 113
};

const int LeftRightTable [64] =         // abcdefgh
{
     0,  1,  2,  3, 32, 33, 34, 35,     // 0123.... 1
     4,  5,  6,  7, 36, 37, 38, 39,     // 4567.... 2
     8,  9, 10, 11, 40, 41, 42, 43,     // 89@@.... 3
    12, 13, 14, 15, 44, 45, 46, 47,     // @@@@.... 4
    16, 17, 18, 19, 48, 49, 50, 51,     // @@@@.... 5
    20, 21, 22, 23, 52, 53, 54, 55,     // @@@@.... 6
    24, 25, 26, 27, 56, 57, 58, 59,     // @@@@.... 7
    28, 29, 30, 31, 60, 61, 62, 63      // @@@@.... 8
};


//----------------------------------------------------------------------------------------------

const int MAX_PIECE_SET = 4;    // Any bigger than this and resulting tables are excessively large for memory!  (e.g. 5 ==> 1.6 GB)
const int MAX_DATABASE_FILENAME = (MAX_PIECE_SET-2)*2 + 4 + 1;      // "wbwn" + ".egm" + '\0'

class tPieceSet
{
public:
    tPieceSet()     // creates a special "null" set that signifies undefined state
    {
        numPieces = 0;
        contains_pawn = false;
        filename[0] = '\0';
        winner_is_white = true;
        compressDatabaseFile = false;
    }

    tPieceSet (SQUARE nonKing)
    {
        numPieces = 3;
        piece[0] = BKING;
        piece[1] = WKING;
        piece[2] = nonKing;
        assert (nonKing & WHITE_MASK);
        init();
    }

    tPieceSet (SQUARE nonKing1, SQUARE nonKing2)
    {
        numPieces = 4;
        piece[0] = BKING;
        piece[1] = WKING;
        piece[2] = nonKing1;
        piece[3] = nonKing2;
        init();
    }

    bool isValid() const
    {
        return numPieces >= 2;
    }

    int getNumPieces() const
    {
        return numPieces;
    }

    int getNumNonKingPiecesForSide (SQUARE sideMask) const
    {
        assert (sideMask==WHITE_MASK || sideMask==BLACK_MASK);
        int count = 0;
        for (int i=2; i < numPieces; ++i)
        {
            if (piece[i] & sideMask)
            {
                ++count;
            }
        }
        return count;
    }

    void setWinnerSide (bool _winner_is_white)
    {
        winner_is_white = _winner_is_white;
    }

    unsigned getTableSize() const
    {
        return table_size;
    }

    SQUARE getPiece (int index) const
    {
        assert ((index >= 0) && (index < numPieces));
        return piece[index];
    }

    bool setPieceOffset (int index, int ofs)
    {
        /*
            To assist in pruning out as many illegal positions as possible,
            this function return false if the piece just placed is definitely
            in an illegal offset, compared only to pieces with lower piece indexes.
            For example, WKING (offset 0) is always legal, but BKING (offset 1)
            is not allowed to overlap or touch WKING.
            The offset is modified whether this function returns true or false.
        */

        assert ((index >= 0) && (index < numPieces));
        assert (OffsetIsValid(ofs));
        offset[index] = ofs;

        if (index == 1)
        {
            if (kingsAreTouching())
            {
                return false;
            }
        }
        else
        {
            for (int i=0; i<index; ++i)
            {
                if (piece[i] == piece[index])
                {
                    // Hack to reduce search space:  if 2 pieces identical, force offsets to stay in order.
                    // Symmetries will take care of computing the missing positions!
                    if (offset[i] >= offset[index])
                    {
                        return false;
                    }

                    // If the two pieces are bishops, don't allow them to be on the same square color...
                    if (piece[index] == WBISHOP)
                    {
                        if (SquareIsWhite(offset[i]) == SquareIsWhite(offset[index]))
                        {
                            return false;
                        }
                    }
                }
                else
                {
                    if (offset[i] == offset[index])
                    {
                        return false;
                    }
                }
            }
        }

        return true;
    }

    int getPieceOffset (int index) const
    {
        assert ((index >= 0) && (index < numPieces));
        assert (OffsetIsValid(offset[index]));
        return offset[index];
    }

    SQUARE movePiece (ChessBoard &board, Move move, int &moved_piece_index, int &original_piece_offset)
    {
        int msource, mdest;
        SQUARE promotion = move.actualOffsets (board, msource, mdest);
        moved_piece_index = -1;
        original_piece_offset = -1;

        for (int p=0; p < numPieces; ++p)
        {
            if (msource == offset[p])
            {
                moved_piece_index = p;
                original_piece_offset = offset[p];
                offset[p] = mdest;
                break;
            }
        }

        assert (moved_piece_index >= 0);    // otherwise we could not find the moved piece!
        return promotion;
    }

    void unmovePiece (int moved_piece_index, int original_piece_offset)
    {
        assert ((moved_piece_index >= 0) && (moved_piece_index < numPieces));
        assert (OffsetIsValid(original_piece_offset));
        offset[moved_piece_index] = original_piece_offset;
    }

    unsigned getTableIndex (int &best_sym) const
    {
        // Returns minimal table index for this position, along with symmetry type used to obtain it.

        best_sym = SYMMETRY_IDENTITY;
        unsigned  ti = getTableIndexForSymmetry (best_sym);

        int sym1, sym2;     // limit of symmetries allowed

        if (contains_pawn)
        {
            // The only symmetry we can validly use is left-right symmetry.
            sym1 = sym2 = SYMMETRY_LEFT_RIGHT_MIRROR;
        }
        else
        {
            // No pawns in the position, so we can use any of other 7 symmetries.
            sym1 = 1;
            sym2 = 7;
        }

        for (int sym = sym1; sym <= sym2; ++sym)
        {
            unsigned xi = getTableIndexForSymmetry (sym);
            if (xi < ti)
            {
                // Found a better index/symmetry pair...
                best_sym = sym;
                ti = xi;
            }
        }

        assert (ti < table_size);
        return ti;
    }

    bool kingsAreTouching() const
    {
        // Due to a bug in ChessBoard that I need to fix some day,
        // if you tell it to put 2 kings touching, it causes the program to die.
        // We avoid that by checking the coordinates with this function.
        // This function also lets you know if 2 kings are on the same square.
        int dx = XPART(offset[0]) - XPART(offset[1]);
        int dy = YPART(offset[0]) - YPART(offset[1]);
        if (dx < 0)
        {
            dx = -dx;
        }
        if (dy < 0)
        {
            dy = -dy;
        }
        return (dx<=1) && (dy<=1);
    }

    const char *getFileName() const
    {
        return filename;
    }

    bool containsPawn()
    {
        return contains_pawn;
    }

    bool findPieces (const ChessBoard &board)
    {
        const SQUARE *bptr = board.queryBoardPointer();

        // Set all pieces to invalid offsets so we know we haven't found any.
        // We assume the board contains exactly the pieces we expect.

        int p;
        for (p=0; p < numPieces; ++p)
        {
            offset[p] = 0;
        }

        for (int x=2; x <= 9; ++x)
        {
            for (int y=2; y <= 9; ++y)
            {
                int ofs = OFFSET(x,y);
                SQUARE s = getAdjustedPiece (bptr, ofs);
                if (s != EMPTY)
                {
                    bool found = false;
                    for (p=0; p < numPieces; ++p)
                    {
                        if ((piece[p] == s) && (offset[p] == 0))
                        {
                            // Found one of the remaining pieces in our list.
                            offset[p] = ofs;
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                    {
                        assert (false);    // there is a piece in the board that is not expected
                        return false;
                    }
                }
            }
        }

        // Make sure all pieces were found...
        for (p=0; p < numPieces; ++p)
        {
            if (offset[p] == 0)
            {
                assert (false);     // we expected a piece that was never found
                return false;
            }
        }

        return true;    // the board matches this piece set exactly
    }

    bool isExactMatch (const ChessBoard &board) const
    {
        int i, x, y;
        SQUARE p;

        static const SQUARE ValidPieceValues[] =
        {
            BPAWN, BKNIGHT, BBISHOP, BROOK, BQUEEN, BKING,
            WPAWN, WKNIGHT, WBISHOP, WROOK, WQUEEN, WKING
        };
        static const int NumValidPieceValues = sizeof(ValidPieceValues) / sizeof(ValidPieceValues[0]);

        // Tally the inventory in this piece set...
        INT16 setInventory [PIECE_ARRAY_SIZE] = {0};
        for (i=0; i < numPieces; ++i)
        {
            ++setInventory [ SPIECE_INDEX(piece[i]) ];
        }

        // Compare board inventory to set inventory, correcting for whose turn it actually is...
        const INT16 *boardInventory = board.queryInventoryPointer();
        const bool whiteToMove = board.WhiteToMove();
        for (i=0; i < NumValidPieceValues; ++i)
        {
            p = ValidPieceValues[i];
            x = SPIECE_INDEX (p);
            y = SPIECE_INDEX (AdjustPiece (p, whiteToMove));
            if (boardInventory[y] != setInventory[x])
            {
                return false;   // the board is not consistent with this piece set
            }
        }

        return true;    // the board exactly matches this piece set
    }

    bool expandMove (const unsigned char record[2], Move &RawMove)
    {
        bool decoded = false;

        // If the record[1] is 0, it means this table entry is null.
        // Otherwise it tells how many plies until checkmate: 1..255.
        if (record[1] != 0)
        {
            RawMove.score = WHITE_WINS - WIN_POSTPONEMENT(record[1]);

            // The move itself is encoded in record[0].
            // Highest 2 bits indicate which of White's pieces is moving: (0..2).
            int pieceIndex = (record[0] >> 6) + 1;  // add 1 because piece[0] is the BKING.
            if ((pieceIndex > 0) && (pieceIndex < numPieces))
            {
                assert (piece[pieceIndex] & WHITE_MASK);    // must be moving one of White's pieces!
                RawMove.source = offset[pieceIndex];

                if (piece[pieceIndex] == WPAWN)
                {
                    // Currently, we assume pawn never captures anything, because Black
                    // has no pieces on the board except his king.
                    if (YPART(offset[pieceIndex]) == 8)
                    {
                        // We need 1 bit to encode whether pawn promotes to ROOK or QUEEN.
                        if (record[0] & 1)
                        {
                            RawMove.dest = (SPECIAL_MOVE_PROMOTE_NORM | Q_INDEX);
                        }
                        else
                        {
                            RawMove.dest = (SPECIAL_MOVE_PROMOTE_NORM | R_INDEX);
                        }
                    }
                    else
                    {
                        if (record[0] & 1)
                        {
                            RawMove.dest = RawMove.source + (2 * NORTH);    // pawn is moving 2 squares forwards
                            assert (YPART(RawMove.source) == 3);
                        }
                        else
                        {
                            RawMove.dest = RawMove.source + NORTH;          // pawn is moving 1 square forward
                        }
                    }
                }
                else
                {
                    // The low 6 bits indicate the destination square: 0..63.
                    RawMove.dest = CalcPieceOffset (record[0] & 63);
                }
                decoded = true;
            }
            else
            {
                assert(false);  // something is wrong with either the database or this decoder
            }
        }

        return decoded;
    }

    bool decodeMove(int entrySize, const void *entryData, Move &rawMove)
    {
        switch (entrySize)
        {
        case sizeof(Move):
            rawMove = *((const Move *)entryData);
            return rawMove.dest != 0;

        case 2:
            return expandMove((unsigned char *)entryData, rawMove);

        default:
            return false;
        }
    }

    bool encodeDatabase (FILE *dbfile, unsigned numTableEntries, const Move *table)
    {
        assert (numTableEntries <= table_size);
        for (unsigned i=0; i < numTableEntries; ++i)
        {
            if (!encodeAndSaveMove (dbfile, table[i], i))
            {
                return false;
            }
        }

        return true;
    }

    void decodeForTableIndex (unsigned ti)
    {
        // Figure out all the piece offsets for this table index.
        // Work our way backwards from the last piece, so the
        // BKING is last.
        unsigned original_ti = ti;

        int p;
        for (p=numPieces-1; p > 0; --p)
        {
            if (piece[p] == WPAWN)
            {
                offset[p] = CalcPieceOffset (8 + (ti % 48));
                ti /= 48;
            }
            else    // FIXFIXFIX - add support for Black pawn(s)
            {
                offset[p] = CalcPieceOffset (ti % 64);
                ti /= 64;
            }
        }

        // Decode the location of the Black king.
        if (contains_pawn)
        {
            assert (ti < 32);
            offset[0] = LeftRightOffsets[ti];
        }
        else
        {
            assert (ti < 10);
            offset[0] = FlipSlashOffsets[ti];
        }

        assert (GetRawTableIndex(piece, offset, numPieces, contains_pawn) == original_ti);
    }

    short databaseEntrySize() const
    {
        return compressDatabaseFile ? 2 : sizeof(Move);
    }

protected:
    bool encodeAndSaveMove (FILE *dbfile, Move move, unsigned ti)
    {
        if (compressDatabaseFile)
        {
            unsigned char record[2] = {0,0};

            if (move.dest == 0)
            {
                // null move
            }
            else
            {
                int msource, mdest, p;
                SQUARE prom = move.actualOffsets (true, msource, mdest);

                decodeForTableIndex (ti);

                bool found = false;
                for (p=1; p < numPieces; ++p)
                {
                    if (offset[p] == msource)
                    {
                        found = true;
                        break;
                    }
                }

                if (found)
                {
                    if (piece[p] == WPAWN)
                    {
                        int y = YPART(msource);
                        if (y == 8)
                        {
                            // pawn is being promoted
                            assert ((prom == WQUEEN) || (prom == WROOK));   // any other choice precludes using a single bit for data compression.
                            record[0] = (prom == WQUEEN) ? 1 : 0;
                        }
                        else
                        {
                            if (mdest == msource + (2*NORTH))
                            {
                                assert (y == 3);    // pawn is being moved 2 squares
                                record[0] = 1;
                            }
                            else
                            {
                                assert (mdest == msource + NORTH);
                                record[0] = 0;
                            }
                        }
                    }
                    else
                    {
                        assert (prom == EMPTY);
                        record[0] = CalcPieceIndex (mdest);
                    }
                    assert ((p >= 1) && (p <= 4));             // In order to compress, (p-1) must fit in 2 bits: 00, 01, 10, 11.
                    record[0] |= ((p-1) << 6);

                    assert (move.score >= WON_FOR_WHITE);   // otherwise move encoder will mess up!
                    int mate_in_plies = (WHITE_WINS - move.score) / WIN_DELAY_PENALTY;
                    assert (mate_in_plies >= 1);
                    assert (mate_in_plies <= 255);
                    record[1] = (unsigned char) mate_in_plies;

                    // sanity check that we will be able to re-decode the move later...
                    Move TestMove;
                    if (expandMove (record, TestMove))
                    {
                        assert (TestMove == move);
                    }
                    else
                    {
                        assert (false);     // ack!
                    }
                }
                else
                {
                    assert (false);     // how can we be moving a White piece we don't know about?
                    return false;
                }
            }

            return 2 == fwrite (record, 1, 2, dbfile);
        }
        else
        {
            // No database compression!
            return sizeof(Move) == fwrite (&move, 1, sizeof(Move), dbfile);
        }
    }

    unsigned getTableIndexForSymmetry (int sym) const
    {
        int         p, k;
        int         image [MAX_PIECE_SET];

        for (p=0; p < numPieces; ++p)
        {
            image[p] = Symmetry (sym, offset[p]);
        }

        // Because of interchangeable piece pruning in setPieceOffset,
        // we also need to compensate here for interchangeable pieces.
        // The interchangeable pieces are guaranteed to be in offset
        // order in the minimal symmetry position.
        // The earlier the piece is in the image/offset arrays, the
        // more weight they add to the table index.  Therefore,
        // we will swap interchangeable pieces so that the lowest
        // offsets occur first.  When the minimal index is found,
        // it will be the correct one.
        // Note that image[0] is where the Black king is, and image[1]
        // is where the White king is, so we only need to search
        // 2..(numPieces-1) for interchangeables.

        for (p=2; p+1 < numPieces; ++p)         // p+1 because we want there to always be at least one more piece after p
        {
            int min = p;    // index where minimum offset was found
            for (k=p+1; k < numPieces; ++k)
            {
                if (piece[k] == piece[p])
                {
                    // Found an interchangeable piece... see if it is out of official order...
                    if (image[k] < image[min])
                    {
                        min = k;
                    }
                }
            }
            if (min > p)
            {
                // We have found an interchangeable piece.
                // Swap the smallest remaining offset to this position.
                int swap   = image[p];
                image[p]   = image[min];
                image[min] = swap;
            }
        }

        return GetRawTableIndex (piece, image, numPieces, contains_pawn);
    }

    int adjustOffset (int ofs) const
    {
        return winner_is_white ? ofs : (143 - ofs);
    }

    SQUARE getAdjustedPiece (const SQUARE *bptr, int ofs) const
    {
        return AdjustPiece (bptr[adjustOffset(ofs)], winner_is_white);
    }

    static unsigned GetRawTableIndex (const SQUARE piece[], const int offset[], int numPieces, bool contains_pawn)
    {
        unsigned ti = 0;
        for (int p=0; p < numPieces; ++p)
        {
            unsigned pieceIndex = CalcPieceIndex (offset[p]);
            if (p == 0)
            {
                // Pack down the database size even smaller, by
                // taking advantage of the finite set of squares
                // the Black king is allowed to be when the minimal score is found.
                if (contains_pawn)
                {
                    pieceIndex = LeftRightTable[pieceIndex];
                    if (pieceIndex > 31)
                    {
                        return 0x7fffffff;  // short cut: we already know this is not the correct table index!
                    }
                }
                else
                {
                    pieceIndex = FlipSlashTable[pieceIndex];
                    if (pieceIndex > 9)
                    {
                        return 0x7fffffff;  // short cut: we already know this is not the correct table index!
                    }
                }
            }
            if (piece[p] & (WP_MASK | BP_MASK))
            {
                assert ((pieceIndex >= 8) && (pieceIndex < 7*8));     // otherwise pawn is in an invalid location!
                ti = (48*ti) + (pieceIndex-8);
            }
            else
            {
                ti = (64*ti) + pieceIndex;
            }
        }

        return ti;
    }

    void init()
    {
        winner_is_white = true;
        initFileName();

        contains_pawn = false;

        int numBlackPieces = 0;
        int numWhitePawns  = 0;

        compressDatabaseFile = true;        // assume we can compress unless we find a reason not to

        for (int i=0; i < numPieces; ++i)
        {
            offset[i] = 0;
            if (piece[i] & (WP_MASK | BP_MASK))
            {
                contains_pawn = true;
                if (piece[i] & WP_MASK)
                {
                    ++numWhitePawns;
                }
            }

            if (piece[i] & BLACK_MASK)
            {
                ++numBlackPieces;
            }

            if (piece[i] & WHITE_MASK)
            {
                if ((i < 1) || (i > 4))         // compression requires all white piece indexes to be in the range 1..4, so (i-1) fits in 2 bits.
                {
                    compressDatabaseFile = false;
                }
            }
        }

        table_size = initTableSize();

        // We cannot compress if there is at least one White pawn and at least one Black non-king piece for it to capture.
        // This is because compression requires White pawns to move forward, and to promote only to rook or queen.
        if (numWhitePawns > 0 && numBlackPieces > 1)        // exclude Black King from black pieces: white pawn will never capture it!
        {
            compressDatabaseFile = false;
        }
    }

    void initFileName()
    {
        char a, b;
        assert (numPieces > 2);
        assert (numPieces <= MAX_PIECE_SET);

        int k = 0;

        for (int i=2; i < numPieces; ++i)
        {
            switch (piece[i])
            {
            case WPAWN:     a='w'; b='p';   break;
            case WKNIGHT:   a='w'; b='n';   break;
            case WBISHOP:   a='w'; b='b';   break;
            case WROOK:     a='w'; b='r';   break;
            case WQUEEN:    a='w'; b='q';   break;

            case BPAWN:     a='b'; b='p';   break;
            case BKNIGHT:   a='b'; b='n';   break;
            case BBISHOP:   a='b'; b='b';   break;
            case BROOK:     a='b'; b='r';   break;
            case BQUEEN:    a='b'; b='q';   break;

            default:        a='*'; b='*';   assert(false);  break;      // even without assert, invalid filename chars will cause pukage
            }

            filename[k++] = a;
            filename[k++] = b;
            assert (k < MAX_DATABASE_FILENAME+4);
        }

        strcpy (filename+k, ".egm");
    }

    unsigned initTableSize() const
    {
        bool found_pawn = false;

        unsigned size = 64;     // for piece[1], the WKING

        for (int p=2; p < numPieces; ++p)
        {
            if (piece[p] & (WP_MASK | BP_MASK))
            {
                size *= 48;
                found_pawn = true;
            }
            else
            {
                size *= 64;
            }
        }

        // Now we know the range of possible places the BKING at piece[0] can be.
        if (found_pawn)
        {
            // Only left/right symmetry allowed, so the king can be in 32 different squares...
            size *= 32;
        }
        else
        {
            // The BKING can only be in 10/64 squares...
            size *= 10;
        }

        return size;
    }

private:
    int         numPieces;
    SQUARE      piece   [MAX_PIECE_SET];
    int         offset  [MAX_PIECE_SET];
    char        filename [MAX_DATABASE_FILENAME];
    bool        contains_pawn;
    unsigned    table_size;
    bool        winner_is_white;
    bool        compressDatabaseFile;
};

//---------------------------------------------------------------------------------------------------------
//   The WorkSet[] array defines the ordered list of endgame situations we
//   will generate *.egm database files for.
//   The order is important due to feedback in the generator algorithm:
//   If a pawn promotion or a capture happens while generating the
//   database for WorkSet[n], this may cause WorkSet[0..(n-1)] to be consulted.
//   WorkSet[] is also used to recognize and find responses for endgames during
//   normal game play.

static const tPieceSet WorkSet[] =
{
    tPieceSet(WQUEEN),
    tPieceSet(WROOK),
    tPieceSet(WPAWN),
    tPieceSet(WQUEEN,BROOK),
    tPieceSet(WQUEEN,BBISHOP),
    tPieceSet(WBISHOP,WBISHOP),
    tPieceSet(WBISHOP,WKNIGHT),
};

const int WorkSetSize = sizeof(WorkSet) / sizeof(WorkSet[0]);

//---------------------------------------------------------------------------------------------------------


bool ReadAndValidatePrefix (FILE *dbfile, tDatabasePrefix &prefix)
{
    bool valid = false;

    if (1 == fread (&prefix, sizeof(prefix), 1, dbfile))
    {
        if (0 == memcmp (prefix.signature, "egdb", 4))
        {
            if (prefix.prefixSize == sizeof(prefix))
            {
                if ((prefix.entrySize == sizeof(Move)) || (prefix.entrySize == 2))      // allow packed or unpacked moves
                {
                    if (prefix.numTableEntries > 0)
                    {
                        valid = true;
                    }
                }
            }
        }
    }

    return valid;
}


bool IsDatabaseComplete (const char *filename)
{
    bool complete = false;
    FILE *dbfile = fopen (filename, "rb");
    if (dbfile)
    {
        tDatabasePrefix     prefix;
        complete = ReadAndValidatePrefix (dbfile, prefix);
        fclose (dbfile);
    }

    return complete;
}


bool SaveDatabase (FILE *dbfile, const Move *table, unsigned tableSize, tPieceSet &set)
{
    tDatabasePrefix prefix;
    memset (&prefix, 0, sizeof(prefix));
    memcpy (prefix.signature, "egdb", 4);
    prefix.prefixSize = sizeof(prefix);
    prefix.entrySize  = set.databaseEntrySize();      // used to be sizeof(Move), but now we always compress the output into 2-byte records

    // Scan through and find the last nonzero move...
    assert (sizeof(Move) == sizeof(unsigned));
    const unsigned *hack = (const unsigned *) table;
    for (unsigned i=0; i < tableSize; ++i)
    {
        if (hack[i] != 0)
        {
            prefix.numTableEntries = i + 1;
        }
    }

    fprintf (
        dblog,
        "SaveDatabase:  tableSize=%u, numTableEntries=%u, ratio=%0.4lf\n",
        tableSize,
        prefix.numTableEntries,
        (double)prefix.numTableEntries / (double)tableSize
    );

    bool saved = false;

    if (1 == fwrite (&prefix, sizeof(prefix), 1, dbfile))
    {
        saved = set.encodeDatabase (dbfile, prefix.numTableEntries, table);
    }

    return saved;
}



bool EGDB_IsLegal ( ChessBoard &board )
{
    board.Update();     // This is absolutely vital to update internal stuff inside the ChessBoard, e.g. inventory, check flags, etc.
    if ( !board.WhiteToMove() )
    {
        ChessFatal ( "Not White's turn to move in EGDB_IsLegal()" );
        return false;  // this should never happen
    }

    unsigned wk = board.GetWhiteKingOffset();
    if ( board.GetSquareContents(wk) != WKING )
    {
        ChessFatal ( "White king missing/misplaced in EGDB_IsLegal()" );
        return false;
    }

    unsigned wkx = XPART(wk);
    unsigned wky = YPART(wk);

    unsigned bk = board.GetBlackKingOffset();
    if ( board.GetSquareContents(bk) != BKING )
    {
        ChessFatal ( "Black king missing/misplaced in EGDB_IsLegal()" );
        return false;
    }
    unsigned bkx = XPART(bk);
    unsigned bky = YPART(bk);

    int dx = int(bkx) - int(wkx);
    if ( dx < 0 )  dx = -dx;

    int dy = int(bky) - int(wky);
    if ( dy < 0 )  dy = -dy;

    if ( dx<2 && dy<2 )
    {
        assert (false); // kings are touching: this should never happen because we should have already filtered this out!
        return false;
    }

    if ( board.IsAttackedByWhite(bk) )
    {
        return false;  // it's White's turn, so Black cannot be in check
    }

    return true;
}



/*
         +-----------------------------------------------------------+
         |132  133  134  135  136  137  138  139  140  141  142  143 |
         |                                                           |
         |120  121  122  123  124  125  126  127  128  129  130  131 |
         |         +---------------------------------------+         |
     [8] |108  109 |110  111  112  113  114  115  116  117 |118  119 |  y=9
         |         |                                       |         |
     [7] | 96   97 | 98   99  100  101  102  103  104  105 |106  107 |  y=8
         |         |                                       |         |
     [6] | 84   85 | 86   87   88   89   90   91   92   93 | 94   95 |  y=7
         |         |                                       |         |
     [5] | 72   73 | 74   75   76   77   78   79   80   81 | 82   83 |  y=6
         |         |                                       |         |
     [4] | 60   61 | 62   63   64   65   66   67   68   69 | 70   71 |  y=5
         |         |                                       |         |
     [3] | 48   49 | 50   51   52   53   54   55   56   57 | 58   59 |  y=4
         |         |                                       |         |
     [2] | 36   37 | 38   39   40   41   42   43   44   45 | 46   47 |  y=3
         |         |                                       |         |
     [1] | 24   25 | 26   27   28   29   30   31   32   33 | 34   35 |  y=2
         |         +---------------------------------------+         |
         | 12   13   14   15   16   17   18   19   20   21   22   23 |
         |                                                           |
         |  0    1    2    3    4    5    6    7    8    9   10   11 |
         +-----------------------------------------------------------+
                     [a]  [b]  [c]  [d]  [e]  [f]  [g]  [h]
                     x=2  x=3  x=4  x=5  x=6  x=7  x=8  x=9
*/


inline SCORE AdjustScoreForPly (SCORE score)
{
    // Adjust the score for a one-ply delay in forcing a win or postponing a loss...

    if (score <= WON_FOR_BLACK)
    {
        return score + WIN_DELAY_PENALTY;
    }
    else if (score >= WON_FOR_WHITE)
    {
        return score - WIN_DELAY_PENALTY;
    }
    else
    {
        return score;
    }
}


int NumCompletedWorkSets = 0;
int NumConsults = 0;            // how many times did we have to open/seek/close a database file?
int NumWinsFound = 0;
INT32 LastDisplayTime = ChessTime();

enum ConsultMode
{
    CONSULT_MODE_SEEK,          // conserve memory at the expense of speed: open/seek/close file each time
    CONSULT_MODE_MEMORY,        // load entire table into memory on first access and re-use it each time afterward.
};

bool ConsultDatabase (tPieceSet set, ChessBoard &board, Move &move, ConsultMode mode);

SCORE EGDB_FeedbackEval (ChessBoard &board)
{
    assert (board.WhiteToMove());
    if (board.WhiteCanMove())
    {
        // See if this is one of the circumstances we know about...
        // Iterate through the well-known set of endgame databases and see if any of them match.
        // Only if we find a match do we try to find the egm file...

        Move move;
        for (int i=0; i < NumCompletedWorkSets; ++i)
        {
            if (WorkSet[i].isExactMatch (board))
            {
                ++NumConsults;      // track these expensive calls (if too high, may rework so we keep prior tables in memory)
                if (ConsultDatabase (WorkSet[i], board, move, CONSULT_MODE_MEMORY))
                {
                    return move.score;
                }
            }
        }

        return 0;       // assume anything we don't know about is inherently a draw
    }
    else
    {
        if (board.WhiteInCheck())
        {
            return BLACK_WINS;      // Black has checkmated White!  (could happen in WKING+WQUEEN vs BKING+BROOK)
        }
        else
        {
            return 0;               // stalemate
        }
    }
}

SCORE EGDB_FeedbackSearch (ChessBoard &board)
{
    SCORE       bestscore;
    MoveList    ml;
    UnmoveInfo  unmove;

    assert (board.BlackToMove());
    board.GenBlackMoves (ml);

    if (ml.num > 0)
    {
        // We cannot consult the databases immediately, because they all assume White has the move.
        // Therefore we need to generate each legal move, then search the database.

        bestscore = POSINF;
        for (int i=0; i < ml.num; ++i)
        {
            board.MakeMove (ml.m[i], unmove);
            SCORE score = AdjustScoreForPly (EGDB_FeedbackEval (board));
            if (score < bestscore)
            {
                bestscore = score;
            }
            board.UnmakeMove (ml.m[i], unmove);
        }
    }
    else
    {
        // The game is over... is it checkmate or stalemate?
        if (board.BlackInCheck())
        {
            bestscore = WHITE_WINS;     // black is checkmated
        }
        else
        {
            bestscore = 0;              // black is stalemated
        }
    }

    return bestscore;
}


SCORE EGDB_BlackSearch (
    ChessBoard      &board,
    ChessUI         &ui,
    tPieceSet       &set,
    Move            *whiteTable,
    Move            *blackTable,
    int              depth,
    SCORE            required_score
);


SCORE EGDB_WhiteSearch (
    ChessBoard      &board,
    ChessUI         &ui,
    tPieceSet       &set,
    Move            *whiteTable,
    Move            *blackTable,
    int              depth,
    SCORE            required_score )
{
    SCORE   bestscore = 0;

    assert (depth==0 || depth==2);
    assert (board.WhiteToMove());

    int     best_sym;
    unsigned ti = set.getTableIndex (best_sym);
    if (whiteTable[ti].source != 0)
    {
        // We have already calculated the correct score for this position.
        bestscore = whiteTable[ti].score;
    }
    else
    {
        if (depth == 0)     // stop recursion at 2 plies deep
        {
            MoveList    ml;
            board.GenMoves (ml);
            if (ml.num == 0)
            {
                if (board.WhiteInCheck())
                {
                    // It used to be that White could never get checkmated,
                    // but I am experimenting with databases for WKING+WQUEEN vs BKING+BROOK.
                    // With a black rook on the board, many positions will be checkmates for White.
                    bestscore = BLACK_WINS;     // Black has White in checkmate!
                }
                else
                {
                    bestscore = 0;              // stalemate
                }
            }
            else
            {
                // The game is NOT over...
                int         moved_piece_index;
                int         original_piece_offset;
                UnmoveInfo  unmove;
                Move        bestmove;

                bestmove.score  = bestscore = NEGINF;
                bestmove.source = 0;
                bestmove.dest   = 0;

                for (int i=0; i < ml.num; ++i)
                {
                    Move move = ml.m[i];
                    SQUARE prom = set.movePiece (board, move, moved_piece_index, original_piece_offset);
                    board.MakeMove (move, unmove);

                    if (prom == EMPTY && unmove.capture == EMPTY)
                    {
                        // Material is unchanged, so the current piece set and table indexes are still valid...
                        move.score = EGDB_BlackSearch (board, ui, set, whiteTable, blackTable, 1+depth, required_score);
                    }
                    else
                    {
                        // The material on the board has changed, so we need to consult a prior endgame database...
                        move.score = EGDB_FeedbackSearch (board);
                    }
                    move.score = AdjustScoreForPly (move.score);

                    board.UnmakeMove (move, unmove);
                    set.unmovePiece (moved_piece_index, original_piece_offset);

                    if (move.score > bestscore)
                    {
                        bestmove  = move;
                        bestscore = move.score;
                        if (bestscore - WIN_POSTPONEMENT(depth) >= required_score)
                        {
                            whiteTable[ti] = RotateMove (bestmove, true, best_sym);
                            ++NumWinsFound;
                            break;  // we found a new optimal forced win!
                        }
                    }
                }
            }
        }
    }

    return bestscore;
}


SCORE EGDB_BlackSearch (
    ChessBoard      &board,
    ChessUI         &ui,
    tPieceSet       &set,
    Move            *whiteTable,
    Move            *blackTable,
    int              depth,
    SCORE            required_score )
{
    assert (depth==1);
    assert (board.BlackToMove());

    SCORE   bestscore = 0;
    int     best_sym;
    unsigned ti = set.getTableIndex (best_sym);
    if (blackTable[ti].source != 0)
    {
        // We have already calculated the correct score for this position.
        bestscore = blackTable[ti].score;
    }
    else
    {
        MoveList    ml;
        board.GenMoves (ml);
        if (ml.num == 0)
        {
            // The game has ended...
            if (board.BlackInCheck())
            {
                bestscore = WHITE_WINS;
            }
            else
            {
                bestscore = 0;
            }

            // Mark this position as having a known correct score, so we don't have to figure it out again...
            blackTable[ti].source = 1;      // invalid but nonzero source indicates non-move but valid score
            blackTable[ti].score  = bestscore;
        }
        else
        {
            // The game is NOT over...
            int         moved_piece_index;
            int         original_piece_offset;
            UnmoveInfo  unmove;
            Move        bestmove;

            bestmove.score  = bestscore = POSINF;
            bestmove.source = 0;
            bestmove.dest   = 0;

            for (int i=0; i < ml.num; ++i)
            {
                Move move = ml.m[i];
                SQUARE promotion = set.movePiece (board, move, moved_piece_index, original_piece_offset);
                assert (promotion == EMPTY);    // Black should never promote anything
                board.MakeMove (move, unmove);
                if (unmove.capture == EMPTY)
                {
                    // Whether or not White captured something before calling here,
                    // we at least know that Black has not captured anything.
                    move.score = AdjustScoreForPly (EGDB_WhiteSearch (board, ui, set, whiteTable, blackTable, 1+depth, required_score));
                }
                else
                {
                    move.score = 0;     // Black just captured something, so it's a draw!  (Assumes White's remaining material is insufficient for mate.)
                }
                board.UnmakeMove (move, unmove);
                set.unmovePiece (moved_piece_index, original_piece_offset);

                assert (move.score < POSINF);
                if (move.score < bestscore)
                {
                    bestmove  = move;
                    bestscore = move.score;
                    if (bestscore - WIN_POSTPONEMENT(depth) < required_score)
                    {
                        break;  // Pruning:  we will never store this position
                    }
                }
            }

            if (bestscore - WIN_POSTPONEMENT(depth) >= required_score)
            {
                // No matter what, White optimally checkmates us in this position!
                blackTable[ti] = RotateMove (bestmove, false, best_sym);
            }
        }
    }

    return bestscore;
}



void CalcEndgameBestPath (
    ChessBoard          &board,
    BestPath            &bp,
    const Move          *whiteTable,
    const Move          *blackTable,
    tPieceSet           &set )
{
    int i, best_sym;
    Move corrected;    // move translated back from symmetry mangling

    UnmoveInfo unmove       [MAX_BESTPATH_DEPTH];
    int     piece_index     [MAX_BESTPATH_DEPTH];
    int     original_offset [MAX_BESTPATH_DEPTH];
    BYTE    promotion = EMPTY;

    bp.depth = 0;
    for (i=0; (promotion==EMPTY) && (i<MAX_BESTPATH_DEPTH); ++i)        // if we promote, 'set' is no longer valid!
    {
        unsigned ti = set.getTableIndex (best_sym);
        const Move *table = board.WhiteToMove() ? whiteTable : blackTable;
        if ( table[ti].source < OFFSET(2,2) )
        {
            break;  // end-of-game marker
        }

        corrected = RotateMove (table[ti], board.WhiteToMove()!=false, INVERSE_SYMMETRY[best_sym]);

        if ( !board.isLegal (corrected) )
        {
            ChessFatal ("bogus move in CalcEndgameBestPath");
            break;
        }

        bp.depth = 1 + i;
        bp.m[i] = corrected;
        promotion = set.movePiece (board, bp.m[i], piece_index[i], original_offset[i]);
        board.MakeMove ( bp.m[i], unmove[i] );
    }

    for (i=bp.depth-1; i >= 0; --i)
    {
        set.unmovePiece (piece_index[i], original_offset[i]);
        board.UnmakeMove ( bp.m[i], unmove[i] );
    }
}


void GeneratePass (
    ChessBoard      &board,
    ChessUI         &ui,
    tPieceSet       &set,
    Move            *whiteTable,
    Move            *blackTable,
    int              depth,
    int              required_mate_moves )
{
    if (depth < set.getNumPieces())
    {
        int x1=2, x2=9;
        int y1, y2;
        bool SlashConfinement = false;

        SQUARE piece = set.getPiece (depth);

        if (piece & (WP_MASK | BP_MASK))
        {
            y1 = 3;
            y2 = 8;
        }
        else
        {
            if (depth == 0)
            {
                if (set.containsPawn())
                {
                    // We can use only left/right symmetry, so force Black king
                    // on the left half of the board.
                    x1 = y1 = 2;
                    x2 = 5;
                    y2 = 9;
                }
                else
                {
                    // Keep Black king on or below slash diagonal in lower left quadrant.
                    x1 = y1 = 2;
                    x2 = y2 = 5;
                    SlashConfinement = true;
                }
            }
            else
            {
                y1 = 2;
                y2 = 9;
            }
        }

        for (int y = y1; y <= y2; ++y)
        {
            for (int x = x1; x <= x2; ++x)
            {
                if (!SlashConfinement || (x >= y))        // Keep Black king on or below slash diagonal in lower left quadrant.
                {
                    int ofs = OFFSET(x,y);
                    if (set.setPieceOffset (depth, ofs))
                    {
                        // getting here means all the pieces so far have distinct offsets
                        board.SetOffsetContents (piece, ofs, true);
                        GeneratePass (board, ui, set, whiteTable, blackTable, 1+depth, required_mate_moves);
                        if (depth >= 2)
                        {
                            // non-king pieces need to be removed back from the board!
                            board.SetOffsetContents (EMPTY, ofs, true);
                        }
                    }
                }
            }
        }
    }
    else
    {
        // We have placed all the pieces... time to analyze the position.
        if (EGDB_IsLegal (board))
        {
            const int blackNonKingPieces = set.getNumNonKingPiecesForSide (BLACK_MASK);
            assert (blackNonKingPieces >= 0 && blackNonKingPieces <= 1);        // otherwise assumption is invalid: forcing mate by capturing a single black piece

            int required_plies;
            if (set.containsPawn())
            {
                // worst case: promotion to rook and checkmate in 16 extra moves
                required_plies = 1 + (2 * (16+required_mate_moves));
            }
            else if (blackNonKingPieces > 0)
            {
                // worst case: White forces capture of lone Black non-king piece
                assert (blackNonKingPieces == 1);   // Black must have exactly one non-king piece for a single capture to lead to checkmate
                required_plies = 1 + (2 * (16+required_mate_moves));    // this is the value for WKING+WROOK vs BKING, but currently it would actually be WQUEEN instead of WROOK.
            }
            else
            {
                assert (blackNonKingPieces == 0);   // otherwise we are not just checkmating a lone king
                required_plies = 1 + (2 * required_mate_moves);
            }

            SCORE required_score = WHITE_WINS - WIN_POSTPONEMENT(required_plies);
            EGDB_WhiteSearch (board, ui, set, whiteTable, blackTable, 0, required_score);
            INT32 currentTime = ChessTime();
            if (currentTime - LastDisplayTime > 50)
            {
                LastDisplayTime = currentTime;
                BestPath bp;
                CalcEndgameBestPath (board, bp, whiteTable, blackTable, set);
                ui.DisplayBestPath (board, bp);
                ui.DrawBoard (board);
            }
        }
    }
}


void Generate (
    ChessBoard      &board,
    ChessUI         &ui,
    tPieceSet       &set,
    Move            *whiteTable,
    Move            *blackTable )
{
    board.Init();
    board.ClearEverythingButKings();

    int TotalWinsFound = 0;

    INT32 startTime = ChessTime();

    for (int required_mate_moves=0; ; ++required_mate_moves)
    {
        NumWinsFound = 0;
        NumConsults = 0;
        GeneratePass (board, ui, set, whiteTable, blackTable, 0, required_mate_moves);
        INT32 currTime = ChessTime();
        double elapsed = static_cast<double>(currTime - startTime) / 100.0;
        TotalWinsFound += NumWinsFound;
        fprintf (
            dblog,
            "Generate:  pass=%d, elapsed=%0.2lf sec, NumWinsFound=%d, TotalWinsFound=%d, Consults=%d\n",
            required_mate_moves, elapsed, NumWinsFound, TotalWinsFound, NumConsults);

        fflush (dblog);
        if (NumWinsFound == 0)
        {
            break;
        }
    }
}


void GenerateEndgameDatabase (ChessBoard &board, ChessUI &ui, tPieceSet set)
{
    const char *filename  = set.getFileName();
    if (IsDatabaseComplete (filename))
    {
        fprintf (dblog, "Database was already complete:  %s\n", filename);
    }
    else
    {
        unsigned    tableSize = set.getTableSize();

        ui.SetAdHocText (3, "Generating %s (size %u)", filename, tableSize);
        fprintf (dblog, "Starting:  %s (size %u)\n", filename, tableSize);
        fflush (dblog);

        Move *whiteTable = new Move [tableSize];
        Move *blackTable = new Move [tableSize];
        if (whiteTable && blackTable)
        {
            memset (whiteTable, 0, sizeof(Move) * tableSize);
            memset (blackTable, 0, sizeof(Move) * tableSize);

            FILE *dbfile = fopen (filename, "wb");     // create zero-byte file and hold open until done
            if (dbfile)
            {
                //BREAKPOINT();
                Generate (board, ui, set, whiteTable, blackTable);
                bool goodsave = SaveDatabase (dbfile, whiteTable, tableSize, set);
                fclose (dbfile);
                dbfile = NULL;

                if (!goodsave)
                {
                    remove (filename);  // prevent anyone from accidentally using the file
                }
            }
        }

        delete[] whiteTable;
        delete[] blackTable;
    }
    fflush (dblog);
}


//-----------------------------------------------------------------------------------------------------

void AnalyzeEndgameDatabase (const char *filename)
{
    unsigned i;

    fprintf (dblog, "AnalyzeEndgameDatabase:  %s\n", filename);

    FILE *dbfile = fopen (filename, "rb");
    if (dbfile)
    {
        tDatabasePrefix prefix;
        if (ReadAndValidatePrefix (dbfile, prefix))
        {
            if (prefix.entrySize == 2)
            {
                unsigned char record[2];
                unsigned    MoveHistogram  [0x100] = {0};
                unsigned    ScoreHistogram [0x100] = {0};
                unsigned   *RecordHistogram = new unsigned [0x10000];

                memset (RecordHistogram, 0, 0x10000*sizeof(RecordHistogram[0]));

                bool success = true;
                for (i=0; i < prefix.numTableEntries; ++i)
                {
                    if (1 != fread (record, 2, 1, dbfile))
                    {
                        fprintf (dblog, "AnalyzeEndgameDatabase:  Error reading from position %d\n", i);
                        success = false;
                        break;
                    }
                    else
                    {
                        ++MoveHistogram  [record[0]];
                        ++ScoreHistogram [record[1]];

                        // Interpret the bytes in the record as a
                        // little-endian 16-bit integer.
                        unsigned x = (record[1] << 8) | record[0];
                        ++RecordHistogram[x];
                    }
                }
                if (success)
                {
                    unsigned NumDistinctMoves   = 0;
                    unsigned NumDistinctScores  = 0;
                    unsigned NumDistinctRecords = 0;
                    unsigned MaxMatePlies       = 0;
                    for (i=0; i<0x100; ++i)
                    {
                        //fprintf (dblog, "AnalyzeEndgameDatabase:  i=0x%02x  %9lu %9lu\n", i, MoveHistogram[i], ScoreHistogram[i]);
                        if (MoveHistogram[i] > 0)
                        {
                            ++NumDistinctMoves;
                        }
                        if (ScoreHistogram[i] > 0)
                        {
                            ++NumDistinctScores;
                            MaxMatePlies = i;
                        }
                    }
                    for (i=0; i<0x10000; ++i)
                    {
                        if (RecordHistogram[i] > 0)
                        {
                            ++NumDistinctRecords;
                        }
                    }
                    fprintf (
                        dblog,
                        "AnalyzeEndgameDatabase:  ---------  distinct moves: %d, distinct scores: %d, distinct records: %d, max mate plies: %d\n\n",
                        NumDistinctMoves,
                        NumDistinctScores,
                        NumDistinctRecords,
                        MaxMatePlies
                    );
                }

                delete[] RecordHistogram;
            }
            else
            {
                fprintf (dblog, "AnalyzeEndgameDatabase:  Ignoring because entrySize=%d\n", prefix.entrySize);
            }
        }
        else
        {
            fprintf (dblog, "AnalyzeEndgameDatabase:  Bad file prefix!\n");
        }

        fclose (dbfile);
        dbfile = NULL;
    }
    else
    {
        fprintf (dblog, "AnalyzeEndgameDatabase:  Cannot open file!\n");
    }
}

//-----------------------------------------------------------------------------------------------------

void GenerateEndgameDatabases (ChessBoard &board, ChessUI &ui)
{
    dblog = fopen ("dblog.txt", "wt");
    if (dblog)
    {
#ifdef _WIN32
        if (SetPriorityClass (GetCurrentProcess(), IDLE_PRIORITY_CLASS))
        {
            fprintf (dblog, "Set process priority to low.\n");
        }
        else
        {
            fprintf (dblog, "SetPriorityClass error %08x.\n", GetLastError());
        }
        fflush (dblog);
#endif

        for (int i=0; i < WorkSetSize; ++i)
        {
            NumCompletedWorkSets = i;
            GenerateEndgameDatabase (board, ui, WorkSet[i]);
            AnalyzeEndgameDatabase (WorkSet[i].getFileName());
        }

        fprintf (dblog, "Finished!\n");
        fclose (dblog);
        dblog = NULL;
        ui.SetAdHocText (3, "Finished generating databases.");
    }
    else
    {
        ChessFatal ("Could not open dblog.txt");
    }
}


//-----------------------------------------------------------------------------------------------------

struct tDatabaseMemoryImage
{
    tDatabasePrefix  prefix;
    tPieceSet        pieceSet;
    unsigned char   *buffer;

    tDatabaseMemoryImage()
        : pieceSet()
        , buffer(NULL)
    {
        memset(&prefix, 0, sizeof(prefix));
    }

    ~tDatabaseMemoryImage()
    {
        Erase();
    }

    void Erase()
    {
        delete[] buffer;
        buffer = NULL;
    }
};


static tDatabaseMemoryImage DatabaseMemoryImage[WorkSetSize];


bool ConsultDatabase (tPieceSet set, ChessBoard &board, Move &move, ConsultMode mode)
{
    // Tricky bit:  If it is Black's turn to move, we need to toggle all the pieces,
    // and if pawns are involved, we need to rotate/unrotate the board and moves.

    bool found = false;

    assert (set.isValid());

    bool white_move = board.WhiteToMove();

    set.setWinnerSide (white_move);
    if (set.findPieces (board))
    {
        int sym;
        int ti = set.getTableIndex(sym);

        // Tricky:  readjust 'set' so that it has canonical piece offsets...
        // We have to do this so that move decoder works properly.
        set.decodeForTableIndex(ti);

        unsigned char entryData[sizeof(Move)];
        memset(entryData, 0, sizeof(entryData));

        const char *filename = set.getFileName();
        int entrySize = 0;
        bool loaded = false;

        // See if we have already loaded this database file.
        // Note that we re-use in-memory data even if told to seek,
        // to take advantage of anyone else who already loaded it in memory mode.
        for (int di=0; di < WorkSetSize; ++di)
        {
            const tDatabaseMemoryImage& d = DatabaseMemoryImage[di];
            if (d.buffer != NULL)
            {
                if (0 == strcmp(d.pieceSet.getFileName(), filename))
                {
                    entrySize = d.prefix.entrySize;
                    memcpy(entryData, &d.buffer[ti * entrySize], entrySize);
                    loaded = true;
                    break;
                }
            }
        }

        if (!loaded)
        {
            FILE *dbfile = fopen (filename, "rb");
            if (dbfile)
            {
                tDatabasePrefix prefix;
                if (ReadAndValidatePrefix (dbfile, prefix))
                {
                    entrySize = prefix.entrySize;
                    if (mode == CONSULT_MODE_MEMORY)
                    {
                        // Slurp entire table into memory.
                        // First find the slot where it goes (first unused, determined by buffer==NULL).
                        for (int di=0; di < WorkSetSize; ++di)
                        {
                            tDatabaseMemoryImage& d = DatabaseMemoryImage[di];
                            if (d.buffer == NULL)
                            {
                                size_t bufferLength =
                                    static_cast<size_t>(prefix.entrySize) *
                                    static_cast<size_t>(prefix.numTableEntries);

                                d.prefix = prefix;
                                d.pieceSet = set;
                                d.buffer = new unsigned char[bufferLength];

                                if (1 == fread(d.buffer, bufferLength, 1, dbfile))
                                {
                                    memcpy(entryData, &d.buffer[ti * entrySize], entrySize);
                                    loaded = true;
                                    break;
                                }
                            }
                        }
                    }
                    else
                    {
                        // Seek to the correct file offset for this move...
                        int FileOffset = sizeof(prefix) + (ti * entrySize);
                        if (0 == fseek (dbfile, FileOffset, SEEK_SET))
                        {
                            if (1 == fread(entryData, entrySize, 1, dbfile))
                            {
                                loaded = true;
                            }
                        }
                    }
                }
                fclose (dbfile);
            }
        }

        if (loaded)
        {
            Move rawMove;
            if (set.decodeMove (entrySize, entryData, rawMove))        // returns false if move is null
            {
                // Adjust for symmetry translation.
                move = RotateMove(rawMove, white_move, INVERSE_SYMMETRY[sym]);

                if (!white_move)
                {
                    // Adjust for black.
                    move = RotateMove (move, white_move, SYMMETRY_ROTATE_180);
                    move.score = -move.score;
                }

                // When we get here, the move MUST be legal (or we should not have put it in the database).
                // Let's just make sure before causing really bad stuff to happen...
                MoveList ml;
                board.GenMoves(ml);
                found = ml.IsLegal(move);
                assert(false);     // the database contains a non-null, illegal move!  (look in RawMove)
            }
        }
    }

    return found;
}

//-----------------------------------------------------------------------------------------------------


bool ComputerChessPlayer::FindEndgameDatabaseMove (ChessBoard &board, Move &move)
{
    // See if this is one of the circumstances we know about...
    // Iterate through the well-known set of endgame databases and see if any of them match.
    // Only if we find a match do we try to find the egm file...
    for (int i=0; i < WorkSetSize; ++i)
    {
        if (WorkSet[i].isExactMatch (board))
        {
            return ConsultDatabase (WorkSet[i], board, move, CONSULT_MODE_SEEK);
        }
    }

    return false;
}


//-----------------------------------------------------------------------------------------------------


/*
    $Log: egdbase.cpp,v $
    Revision 1.24  2008/11/14 23:25:56  Don.Cross
    I did some experimenting with 5-piece endgame databases, but it turns out they would require gigabytes of memory.
    I also experimented with endgames where pawns could capture enemy pieces, and realized that the compressed
    data structures I was using would no longer work.  Therefore, I generalized the code to know when compression was
    possible, and when it would have to revert back to using the old uncompressed move data structures on disk.
    Amusingly, the code was still there for automatically detecting the old format and reading it!
    Added WQUEEN+WKING vs BBISHOP+BKING, because without it Chenard has no idea how to win.
    Removed spurious assert in endgame search that was left over from before I had feedback in the search algorithm.

    Revision 1.23  2008/11/12 23:06:07  Don.Cross
    Massive refactoring of the concept of an 'editedFlag' in ChessBoard.
    Now instead of just saying the board was edited, we will always remember the most recent position
    after which there are no more pseudo-moves representing edit actions.
    This allows us to save/load PGN files based on edited positions.
    More work needs to be done:
    - Need to allow move undo/redo to work in edited positions.
    - Because the game history is stored now inside the ChessBoard, it seems silly to have an external copy also!
    - Move numbers should be based on FEN when PGN is saved.
    In general, this is a scary change, so it needs more careful testing before being published.

    Revision 1.22  2008/11/12 17:44:44  Don.Cross
    Replaced ComputerChessPlayer::WhiteDbEndgame and ComputerChessPlayer::BlackDbEndgame
    with a single method ComputerChessPlayer::FindEndgameDatabaseMove.
    Replaced complicated ad hoc logic for captures and promotions changing the situation in endgame searches
    with generic solution that uses feedback from prior endgame database calculations.
    This allows us to accurately calculate optimal moves and accurately report how long it will take to
    checkmate the opponent.  The extra file I/O required in the search does not appear to slow things down too badly.

    Revision 1.21  2008/11/11 22:56:13  Don.Cross
    Added support for endgame databases where Black has a single non-king piece.
    Specifically, implemented generator for WKING+WQUEEN vs BKING+BROOK.
    Dramatically simplified the code for checking for a case that we might have an endgame match for.

    Revision 1.20  2008/10/27 21:47:07  Don.Cross
    Made fixes to help people build text-only version of Chenard in Linux:
    1. The function 'stricmp' is called 'strcasecmp' in Linux Land.
    2. Found a fossil 'FALSE' that should have been 'false'.
    3. Conditionally remove some relatively unimportant conio.h code that does not translate nicely in the Linux/Unix keyboard model.
    4. Added a script for building using g++ in Linux.

    Revision 1.19  2006/01/18 19:58:11  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.18  2006/01/10 22:09:12  dcross
    Added logging of maximum mate plies for each endgame database.

    Revision 1.17  2006/01/09 17:53:31  dcross
    Added some analysis of the databases after generating them, just to see how much compression might be possible.
    Here are the results:  (edited for brevity)

    AnalyzeEndgameDatabase:  wq.egm   ---------  distinct moves: 120, distinct scores: 11, distinct records:  560
    AnalyzeEndgameDatabase:  wr.egm   ---------  distinct moves: 126, distinct scores: 17, distinct records:  908
    AnalyzeEndgameDatabase:  wp.egm   ---------  distinct moves:  59, distinct scores: 23, distinct records:  464
    AnalyzeEndgameDatabase:  wbwb.egm ---------  distinct moves: 190, distinct scores: 20, distinct records: 2140
    AnalyzeEndgameDatabase:  wbwn.egm ---------  distinct moves: 190, distinct scores: 34, distinct records: 4020

    Compression may be possible, but not without sacrificing byte-aligned records, so I'm losing interest in this idea.

    Revision 1.16  2006/01/09 15:38:07  dcross
    1. Minor code cleanup.
    2. Oops!  Found out I had forgotten to set low CPU priority... did that now.
    3. Made GenerateEndgameDatabases use an array of tPieceSet to define its work set.

    Revision 1.15  2006/01/09 01:33:22  dcross
    Made wbwb.egm generation faster by preventing 2 bishops from being on the same color square.

    Revision 1.14  2006/01/08 23:52:29  dcross
    Did a huge amount of work just to cut database sizes in half.
    Now the table stored on disk is an array of 2-byte compressed records
    instead of 4-byte Move structures.  Needs a lot more testing before
    I feel confident though.

    Revision 1.13  2006/01/08 20:16:41  dcross
    Now I am making the database much smaller by encoding the Black king
    position as efficiently as possible.  In databases that include the pawn,
    the Black king can be in any of 32 squares on the left half of the board.
    In non-pawn endgames, the Black king can be in one of 10 squares.
    Before packing:
        737,312 wp.egm
        458,752 wq.egm
    After packing:
        393,248 wp.egm
        163,840 wq.egm

    Revision 1.12  2006/01/08 18:50:34  dcross
    Fixed bug in ConsultDatabase:  it was never working for White, because
    it always returned false, due to putting legal move check in the wrong place!
    I had been testing for Black only, because I figured THAT was where all the bugs would be!

    Revision 1.11  2006/01/08 17:25:54  dcross
    Changes needed to get wp.egm working:
    1. Had to make RotateMove know about the elaborate, complex Move structure for pawn promotion.
    2. Had to add new function EGDB_PromotionSearch, that handles Black's ply after a promotion.
    3. Now pass required_score instead of required_mate_moves to EGDB_WhiteSearch and EGDB_BlackSearch,
       because in the promotion case we don't find immediate checkmates; we find promotions which have a worst
       case additional 16 moves until mate (for promoting to a rook).  So now GeneratePass is responsible for
       calculating the required_score and passing it down.
    4. Had to fix GeneratePass to allow the Black king to be placed anywhere on the left half of the
       board in the case of wp.egm, but still in the same 10 minimal set of squares in all other cases.

    Revision 1.10  2006/01/08 14:41:40  dcross
    1. Minor optimization in tPieceSet::setPieceOffset:  if index==1, only call kingsAreTouching().
       Once that is done, there is no need to look for overlapping kings.
    2. Fixed serious flaw in tPieceSet::getTableIndex:  the interchangeable piece pruning
       in setPieceOffset was causing symmetric positions to not be found.  Now we sort
       interchangeable pieces in increasing offset order when calculating table indexes.
       This was causing K+B+B vs K to not know it could force mate in some cases.

    Revision 1.9  2006/01/08 13:07:23  dcross
    ConsultDatabase now checks for the fetched table entry being a null move.
    Then, to be extra safe, it also verifies that the move is legal and asserts if not.

    Revision 1.8  2006/01/08 12:42:15  dcross
    Made bishop+knight search faster:  no longer search for the pieces in the board.
    Fixed silly bug in functions for recognizing known endgame databases:
    Had Bishop+Bishop and Bishop+Knight cases swapped.

    Revision 1.7  2006/01/08 02:31:17  dcross
    Added back rook, double bishop, and bishop+knight cases.

    Revision 1.6  2006/01/08 02:11:08  dcross
    Initial tests of wq.egm are very promising!

    1. I was having a fit because I needed to get a move's actual offsets without having the correct board position.
       When I looked at the code for Move::actualOffsets, I suddenly realized it only needs to know whether
       it is White's or Black's turn.  So I overloaded the function to send in either a ChessBoard & (for compatibility)
       or a boolean telling whether it is White's move.

    2. Coded generic ConsultDatabase, WhiteDbEndgame, BlackDbEndgame.
       Still needs work for pawns to have any possibility of working right.

    Revision 1.5  2006/01/07 23:30:01  dcross
    It looks like it might be working... traced through with debugger, plus got following debug output:

    Starting:  wq.egm (size 262144)
    Generate:  pass=0, NumWinsFound=306
    Generate:  pass=1, NumWinsFound=629
    Generate:  pass=2, NumWinsFound=1135
    Generate:  pass=3, NumWinsFound=2499
    Generate:  pass=4, NumWinsFound=3273
    Generate:  pass=5, NumWinsFound=4010
    Generate:  pass=6, NumWinsFound=4016
    Generate:  pass=7, NumWinsFound=1877
    Generate:  pass=8, NumWinsFound=335
    Generate:  pass=9, NumWinsFound=1
    Generate:  pass=10, NumWinsFound=0
    SaveDatabase:  tableSize=262144, numTableEntries=114680, ratio=0.4375
    Finished!

    Revision 1.4  2006/01/07 16:07:18  dcross
    Added padding to database prefix so it is now 32 bytes, making it easy to read hex dump.

    Revision 1.3  2006/01/07 16:04:07  dcross
    Got more infrastructure done:  auto-detect minimal database size, plus nuts-n-bolts like index calculation.

    Revision 1.2  2006/01/07 14:58:02  dcross
    Starting to add infrastructure.
    Defined class tPieceSet.
    Copied symmetry code from egdb.cpp.
    Documented my design ideas in comments.

    Revision 1.1  2006/01/07 13:19:54  dcross
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

*/
