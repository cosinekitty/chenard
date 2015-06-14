/*==========================================================================

     eval.cpp  -  Copyright (C) 1993-2005 by Don Cross

     Contains evaluation heuristics for chess.

==========================================================================*/

#include "chess.h"
#include "profiler.h"

#define SAFE_EVAL_PRUNE_MARGIN  (gene.v[4])
#define EVAL_PRUNE_BOTH_WAYS      1

//----------------------------------------------------------------------
// All heuristic constants below are positive, whether they are
// bonuses or penalties.  This is done so that, when looking at
// code, it is clear whether they are bonuses or penalties by
// looking at how they are used, without referring to the definition
// of the constant to see if it has a '-' in front of it!
//----------------------------------------------------------------------

// Miscellaneous -------------------------------------------------------
#define  CHECK_BONUS           (gene.v[5])
#define  TEMPO_BONUS           (gene.v[6])

// Castling and king ---------------------------------------------------
#define  KNIGHT_ATTACK_KPOS     (gene.v[7])
#define  BISHOP_ATTACK_KPOS     (gene.v[8])
#define  ROOK_ATTACK_KPOS       (gene.v[9])
#define  QUEEN_ATTACK_KPOS      (gene.v[10])

#define  KNIGHT_PROTECT_KPOS    (gene.v[11])
#define  BISHOP_PROTECT_KPOS    (gene.v[12])
#define  ROOK_PROTECT_KPOS      (gene.v[13])
#define  QUEEN_PROTECT_KPOS     (gene.v[14])


// Opening and Midgame
static const SCORE WKingPosition1 [] =
{
    0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,
    0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,

    0000,0000,    10,  12,  13, -17, -16, -18,  13,  10,  0000,0000,
    0000,0000,    -5,  -7, -10, -20, -20, -10,  -7,  -5,  0000,0000,
    0000,0000,   -30, -40, -40, -45, -45, -40, -40, -30,  0000,0000,
    0000,0000,   -50, -55, -60, -65, -65, -60, -55, -50,  0000,0000,
    0000,0000,   -55, -60, -65, -75, -75, -65, -60, -55,  0000,0000,
    0000,0000,   -70, -80, -90,-100,-100, -90, -80, -70,  0000,0000,
    0000,0000,  -250,-240,-230,-220,-220,-230,-240,-250,  0000,0000,
    0000,0000,  -350,-340,-330,-320,-320,-330,-340,-350
};


// Opening and midgame
static const SCORE BKingPosition1 [] =
{
    0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,
    0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,

    0000,0000,  -350,-340,-330,-320,-320,-330,-340,-350,  0000,0000,
    0000,0000,  -250,-240,-230,-220,-220,-230,-240,-250,  0000,0000,
    0000,0000,   -70, -80, -90,-100,-100, -90, -80, -70,  0000,0000,
    0000,0000,   -55, -60, -65, -75, -75, -65, -60, -55,  0000,0000,
    0000,0000,   -50, -55, -60, -65, -65, -60, -55, -50,  0000,0000,
    0000,0000,   -30, -40, -40, -45, -45, -40, -40, -30,  0000,0000,
    0000,0000,    -5,  -7, -10, -20, -20, -10,  -7,  -5,  0000,0000,
    0000,0000,    10,  12,  13, -17, -16, -18,  13,  10
};


// Endgame
static const SCORE WKingPosition2 [] =
{
    0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,
    0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,

    0000,0000,   -30, -20, -15, -10, -10, -15, -20, -30,  0000,0000,
    0000,0000,   -25, -10,  -5,   0,   0,  -5, -10, -25,  0000,0000,
    0000,0000,   -30,   5,  10,  15,  15,  10,   5, -30,  0000,0000,
    0000,0000,   -20,  10,  20,  20,  20,  20,  10, -20,  0000,0000,
    0000,0000,   -10,  10,  15,  20,  20,  15,  10, -10,  0000,0000,
    0000,0000,   -10,   0,   5,  15,  15,   5,   0, -10,  0000,0000,
    0000,0000,   -20, -12,   0,   0,   0,   0, -12, -20,  0000,0000,
    0000,0000,   -35, -25, -20, -15, -15, -20, -25, -35
};

// Endgame
static const SCORE BKingPosition2 [] =
{
    0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,
    0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,

    0000,0000,   -35, -25, -20, -15, -15, -20, -25, -35,  0000,0000,
    0000,0000,   -20, -12,   0,   0,   0,   0, -12, -20,  0000,0000,
    0000,0000,   -10,  10,  15,  20,  20,  15,  10, -10,  0000,0000,
    0000,0000,   -10,   0,   5,  15,  15,   5,   0, -10,  0000,0000,
    0000,0000,   -20,  10,  20,  20,  20,  20,  10, -20,  0000,0000,
    0000,0000,   -30,   5,  10,  15,  15,  10,   5, -30,  0000,0000,
    0000,0000,   -25, -10,  -5,   0,   0,  -5, -10, -25,  0000,0000,
    0000,0000,   -30, -20, -15, -10, -10, -15, -20, -30
};

#define  ROOK_TRAPPED_BY_KING  (gene.v[15])
#define  PAWN_PROTECTS_KING1   (gene.v[16])
#define  PAWN_PROTECTS_KING2   (gene.v[17])
#define  PAWN_PROTECTS_KING3   (gene.v[18])
#define  CASTLE_KNIGHT_GUARD   (gene.v[19])
#define  CASTLE_HOLE1          (gene.v[20])
#define  CASTLE_HOLE2          (gene.v[21])
#define  CASTLE_HOLE3          (gene.v[22])
#define  CASTLE_HOLE_DANGER    (gene.v[23])

#define  KING_OPPOSITION       (gene.v[24])    // used only in endgame

#define  CAN_KCASTLE_BONUS    (gene.v[25])
#define  CAN_QCASTLE_BONUS    (gene.v[26])
#define  CAN_KQCASTLE_BONUS   (gene.v[27])
#define  KCASTLE_PATH_EMPTY   (gene.v[28])
#define  QCASTLE_PATH_EMPTY   (gene.v[29])

// The following 'CTEK' values are bonuses for having pieces
// 'Close To Enemy King'.

#define CTEK_HOLE     (gene.v[30])   // goes with CTEK_PAWN... values below
#define CTEK_HOLE_Q   (gene.v[31])   // oooh...scary kids!  Queen in good mating position
#define CTEK_PAWN1    (gene.v[32])   // pawn attacking square in front of king on back row
#define CTEK_PAWN2    (gene.v[33])   // pawn attacking square on diagonal of king on back row
#define CTEK_KNIGHT   (gene.v[34])   // knight less than 4 away
#define CTEK_BISHOP   (gene.v[35])   // bishop less than 4 away
#define CTEK_ROOK     (gene.v[36])   // rook less than 3 away
#define CTEK_QUEEN3   (gene.v[37])   // queen less than 3 away
#define CTEK_QUEEN2   (gene.v[38])   // queen less than 2 away

// Knight --------------------------------------------------------------

#define KNIGHT_FORK_UNCERTAINTY   (gene.v[81])


static const SCORE KnightPosition [] =
{
    0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,
    0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,

    0000,0000,    -9,  -7,  -5,  -4,  -4,  -5,  -7,  -9,  0000,0000,
    0000,0000,    -6,   2,   1,   0,   0,   1,   2,  -6,  0000,0000,
    0000,0000,    -4,   3,   6,   8,   8,   6,   3,  -4,  0000,0000,
    0000,0000,    -4,   6,   8,  10,  10,   8,   6,  -4,  0000,0000,
    0000,0000,    -5,   2,   6,   7,   7,   6,   2,  -5,  0000,0000,
    0000,0000,    -7,   1,   5,   3,   3,   5,   1,  -7,  0000,0000,
    0000,0000,    -8,  -3,  -1,  -1,  -1,  -1,  -3,  -8,  0000,0000,
    0000,0000,   -10,  -9,  -8,  -7,  -7,  -8,  -9, -10
};

// Bishop --------------------------------------------------------------

static const SCORE BishopPosition [] =
{
    0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,
    0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,

    0000,0000,    -7,  -6,  -6,  -3,  -3,  -6,  -6,  -7,  0000,0000,
    0000,0000,    -3,   5,   0,   2,   2,   0,   5,  -3,  0000,0000,
    0000,0000,    -1,   1,   3,   0,   0,   3,   1,  -1,  0000,0000,
    0000,0000,    -1,   3,   4,   3,   3,   4,   3,  -1,  0000,0000,
    0000,0000,    -1,   4,   2,   3,   3,   2,   4,  -1,  0000,0000,
    0000,0000,    -2,   2,   3,   3,   3,   3,   2,  -2,  0000,0000,
    0000,0000,    -5,   1,   0,   0,   0,   0,   1,  -5,  0000,0000,
    0000,0000,    -7,  -5,  -4,  -3,  -3,  -4,  -5,  -7
};

#define  BISHOP_IMMOBILE        (gene.v[39])
#define  CENTER_BLOCK_BISHOP1   (gene.v[40])
#define  CENTER_BLOCK_BISHOP2   (gene.v[41])
#define  TWO_BISHOP_SYNERGY     (gene.v[42])
#define  BISHOP_PIN_K           (gene.v[43])   // bishop pins piece against enemy king
#define  BISHOP_PIN_Q           (gene.v[44])   // ditto for queen
#define  BISHOP_PIN_R           (gene.v[45])   // ditto for rook
#define  WB_PIN_MASK   (BN_MASK | BR_MASK | BQ_MASK)
#define  BB_PIN_MASK   (WN_MASK | WR_MASK | WQ_MASK)


// Pawn ----------------------------------------------------------------
#define  PAWN_FORK                (gene.v[46])
#define  PAWN_SIDE_FILE           (gene.v[47])
#define  PAWN_DOUBLED             (gene.v[48])
#define  PAWN_SPLIT               (gene.v[49])
#define  PAWN_PROTECT1            (gene.v[50])
#define  PAWN_PROTECT2            (gene.v[51])
#define  BISHOP_PROTECT_PAWN      (gene.v[52])
#define  PASSED_PAWN_PROTECT1     (gene.v[53])
#define  PASSED_PAWN_PROTECT2     (gene.v[54])
#define  PASSED_PAWN_ALONE        (gene.v[55])
#define  PASSED_PAWN_VULNERABLE   (gene.v[56])
#define  PASSED_3_FROM_PROM       (gene.v[57])
#define  PASSED_2_FROM_PROM       (gene.v[58])
#define  PASSED_1_FROM_PROM       (gene.v[59])
#define  PASSED_PIECE_BLOCK       (gene.v[60])    // passed pawn blocked by any non-pawn piece
#define  BLOCKED_2_FROM_PROM      (gene.v[61])

// Pawn center attacks.
// NOTE: This table exists in addition to other
// pawn position heuristics.  Therefore, it may
// appear that certain key ideas (e.g. pawns
// close to promotion) have been ignored, but not really.

static const SCORE PawnCenter [] =
{
    0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,
    0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,

    0000,0000,     0,   0,   0,   0,   0,   0,   0,   0,  0000,0000,
    0000,0000,     0,   0,   0,   0,   0,   0,   0,   0,  0000,0000,
    0000,0000,     0,   0,   0,   0,   0,   0,   0,   0,  0000,0000,
    0000,0000,     0,   0,   1,   3,   3,   1,   0,   0,  0000,0000,
    0000,0000,     0,   0,   4,   7,   7,   4,   0,   0,  0000,0000,
    0000,0000,     1,   0,  -1,   1,   1,  -1,   0,   1,  0000,0000,
    0000,0000,     0,   0,   1,  -1,  -1,   1,   0,   0,  0000,0000,
    0000,0000,     0,   0,   0,   0,   0,   0,   0,   0
};



#define PAWN_BALANCE 1
#if PAWN_BALANCE

    // The following table gives pawn imbalance scores.
    // This must be scaled relative to the enemy pieces which
    // confront the pawns.

    static const SCORE PawnBalance [9][9] =
    {
        //   0     1     2     3     4     5     6     7     8

        {    0,  120,  140,  150,  157,  162,  167,  170,  172},    // 0
        {    0,    0,   55,   70,   80,   90,   97,  105,  110},    // 1
        {    0,    0,    0,   10,   20,   30,   40,   50,   60},    // 2
        {    0,    0,    0,    0,    6,   15,   20,   25,   30},    // 3
        {    0,    0,    0,    0,    0,    4,   10,   12,   15},    // 4
        {    0,    0,    0,    0,    0,    0,    3,    5,   10},    // 5
        {    0,    0,    0,    0,    0,    0,    0,    2,    3},    // 6
        {    0,    0,    0,    0,    0,    0,    0,    0,    1},    // 7
        {    0,    0,    0,    0,    0,    0,    0,    0,    0},    // 8
    };

#endif // PAWN_BALANCE

// Rook ----------------------------------------------------------------
#define  ROOK_PIN_Q                (gene.v[62])   // rook pins B/N against Q
#define  ROOK_PIN_K                (gene.v[63])   
#define  ROOK_OPEN_FILE            (gene.v[64])
#define  ROOK_CAN_REACH_7TH_RANK   (gene.v[65])
#define  ROOK_ON_7TH_RANK          (gene.v[66])
#define  ROOK_CONNECT_VERT         (gene.v[67])
#define  ROOK_CONNECT_HOR          (gene.v[68])
#define  ROOK_IMMOBILE_HORIZ       (gene.v[69])
#define  ROOK_IMMOBILE             (gene.v[70])
#define  ROOK_BACKS_PASSED_PAWN1   (gene.v[71])
#define  ROOK_BACKS_PASSED_PAWN2   (gene.v[72])

//----------------------------------------------------------------------


static bool AttackBlackKingPos ( const SQUARE *inBoard )
{
    if ( *inBoard & OFFBOARD )
        return false;

    if (   (inBoard[NORTH] & BK_MASK) 
        || (inBoard[NORTHEAST] & BK_MASK)
        || (inBoard[NORTHWEST] & BK_MASK)
        || (inBoard[EAST] & BK_MASK)
        || (inBoard[WEST] & BK_MASK) )
    {
        if ( *inBoard & BLACK_MASK )
        {
            if ( (inBoard[NORTHEAST] | inBoard[NORTHWEST]) & BP_MASK )
                return false;

            return true;
        }
        else
            return true;
    }

    return false;
}


static bool AttackWhiteKingPos ( const SQUARE *inBoard )
{
    if ( *inBoard & OFFBOARD )
        return false;

    if (   (inBoard[SOUTH] & WK_MASK) 
        || (inBoard[SOUTHEAST] & WK_MASK)
        || (inBoard[SOUTHWEST] & WK_MASK)
        || (inBoard[EAST] & WK_MASK)
        || (inBoard[WEST] & WK_MASK) )
    {
        if ( *inBoard & WHITE_MASK )
        {
            if ( (inBoard[SOUTHEAST] | inBoard[SOUTHWEST]) & WP_MASK )
                return false;
        }

        return true;
    }

    return false;
}


SCORE ComputerChessPlayer::WhiteRookBonus ( const SQUARE *b, int ofs, int wk_offset )
{
    int z;
    SCORE bonus = 0;

    // See if we are on the seventh rank or the eighth rank...

    if ( ofs >= OFFSET(2,8) )
    {
        if ( ofs <= OFFSET(9,8) )
            bonus += ROOK_ON_7TH_RANK;
        else
            bonus += (ROOK_CAN_REACH_7TH_RANK + ROOK_OPEN_FILE);
    }
    else
    {
        // A file is open if none of our pawns is blocking it before 7th.
        for ( z=ofs + NORTH; b[z] == EMPTY && z < OFFSET(2,8); z += NORTH );

        bonus += (YPART(z) - 2) / 2;

        if ( z < OFFSET(2,8) )
        {
            if ( !(b[z] & WP_MASK) )
                bonus += ROOK_OPEN_FILE;
        }
        else if ( z > OFFSET(9,8) )
            bonus += ROOK_CAN_REACH_7TH_RANK;

        for ( z=ofs + SOUTH; b[z] == EMPTY; z += SOUTH );
        if ( b[z] & (WR_MASK | WQ_MASK) )
            bonus += ROOK_CONNECT_VERT;

        for ( z=ofs + WEST; b[z] == EMPTY; z += WEST );
        if ( b[z] & WR_MASK )
            bonus += ROOK_CONNECT_HOR;
    }

    if ( (b[ofs+EAST] & (WHITE_MASK | OFFBOARD)) &&
         (b[ofs+WEST] & (WHITE_MASK | OFFBOARD)) )
    {
        if ( (b[ofs+NORTH] & (WHITE_MASK | OFFBOARD)) &&
             (b[ofs+SOUTH] & (WHITE_MASK | OFFBOARD)) )
            bonus -= ROOK_IMMOBILE;
        else
            bonus -= ROOK_IMMOBILE_HORIZ;
    }

    if ( Distance ( ofs, wk_offset ) < 3 )
        bonus += CTEK_ROOK;

// NORTH -------------------------------------------------------------------

    // Look north for pins caused by rook
    for ( z=ofs + NORTH; b[z] == EMPTY; z += NORTH );
    int holdz = z;
    if ( b[z] & (BB_MASK | BN_MASK) )
    {
        for ( z += NORTH; b[z] == EMPTY; z += NORTH );
        if ( b[z] & BQ_MASK )
            bonus += ROOK_PIN_Q;
        else if ( b[z] & BK_MASK )
            bonus += ROOK_PIN_K;
    }

    // Continue NORTH for king position attack and protection

    for ( z = holdz; (b[z] & ~(WR_MASK | WQ_MASK)) == EMPTY; z += NORTH );
    if ( AttackBlackKingPos(b+z) )
        bonus += ROOK_ATTACK_KPOS;

    if ( AttackWhiteKingPos(b+z) )
        bonus += ROOK_PROTECT_KPOS;


// EAST ---------------------------------------------------------------------

    // Look east for pins caused by rook
    for ( z=ofs + EAST; b[z] == EMPTY; z += EAST );
    holdz = z;
    if ( b[z] & (BB_MASK | BN_MASK) )
    {
        for ( z += EAST; b[z] == EMPTY; z += EAST );
        if ( b[z] & BQ_MASK )
            bonus += ROOK_PIN_Q;
        else if ( b[z] & BK_MASK )
            bonus += ROOK_PIN_K;
    }

    // Continue EAST for king position attack and protection

    for ( z = holdz; (b[z] & ~(WR_MASK | WQ_MASK)) == EMPTY; z += EAST );
    if ( AttackBlackKingPos(b+z) )
        bonus += ROOK_ATTACK_KPOS;

    if ( AttackWhiteKingPos(b+z) )
        bonus += ROOK_PROTECT_KPOS;

// WEST -------------------------------------------------------------------

    // Look west for pins caused by rook
    for ( z=ofs + WEST; b[z] == EMPTY; z += WEST );
    holdz = z;
    if ( b[z] & (BB_MASK | BN_MASK) )
    {
        for ( z += WEST; b[z] == EMPTY; z += WEST );
        if ( b[z] & BQ_MASK )
            bonus += ROOK_PIN_Q;
        else if ( b[z] & BK_MASK )
            bonus += ROOK_PIN_K;
    }

    // Continue WEST for king position attack and protection

    for ( z = holdz; (b[z] & ~(WR_MASK | WQ_MASK)) == EMPTY; z += WEST );
    if ( AttackBlackKingPos(b+z) )
        bonus += ROOK_ATTACK_KPOS;

    if ( AttackWhiteKingPos(b+z) )
        bonus += ROOK_PROTECT_KPOS;

// SOUTH -----------------------------------------------------------------------

    // Look south for pins caused by rook
    for ( z=ofs + SOUTH; b[z] == EMPTY; z += SOUTH );
    holdz = z;
    if ( b[z] & (BB_MASK | BN_MASK) )
    {
        for ( z += SOUTH; b[z] == EMPTY; z += SOUTH );
        if ( b[z] & BQ_MASK )
            bonus += ROOK_PIN_Q;
        else if ( b[z] & BK_MASK )
            bonus += ROOK_PIN_K;
    }

    // Continue SOUTH for king position attack and protection

    for ( z = holdz; (b[z] & ~(WR_MASK | WQ_MASK)) == EMPTY; z += SOUTH );
    if ( AttackBlackKingPos(b+z) )
        bonus += ROOK_ATTACK_KPOS;

    if ( AttackWhiteKingPos(b+z) )
        bonus += ROOK_PROTECT_KPOS;

    return bonus;
}


SCORE ComputerChessPlayer::BlackRookBonus ( const SQUARE *b, int ofs, int wk_offset )
{
    SCORE bonus = 0;
    int z;

    // See if we are on the seventh rank or the eighth rank...

    if ( ofs <= OFFSET(9,3) )
    {
        if ( ofs >= OFFSET(2,3) )
            bonus += ROOK_ON_7TH_RANK;
        else
            bonus += (ROOK_CAN_REACH_7TH_RANK + ROOK_OPEN_FILE);
    }
    else
    {
        // A file is open if none of our pawns is blocking it before 7th.
        for ( z=ofs + SOUTH; b[z] == EMPTY && z > OFFSET(9,3); z += SOUTH );
        bonus += (9 - YPART(z)) / 2;

        if ( z > OFFSET(9,3) )
        {
            if ( !(b[z] & BP_MASK) )
                bonus += ROOK_OPEN_FILE;
        }
        else if ( z < OFFSET(3,3) )
            bonus += ROOK_CAN_REACH_7TH_RANK;

        for ( z=ofs + NORTH; b[z] == EMPTY; z += NORTH );
        if ( b[z] & (BR_MASK | BQ_MASK) )
            bonus += ROOK_CONNECT_VERT;

        for ( z=ofs + WEST; b[z] == EMPTY; z += WEST );
        if ( b[z] & BR_MASK )
            bonus += ROOK_CONNECT_HOR;
    }

    if ( (b[ofs+EAST] & (BLACK_MASK | OFFBOARD)) &&
         (b[ofs+WEST] & (BLACK_MASK | OFFBOARD)) )
    {
        if ( (b[ofs+NORTH] & (BLACK_MASK | OFFBOARD)) &&
             (b[ofs+SOUTH] & (BLACK_MASK | OFFBOARD)) )
            bonus -= ROOK_IMMOBILE;
        else
            bonus -= ROOK_IMMOBILE_HORIZ;
    }

    if ( Distance ( ofs, wk_offset ) < 3 )
        bonus += CTEK_ROOK;

// NORTH ---------------------------------------------------------------

    // Look north for pins caused by rook
    for ( z=ofs + NORTH; b[z] == EMPTY; z += NORTH );
    int holdz = z;
    if ( b[z] & (WB_MASK | WN_MASK) )
    {
        for ( z += NORTH; b[z] == EMPTY; z += NORTH );
        if ( b[z] & WQ_MASK )
            bonus += ROOK_PIN_Q;
        else if ( b[z] & WK_MASK )
            bonus += ROOK_PIN_K;
    }

    // Look NORTH for king position attack and protection

    for ( z = holdz; (b[z] & ~(BR_MASK | BQ_MASK)) == EMPTY; z += NORTH );
    if ( AttackWhiteKingPos(b+z) )
        bonus += ROOK_ATTACK_KPOS;

    if ( AttackBlackKingPos(b+z) )
        bonus += ROOK_PROTECT_KPOS;

// EAST ---------------------------------------------------------------

    // Look east for pins caused by rook
    for ( z=ofs + EAST; b[z] == EMPTY; z += EAST );
    holdz = z;
    if ( b[z] & (WB_MASK | WN_MASK) )
    {
        for ( z += EAST; b[z] == EMPTY; z += EAST );
        if ( b[z] & WQ_MASK )
            bonus += ROOK_PIN_Q;
        else if ( b[z] & WK_MASK )
            bonus += ROOK_PIN_K;
    }

    // Continue EAST for king position attack and protection

    for ( z = holdz; (b[z] & ~(BR_MASK | BQ_MASK)) == EMPTY; z += EAST );
    if ( AttackWhiteKingPos(b+z) )
        bonus += ROOK_ATTACK_KPOS;

    if ( AttackBlackKingPos(b+z) )
        bonus += ROOK_PROTECT_KPOS;

// WEST ----------------------------------------------------------------

    // Look west for pins caused by rook
    for ( z=ofs + WEST; b[z] == EMPTY; z += WEST );
    holdz = z;
    if ( b[z] & (WB_MASK | WN_MASK) )
    {
        for ( z += WEST; b[z] == EMPTY; z += WEST );
        if ( b[z] & WQ_MASK )
            bonus += ROOK_PIN_Q;
        else if ( b[z] & WK_MASK )
            bonus += ROOK_PIN_K;
    }

    // Look WEST for king position attack and protection

    for ( z = holdz; (b[z] & ~(BR_MASK | BQ_MASK)) == EMPTY; z += WEST );
    if ( AttackWhiteKingPos(b+z) )
        bonus += ROOK_ATTACK_KPOS;

    if ( AttackBlackKingPos(b+z) )
        bonus += ROOK_PROTECT_KPOS;

// SOUTH ---------------------------------------------------------------

    // Look south for pins caused by rook
    for ( z=ofs + SOUTH; b[z] == EMPTY; z += SOUTH );
    holdz = z;
    if ( b[z] & (WB_MASK | WN_MASK) )
    {
        for ( z += SOUTH; b[z] == EMPTY; z += SOUTH );
        if ( b[z] & WQ_MASK )
            bonus += ROOK_PIN_Q;
        else if ( b[z] & WK_MASK )
            bonus += ROOK_PIN_K;
    }

    // Look SOUTH for king position attack and protection

    for ( z = holdz; (b[z] & ~(BR_MASK | BQ_MASK)) == EMPTY; z += SOUTH );
    if ( AttackWhiteKingPos(b+z) )
        bonus += ROOK_ATTACK_KPOS;

    if ( AttackBlackKingPos(b+z) )
        bonus += ROOK_PROTECT_KPOS;

    return bonus;
}


SCORE ComputerChessPlayer::WhiteBishopBonus ( 
    const SQUARE *b,
    int ofs,
    int bk_offset )
{
    const SQUARE *insideBoard = b + ofs;

    SCORE score = BishopPosition [ofs];

    if ( (insideBoard[NORTHEAST] & (WHITE_MASK|OFFBOARD)) &&
         (insideBoard[NORTHWEST] & (WHITE_MASK|OFFBOARD)) &&
         (insideBoard[SOUTHEAST] & (WHITE_MASK|OFFBOARD)) &&
         (insideBoard[SOUTHWEST] & (WHITE_MASK|OFFBOARD)) )
    {
        score -= BISHOP_IMMOBILE;
    }
    else
    {
        int count = 0;
        const SQUARE *p;

        for ( p = insideBoard + NORTHEAST; *p == EMPTY; p += NORTHEAST )
            if ( !(p[NORTHEAST] & BP_MASK) && !(p[NORTHWEST] & BP_MASK) )
                ++count;

        for ( ; (*p & ~(WB_MASK | WQ_MASK)) == EMPTY; p += NORTHEAST );
        if ( AttackBlackKingPos(p) )
            score += BISHOP_ATTACK_KPOS;

        if ( AttackWhiteKingPos(p) )
            score += BISHOP_PROTECT_KPOS;

        if ( *p & WB_PIN_MASK )
        {
            for ( p += NORTHEAST; *p == EMPTY; p += NORTHEAST );

            if ( *p & (BQ_MASK | BK_MASK | BR_MASK) )
            {
                if ( *p & BR_MASK )
                    count += BISHOP_PIN_R;
                else if ( *p & BQ_MASK )
                    count += BISHOP_PIN_Q;
                else
                    count += BISHOP_PIN_K;
            }
        }

        for ( p = insideBoard + NORTHWEST; *p == EMPTY; p += NORTHWEST )
            if ( !(p[NORTHEAST] & BP_MASK) && !(p[NORTHWEST] & BP_MASK) )
                ++count;

        for ( ; (*p & ~(WB_MASK | WQ_MASK)) == EMPTY; p += NORTHWEST );
        if ( AttackBlackKingPos(p) )
            score += BISHOP_ATTACK_KPOS;

        if ( AttackWhiteKingPos(p) )
            score += BISHOP_PROTECT_KPOS;

        if ( *p & WB_PIN_MASK )
        {
            for ( p += NORTHWEST; *p == EMPTY; p += NORTHWEST );
            if ( *p & (BQ_MASK | BK_MASK | BR_MASK) )
            {
                if ( *p & BR_MASK )
                    count += BISHOP_PIN_R;
                else if ( *p & BQ_MASK )
                    count += BISHOP_PIN_Q;
                else
                    count += BISHOP_PIN_K;
            }
        }

        for ( p = insideBoard + SOUTHEAST; *p == EMPTY; p += SOUTHEAST )
            if ( !(p[NORTHEAST] & BP_MASK) && !(p[NORTHWEST] & BP_MASK) )
                ++count;

        if ( *p & WB_PIN_MASK )
        {
            for ( p += SOUTHEAST; *p == EMPTY; p += SOUTHEAST );
            if ( *p & (BQ_MASK | BK_MASK | BR_MASK) )
            {
                if ( *p & BR_MASK )
                    count += BISHOP_PIN_R;
                else if ( *p & BQ_MASK )
                    count += BISHOP_PIN_Q;
                else
                    count += BISHOP_PIN_K;
            }
        }

        for ( p = insideBoard + SOUTHWEST; *p == EMPTY; p += SOUTHWEST )
            if ( !(p[NORTHEAST] & BP_MASK) && !(p[NORTHWEST] & BP_MASK) )
                ++count;

        if ( *p & WB_PIN_MASK )
        {
            for ( p += SOUTHWEST; *p == EMPTY; p += SOUTHWEST );
            if ( *p & (BQ_MASK | BK_MASK | BR_MASK) )
            {
                if ( *p & BR_MASK )
                    count += BISHOP_PIN_R;
                else if ( *p & BQ_MASK )
                    count += BISHOP_PIN_Q;
                else
                    count += BISHOP_PIN_K;
            }
        }

        score += count;
    }

    if ( Distance ( ofs, bk_offset ) < 4 )
        score += CTEK_BISHOP;

    // This is a kludge to try to prevent computer from putting
    // pieces in front of QP or KP and blocking the other bishop
    // from development through the center.

    if ( ofs == OFFSET(7,2) )
    {
        if ( b [ OFFSET(6,4) ] != EMPTY && (b [ OFFSET(6,3) ] & WP_MASK) )
            score -= CENTER_BLOCK_BISHOP1;

        if ( b [ OFFSET(5,4) ] != EMPTY )
            score -= CENTER_BLOCK_BISHOP2;
    }
    else if ( ofs == OFFSET(4,2) )
    {
        if ( b [ OFFSET(5,4) ] != EMPTY && (b [ OFFSET(5,3) ] & WP_MASK) )
            score -= CENTER_BLOCK_BISHOP1;

        if ( b [ OFFSET(6,4) ] != EMPTY )
            score -= CENTER_BLOCK_BISHOP2;
    }

    return score;
}


SCORE ComputerChessPlayer::BlackBishopBonus ( 
    const SQUARE *b,
    int ofs,
    int bk_offset )
{
    const SQUARE *insideBoard = b + ofs;
    SCORE score = BishopPosition [ OFFSET(11,11) - ofs ];

    if ( (insideBoard[NORTHEAST] & (BLACK_MASK|OFFBOARD)) &&
         (insideBoard[NORTHWEST] & (BLACK_MASK|OFFBOARD)) &&
         (insideBoard[SOUTHEAST] & (BLACK_MASK|OFFBOARD)) &&
         (insideBoard[SOUTHWEST] & (BLACK_MASK|OFFBOARD)) )
    {
        score -= BISHOP_IMMOBILE;
    }
    else
    {
        int count = 0;
        const SQUARE *p;

        for ( p = insideBoard + NORTHEAST; *p == EMPTY; p += NORTHEAST )
            if ( !(p[SOUTHEAST] & WP_MASK) && !(p[SOUTHWEST] & WP_MASK) )
                ++count;

        if ( *p & BB_PIN_MASK )
        {
            for ( p += NORTHEAST; *p == EMPTY; p += NORTHEAST );
            if ( *p & (WQ_MASK | WK_MASK | WR_MASK) )
            {
                if ( *p & WR_MASK )
                    count += BISHOP_PIN_R;
                else if ( *p & WQ_MASK )
                    count += BISHOP_PIN_Q;
                else
                    count += BISHOP_PIN_K;
            }
        }

        for ( p = insideBoard + NORTHWEST; *p == EMPTY; p += NORTHWEST )
            if ( !(p[SOUTHEAST] & WP_MASK) && !(p[SOUTHWEST] & WP_MASK) )
                ++count;

        if ( *p & BB_PIN_MASK )
        {
            for ( p += NORTHWEST; *p == EMPTY; p += NORTHWEST );
            if ( *p & (WQ_MASK | WK_MASK | WR_MASK) )
            {
                if ( *p & WR_MASK )
                    count += BISHOP_PIN_R;
                else if ( *p & WQ_MASK )
                    count += BISHOP_PIN_Q;
                else
                    count += BISHOP_PIN_K;
            }
        }

        for ( p = insideBoard + SOUTHEAST; *p == EMPTY; p += SOUTHEAST )
            if ( !(p[SOUTHEAST] & WP_MASK) && !(p[SOUTHWEST] & WP_MASK) )
                ++count;

        for ( ; (*p & ~(BB_MASK | BQ_MASK)) == EMPTY; p += SOUTHEAST );
        if ( AttackWhiteKingPos(p) )
            score += BISHOP_ATTACK_KPOS;

        if ( AttackBlackKingPos(p) )
            score += BISHOP_PROTECT_KPOS;

        if ( *p & BB_PIN_MASK )
        {
            for ( p += SOUTHEAST; *p == EMPTY; p += SOUTHEAST );
            if ( *p & (WQ_MASK | WK_MASK | WR_MASK) )
            {
                if ( *p & WR_MASK )
                    count += BISHOP_PIN_R;
                else if ( *p & WQ_MASK )
                    count += BISHOP_PIN_Q;
                else
                    count += BISHOP_PIN_K;
            }
        }

        for ( p = insideBoard + SOUTHWEST; *p == EMPTY; p += SOUTHWEST )
            if ( !(p[SOUTHEAST] & WP_MASK) && !(p[SOUTHWEST] & WP_MASK) )
                ++count;

        for ( ; (*p & ~(BB_MASK | BQ_MASK)) == EMPTY; p += SOUTHWEST );
        if ( AttackWhiteKingPos(p) )
            score += BISHOP_ATTACK_KPOS;

        if ( AttackBlackKingPos(p) )
            score += BISHOP_PROTECT_KPOS;

        if ( *p & BB_PIN_MASK )
        {
            for ( p += SOUTHWEST; *p == EMPTY; p += SOUTHWEST );
            if ( *p & (WQ_MASK | WK_MASK | WR_MASK) )
            {
                if ( *p & WR_MASK )
                    count += BISHOP_PIN_R;
                else if ( *p & WQ_MASK )
                    count += BISHOP_PIN_Q;
                else
                    count += BISHOP_PIN_K;
            }
        }

        score += count;
    }

    if ( Distance ( ofs, bk_offset ) < 4 )
        score += CTEK_BISHOP;

    if ( ofs == OFFSET(7,9) )
    {
        if ( b [ OFFSET(6,7) ] != EMPTY && (b [ OFFSET(6,8) ] & BP_MASK) )
            score -= CENTER_BLOCK_BISHOP1;

        if ( b [ OFFSET(5,7) ] != EMPTY )
            score -= CENTER_BLOCK_BISHOP2;
    }
    else if ( ofs == OFFSET(4,9) )
    {
        if ( b [ OFFSET(5,7) ] != EMPTY && (b [ OFFSET(5,8) ] & BP_MASK) )
            score -= CENTER_BLOCK_BISHOP1;

        if ( b [ OFFSET(6,7) ] != EMPTY )
            score -= CENTER_BLOCK_BISHOP2;
    }

    return score;
}


SCORE ComputerChessPlayer::WhiteKnightFork ( const SQUARE *p )
{
    int count = 0;
    int rcount = 0;
    int qcount = 0;
    int kcount = 0;
    SQUARE s;

    if ( (s = p[OFFSET(1,2)]) & (BR_MASK | BQ_MASK | BK_MASK) )
    {
        ++count;
        if ( s & BK_MASK )       ++kcount;
        else if ( s & BQ_MASK )  ++qcount;
        else                     ++rcount;
    }

    if ( (s = p[OFFSET(-1,2)]) & (BR_MASK | BQ_MASK | BK_MASK) )
    {
        ++count;
        if ( s & BK_MASK)        ++kcount;
        else if ( s & BQ_MASK )  ++qcount;
        else                     ++rcount;
    }

    if ( (s = p[OFFSET(1,-2)]) & (BR_MASK | BQ_MASK | BK_MASK) )
    {
        ++count;
        if ( s & BK_MASK )       ++kcount;
        else if ( s & BQ_MASK )  ++qcount;
        else                     ++rcount;
    }

    if ( (s = p[OFFSET(-1,-2)]) & (BR_MASK | BQ_MASK | BK_MASK) )
    {
        ++count;
        if ( s & BK_MASK )       ++kcount;
        else if ( s & BQ_MASK )  ++qcount;
        else                     ++rcount;
    }

    if ( (s = p[OFFSET(2,1)]) & (BR_MASK | BQ_MASK | BK_MASK) )
    {
        ++count;
        if ( s & BK_MASK )       ++kcount;
        else if ( s & BQ_MASK )  ++qcount;
        else                     ++rcount;
    }

    if ( (s = p[OFFSET(2,-1)]) & (BR_MASK | BQ_MASK | BK_MASK) )
    {
        ++count;
        if ( s & BK_MASK )       ++kcount;
        else if ( s & BQ_MASK )  ++qcount;
        else                     ++rcount;
    }

    if ( (s = p[OFFSET(-2,1)]) & (BR_MASK | BQ_MASK | BK_MASK) )
    {
        ++count;
        if ( s & BK_MASK )       ++kcount;
        else if ( s & BQ_MASK )  ++qcount;
        else                     ++rcount;
    }

    if ( (s = p[OFFSET(-2,-1)]) & (BR_MASK | BQ_MASK | BK_MASK) )
    {
        ++count;
        if ( s & BK_MASK )       ++kcount;
        else if ( s & BQ_MASK )  ++qcount;
        else                     ++rcount;
    }

    if ( count > 1 )
    {
        // Something looks forked

        if ( kcount > 0 )
        {
            if ( qcount > 0 )
                return KNIGHT_FORK_UNCERTAINTY*(QUEEN_VAL - KNIGHT_VAL);
            else
                return KNIGHT_FORK_UNCERTAINTY*(ROOK_VAL - KNIGHT_VAL);
        }
        else if ( qcount > 1 )
            return KNIGHT_FORK_UNCERTAINTY*(QUEEN_VAL - KNIGHT_VAL);
        else
            return KNIGHT_FORK_UNCERTAINTY*(ROOK_VAL - KNIGHT_VAL);
    }

    return 0;
}


SCORE ComputerChessPlayer::BlackKnightFork ( const SQUARE *p )
{
    int count = 0;
    int rcount = 0;
    int qcount = 0;
    int kcount = 0;
    SQUARE s;

    if ( (s = p[OFFSET(1,2)]) & (WR_MASK | WQ_MASK | WK_MASK) )
    {
        ++count;
        if ( s & WK_MASK )       ++kcount;
        else if ( s & WQ_MASK )  ++qcount;
        else                     ++rcount;
    }

    if ( (s = p[OFFSET(-1,2)]) & (WR_MASK | WQ_MASK | WK_MASK) )
    {
        ++count;
        if ( s & WK_MASK )       ++kcount;
        else if ( s & WQ_MASK )  ++qcount;
        else                     ++rcount;
    }

    if ( (s = p[OFFSET(1,-2)]) & (WR_MASK | WQ_MASK | WK_MASK) )
    {
        ++count;
        if ( s & WK_MASK )       ++kcount;
        else if ( s & WQ_MASK )  ++qcount;
        else                     ++rcount;
    }

    if ( (s = p[OFFSET(-1,-2)]) & (WR_MASK | WQ_MASK | WK_MASK) )
    {
        ++count;
        if ( s & WK_MASK )       ++kcount;
        else if ( s & WQ_MASK )  ++qcount;
        else                     ++rcount;
    }

    if ( (s = p[OFFSET(2,1)]) & (WR_MASK | WQ_MASK | WK_MASK) )
    {
        ++count;
        if ( s & WK_MASK )       ++kcount;
        else if ( s & WQ_MASK )  ++qcount;
        else                     ++rcount;
    }

    if ( (s = p[OFFSET(2,-1)]) & (WR_MASK | WQ_MASK | WK_MASK) )
    {
        ++count;
        if ( s & WK_MASK )       ++kcount;
        else if ( s & WQ_MASK )  ++qcount;
        else                     ++rcount;
    }

    if ( (s = p[OFFSET(-2,1)]) & (WR_MASK | WQ_MASK | WK_MASK) )
    {
        ++count;
        if ( s & WK_MASK )       ++kcount;
        else if ( s & WQ_MASK )  ++qcount;
        else                     ++rcount;
    }

    if ( (s = p[OFFSET(-2,-1)]) & (WR_MASK | WQ_MASK | WK_MASK) )
    {
        ++count;
        if ( s & WK_MASK )       ++kcount;
        else if ( s & WQ_MASK )  ++qcount;
        else                     ++rcount;
    }

    if ( count > 1 )
    {
        // Something looks forked

        if ( kcount > 0 )
        {
            if ( qcount > 0 )
                return KNIGHT_FORK_UNCERTAINTY*(QUEEN_VAL - KNIGHT_VAL);
            else
                return KNIGHT_FORK_UNCERTAINTY*(ROOK_VAL - KNIGHT_VAL);
        }
        else if ( qcount > 1 )
            return KNIGHT_FORK_UNCERTAINTY*(QUEEN_VAL - KNIGHT_VAL);
        else
            return KNIGHT_FORK_UNCERTAINTY*(ROOK_VAL - KNIGHT_VAL);
    }

    return 0;
}


SCORE ComputerChessPlayer::WhiteKnightBonus ( 
    const SQUARE *inBoard,
    int ofs,
    int bk_offset )
{
    // The offset is subtracted from OFFSET(11,11) as
    // a trick for "rotating" the array, since the contents
    // are asymmetric.

    SCORE score = KnightPosition [ OFFSET(11,11) - ofs ];

    if ( Distance ( ofs, bk_offset ) < 4 )
        score += CTEK_KNIGHT;

    score += WhiteKnightFork ( inBoard );
    
    if ( AttackBlackKingPos ( inBoard + OFFSET(2,1) ) )
        score += KNIGHT_ATTACK_KPOS;

    if ( AttackWhiteKingPos ( inBoard + OFFSET(2,1) ) )
        score += KNIGHT_PROTECT_KPOS;

    if ( AttackBlackKingPos ( inBoard + OFFSET(2,-1) ) )
        score += KNIGHT_ATTACK_KPOS;

    if ( AttackWhiteKingPos ( inBoard + OFFSET(2,-1) ) )
        score += KNIGHT_PROTECT_KPOS;

    if ( AttackBlackKingPos ( inBoard + OFFSET(1,2) ) )
        score += KNIGHT_ATTACK_KPOS;

    if ( AttackWhiteKingPos ( inBoard + OFFSET(1,2) ) )
        score += KNIGHT_PROTECT_KPOS;

    if ( AttackBlackKingPos ( inBoard + OFFSET(-1,2) ) )
        score += KNIGHT_ATTACK_KPOS;

    if ( AttackWhiteKingPos ( inBoard + OFFSET(-1,2) ) )
        score += KNIGHT_PROTECT_KPOS;

    if ( AttackBlackKingPos ( inBoard + OFFSET(-2,-1) ) )
        score += KNIGHT_ATTACK_KPOS;

    if ( AttackWhiteKingPos ( inBoard + OFFSET(-2,-1) ) )
        score += KNIGHT_PROTECT_KPOS;

    if ( AttackBlackKingPos ( inBoard + OFFSET(-2,1) ) )
        score += KNIGHT_ATTACK_KPOS;

    if ( AttackWhiteKingPos ( inBoard + OFFSET(-2,1) ) )
        score += KNIGHT_PROTECT_KPOS;

    if ( AttackBlackKingPos ( inBoard + OFFSET(-1,-2) ) )
        score += KNIGHT_ATTACK_KPOS;

    if ( AttackWhiteKingPos ( inBoard + OFFSET(-1,-2) ) )
        score += KNIGHT_PROTECT_KPOS;

    if ( AttackBlackKingPos ( inBoard + OFFSET(1,-2) ) )
        score += KNIGHT_ATTACK_KPOS;

    if ( AttackWhiteKingPos ( inBoard + OFFSET(1,-2) ) )
        score += KNIGHT_PROTECT_KPOS;

    return score;
}


SCORE ComputerChessPlayer::BlackKnightBonus ( 
    const SQUARE *inBoard,
    int ofs,
    int wk_offset )
{
    SCORE score = KnightPosition [ ofs ];

    if ( Distance ( ofs, wk_offset ) < 4 )
        score += CTEK_KNIGHT;

    score += BlackKnightFork ( inBoard );

    if ( AttackWhiteKingPos ( inBoard + OFFSET(2,1) ) )
        score += KNIGHT_ATTACK_KPOS;

    if ( AttackBlackKingPos ( inBoard + OFFSET(2,1) ) )
        score += KNIGHT_PROTECT_KPOS;

    if ( AttackWhiteKingPos ( inBoard + OFFSET(-2,1) ) )
        score += KNIGHT_ATTACK_KPOS;

    if ( AttackBlackKingPos ( inBoard + OFFSET(-2,1) ) )
        score += KNIGHT_PROTECT_KPOS;

    if ( AttackWhiteKingPos ( inBoard + OFFSET(2,-1) ) )
        score += KNIGHT_ATTACK_KPOS;

    if ( AttackBlackKingPos ( inBoard + OFFSET(2,-1) ) )
        score += KNIGHT_PROTECT_KPOS;

    if ( AttackWhiteKingPos ( inBoard + OFFSET(-2,-1) ) )
        score += KNIGHT_ATTACK_KPOS;

    if ( AttackBlackKingPos ( inBoard + OFFSET(-2,-1) ) )
        score += KNIGHT_PROTECT_KPOS;

    if ( AttackWhiteKingPos ( inBoard + OFFSET(1,2) ) )
        score += KNIGHT_ATTACK_KPOS;

    if ( AttackBlackKingPos ( inBoard + OFFSET(1,2) ) )
        score += KNIGHT_PROTECT_KPOS;

    if ( AttackWhiteKingPos ( inBoard + OFFSET(-1,2) ) )
        score += KNIGHT_ATTACK_KPOS;

    if ( AttackBlackKingPos ( inBoard + OFFSET(-1,2) ) )
        score += KNIGHT_PROTECT_KPOS;

    if ( AttackWhiteKingPos ( inBoard + OFFSET(1,-2) ) )
        score += KNIGHT_ATTACK_KPOS;

    if ( AttackBlackKingPos ( inBoard + OFFSET(1,-2) ) )
        score += KNIGHT_PROTECT_KPOS;

    if ( AttackWhiteKingPos ( inBoard + OFFSET(-1,-2) ) )
        score += KNIGHT_ATTACK_KPOS;

    if ( AttackBlackKingPos ( inBoard + OFFSET(-1,-2) ) )
        score += KNIGHT_PROTECT_KPOS;

    return score;
}


static const SCORE QueenPosition [] =
{
    0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,
    0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,

    0000,0000,   -14, -10,  -8,  -2,  -2,  -8, -10, -14,  0000,0000,
    0000,0000,   -10,  -5,  -2,   0,   0,  -2,  -5, -10,  0000,0000,
    0000,0000,    -8,  -2,  -1,   0,   0,  -1,  -2,  -8,  0000,0000,
    0000,0000,    -7,  -1,   0,   1,   1,   0,  -1,  -7,  0000,0000,
    0000,0000,    -7,  -1,   0,   1,   1,   0,  -1,  -7,  0000,0000,
    0000,0000,    -8,  -2,  -1,   0,   0,  -1,  -2,  -8,  0000,0000,
    0000,0000,   -11,  -8,  -5,  -3,  -3,  -5,  -8, -11,  0000,0000,
    0000,0000,   -15, -12,  -8,  -5,  -5,  -8, -12, -15
};


SCORE ComputerChessPlayer::WhiteQueenBonus ( 
    const SQUARE *b,
    int ofs,
    int bk_offset )
{
    SCORE score = QueenPosition[ofs];
    int dist = Distance ( ofs, bk_offset );

    if ( dist < 2 )
        score += CTEK_QUEEN2;
    else if ( dist < 3 )
        score += CTEK_QUEEN3;

    const SQUARE *p;
    for ( p=b+ofs+NORTH; (*p & ~(WQ_MASK | WR_MASK)) == EMPTY; p += NORTH );    
    if ( AttackBlackKingPos(p) )
        score += QUEEN_ATTACK_KPOS;

    if ( AttackWhiteKingPos(p) )
        score += QUEEN_PROTECT_KPOS;

    for ( p=b+ofs+NORTHEAST; (*p & ~(WQ_MASK | WB_MASK)) == EMPTY; p += NORTHEAST );
    if ( AttackBlackKingPos(p) )
        score += QUEEN_ATTACK_KPOS;

    if ( AttackWhiteKingPos(p) )
        score += QUEEN_PROTECT_KPOS;

    for ( p=b+ofs+NORTHWEST; (*p & ~(WQ_MASK | WB_MASK)) == EMPTY; p += NORTHWEST );
    if ( AttackBlackKingPos(p) )
        score += QUEEN_ATTACK_KPOS;

    if ( AttackWhiteKingPos(p) )
        score += QUEEN_PROTECT_KPOS;

    for ( p=b+ofs+EAST; (*p & ~(WQ_MASK | WR_MASK)) == EMPTY; p += EAST );
    if ( AttackBlackKingPos(p) )
        score += QUEEN_ATTACK_KPOS;

    if ( AttackWhiteKingPos(p) )
        score += QUEEN_PROTECT_KPOS;

    for ( p=b+ofs+WEST; (*p & ~(WQ_MASK | WR_MASK)) == EMPTY; p += WEST );
    if ( AttackBlackKingPos(p) )
        score += QUEEN_ATTACK_KPOS;

    if ( AttackWhiteKingPos(p) )
        score += QUEEN_PROTECT_KPOS;

    return score;
}


SCORE ComputerChessPlayer::BlackQueenBonus ( 
    const SQUARE *b,
    int ofs,
    int wk_offset )
{
    SCORE score = QueenPosition [OFFSET(11,11) - ofs];
    int dist = Distance ( ofs, wk_offset );

    if ( dist < 2 )
        score += CTEK_QUEEN2;
    else if ( dist < 3 ) 
        score += CTEK_QUEEN3;

    const SQUARE *p;
    for ( p=b+ofs+SOUTH; (*p & ~(BQ_MASK | BR_MASK)) == EMPTY; p += SOUTH );    
    if ( AttackWhiteKingPos(p) )
        score += QUEEN_ATTACK_KPOS;

    if ( AttackBlackKingPos(p) )
        score += QUEEN_PROTECT_KPOS;

    for ( p=b+ofs+SOUTHEAST; (*p & ~(BQ_MASK | BB_MASK)) == EMPTY; p += SOUTHEAST );
    if ( AttackWhiteKingPos(p) )
        score += QUEEN_ATTACK_KPOS;

    if ( AttackBlackKingPos(p) )
        score += QUEEN_PROTECT_KPOS;

    for ( p=b+ofs+SOUTHWEST; (*p & ~(BQ_MASK | BB_MASK)) == EMPTY; p += SOUTHWEST );
    if ( AttackWhiteKingPos(p) )
        score += QUEEN_ATTACK_KPOS;

    if ( AttackBlackKingPos(p) )
        score += QUEEN_PROTECT_KPOS;
    
    for ( p=b+ofs+EAST; (*p & ~(BQ_MASK | BR_MASK)) == EMPTY; p += EAST );
    if ( AttackWhiteKingPos(p) )
        score += QUEEN_ATTACK_KPOS;

    if ( AttackBlackKingPos(p) )
        score += QUEEN_PROTECT_KPOS;

    for ( p=b+ofs+WEST; (*p & ~(BQ_MASK | BR_MASK)) == EMPTY; p += WEST );
    if ( AttackWhiteKingPos(p) )
        score += QUEEN_ATTACK_KPOS;

    if ( AttackBlackKingPos(p) )
        score += QUEEN_PROTECT_KPOS;

    return score;
}



SCORE ComputerChessPlayer::CastleHoleDanger ( 
    const SQUARE *p,
    int dir,
    SQUARE mask )
{
    SCORE s = 0;

    for(;;)
    {
        if ( *p & mask )
            s += CASTLE_HOLE_DANGER;
        else if ( *p != EMPTY )
            break;

        p += dir;
    }

    return s;
}


// Put stuff in CommonMidgameEval() which does not depend on whose turn it is.

SCORE ComputerChessPlayer::CommonMidgameEval ( ChessBoard &board )
{
    SCORE score = 0;
    const SQUARE *b = board.board;
    const int wk = board.wk_offset;
    const int bk = board.bk_offset;

    //---------------------------- WHITE KING ----------------------------

    // White king is timid if there is a black queen or a pair of black rooks with
    // assistance from either black knight(s) or black bishop(s).
    bool timidWKing =
        ( (board.inventory[BQ_INDEX] > 0 || board.inventory[BR_INDEX] > 1) &&
        (board.inventory[BB_INDEX] > 0 || board.inventory[BN_INDEX] > 0) );

    // Check white castling...

    if ( board.flags & SF_WKMOVED )
    {
        if ( timidWKing )
        {
            // Play it safe with king...

            if ( wk == OFFSET(8,2) )
            {
                if ( b[OFFSET(9,2)] & WR_MASK )
                    score -= ROOK_TRAPPED_BY_KING;

                if ( b[OFFSET(9,3)] & WR_MASK )
                    score -= ROOK_TRAPPED_BY_KING;

                if ( b[OFFSET(7,4)] & WN_MASK )
                    score += CASTLE_KNIGHT_GUARD;
            }
            else if ( wk == OFFSET(7,2) )
            {
                if ( b[OFFSET(8,2)] & WR_MASK )
                    score -= ROOK_TRAPPED_BY_KING;

                if ( b[OFFSET(9,2)] & WR_MASK )
                    score -= ROOK_TRAPPED_BY_KING;

                if ( b[OFFSET(8,3)] & WR_MASK )
                    score -= ROOK_TRAPPED_BY_KING;

                if ( b[OFFSET(9,3)] & WR_MASK )
                    score -= ROOK_TRAPPED_BY_KING;

                if ( b[OFFSET(7,4)] & WN_MASK )
                    score += CASTLE_KNIGHT_GUARD;
            }
            else if ( wk == OFFSET(4,2) )
            {
                if ( b[OFFSET(3,2)] & WR_MASK )
                    score -= ROOK_TRAPPED_BY_KING;

                if ( b[OFFSET(2,2)] & WR_MASK )
                    score -= ROOK_TRAPPED_BY_KING;

                if ( b[OFFSET(2,3)] & WR_MASK )
                    score -= ROOK_TRAPPED_BY_KING;

                if ( b[OFFSET(3,3)] & WR_MASK )
                    score -= ROOK_TRAPPED_BY_KING;

                if ( b[OFFSET(4,4)] & WN_MASK )
                    score += CASTLE_KNIGHT_GUARD;
            }
            else if ( wk == OFFSET(3,2) )
            {
                if ( b[OFFSET(2,2)] & WR_MASK )
                    score -= ROOK_TRAPPED_BY_KING;

                if ( b[OFFSET(3,3)] & WR_MASK )
                    score -= ROOK_TRAPPED_BY_KING;

                if ( b[OFFSET(4,4)] & WN_MASK )
                    score += CASTLE_KNIGHT_GUARD;
            }
            else if ( wk == OFFSET(2,2) )
            {
                if ( b[OFFSET(4,4)] & WN_MASK )
                    score += CASTLE_KNIGHT_GUARD;
            }
            else if ( wk == OFFSET(9,2) )
            {
                if ( b[OFFSET(7,4)] & WN_MASK )
                    score += CASTLE_KNIGHT_GUARD;
            }

            if ( wk != OFFSET(6,2) && wk != OFFSET(5,2) )
            {
                // Try to keep good pawn formation in front of king

                if ( b[wk + OFFSET(-1,1)] & WP_MASK )
                    score += PAWN_PROTECTS_KING2;
                else if ( wk < OFFSET(2,6) &&
                      !(b[wk + OFFSET(-1,2)] & (WP_MASK | OFFBOARD)) &&
                      !(b[wk + OFFSET(-1,3)] & (WP_MASK | OFFBOARD)) )
                {
                    score -= CASTLE_HOLE1;
                    score -= CastleHoleDanger ( 
                        &b[wk+OFFSET(-1,0)], NORTH, (BR_MASK | BQ_MASK) );
                }

                if ( b[wk + OFFSET(0,1)] & WP_MASK )
                    score += PAWN_PROTECTS_KING1;
                else if ( wk < OFFSET(2,6) &&
                      !(b[wk + OFFSET(0,2)] & WP_MASK) &&
                      !(b[wk + OFFSET(0,3)] & WP_MASK) )
                {
                    score -= CASTLE_HOLE2;
                    score -= CastleHoleDanger ( 
                        &b[wk], NORTH, (BR_MASK | BQ_MASK) );
                }

                if ( b[wk + OFFSET(1,1)] & WP_MASK )
                    score += PAWN_PROTECTS_KING2;
                else if ( wk < OFFSET(2,6) &&
                      !(b[wk + OFFSET(0,2)] & WP_MASK) &&
                      !(b[wk + OFFSET(0,3)] & WP_MASK) )
                {
                    score -= CASTLE_HOLE3;
                    score -= CastleHoleDanger ( 
                        &b[wk+OFFSET(1,0)], NORTH, (BR_MASK | BQ_MASK) );
                }

                if ( b[wk + OFFSET(-1,2)] & WP_MASK )
                    score += PAWN_PROTECTS_KING3;

                if ( b[wk + OFFSET(0,2)] & WP_MASK )
                    score += PAWN_PROTECTS_KING2;

                if ( b[wk + OFFSET(1,2)] & WP_MASK )
                    score += PAWN_PROTECTS_KING3;
            }
        }
        else   // Be aggressive with white king...
        {
            if ( board.white_to_move )
            {
                // Look for opposition

                int kk = wk - bk;

                if ( kk==OFFSET(2,0)  || kk==OFFSET(0,2) || 
                     kk==OFFSET(-2,0) || kk==OFFSET(0,-2) )
                    score += KING_OPPOSITION;
            }
        }
    }
    else
    {
        // The white king has not yet moved
        if ( (board.flags & SF_WKRMOVED) == 0 )
        {
            if ( b[OFFSET(7,2)] == EMPTY )
                score += KCASTLE_PATH_EMPTY;

            if ( b[OFFSET(8,2)] == EMPTY )
                score += KCASTLE_PATH_EMPTY;

            if ( (board.flags & SF_WQRMOVED) == 0 )
            {
                // Might castle either way
                score += CAN_KQCASTLE_BONUS;

                if ( b[OFFSET(3,2)] == EMPTY )
                    score += QCASTLE_PATH_EMPTY;

                if ( b[OFFSET(4,2)] == EMPTY )
                    score += QCASTLE_PATH_EMPTY;

                if ( b[OFFSET(5,2)] == EMPTY )
                    score += QCASTLE_PATH_EMPTY;
            }
            else
            {
                // Might castle kingside only
                score += CAN_KCASTLE_BONUS;
            }
        }
        else if ( (board.flags & SF_WQRMOVED) == 0 )
        {
            // Might castle queenside only
            score += CAN_QCASTLE_BONUS;

            if ( b[OFFSET(3,2)] == EMPTY )
                score += QCASTLE_PATH_EMPTY;

            if ( b[OFFSET(4,2)] == EMPTY )
                score += QCASTLE_PATH_EMPTY;

            if ( b[OFFSET(5,2)] == EMPTY )
                score += QCASTLE_PATH_EMPTY;
        }
    }

    //---------------------------- BLACK KING ----------------------------

    // Black king is timid if there is a white queen or a pair of white rooks 
    // with assistance from either white knight(s) or white bishop(s).
    bool timidBKing =
        ( (board.inventory[WQ_INDEX] > 0 || board.inventory[WR_INDEX] > 1) &&
        (board.inventory[WB_INDEX] > 0 || board.inventory[WN_INDEX] > 0) );

    // Check black castling...

    if ( board.flags & SF_BKMOVED )
    {
        if ( timidBKing )
        {
            // Play it safe with king...

            if ( bk == OFFSET(8,9) )
            {
                if ( b[OFFSET(9,9)] & BR_MASK )
                    score += ROOK_TRAPPED_BY_KING;

                if ( b[OFFSET(9,8)] & BR_MASK )
                    score += ROOK_TRAPPED_BY_KING;
            }
            else if ( bk == OFFSET(7,9) )
            {
                if ( b[OFFSET(8,9)] & BR_MASK )
                    score += ROOK_TRAPPED_BY_KING;

                if ( b[OFFSET(8,8)] & BR_MASK )
                    score += ROOK_TRAPPED_BY_KING;

                if ( b[OFFSET(9,8)] & BR_MASK )
                    score += ROOK_TRAPPED_BY_KING;

                if ( b[OFFSET(9,9)] & BR_MASK )
                    score += ROOK_TRAPPED_BY_KING;
            }
            else if ( bk == OFFSET(9,9) )
            {
                if ( b[OFFSET(7,7)] & BN_MASK )
                    score -= CASTLE_KNIGHT_GUARD;
            }
            else if ( bk == OFFSET(4,9) )
            {
                if ( b[OFFSET(3,9)] & BR_MASK )
                    score += ROOK_TRAPPED_BY_KING;

                if ( b[OFFSET(3,8)] & BR_MASK )
                    score += ROOK_TRAPPED_BY_KING;

                if ( b[OFFSET(2,9)] & BR_MASK )
                    score += ROOK_TRAPPED_BY_KING;

                if ( b[OFFSET(2,8)] & BR_MASK )
                    score += ROOK_TRAPPED_BY_KING;

                if ( b[OFFSET(4,7)] & BN_MASK )
                    score -= CASTLE_KNIGHT_GUARD;
            }
            else if ( bk == OFFSET(3,9) )
            {
                if ( b[OFFSET(2,9)] & BR_MASK )
                    score += ROOK_TRAPPED_BY_KING;

                if ( b[OFFSET(2,8)] & BR_MASK )
                    score += ROOK_TRAPPED_BY_KING;

                if ( b[OFFSET(4,7)] & BN_MASK )
                    score -= CASTLE_KNIGHT_GUARD;
            }
            else if ( bk == OFFSET(2,9) )
            {
                if ( b[OFFSET(4,7)] & BN_MASK )
                    score -= CASTLE_KNIGHT_GUARD;
            }

            if ( bk != OFFSET(6,9) && bk != OFFSET(5,9) )
            {
                // Try to keep good pawn formation in front of king

                if ( b[bk + OFFSET(-1,-1)] & BP_MASK )
                    score -= PAWN_PROTECTS_KING2;
                else if ( bk > OFFSET(9,5) &&
                      !(b[bk + OFFSET(-1,-2)] & (BP_MASK | OFFBOARD) ) &&
                      !(b[bk + OFFSET(-1,-3)] & (BP_MASK | OFFBOARD) ) )
                {
                    score += CASTLE_HOLE1;
                    score += CastleHoleDanger ( 
                        &b[bk+OFFSET(-1,0)],
                        SOUTH,
                        (WR_MASK | WQ_MASK) );
                }

                if ( b[bk + OFFSET(0,-1)] & BP_MASK )
                    score -= PAWN_PROTECTS_KING1;
                else if ( bk > OFFSET(9,5) &&
                      !(b[bk + OFFSET(0,-2)] & BP_MASK) &&
                      !(b[bk + OFFSET(0,-3)] & BP_MASK) )
                {
                    score += CASTLE_HOLE2;
                    score += CastleHoleDanger ( 
                        &b[bk],
                        SOUTH,
                        (WR_MASK | WQ_MASK) );
                }

                if ( b[bk + OFFSET(1,-1)] & BP_MASK )
                    score -= PAWN_PROTECTS_KING2;
                else if ( bk > OFFSET(9,5) &&
                      !(b[bk + OFFSET(1,-2)] & BP_MASK) &&
                      !(b[bk + OFFSET(1,-3)] & BP_MASK) )
                {
                    score += CASTLE_HOLE3;
                    score += CastleHoleDanger ( 
                        &b[bk + OFFSET(1,0)],
                        SOUTH,
                        (WR_MASK | WQ_MASK) );
                }

                if ( b[bk + OFFSET(-1,-2)] & BP_MASK )
                    score -= PAWN_PROTECTS_KING3;

                if ( b[bk + OFFSET(0,-2)] & BP_MASK )
                    score -= PAWN_PROTECTS_KING2;

                if ( b[bk + OFFSET(1,-2)] & BP_MASK )
                    score -= PAWN_PROTECTS_KING3;
            }
        }
        else
        {
            // Be aggressive with king...

            if ( !board.white_to_move )
            {
                // Look for opposition

                int kk = wk - bk;

                if ( kk==OFFSET(2,0)  || kk==OFFSET(0,2) || 
                     kk==OFFSET(-2,0) || kk==OFFSET(0,-2) )
                    score -= KING_OPPOSITION;
            }
        }
    }
    else
    {
        // The black king has not yet moved

        if ( (board.flags & SF_BKRMOVED) == 0 )
        {
            if ( b[OFFSET(7,9)] == EMPTY )
                score -= KCASTLE_PATH_EMPTY;

            if ( b[OFFSET(8,9)] == EMPTY )
                score -= KCASTLE_PATH_EMPTY;

            if ( (board.flags & SF_BQRMOVED) == 0 )
            {
                // Might castle either way
                score -= CAN_KQCASTLE_BONUS;

                if ( b[OFFSET(3,9)] == EMPTY )
                    score -= QCASTLE_PATH_EMPTY;

                if ( b[OFFSET(4,9)] == EMPTY )
                    score -= QCASTLE_PATH_EMPTY;

                if ( b[OFFSET(5,9)] == EMPTY )
                    score -= QCASTLE_PATH_EMPTY;
            }
            else
            {
                // Might castle kingside only
                score -= CAN_KCASTLE_BONUS;
            }
        }
        else if ( (board.flags & SF_BQRMOVED) == 0 )
        {
            // Might castle queenside only
            score -= CAN_QCASTLE_BONUS;

            if ( b[OFFSET(3,9)] == EMPTY )
                score -= QCASTLE_PATH_EMPTY;

            if ( b[OFFSET(4,9)] == EMPTY )
                score -= QCASTLE_PATH_EMPTY;

            if ( b[OFFSET(5,9)] == EMPTY )
                score -= QCASTLE_PATH_EMPTY;
        }
    }

    // *** POSITIONAL STUFF ***

    for ( int ybase = OFFSET(2,2); ybase <= OFFSET(2,9); ybase += NORTH )
    {
        int x, ofs;
        for ( x=0, ofs=ybase; x < 8; x++, ofs++ )
        {
            SQUARE piece = b[ofs];
            if ( piece == EMPTY ) continue;

            if ( piece & WHITE_MASK )
            {
                // It's a White piece
                if ( piece & WP_MASK )
                    score += WhitePawnBonus ( board, ofs, x, ybase );
                else if ( piece & (WN_MASK|WB_MASK) )
                {
                    if ( piece & WN_MASK )
                        score += WhiteKnightBonus ( b + ofs, ofs, bk );
                    else
                        score += WhiteBishopBonus ( b, ofs, bk );
                }
                else if ( piece & WR_MASK )
                    score += WhiteRookBonus ( b, ofs, bk );
                else if ( piece & WQ_MASK )
                    score += WhiteQueenBonus ( b, ofs, bk );
            }
            else  // It's a Black piece
            {
                if ( piece & BP_MASK )
                    score -= BlackPawnBonus ( board, ofs, x, ybase );
                else if ( piece & (BN_MASK|BB_MASK) )
                {
                    if ( piece & BN_MASK )
                        score -= BlackKnightBonus ( b + ofs, ofs, wk );
                    else
                        score -= BlackBishopBonus ( b, ofs, wk );
                }
                else if ( piece & BR_MASK )
                    score -= BlackRookBonus ( b, ofs, wk );
                else if ( piece & BQ_MASK )
                    score -= BlackQueenBonus ( b, ofs, wk );
            }
        }
    }

#if PAWN_BALANCE

    // Correct for the strategic importance of possible pawn promotion.
    // (This is not reflected in the simplistic material evaluation.)
    const int nwp = board.inventory [WP_INDEX];   // number of white pawns
    const int nbp = board.inventory [BP_INDEX];   // number of black pawns

    if ( nwp > nbp )
    {
        // 'denom' is value of White's pieces (excludes pawns)
        int denom = board.bmaterial - PAWN_VAL*nbp;
        int balance = (KING_VAL * PawnBalance[nbp][nwp]) / denom;
        score += balance;
    }
    else if ( nbp > nwp )
    {
        // 'denom' is value of Black's pieces (excludes pawns)
        int denom = board.wmaterial - PAWN_VAL*nwp;
        int balance = (KING_VAL * PawnBalance[nwp][nbp]) / denom;
        score -= balance;
    }

#endif // PAWN_BALANCE

    // Check for two-bishop synergy

    if ( board.inventory[WB_INDEX] == 2 )
        score += TWO_BISHOP_SYNERGY;

    if ( board.inventory[BB_INDEX] == 2 )
        score -= TWO_BISHOP_SYNERGY;

    return score;
}


SCORE ComputerChessPlayer::WhiteMidgameEval ( 
    ChessBoard &board,
    int depth,
    SCORE alpha,
    SCORE beta )
{
    PROFILER_ENTER(PX_EVAL);
    ++evaluated;
    SCORE score;

    if ( board.WhiteCanMove() )
    {
        if ( board.IsDefiniteDraw() )
            return DRAW;   // We have found a non-stalemate draw

        score = MaterialEval ( board.wmaterial, board.bmaterial );

        // White king is timid if there is a black queen or a pair of black rooks with
        // assistance from either black knight(s) or black bishop(s).
        bool timidWKing =
            ( (board.inventory[BQ_INDEX] > 0 || board.inventory[BR_INDEX] > 1) &&
            (board.inventory[BB_INDEX] > 0 || board.inventory[BN_INDEX] > 0) );

        if ( timidWKing )
            score += WKingPosition1 [board.wk_offset];    // cautious position table
        else
            score += WKingPosition2 [board.wk_offset];    // aggressive position table

        if ( score > beta + SAFE_EVAL_PRUNE_MARGIN )
            return score;

#if EVAL_PRUNE_BOTH_WAYS
        if ( score < alpha - SAFE_EVAL_PRUNE_MARGIN )
            return score;
#endif

        score += CommonMidgameEval ( board );

        if ( board.flags & SF_WCHECK )
            score -= CHECK_BONUS;
    }
    else
    {
        if ( board.flags & SF_WCHECK )
            return BLACK_WINS + WIN_POSTPONEMENT(depth);
        else
            return DRAW;      // This is a stalemate
    }

    // This is a quiescence nudge for ending up with a tempo.
    // It helps because quiescence implies variable depth search.
    score += TEMPO_BONUS;

    if ( score < 0 )
        score += depth;      // postpone bad things for White
    else if ( score > 0 )
        score -= depth;      // expedite good things for White

    PROFILER_EXIT()

    return score;
}


SCORE ComputerChessPlayer::BlackMidgameEval ( 
    ChessBoard &board,
    int depth,
    SCORE alpha,
    SCORE beta )
{
    PROFILER_ENTER(PX_EVAL)
    ++evaluated;
    SCORE score;

    if ( board.BlackCanMove() )
    {
        if ( board.IsDefiniteDraw() )
            return DRAW;   // We have found a non-stalemate draw

        score = MaterialEval ( board.wmaterial, board.bmaterial );

        // Black king is timid if there is a white queen or a pair of white rooks 
        // with assistance from either white knight(s) or white bishop(s).
        bool timidBKing =
            ( (board.inventory[WQ_INDEX] > 0 || board.inventory[WR_INDEX] > 1) &&
            (board.inventory[WB_INDEX] > 0 || board.inventory[WN_INDEX] > 0) );

        if ( timidBKing )
            score -= BKingPosition1 [board.bk_offset];  // cautious position table
        else
            score -= BKingPosition2 [board.bk_offset];  // aggressive position table

        if ( score < alpha - SAFE_EVAL_PRUNE_MARGIN )
            return score;

#if EVAL_PRUNE_BOTH_WAYS
        if ( score > beta + SAFE_EVAL_PRUNE_MARGIN )
            return score;
#endif

        score += CommonMidgameEval ( board );

        if ( board.flags & SF_BCHECK )
            score += CHECK_BONUS;
    }
    else
    {
        if ( board.flags & SF_BCHECK )
            return WHITE_WINS - WIN_POSTPONEMENT(depth);
        else
            return DRAW;      // This is a stalemate
    }

    // This is a quiescence nudge for ending up with a tempo.
    // It helps because quiescence implies variable depth search.
    score -= TEMPO_BONUS;

    if ( score < 0 )
        score += depth;     // postpone bad things for Black
    else if ( score > 0 )
        score -= depth;     // expedite good things for Black

    PROFILER_EXIT();

    return score;
}


SCORE ComputerChessPlayer::WhitePawnBonus ( 
    const ChessBoard &b,
    const int   ofs,
    const int   x,
    const int   ybase ) const
{
    // We subtract 'ofs' from OFFSET(11,11) to "rotate" the
    // asymmetric values in the array to be appropriate for White.

    SCORE score = PawnCenter [ OFFSET(11,11) - ofs ];

    if ( x == 0 || x == 7 )
        score -= PAWN_SIDE_FILE;

    // Look for pawns in front of us only.
    // We don't look for pawns behind us, because they would have
    // found us already.
    // Also, we stop as soon as we find a pawn in front of us,
    // because we will let that pawn find any ones ahead of it.
    // This is not just for efficiency; it changes the way things
    // are scored.

    int z;
    bool isPassedPawn = true;
    const SQUARE *board = b.board;

    for ( z = ofs + NORTH; z < OFFSET(2,9); z += NORTH )
    {
        if ( board[z] & WP_MASK )
        {
            isPassedPawn = false;
            score -= PAWN_DOUBLED;

            // Subtle: we want to count a doubled pawn only once,
            // even if there are more friendly pawns farther on this
            // same file.  This function will later be called for the
            // pawn we just found here, and that call will properly
            // count any tripled/quadrupled/... pawns.
            break;
        }

        if ( (board[z] & BP_MASK) || (board[z+EAST] & BP_MASK) || (board[z+WEST] & BP_MASK) )
        {
            isPassedPawn = false;
            // Do NOT break here; keep looking for doubled pawns!
        }
    }

    bool isSplitPawn = true;
    for ( z = OFFSET(XPART(ofs),3); z < OFFSET(2,9); z += NORTH )
        if ( (board[z+EAST] & WP_MASK) || (board[z+WEST] & WP_MASK) )
        {
            isSplitPawn = false;
            break;
        }

    if ( isSplitPawn )
        score -= PAWN_SPLIT;

    bool protectPossible = false;

    if ( board [ofs + SOUTHWEST] & WP_MASK )
    {
        protectPossible = true;
        if ( board [ofs + SOUTHEAST] & WP_MASK )
            score += isPassedPawn ? PASSED_PAWN_PROTECT2 : PAWN_PROTECT2;
        else
            score += isPassedPawn ? PASSED_PAWN_PROTECT1 : PAWN_PROTECT1;
    }
    else if ( board [ofs + SOUTHEAST] & WP_MASK )
    {
        protectPossible = true;
        score += isPassedPawn ? PASSED_PAWN_PROTECT1 : PAWN_PROTECT1;
    }
    else if ( isPassedPawn )
    {
        score += PASSED_PAWN_ALONE;
        int leftProtectPossible = 
            (board[ofs+SOUTHWEST]==EMPTY) 
            && (board[ofs+2*SOUTH+WEST] & WP_MASK);

        int rightProtectPossible =
            (board[ofs+SOUTHEAST]==EMPTY)
            && (board[ofs+2*SOUTH+EAST] & WP_MASK);

        int advancePossible = (board[ofs+EAST] | board[ofs+WEST]) & WP_MASK;
        if ( leftProtectPossible || rightProtectPossible || advancePossible )
            protectPossible = true;
        else
            score -= PASSED_PAWN_VULNERABLE;
    }

    if ( (board [ofs + NORTHEAST] & WB_MASK) 
        || (board [ofs + NORTHWEST] & WB_MASK) )
        score += BISHOP_PROTECT_PAWN;

    if ( (board[ofs+NORTHEAST] & (BN_MASK|BB_MASK|BR_MASK|BQ_MASK|BK_MASK)) &&
         (board[ofs+NORTHWEST] & (BN_MASK|BB_MASK|BR_MASK|BQ_MASK|BK_MASK)) )
        score += PAWN_FORK;

    if ( isPassedPawn )
    {
        SCORE passedPawnBonus = 0;
        switch ( ybase )
        {
            case OFFSET(2,6):  passedPawnBonus = PASSED_3_FROM_PROM;   break;
            case OFFSET(2,7):  passedPawnBonus = PASSED_2_FROM_PROM;   break;
            case OFFSET(2,8):  passedPawnBonus = PASSED_1_FROM_PROM;   break;
        }

        if ( !protectPossible )
            passedPawnBonus /= 2;

        // look behind the passed pawn for backup rooks...

        int bofs, rcount=0;
        for ( bofs=ofs + SOUTH; ; bofs += SOUTH )
        {
            if ( board[bofs] & (WR_MASK | WQ_MASK) )
            {
                if ( ++rcount > 1 )
                    break;
            }
            else if ( board[bofs] != EMPTY )
                break;
        }

        if ( rcount > 0 )
        {
            if ( rcount > 1 )
                passedPawnBonus += ROOK_BACKS_PASSED_PAWN2;
            else
                passedPawnBonus += ROOK_BACKS_PASSED_PAWN1;
        }

        // look forward for non-pawn pieces that block the pawn's 
        // potential to advance...

        for ( bofs=ofs + NORTH; board[bofs] != OFFBOARD; bofs += NORTH )
        {
            if ( board[bofs] != EMPTY )
                passedPawnBonus -= PASSED_PIECE_BLOCK;
        }

        score += passedPawnBonus;
    }
    else
    {
        if ( ybase == OFFSET(2,7) )
            score += BLOCKED_2_FROM_PROM;
    }

    if ( ybase == OFFSET(2,7) )
    {
        if ( board[ofs+2*NORTH+EAST] & BK_MASK )
        {
            score += CTEK_PAWN1;
            if ( b.inventory[WQ_INDEX] + b.inventory[WR_INDEX] > 0 )
            {
                if ( (board[ofs+NORTHEAST] & BP_MASK) == 0 )
                {
                    score += CTEK_HOLE;
                    if ( board[ofs+2*EAST] & OFFBOARD )
                        score += CTEK_HOLE-2;
                    else if ( (board[ofs+3*EAST+NORTH] & BP_MASK) == 0 )
                    {
                        score += CTEK_HOLE;
                        if ( board[ofs+2*EAST] & WQ_MASK )
                            score += CTEK_HOLE_Q;
                    }
                }
            }
        }
        else if ( board[ofs+2*NORTH+WEST] & BK_MASK )
        {
            if ( b.inventory[WQ_INDEX] + b.inventory[WR_INDEX] > 0 )
            {
                if ( (board[ofs+NORTHWEST] & BP_MASK) == 0 )
                {
                    score += CTEK_HOLE;
                    if ( board[ofs+2*WEST] & OFFBOARD )
                        score += CTEK_HOLE-2;
                    else if ( (board[ofs+3*WEST+NORTH] & BP_MASK) == 0 )
                    {
                        score += CTEK_HOLE;
                        if ( board[ofs+2*WEST] & WQ_MASK )
                            score += CTEK_HOLE_Q;
                    }
                }
            }
        }
        else if ( board[ofs+2*NORTHEAST] & BK_MASK )
            score += CTEK_PAWN2;
        else if ( board[ofs+2*NORTHWEST] & BK_MASK )
            score += CTEK_PAWN2;
    }

    return score;
}


SCORE ComputerChessPlayer::BlackPawnBonus ( 
    const ChessBoard &b,
    const int         ofs,
    const int         x,
    const int         ybase ) const
{
    SCORE score = PawnCenter [ofs];

    if ( x == 0 || x == 7 )
        score -= PAWN_SIDE_FILE;

    // Look for pawns in front of us only.
    // We don't look for pawns behind us, because they would have
    // found us already.
    // Also, we stop as soon as we find a pawn in front of us,
    // because we will let that pawn find any ones ahead of it.
    // This is not just for efficiency; it changes the way things
    // are scored.

    int z;
    bool isPassedPawn = true;
    const SQUARE *board = b.board;

    for ( z = ofs + SOUTH; z > OFFSET(9,2); z += SOUTH )
    {
        if ( board[z] & BP_MASK )
        {
            isPassedPawn = false;
            score -= PAWN_DOUBLED;

            // Subtle: we want to count a doubled pawn only once,
            // even if there are more friendly pawns farther on this
            // same file.  This function will later be called for the
            // pawn we just found here, and that call will properly
            // count any tripled/quadrupled/... pawns.
            break;
        }

        if ( (board[z] & WP_MASK) || (board[z+EAST] & WP_MASK) || (board[z+WEST] & WP_MASK) )
        {
            isPassedPawn = false;
            // Do NOT break here; keep looking for doubled pawns!
        }
    }

    bool isSplitPawn = true;
    for ( z = OFFSET(XPART(ofs),8); z > OFFSET(9,2); z += SOUTH )
        if ( (board[z+EAST] & BP_MASK) || (board[z+WEST] & BP_MASK) )
        {
            isSplitPawn = false;
            break;
        }

    if ( isSplitPawn )
        score -= PAWN_SPLIT;

    bool protectPossible = false;

    if ( board [ofs + NORTHWEST] & BP_MASK )
    {
        protectPossible = true;
        if ( board [ofs + NORTHEAST] & BP_MASK )
            score += isPassedPawn ? PASSED_PAWN_PROTECT2 : PAWN_PROTECT2;
        else
            score += isPassedPawn ? PASSED_PAWN_PROTECT1 : PAWN_PROTECT1;
    }
    else if ( board [ofs + NORTHEAST] & BP_MASK )
    {
        protectPossible = true;
        score += isPassedPawn ? PASSED_PAWN_PROTECT1 : PAWN_PROTECT1;
    }
    else if ( isPassedPawn )
    {
        score += PASSED_PAWN_ALONE;
        int leftProtectPossible = 
            (board[ofs+NORTHWEST]==EMPTY) 
            && (board[ofs+2*NORTH+WEST] & BP_MASK);

        int rightProtectPossible =
            (board[ofs+NORTHEAST]==EMPTY)
            && (board[ofs+2*NORTH+EAST] & BP_MASK);

        int advancePossible = (board[ofs+EAST] | board[ofs+WEST]) & BP_MASK;
        if ( leftProtectPossible || rightProtectPossible || advancePossible )
            protectPossible = true;
        else
            score -= PASSED_PAWN_VULNERABLE;
    }

    if ( (board [ofs + SOUTHEAST] & BB_MASK) 
        || (board [ofs + SOUTHWEST] & BB_MASK) )
        score += BISHOP_PROTECT_PAWN;

    if ( (board[ofs+SOUTHEAST] & (WN_MASK|WB_MASK|WR_MASK|WQ_MASK|WK_MASK)) &&
         (board[ofs+SOUTHWEST] & (WN_MASK|WB_MASK|WR_MASK|WQ_MASK|WK_MASK)) )
        score += PAWN_FORK;

    if ( isPassedPawn )
    {
        SCORE passedPawnBonus = 0;
        switch ( ybase )
        {
            case OFFSET(2,5):  passedPawnBonus = PASSED_3_FROM_PROM;   break;
            case OFFSET(2,4):  passedPawnBonus = PASSED_2_FROM_PROM;   break;
            case OFFSET(2,3):  passedPawnBonus = PASSED_1_FROM_PROM;   break;
        }

        if ( !protectPossible )
            passedPawnBonus /= 2;

        // look behind the passed pawn for backup rooks

        int bofs, rcount=0;
        for ( bofs=ofs + NORTH; ; bofs += NORTH )
        {
            if ( board[bofs] & (BR_MASK | BQ_MASK) )
            {
                if ( ++rcount > 1 )
                    break;
            }
            else if ( board[bofs] != EMPTY )
                break;
        }

        if ( rcount > 0 )
        {
            if ( rcount > 1 )
                passedPawnBonus += ROOK_BACKS_PASSED_PAWN2;
            else
                passedPawnBonus += ROOK_BACKS_PASSED_PAWN1;
        }

        // look forward for non-pawn pieces that block the pawn's 
        // potential to advance...

        for ( bofs=ofs + SOUTH; board[bofs] != OFFBOARD; bofs += SOUTH )
        {
            if ( board[bofs] != EMPTY )
                passedPawnBonus -= PASSED_PIECE_BLOCK;
        }

        score += passedPawnBonus;
    }
    else
    {
        if ( ybase == OFFSET(2,3) )
            score += BLOCKED_2_FROM_PROM;
    }

    if ( ybase == OFFSET(2,4) )
    {
        if ( board[ofs+2*SOUTH+EAST] & WK_MASK )
        {
            score += CTEK_PAWN1;
            if ( b.inventory[BQ_INDEX] + b.inventory[BR_INDEX] > 0 )
            {
                if ( (board[ofs+SOUTHEAST] & WP_MASK) == 0 )
                {
                    score += CTEK_HOLE;
                    if ( board[ofs+2*EAST] & OFFBOARD )
                        score += CTEK_HOLE-2;
                    else if ( (board[ofs+3*EAST+SOUTH] & WP_MASK) == 0 )
                    {
                        score += CTEK_HOLE;
                        if ( board[ofs+2*EAST] & BQ_MASK )
                            score += CTEK_HOLE_Q;
                    }
                }
            }
        }
        else if ( board[ofs+2*SOUTH+WEST] & WK_MASK )
        {
            if ( b.inventory[BQ_INDEX] + b.inventory[BR_INDEX] > 0 )
            {
                if ( (board[ofs+SOUTHWEST] & WP_MASK) == 0 )
                {
                    score += CTEK_HOLE;
                    if ( board[ofs+2*WEST] & OFFBOARD )
                        score += CTEK_HOLE-2;
                    else if ( (board[ofs+3*WEST+SOUTH] & WP_MASK) == 0 )
                    {
                        score += CTEK_HOLE;
                        if ( board[ofs+2*WEST] & BQ_MASK )
                            score += CTEK_HOLE_Q;
                    }
                }
            }
        }
        else if ( board[ofs+2*SOUTHEAST] & BK_MASK )
            score += CTEK_PAWN2;
        else if ( board[ofs+2*SOUTHWEST] & BK_MASK )
            score += CTEK_PAWN2;
    }

    return score;
}


/*
    $Log: eval.cpp,v $
    Revision 1.4  2006/01/18 19:58:11  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.3  2005/11/23 21:30:29  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:39  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!



         Revision history:
    
    1993 August 30 [Don Cross]
         Changing pointers to references in the interfaces where
         appropriate.
    
    1993 October 19 [Don Cross]
         Added bonuses for being castled and being able to castle.
         Added small bonus for putting opponent in check.
         Added knowledge of definite draws that aren't stalements.
         Added all kinds of cool positional knowledge about
         bishops, knights, and especially pawns.
    
    1993 October 24 [Don Cross]
         Added MaterialEval() which encourages trades when ahead
         in material, and discourages trades when down in material.
    
    1993 October 25 [Don Cross]
         Added castling security knowledge.
    
    1993 October 31 [Don Cross]
         Added knowledge of knight forks.
    
    1993 December 31 [Don Cross]
         Added knowledge of pawn forks.
    
    1994 January 5 [Don Cross]
         Increased the value of pawn forks.
         Improved blocked-bishop heuristics.
         Improved efficiency of split-pawn heuristics.
         Increased value of split (isolated) pawn.
    
    1994 February 3 [Don Cross]
         Tried to improve knight fork heuristics.
    
    1994 February 6 [Don Cross]
         Changing knight and king position heuristics over to
         lookup tables.
    
    1994 February 10 [Don Cross]
         Changing over to using indexed pieces.
    
    1994 February 14 [Don Cross]
         Made castled position heuristics a lot better!
    
    1994 February 17 [Don Cross]
         Moved all piece positional code into bonus functions.
    
    1994 February 18 [Don Cross]
         Changed eval function to not do positional eval if material
         eval is far enough beyond pruning that position doesn't
         matter anyway.
    
    1995 March 31 [Don Cross]
         Adding bonuses for having pawns attack key center squares.
         This will be most important for opening play.
         Also noticed & fixed a little problem with the asymmetry
         in the KnightPosition array.  (I wasn't correcting for
         White's point of view.)
    
    1995 April 16 [Don Cross]
         Added pawn balance code.
         This code corrects for the strategic benefit
         of being the only player with pawns.
         See ComputerChessPlayer::CommonMidgameEval().
    
    1996 July 21 [Don Cross]
         Refining castling heuristics.  Was ignoring the "KingPosition"
         tables when king had not yet moved.  This resulted in less than
         desired net bonus for castling behavior.
    
    1996 July 22 [Don Cross]
         Replaced simple back-rank penalties for bishops with a complete
         position table.
         Fixed bug in BlackKnightFork() and WhiteKnightFork()... they
         were not finding kings except in the first 'if' statement.
         Adding bonus for every square surrounding an enemy king which
         is under attack.
    
    1996 July 23 [Don Cross]
         Added CTEK_PAWN stuff.
    
    1996 July 27 [Don Cross]
         Toned down king position heuristics.
    
    1996 August 28 [Don Cross]
         Decreasing the importance of split pawns, rook-file pawns, and
         doubled pawns.  Chenard seems to be giving away knights too
         easily, and I want to see if this is a fix.
    
    1997 June 10 [Don Cross]
         Adding support for built-in profiler code.
    
    1999 January 5 [Don Cross]
         Fixed indentation and updated coding style.
    
    1999 January 13 [Don Cross]
         Added code to find pins caused by bishops against
         enemy rooks, queens, and kings.
    
    1999 January 24 [Don Cross]
         Checking for pins caused by rooks against enemy king and queen.
         Making bishop pins and rook pins count more against a king
         than a queen.
         Added QueenPosition[] array, an attempt to keep computer's queen
         out of corners, (or force enemy's there).
    
    1999 January 27 [Don Cross]
         Trying to make Chenard more aggressive against enemy king and
         more defensive about own king.  Toward this end, I have added
         AttackWhiteKingPos() and AttackBlackKingPos() functions, which
         attempt to determine whether a square known to be under attack
         puts any real pressure on a nearby king.  This really slows
         down the search, but it may be worth it for strategic strength.
    
    1999 January 28 [Don Cross]
         Noticed that knight-fork bonus was too small: I was using the
         QUEEN_VAL, ROOK_VAL, and KNIGHT_VAL constants directly, even
         though these are scaled by a factor of 1/10 so that they can be
         used for array lookups.  Have removed 'NFORK_UNCERTAINTY' subtracted
         constant, and replaced with a factor of 7 (for effective 0.7 factor)
         as a form of reckoning uncertainty.
         Decreased SAFE_EVAL_PRUNE_MARGIN from 400 to 180.
    
    1999 January 29 [Don Cross]
         Adding some new criteria for passed pawns being good:
         making passed pawns that are not as easily protected by other
         pawns be not so exciting.
         Adding bonuses for getting rook(s) behind a passed pawn.
         Adding penalty for having pieces (of either side) in the
         path of a passed pawn.
    
    1999 February 5 [Don Cross]
         Moved inclusion of king position table value into score before
         deciding whether to do "safe eval prune".  Also increased
         SAFE_EVAL_PRUNE_MARGIN back up to 220.  Both changes are intended
         to decrease the likelihood of messing up the search in some
         oddball position, even though both will decrease average search 
         efficiency.
    
    1999 February 10 [Don Cross]
         Doh!  Realized that the bishop pin stuff was incorrectly 
         coded such that it never had any effect.  This has been fixed.
    
    1999 February 17 [Don Cross]
         Increasing the values of the xxx_ATTACK_KPOS constants.
         Adding calls to Attack...KingPos() functions to motivate
         protection of one's own king.
*/

