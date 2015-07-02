/*===========================================================================

     search.cpp  -  Copyright (C) 1993-2005 by Don Cross

     Contains min-max search algorithms with alpha-beta pruning for chess.
     This is the "brains" of Chenard's computer player.

===========================================================================*/
#include <stdio.h>
#include <string.h>

#include "chess.h"
#include "lrntree.h"
#include "profiler.h"


#define  ESCAPE_CHECK_DEPTH   (gene.v[0])
#define  MAX_CHECK_DEPTH      (gene.v[1])

#define  HASH_HIST_MAX          (gene.v[2])
#define  HASH_HIST_FUNC(h,d)    (gene.v[3])

#define DEBUG_BOARD_CORRUPTION  0


ComputerChessPlayer::ComputerChessPlayer ( ChessUI &ui ):
    ChessPlayer ( ui ),
    minlevel ( 0 ),
    maxlevel ( 3 ),
    level ( 0 ),
    maxCheckDepth ( MAX_CHECK_DEPTH ),
    visited ( 0 ),
    evaluated ( 0 ),
    generated ( 0 ),
    searchBias ( 1 ),
    extendSearchFlag ( false ),
    computerPlayingWhite ( false ),
    expectedScorePrev ( 0 ),
    expectedScoreNow ( 0 ),
    revertTimeLimit ( 0 ),
    searchType ( CCPST_DEPTH_SEARCH ),
    maxNodesEvaluated ( 0 ),
    searchAborted ( false ),
    stopTime ( 0 ),
    timeLimit ( 0 ),
    timeCheckCounter ( 0 ),
    timeCheckLimit ( 100 ),
    prevTime ( 0 ),
    whiteEval ( &ComputerChessPlayer::WhiteMidgameEval ),
    blackEval ( &ComputerChessPlayer::BlackMidgameEval ),
    eachBestPathCount ( 0 ),
    expectedNextBoardHash ( 0 ),
    prevCompletedLevel ( 0 ),
    moveOrder_depth ( 0 ),
    moveOrder_bestPathFlag ( 0 ),
    hitMaxHistory ( false ),
    whiteHist ( new SCORE [4096] ),
    blackHist ( new SCORE [4096] ),
    openingBookSearchEnabled ( true ),
    trainingEnabled ( true ),
    allowResignation ( false ),
    enableMoveDisplayFlag ( true ),
    resignThreshold ( 1200 ),
    firstTimeChooseEval ( true ),
    KingPosTable ( KingPosTableQR ),
    oppTimeInstance ( false ),
    oppTimeEnable ( false ),
    blunderAlertInstance(false)
{
    rootml.num = 0;

    if ( !whiteHist || !blackHist )
        ChessFatal ( "Out of memory creating ComputerChessPlayer" );

    ResetHistoryBuffers();
    predictedOppMove.score = 0;
    predictedOppMove.source = predictedOppMove.dest = 0;
}


ComputerChessPlayer::~ComputerChessPlayer()
{
    delete[] whiteHist;
    whiteHist = 0;

    delete[] blackHist;
    blackHist = 0;
}


/*static*/ void ComputerChessPlayer::LazyInitXposTable()
{
    if (!XposTable)
    {
        // Create a default 1MB transposition table.
        XposTable = new TranspositionTable(1);
        if (!XposTable)
        {
            ChessFatal("Out of memory in ComputerChessPlayer::LazyInitXposTable");
        }
    }
}


static void InsertPrediction (
    ChessBoard  &board,
    BestPath    &path,
    ChessUI     &userInterface )
{
    if ( board.GetCurrentPlyNumber() >= 2 && !board.HasBeenEdited() )
    {
        BestPath pastPath = path;
        ChessBoard pastBoard;
        int stopPly = board.GetCurrentPlyNumber() - 2;
        UnmoveInfo unmove;
        int ply = pastPath.depth;
        int depthIncrease = 2;
        if ( ply + 2 >= MAX_BESTPATH_DEPTH )
        {
            ply = MAX_BESTPATH_DEPTH - 3;
            depthIncrease = 0;
        }

        for ( ply=pastPath.depth; ply>=0; --ply )
            pastPath.m[ply+2] = pastPath.m[ply];

        for ( ply=0; ply<stopPly; ++ply )
        {
            Move pastMove = board.GetPastMove(ply);
            pastBoard.MakeMove (pastMove, unmove);
        }

        pastPath.m[0] = board.GetPastMove(ply++);
        pastPath.m[0].score = pastPath.m[2].score;  // so that correct score is displayed
        pastPath.m[1] = board.GetPastMove(ply);
        pastPath.depth += depthIncrease;

        userInterface.DisplayBestPath ( pastBoard, pastPath );
    }
    else
        userInterface.DisplayBestPath ( board, path );
}


static int NumPiecesOnGivenColor (
    const SQUARE *board,
    SQUARE pieceMask,
    int colorBit  /*white=1, black=0*/ )
{
    // search board for given piece(s)

    int counter = 0;
    for ( int y=OFFSET(2,2); y <= OFFSET(2,9); y += NORTH )
    {
        for ( int x=0; x<8; x++ )
        {
            int ofs = x+y;
            if ( (ofs&1)==colorBit && (board[ofs]&pieceMask) )
                ++counter;
        }
    }

    return counter;
}


static inline int NumPiecesOnWhite (
    const SQUARE *board,
    SQUARE pieceMask )
{
    return NumPiecesOnGivenColor ( board, pieceMask, 1 );
}


static inline int NumPiecesOnBlack (
    const SQUARE *board,
    SQUARE pieceMask )
{
    return NumPiecesOnGivenColor ( board, pieceMask, 0 );
}


void ComputerChessPlayer::ChooseEvalFunctions ( ChessBoard &board )
{
    // Determine from the chess board which side we are.
    // Then try to open the appropriate gene file.
    // If that file exists, load the gene into this player.

    if ( firstTimeChooseEval )
    {
        firstTimeChooseEval = false;
        const char *geneFilename = board.WhiteToMove() ? "white.gen" : "black.gen";
        if ( gene.load(geneFilename) )
        {
            char message [128];
            sprintf ( message, "%s has been loaded.", geneFilename );
            userInterface.NotifyUser ( message );
            SetTrainingEnable ( false );   // don't want to pollute the experience tree
        }
        else
            gene.reset();
    }

    INT16 *inv = board.inventory;

    // If either side has a lone king, and the other
    // has rook(s) and/or queen(s) use endgame eval #1.
    // Also use if lone king vs king and two bishops.

    if ( inv [WP_INDEX] == 0 &&
         inv [WN_INDEX] == 0 &&
         inv [WB_INDEX] == 0 &&
         inv [WR_INDEX] == 0 &&
         inv [WQ_INDEX] == 0 &&
         (inv [BR_INDEX] + inv [BQ_INDEX] > 0 || inv[BB_INDEX] + inv[BN_INDEX] >= 2) )
    {
        whiteEval = &ComputerChessPlayer::EndgameEval1;
        blackEval = &ComputerChessPlayer::EndgameEval1;

        // Figure out which king position table to use, and thus
        // which corners we want to drive enemy king toward...
        KingPosTable = KingPosTableQR;
        if ( inv[BR_INDEX]+inv[BQ_INDEX]==0 && inv[BB_INDEX]>0 )
        {
            if ( NumPiecesOnBlack(board.board,BB_MASK) == 0 )
                KingPosTable = KingPosTableBW;
            else if ( NumPiecesOnWhite(board.board,BB_MASK) == 0 )
                KingPosTable = KingPosTableBB;
        }
    }
    else if ( inv [BP_INDEX] == 0 &&
              inv [BN_INDEX] == 0 &&
              inv [BB_INDEX] == 0 &&
              inv [BR_INDEX] == 0 &&
              inv [BQ_INDEX] == 0 &&
              (inv [WR_INDEX] + inv[WQ_INDEX] > 0 || inv[WB_INDEX] + inv[WN_INDEX] >= 2) )
    {
        whiteEval = &ComputerChessPlayer::EndgameEval1;
        blackEval = &ComputerChessPlayer::EndgameEval1;
        // Figure out which king position table to use, and thus
        // which corners we want to drive enemy king toward...
        // Figure out which king position table to use, and thus
        // which corners we want to drive enemy king toward...
        KingPosTable = KingPosTableQR;
        if ( inv[WR_INDEX]+inv[WQ_INDEX]==0 && inv[WB_INDEX]>0 )
        {
            if ( NumPiecesOnBlack(board.board,WB_MASK) == 0 )
                KingPosTable = KingPosTableBW;
            else if ( NumPiecesOnWhite(board.board,WB_MASK) == 0 )
                KingPosTable = KingPosTableBB;
        }
    }
    else
    {
        // Use the default midgame evaluator...

        whiteEval = &ComputerChessPlayer::WhiteMidgameEval;
        blackEval = &ComputerChessPlayer::BlackMidgameEval;
    }
}


void ComputerChessPlayer::ResetHistoryBuffers()
{
    for ( int i=0; i<4096; ++i )
        whiteHist[i] = blackHist[i] = 0;

    LazyInitXposTable();
    XposTable->reset();
}


void ComputerChessPlayer::setResignThreshold(unsigned _resignThreshold)
{
    if ( _resignThreshold < 400 )
        resignThreshold = 400;
    else if ( _resignThreshold > 20000 )
        resignThreshold = 20000;
    else
        resignThreshold = _resignThreshold;
}



INT32 TimeSearchStarted = 0;
ComputerChessPlayer *ComputerPlayerSearching = 0;


void ComputerChessPlayer::InformResignation()
{
    // The opponent has resigned, so it is time to abort any pondering that may be in progress.
    userInterface.oppTime_abortSearch();
}



bool ComputerChessPlayer::GetMove (
    ChessBoard  &board,
    Move        &bestmove,
    INT32       &timeSpent )
{
    PROFILER_ENTER(PX_SEARCH);
    timeSpent = 0;

    //---------------------------------------------------------------------
    //   Before doing anything else, see if we need to continue
    //   thinking on the opponent's turn.

    if ( oppTimeEnable && !oppTimeInstance && searchType == CCPST_TIMED_SEARCH )
    {
        bool oppTime = userInterface.oppTime_finishThinking ( 
            board,
            timeLimit,
            timeSpent,
            bestmove,
            predictedOppMove );

        if ( oppTime )
        {
            if ( enableMoveDisplayFlag )
                userInterface.DisplayMove ( board, bestmove );
            return true;
        }
    }

    cancelPredictedOpponentMove();

    INT32 timeBefore = TimeSearchStarted = ChessTime();
    ComputerPlayerSearching = this;
    if ( searchType == CCPST_TIMED_SEARCH )
    {
        prevTime = timeBefore;
        stopTime = prevTime + timeLimit;
        timeCheckCounter = 0;
    }

    searchAborted = false;
    hitMaxHistory = false;
    userInterface.ComputerIsThinking ( true, *this );
    ChooseEvalFunctions ( board );
    XposTable->startNewSearch();

    if ( board.WhiteToMove() )
        GetWhiteMove ( board, bestmove );
    else
        GetBlackMove ( board, bestmove );

    INT32 timeAfter = ChessTime();
    timeSpent = timeAfter - timeBefore;
    ComputerPlayerSearching = 0;
    userInterface.ComputerIsThinking ( false, *this );

    if ( enableMoveDisplayFlag )
        userInterface.DisplayMove ( board, bestmove );

    userInterface.ReportComputerStats (
        timeSpent,
        visited,
        evaluated,
        generated,
        level+1,
        visnodes,
        gennodes );

    FindPrevBestPath ( bestmove );
    expectedNextBoardHash = 0;   // will never match any board hash

    static int NextBoardTotal=0, NextBoardHit1=0, NextBoardHit2=0;
    bool havePredictedOppMove = false;
    if ( currentBestPath.depth > 1 )
    {
        ++NextBoardTotal;

        // Keep hash code of expected board position if opponent makes the
        // move we think he will make.  This helps us to know whether we
        // can recycle best path information on our next search.
        UnmoveInfo unmove[2];
        if ( board.isLegal(bestmove) )
        {
            ++NextBoardHit1;
            board.MakeMove ( bestmove, unmove[0] );
            if ( board.isLegal(currentBestPath.m[1]) )
            {
                ++NextBoardHit2;
                havePredictedOppMove = true;
                predictedOppMove = currentBestPath.m[1];
                board.MakeMove ( predictedOppMove, unmove[1] );
                expectedNextBoardHash = board.Hash();
                board.UnmakeMove ( currentBestPath.m[1], unmove[1] );
            }
            board.UnmakeMove ( bestmove, unmove[0] );
        }
    }

    PROFILER_EXIT();

    if ( allowResignation )
    {
        if ( board.WhiteToMove() &&
             bestmove.score <= -resignThreshold )
        {
            return false;   // resign
        }
        else if ( board.BlackToMove() &&
                  bestmove.score >= resignThreshold )
        {
            return false;   // resign
        }
    }

    //----------- thinking on opponent's time -----------------------

    if ( havePredictedOppMove )
    {
        if ( oppTimeEnable && !oppTimeInstance && timeSpent>=100 )
        {
            userInterface.oppTime_startThinking (
                board,
                bestmove,
                predictedOppMove );
        }
    }
    else
    {
        predictedOppMove.source = predictedOppMove.dest = 0;
        predictedOppMove.score = 0;
    }

    return true;   // keep on playing!
}


void ComputerChessPlayer::GetWhiteMove (
    ChessBoard &board,
    Move &bestmove )
{
    computerPlayingWhite = true;
    moveOrder_bestPathFlag = false;
    board.GenWhiteMoves ( rootml, this );
    bestmove = rootml.m[0];   // just in case!
    gennodes[0] = generated = rootml.num;
    visited = evaluated = 0;

    int i;
    for ( i=0; i < NODES_ARRAY_SIZE; i++ )
        visnodes[i] = gennodes[i] = 0;

    for ( i=0; i < 4096; i++ )
    {
        whiteHist[i] /= 2;
        blackHist[i] /= 2;
    }

    if ( rootml.num == 1 )
    {
        // If we are doing a blunder alert analysis,
        // we really need to know the score for the position
        // even if there is only a single move.
        // Otherwise, don't waste time thinking when there is no choice.
        if (!blunderAlert_QueryInstanceFlag())
        {
            // Always fill out some kind of valid best path before leaving.
            currentBestPath.depth = 0;
            currentBestPath.m[0] = rootml.m[0];
            currentBestPath.m[0].score = 0;     // we have no idea how good the move is

            return;   // only one legal move exists, so just do it!
        }
    }

    if ( openingBookSearchEnabled && WhiteOpening ( board, bestmove ) )
    {
        userInterface.ReportSpecial ( "opening" );

        // Always fill out some kind of valid best path before leaving.
        currentBestPath.depth = 0;
        currentBestPath.m[0] = bestmove;

        return;   // We found a groovy opening move
    }

    char buffer [64];
    if ( FindEndgameDatabaseMove (board, bestmove) )
    {
        sprintf ( buffer, "endgame (%6d)", int(bestmove.score) );
        userInterface.ReportSpecial ( buffer );

        if ( bestmove.score >= WON_FOR_WHITE &&
             bestmove.score < WHITE_WINS - WIN_DELAY_PENALTY &&
             !isBackgroundThinker() )
        {
            int plies = (WHITE_WINS - long(bestmove.score)) / WIN_DELAY_PENALTY;
            userInterface.PredictMate (1 + plies/2);
        }

        // Always fill out some kind of valid best path before leaving.
        currentBestPath.depth = 0;
        currentBestPath.m[0] = bestmove;

        return;
    }

    LearnTree tree;
    if ( (searchType == CCPST_TIMED_SEARCH) && trainingEnabled && 
         tree.familiarPosition ( board, bestmove, timeLimit, rootml ) )
    {
        sprintf ( buffer, "experience (%6d)", int(bestmove.score) );
        userInterface.ReportSpecial ( buffer );

        // Always fill out some kind of valid best path before leaving.
        currentBestPath.depth = 0;
        currentBestPath.m[0] = bestmove;

        return;   // We found a move we know is best from a previous (longer) search
    }

    // See if we can recycle best path info from the previous move's search.
    UINT32 hash = board.Hash();
    int startLevel = minlevel;
    if ( hash==expectedNextBoardHash && 
         currentBestPath.depth>=2 && prevCompletedLevel>1 )
    {
        // Strip the top two plies because they are in the past!
        for ( i=2; i <= currentBestPath.depth; i++ )
            currentBestPath.m[i-2] = currentBestPath.m[i];

        currentBestPath.depth -= 2;

        eachBestPathCount = 1;
        eachBestPath[0] = currentBestPath;
        startLevel = prevCompletedLevel - 1;
        rootml.SendToFront ( currentBestPath.m[0] );
        bestmove = rootml.m[0];
    }
    else
    {
        eachBestPathCount = 0;
        currentBestPath.depth = 0;
    }

    nextBestPath[0].depth = 0;
    prevCompletedLevel = 0;
    expectedScorePrev = NEGINF;
    expectedScoreNow  = NEGINF;
    revertTimeLimit = 0;
    const INT32 startThinkTime = ChessTime();
    for ( level=startLevel; !searchAborted && level <= maxlevel; level++ )
    {
        WhiteSearchRoot ( board, bestmove );
        if ( !searchAborted )
            prevCompletedLevel = level;

        if ( bestmove.score >= WON_FOR_WHITE )
            break;   // We have found a forced win!

        if ( bestmove.score <= WON_FOR_BLACK )
            break;   // Time to give up - no deeper search will improve things.

        expectedScorePrev = expectedScoreNow;
    }
    const INT32 actualThinkTime = ChessTime() - startThinkTime;

    if ( bestmove.score >= WON_FOR_WHITE &&
         bestmove.score < WHITE_WINS - WIN_DELAY_PENALTY &&
         !isBackgroundThinker() )
    {
        // If we get here, it means we predict a forced checkmate
        // for White!  We can calculate how many moves it will take
        // using the score.

        int plies = (WHITE_WINS - long(bestmove.score)) / WIN_DELAY_PENALTY;
        userInterface.PredictMate (1 + plies/2);
    }

    if ( revertTimeLimit )
    {
        SetTimeLimit (revertTimeLimit);
        revertTimeLimit = 0;
    }

    if ( searchType==CCPST_TIMED_SEARCH && trainingEnabled )
    {
        tree.rememberPosition ( board, bestmove, actualThinkTime, evaluated, 1, 0 );
    }
}


void ComputerChessPlayer::GetBlackMove (
    ChessBoard &board,
    Move &bestmove )
{
    computerPlayingWhite = false;
    moveOrder_bestPathFlag = false;
    board.GenBlackMoves ( rootml, this ); // Generate sorted list of legal moves
    bestmove = rootml.m[0];   // just in case!
    gennodes[0] = generated = rootml.num;
    visited = evaluated = 0;

    int i;
    for ( i=0; i < NODES_ARRAY_SIZE; i++ )
        visnodes[i] = gennodes[i] = 0;

    for ( i=0; i < 4096; i++ )
    {
        whiteHist[i] /= 2;
        blackHist[i] /= 2;
    }

    if ( rootml.num == 1 )
    {
        // If we are doing a blunder alert analysis,
        // we really need to know the score for the position
        // even if there is only a single move.
        // Otherwise, don't waste time thinking when there is no choice.
        if (!blunderAlert_QueryInstanceFlag())
        {
            // Always fill out some kind of valid best path before leaving.
            currentBestPath.depth = 0;
            currentBestPath.m[0] = rootml.m[0];
            currentBestPath.m[0].score = 0;     // we have no idea how good the move is

            return;   // only one legal move exists, so just do it!
        }
    }

    if ( openingBookSearchEnabled && BlackOpening ( board, bestmove ) )
    {
        userInterface.ReportSpecial ( "opening" );

        // Always fill out some kind of valid best path before leaving.
        currentBestPath.depth = 0;
        currentBestPath.m[0] = bestmove;

        return;   // We found a groovy opening move
    }

    char buffer [64];
    if ( FindEndgameDatabaseMove (board, bestmove) )
    {
        sprintf ( buffer, "endgame (%6d)", int(bestmove.score) );
        userInterface.ReportSpecial ( buffer );
        if ( bestmove.score <= WON_FOR_BLACK &&
             bestmove.score > BLACK_WINS + WIN_DELAY_PENALTY &&
             !isBackgroundThinker() )
        {
            int plies = (long(bestmove.score) - BLACK_WINS) / WIN_DELAY_PENALTY;
            userInterface.PredictMate (1 + plies/2);
        }

        // Always fill out some kind of valid best path before leaving.
        currentBestPath.depth = 0;
        currentBestPath.m[0] = bestmove;

        return;
    }

    LearnTree tree;
    if ( searchType==CCPST_TIMED_SEARCH && trainingEnabled && 
         tree.familiarPosition ( board, bestmove, timeLimit, rootml ) )
    {
        sprintf ( buffer, "experience (%6d)", int(bestmove.score) );
        userInterface.ReportSpecial ( buffer );

        // Always fill out some kind of valid best path before leaving.
        currentBestPath.depth = 0;
        currentBestPath.m[0] = bestmove;

        return;   // We found a move we know is best from a previous (longer) search
    }

    // See if we can recycle best path info from the previous move's search.
    UINT32 hash = board.Hash();
    int startLevel = minlevel;
    if ( hash==expectedNextBoardHash && 
         currentBestPath.depth>=2 && prevCompletedLevel>1 )
    {
        // Strip the top two plies because they are in the past!
        for ( i=2; i <= currentBestPath.depth; i++ )
            currentBestPath.m[i-2] = currentBestPath.m[i];

        currentBestPath.depth -= 2;

        eachBestPathCount = 1;
        eachBestPath[0] = currentBestPath;
        startLevel = prevCompletedLevel - 1;
        rootml.SendToFront ( currentBestPath.m[0] );
        bestmove = rootml.m[0];
    }
    else
    {
        eachBestPathCount = 0;
        currentBestPath.depth = 0;
    }

    nextBestPath[0].depth = 0;
    prevCompletedLevel = 0;
    expectedScorePrev = POSINF;
    expectedScoreNow  = POSINF;
    revertTimeLimit = 0;
    const INT32 startThinkTime = ChessTime();
    for ( level=startLevel; !searchAborted && level <= maxlevel; level++ )
    {
        BlackSearchRoot ( board, bestmove );
        if ( !searchAborted )
            prevCompletedLevel = level;

        if ( bestmove.score <= WON_FOR_BLACK )
            break;

        if ( bestmove.score >= WON_FOR_WHITE )
            break;

        expectedScorePrev = expectedScoreNow;
    }
    const INT32 actualThinkTime = ChessTime() - startThinkTime;

    if ( bestmove.score <= WON_FOR_BLACK &&
         bestmove.score > BLACK_WINS + WIN_DELAY_PENALTY &&
         !isBackgroundThinker() )
    {
        // If we get here, it means we predict a forced checkmate
        // for Black!  We can calculate how many moves it will take
        // using the score.

        int plies = (long(bestmove.score) - BLACK_WINS) / WIN_DELAY_PENALTY;
        userInterface.PredictMate (1 + plies/2);
    }

    if ( revertTimeLimit )
    {
        SetTimeLimit (revertTimeLimit);
        revertTimeLimit = 0;
    }

    if ( searchType==CCPST_TIMED_SEARCH && trainingEnabled )
    {
        tree.rememberPosition ( board, bestmove, actualThinkTime, evaluated, 1, 0 );
    }
}


void ComputerChessPlayer::FindPrevBestPath ( Move m )
{
    // This function tries to find the previous top-level move
    // in the best path table and copy it into currentBestPath.

    for ( int i=0; i < eachBestPathCount; i++ )
    {
        if ( eachBestPath[i].depth > 0 && eachBestPath[i].m[0] == m )
        {
            currentBestPath = eachBestPath[i];
            return;
        }
    }

    currentBestPath.depth = 0;   // keep search from using best path
}


BestPath *ComputerChessPlayer::SaveTLMBestPath ( Move move )
{
    // Try to find move in existing TLM BestPaths...

    int i;
    for ( i=0; i < eachBestPathCount; i++ )
    {
        if ( eachBestPath[i].m[0] == move )
        {
            eachBestPath[i] = nextBestPath[1];
            eachBestPath[i].m[0] = move;
            return & ( eachBestPath[i] );
        }
    }

    // Need to add a new one!

    if ( eachBestPathCount >= MAX_MOVES )
    {
        ChessFatal ( "BestPath top-level-move overflow!" );
        return 0;
    }

    i = eachBestPathCount++;
    eachBestPath[i] = nextBestPath[1];
    eachBestPath[i].m[0] = move;
    return & ( eachBestPath[i] );
}


void ComputerChessPlayer::FoundBestMove (
    Move bestmove,
    int depth )
{
    if ( depth < MAX_BESTPATH_DEPTH-1 )
    {
        // Insert the bestmove found at this depth...

        nextBestPath[depth].m[depth] = bestmove;

#if DEBUG_BEST_PATH
        static long bestPathCounter = 0;
        nextBestPath[depth].counter[depth] = ++bestPathCounter;
#endif

        // Copy best path from layer below to this layer.

        int copyDepth =
            nextBestPath[depth].depth =
            nextBestPath[depth+1].depth;

        if ( copyDepth >= MAX_BESTPATH_DEPTH-1 )
            copyDepth = MAX_BESTPATH_DEPTH-1;

        Move *source = &nextBestPath[depth+1].m[0];
        Move *dest   = &nextBestPath[depth].m[0];

        for ( int d=depth+1; d <= copyDepth; d++ )
        {
            dest[d] = source[d];
#if DEBUG_BEST_PATH
            nextBestPath[depth].counter[d] = nextBestPath[depth+1].counter[d];
#endif
        }
    }
}


SCORE ComputerChessPlayer::WhiteSearchRoot (
    ChessBoard &board,
    Move       &bestmove )
{
#if DEBUG_BOARD_CORRUPTION
    UINT32 bcopy [144];
    memcpy ( bcopy, board.board, sizeof(bcopy) );
#endif

    SCORE   score;
    SCORE   bestscore = NEGINF;
    SCORE   alpha = MIN_WINDOW;   // Best so far for White
    SCORE   beta  = MAX_WINDOW;   // Best so far for Black

    hashPath[0] = board.Hash();

    int  i;
    Move        *move;
    UnmoveInfo   unmove;

    for ( i=0, move=&rootml.m[0];
          !searchAborted && i < rootml.num;
          i++, move++ )
    {
        // If this isn't the first move we are considering, and it
        // is a definite loser, just skip it!

        if ( i>0 && move->score <= WON_FOR_BLACK )
            continue;

        ++visited;
        ++visnodes[0];

        if ( level > 1 )
            userInterface.DisplayCurrentMove ( board, *move, level );

        FindPrevBestPath ( *move );

        if (!this->blunderAlert_QueryInstanceFlag())    // band-aid to prevent thread-safety problem with static variables
        {
            userInterface.DebugPly ( 0, board, *move );
        }

        board.MakeWhiteMove ( *move, unmove, false, false );

        if ( level > 0 )
        {
            score = BlackSearch ( board, 1, alpha, beta, true );
            if ( !searchAborted )
                move->score = score;
        }
        else
        {
            score = move->score = BlackQSearch ( board, 1, alpha, beta, true );
        }
        board.UnmakeWhiteMove ( *move, unmove );

        BestPath *path = 0;
        if ( !searchAborted )
            path = SaveTLMBestPath ( *move );

        if ( searchAborted )
        {
            if ( i > 0 )
            {
                // Invalidate this and all the remaining moves,
                // to keep search bias from picking from them...

                while ( i < rootml.num )
                    rootml.m[i++].score = NEGINF;
            }
        }
        else
        {
            if ( score > bestscore )
            {
                bestmove = *move;
                expectedScoreNow = bestscore = score;
                if ( level > 1 )
                {
                    userInterface.DisplayBestMoveSoFar ( board, bestmove, level );
                    if ( path )
                    {
                        if ( oppTimeInstance )
                            InsertPrediction ( board, *path, userInterface );
                        else
                            userInterface.DisplayBestPath ( board, *path );
                    }
                }
            }
        }

        if ( score > alpha )
            alpha = score;
    }

    rootml.WhiteSort();

    userInterface.DebugExit ( 0, board, bestscore );

#if DEBUG_BOARD_CORRUPTION
    if ( memcmp(bcopy,board.board,sizeof(bcopy)) != 0 )
        ChessFatal ("Board corruption found in WhiteSearchRoot");
#endif

    return bestscore;
}


SCORE ComputerChessPlayer::BlackSearchRoot (
    ChessBoard &board,
    Move       &bestmove )
{
#if DEBUG_BOARD_CORRUPTION
    UINT32 bcopy [144];
    memcpy ( bcopy, board.board, sizeof(bcopy) );
#endif

    SCORE   score;
    SCORE   bestscore = POSINF;
    SCORE   alpha = MIN_WINDOW;   // Best so far for White
    SCORE   beta  = MAX_WINDOW;   // Best so far for Black

    hashPath[0] = board.Hash();

    int  i;
    Move        *move;
    UnmoveInfo   unmove;

    for ( i=0, move=&rootml.m[0];
          !searchAborted && i < rootml.num;
          i++, move++ )
    {
        // If this isn't the first move we are considering, and it
        // is a definite loser, just skip it!

        if ( i>0 && move->score >= WON_FOR_WHITE )
            continue;

        ++visited;
        ++visnodes[0];

        if ( level > 1 )
            userInterface.DisplayCurrentMove ( board, *move, level );

        FindPrevBestPath ( *move );

        if (!this->blunderAlert_QueryInstanceFlag())    // band-aid to prevent thread-safety problem with static variables
        {
            userInterface.DebugPly ( 0, board, *move );
        }

        board.MakeBlackMove ( *move, unmove, false, false );

        if ( level > 0 )
        {
            score = WhiteSearch ( board, 1, alpha, beta, true );
            if ( !searchAborted )
                move->score = score;
        }
        else
        {
            score = move->score = WhiteQSearch ( board, 1, alpha, beta, true );
        }
        board.UnmakeBlackMove ( *move, unmove );

        BestPath *path = 0;
        if ( !searchAborted )
            path = SaveTLMBestPath ( *move );

        if ( searchAborted )
        {
            if ( i > 0 )
            {
                // Invalidate all the remaining moves, to keep search
                // bias from picking from them...

                while ( i < rootml.num )
                    rootml.m[i++].score = POSINF;
            }
        }
        else
        {
            if ( score < bestscore )
            {
                bestmove = *move;
                expectedScoreNow = bestscore = score;
                if ( level > 1 )
                {
                    userInterface.DisplayBestMoveSoFar ( board, bestmove, level );
                    if ( path )
                    {
                        if ( oppTimeInstance )
                            InsertPrediction ( board, *path, userInterface );
                        else
                            userInterface.DisplayBestPath ( board, *path );
                    }
                }
            }
        }

        if ( score < beta )
            beta = score;
    }

    rootml.BlackSort();

    userInterface.DebugExit ( 0, board, bestscore );

#if DEBUG_BOARD_CORRUPTION
    if ( memcmp(bcopy,board.board,sizeof(bcopy)) != 0 )
        ChessFatal ("Board corruption found in BlackSearchRoot");
#endif

    return bestscore;
}


static SCORE AdjustCheckmateScore (
    const TranspositionEntry *xpos,
    int depth )
{
    SCORE score = xpos->bestReply.score;
    if ( score > WON_FOR_WHITE )
    {
        int increasedDepth = depth - int(xpos->future);
        score -= WIN_POSTPONEMENT(increasedDepth);
    }
    else if ( score < WON_FOR_BLACK )
    {
        int increasedDepth = depth - int(xpos->future);
        score += WIN_POSTPONEMENT(increasedDepth);
    }

    return score;
}


SCORE ComputerChessPlayer::WhiteSearch (
    ChessBoard  &board,
    int          depth,
    SCORE        alpha,
    SCORE        beta,
    bool     bestPathFlag )
{
    if ( depth < MAX_BESTPATH_DEPTH )
        nextBestPath[depth].depth = depth - 1;

    if ( CheckTimeLimit() )
        return NEGINF;

    if ( depth < level && depth < MAX_BESTPATH_DEPTH )
    {
        hashPath[depth] = board.Hash();
        const int limit = depth - 2;
        for ( int rd = depth & 1; rd < limit; rd += 2 )
        {
            if ( hashPath[rd] == hashPath[depth] )
            {
                if ( board.NumberOfRepetitions() != 1 )   //NOTE: 0 means board was edited
                    return DRAW;
                else
                    break;
            }
        }
    }

#if DEBUG_BOARD_CORRUPTION
    UINT32 bcopy [144];
    memcpy ( bcopy, board.board, sizeof(bcopy) );
#endif

    int          score;
    int          bestscore = NEGINF;
    int          i;
    MoveList     ml;
    UnmoveInfo   unmove;
    Move        *move;
    Move        *bestMove = 0;


    moveOrder_bestPathFlag = bestPathFlag;
    moveOrder_depth = depth;

    const TranspositionEntry *xpos = XposTable->locateWhiteMove (board);

    if ( xpos )
        moveOrder_xposBestMove = xpos->bestReply;
    else
        moveOrder_xposBestMove.source = 0;  // so it will not match any legal move

    board.GenWhiteMoves ( ml, this );
    generated += ml.num;
    if ( depth < NODES_ARRAY_SIZE )
        gennodes[depth] += ml.num;

    int numReps = 0;
    if ( ml.num == 0 )
    {
        // This is the end of the game!
        bestscore = (board.flags & SF_WCHECK) 
            ? (BLACK_WINS + WIN_POSTPONEMENT(depth)) 
            : DRAW;

        userInterface.DebugExit ( depth, board, bestscore );
        return bestscore;
    }
    else if ( board.IsDefiniteDraw(&numReps) )
    {
        bestscore = DRAW;
        userInterface.DebugExit ( depth, board, bestscore );
        return bestscore;
    }

    // Sometimes we can completely eliminate the search beneath this point!
    // But we make sure to use transposition *only* after checking for
    // draw by repetition.

    if ( xpos 
        && xpos->searchedDepth >= level-depth 
        && numReps < 2
        && ml.IsLegal(xpos->bestReply) )
    {
        if ( xpos->scoreIsInsideWindow() && xpos->compatibleWindow(alpha,beta) )
        {
            if ( depth < MAX_BESTPATH_DEPTH )
                nextBestPath[depth].m[depth] = xpos->bestReply;

            userInterface.DebugExit ( depth, board, xpos->bestReply.score );
            return AdjustCheckmateScore ( xpos, depth );
        }
    }

    for ( i=0, move=&ml.m[0]; i < ml.num; i++, move++ )
    {
        ++visited;
        if ( depth < NODES_ARRAY_SIZE )
            ++visnodes[depth];

        bool nextBestPathFlag =
            bestPathFlag &&
            depth <= currentBestPath.depth &&
            *move == currentBestPath.m[depth];

        if (!this->blunderAlert_QueryInstanceFlag())    // band-aid to prevent thread-safety problem with static variables
        {
            userInterface.DebugPly ( depth, board, *move );
        }

        board.MakeWhiteMove ( *move, unmove, false, false );

        if ( depth < level )
            score = BlackSearch ( board, depth+1, alpha, beta, nextBestPathFlag );
        else
            score = BlackQSearch ( board, depth+1, alpha, beta, nextBestPathFlag );

        board.UnmakeWhiteMove ( *move, unmove );

        if ( score > bestscore )
        {
            bestMove = move;
            bestMove->score = bestscore = score;
            FoundBestMove ( *move, depth );
        }

        if ( score >= beta )
            break;   // PRUNE: Black has better (or at least as good) choices than getting here.

        if ( score > alpha )
            alpha = score;
    }

    if ( bestMove )
    {
        SCORE *h = &whiteHist[bestMove->whiteHash()];
        if ( *h < HASH_HIST_MAX )
            *h += HASH_HIST_FUNC(*h,depth);
        else
            hitMaxHistory = true;

        XposTable->rememberWhiteMove ( board, level, depth, *bestMove, alpha, beta );
    }

    userInterface.DebugExit ( depth, board, bestscore );

#if DEBUG_BOARD_CORRUPTION
    if ( memcmp(bcopy,board.board,sizeof(bcopy)) != 0 )
        ChessFatal ("Board corruption found in WhiteSearch");
#endif

    return bestscore;
}


SCORE ComputerChessPlayer::BlackSearch (
    ChessBoard  &board,
    int          depth,
    SCORE        alpha,
    SCORE        beta,
    bool     bestPathFlag )
{
    if ( depth < MAX_BESTPATH_DEPTH )
        nextBestPath[depth].depth = depth - 1;

    if ( CheckTimeLimit() )
        return POSINF;

    if ( depth < level && depth < MAX_BESTPATH_DEPTH )
    {
        hashPath[depth] = board.Hash();
        const int limit = depth - 2;
        for ( int rd = depth & 1; rd < limit; rd += 2 )
        {
            if ( hashPath[rd] == hashPath[depth] )
            {
                if ( board.NumberOfRepetitions() != 1 )   //NOTE: 0 means board was edited
                    return DRAW;
                else
                    break;
            }
        }
    }

#if DEBUG_BOARD_CORRUPTION
    UINT32 bcopy [144];
    memcpy ( bcopy, board.board, sizeof(bcopy) );
#endif

    int          score;
    int          bestscore = POSINF;
    int          i;
    MoveList     ml;
    UnmoveInfo   unmove;
    Move        *move;
    Move        *bestMove = 0;

    moveOrder_bestPathFlag = bestPathFlag;
    moveOrder_depth = depth;

    const TranspositionEntry *xpos = XposTable->locateBlackMove (board);

    if ( xpos )
        moveOrder_xposBestMove = xpos->bestReply;
    else
        moveOrder_xposBestMove.source = 0;  // so it will not match any legal move

    board.GenBlackMoves ( ml, this );
    generated += ml.num;
    if ( depth < NODES_ARRAY_SIZE )
        gennodes[depth] += ml.num;

    int numReps = 0;
    if ( ml.num == 0 )
    {
        // This is the end of the game!
        bestscore = (board.flags & SF_BCHECK) 
            ? (WHITE_WINS - WIN_POSTPONEMENT(depth)) 
            : DRAW;

        userInterface.DebugExit ( depth, board, bestscore );
        return bestscore;
    }
    else if ( board.IsDefiniteDraw(&numReps) )
    {
        bestscore = DRAW;
        userInterface.DebugExit ( depth, board, bestscore );
        return bestscore;
    }

    // Sometimes we can completely eliminate the search beneath this point!
    // But we make sure to use transposition *only* after checking for
    // draw by repetition.

    if ( xpos 
        && xpos->searchedDepth >= level-depth 
        && numReps < 2
        && ml.IsLegal(xpos->bestReply) )
    {                                   /*||*/
        if ( xpos->scoreIsInsideWindow() && xpos->compatibleWindow(alpha,beta) )
        {
            if ( depth < MAX_BESTPATH_DEPTH )
                nextBestPath[depth].m[depth] = xpos->bestReply;

            userInterface.DebugExit ( depth, board, xpos->bestReply.score );
            return AdjustCheckmateScore ( xpos, depth );
        }
    }

    for ( i=0, move=&ml.m[0]; i < ml.num; i++, move++ )
    {
        ++visited;
        if ( depth < NODES_ARRAY_SIZE )
            ++visnodes[depth];

        bool nextBestPathFlag =
            bestPathFlag &&
            depth <= currentBestPath.depth &&
            *move == currentBestPath.m[depth];

        if (!this->blunderAlert_QueryInstanceFlag())    // band-aid to prevent thread-safety problem with static variables
        {
            userInterface.DebugPly ( depth, board, *move );
        }

        board.MakeBlackMove ( *move, unmove, false, false );

        if ( depth < level )
            score = WhiteSearch ( board, depth+1, alpha, beta, nextBestPathFlag );
        else
            score = WhiteQSearch ( board, depth+1, alpha, beta, nextBestPathFlag );

        board.UnmakeBlackMove ( *move, unmove );

        if ( score < bestscore )
        {
            bestMove  = move;
            bestMove->score = bestscore = score;
            FoundBestMove ( *move, depth );
        }

        if ( score <= alpha )
            break;   // PRUNE: White has better (or at least as good) choices than getting here.

        if ( score < beta )
            beta = score;
    }

    if ( bestMove )
    {
        SCORE *h = &blackHist [bestMove->blackHash()];
        if ( *h < HASH_HIST_MAX )
            *h += HASH_HIST_FUNC(*h,depth);
        else
            hitMaxHistory = true;

        XposTable->rememberBlackMove ( board, level, depth, *bestMove, alpha, beta );
    }

    userInterface.DebugExit ( depth, board, bestscore );

#if DEBUG_BOARD_CORRUPTION
    if ( memcmp(bcopy,board.board,sizeof(bcopy)) != 0 )
        ChessFatal ("Board corruption found in BlackSearch");
#endif

    return bestscore;
}


SCORE ComputerChessPlayer::WhiteQSearch (
    ChessBoard  &board,
    int          depth,
    SCORE        alpha,
    SCORE        beta,
    bool     bestPathFlag )
{
    if ( depth < MAX_BESTPATH_DEPTH )
        nextBestPath[depth].depth = depth - 1;

    if ( CheckTimeLimit() )
        return NEGINF;

#if DEBUG_BOARD_CORRUPTION
    UINT32 bcopy [144];
    memcpy ( bcopy, board.board, sizeof(bcopy) );
#endif

    SCORE       score;
    SCORE       bestscore = (this->*whiteEval) ( board, depth, alpha, beta );
    MoveList    ml;
    UnmoveInfo  unmove;
    int         i;
    Move       *move;

    const bool escapeCheck =
        (board.flags & SF_WCHECK)
        && depth <= level + ESCAPE_CHECK_DEPTH;

    if ( bestscore < beta || escapeCheck )
    {
        moveOrder_bestPathFlag = bestPathFlag;
        moveOrder_depth = depth;

        if ( escapeCheck )
            board.GenWhiteMoves ( ml, this );
        else if ( depth <= level + maxCheckDepth )
            GenWhiteCapturesAndChecks ( board, ml );
        else
            board.GenWhiteCaptures ( ml, this );

        if ( depth < NODES_ARRAY_SIZE )
            gennodes[depth] += ml.num;

        generated += ml.num;
        for ( i=0, move=&ml.m[0]; i < ml.num; i++, move++ )
        {
            ++visited;
            if ( depth < NODES_ARRAY_SIZE )
                ++visnodes[depth];

            bool nextBestPathFlag =
                bestPathFlag &&
                depth <= currentBestPath.depth &&
                *move == currentBestPath.m[depth];

            if (!this->blunderAlert_QueryInstanceFlag())    // band-aid to prevent thread-safety problem with static variables
            {
                userInterface.DebugPly ( depth, board, *move );
            }

            board.MakeWhiteMove ( *move, unmove, false, false );

            if ( (board.flags & SF_BCHECK) || escapeCheck )
                score = BlackSearch ( board, depth+1, alpha, beta, nextBestPathFlag );
            else
                score = BlackQSearch ( board, depth+1, alpha, beta, nextBestPathFlag );

            board.UnmakeWhiteMove ( *move, unmove );

            if ( score > bestscore )
            {
                move->score = bestscore = score;
                FoundBestMove ( *move, depth );
            }

            if ( score >= beta )
                break;      // PRUNE

            if ( score > alpha )
                alpha = score;
        }
    }

    userInterface.DebugExit ( depth, board, bestscore );

#if DEBUG_BOARD_CORRUPTION
    if ( memcmp(bcopy,board.board,sizeof(bcopy)) != 0 )
        ChessFatal ("Board corruption found in WhiteQSearch");
#endif

    return bestscore;
}


SCORE ComputerChessPlayer::BlackQSearch (
    ChessBoard  &board,
    int          depth,
    SCORE        alpha,
    SCORE        beta,
    bool     bestPathFlag )
{
    if ( depth < MAX_BESTPATH_DEPTH )
        nextBestPath[depth].depth = depth - 1;

    if ( CheckTimeLimit() )
        return POSINF;

#if DEBUG_BOARD_CORRUPTION
    UINT32 bcopy [144];
    memcpy ( bcopy, board.board, sizeof(bcopy) );
#endif

    SCORE       score;
    SCORE       bestscore = (this->*blackEval) ( board, depth, alpha, beta );
    MoveList    ml;
    UnmoveInfo  unmove;
    int         i;
    Move       *move;

    const bool escapeCheck =
        (board.flags & SF_BCHECK)
        && depth <= level + ESCAPE_CHECK_DEPTH;

    if ( bestscore > alpha || escapeCheck )
    {
        moveOrder_bestPathFlag = bestPathFlag;
        moveOrder_depth = depth;

        if ( escapeCheck )
            board.GenBlackMoves ( ml, this );
        else if ( depth <= level + maxCheckDepth )
            GenBlackCapturesAndChecks ( board, ml );
        else
            board.GenBlackCaptures ( ml, this );

        if ( depth < NODES_ARRAY_SIZE )
            gennodes[depth] += ml.num;

        generated += ml.num;
        for ( i=0, move=&ml.m[0]; i < ml.num; i++, move++ )
        {
            ++visited;
            if ( depth < NODES_ARRAY_SIZE )
                ++visnodes[depth];

            bool nextBestPathFlag =
                bestPathFlag &&
                depth <= currentBestPath.depth &&
                *move == currentBestPath.m[depth];

            if (!this->blunderAlert_QueryInstanceFlag())    // band-aid to prevent thread-safety problem with static variables
            {
                userInterface.DebugPly ( depth, board, *move );
            }

            board.MakeBlackMove ( *move, unmove, false, false );

            if ( (board.flags & SF_WCHECK) || escapeCheck )
                score = WhiteSearch ( board, depth+1, alpha, beta, nextBestPathFlag );
            else
                score = WhiteQSearch ( board, depth+1, alpha, beta, nextBestPathFlag );

            board.UnmakeBlackMove ( *move, unmove );

            if ( score < bestscore )
            {
                move->score = bestscore = score;
                FoundBestMove ( *move, depth );
            }

            if ( score <= alpha )
                break;      // PRUNE

            if ( score < beta )
                beta = score;
        }
    }

    userInterface.DebugExit ( depth, board, bestscore );

#if DEBUG_BOARD_CORRUPTION
    if ( memcmp(bcopy,board.board,sizeof(bcopy)) != 0 )
        ChessFatal ("Board corruption found in BlackQSearch");
#endif

    return bestscore;
}


void ComputerChessPlayer::GenWhiteCapturesAndChecks (
    ChessBoard &board,
    MoveList &ml )
{
    PROFILER_ENTER(PX_GENCAPS);
    MoveList all;
    board.GenWhiteMoves ( all, this );

    // Keep only moves which cause check or are captures in 'ml'
    ml.num = 0;
    for ( int i=0; i<all.num; i++ )
    {
        Move move = all.m[i];
        if ( move.source & CAUSES_CHECK_BIT )
            ml.AddMove ( move );
        else if ( move.dest > OFFSET(9,9) )
        {
            int special = move.dest & SPECIAL_MOVE_MASK;
            if ( special != SPECIAL_MOVE_KCASTLE &&
                 special != SPECIAL_MOVE_QCASTLE )
                ml.AddMove ( move );
        }
        else if ( board.board[move.dest] != EMPTY )
            ml.AddMove ( move );
    }
    PROFILER_EXIT();
}


void ComputerChessPlayer::GenBlackCapturesAndChecks (
    ChessBoard &board,
    MoveList &ml )
{
    PROFILER_ENTER(PX_GENCAPS);
    MoveList all;
    board.GenBlackMoves ( all, this );

    // Keep only moves which cause check or are captures in 'ml'
    ml.num = 0;
    for ( int i=0; i<all.num; i++ )
    {
        Move move = all.m[i];
        if ( move.source & CAUSES_CHECK_BIT )
            ml.AddMove ( move );
        else if ( move.dest > OFFSET(9,9) )
        {
            int special = move.dest & SPECIAL_MOVE_MASK;
            if ( special != SPECIAL_MOVE_KCASTLE &&
                 special != SPECIAL_MOVE_QCASTLE )
                ml.AddMove ( move );
        }
        else if ( board.board[move.dest] != EMPTY )
            ml.AddMove ( move );
    }
    PROFILER_EXIT();
}


void ComputerChessPlayer::SetMinSearchDepth (int _minSearchDepth)
{
    expectedNextBoardHash = 0;  // prevent any possible recycling of best path info
    minlevel = _minSearchDepth;
}


void ComputerChessPlayer::SetSearchDepth ( int NewSearchDepth )
{
    maxlevel = NewSearchDepth;
    searchType = CCPST_DEPTH_SEARCH;
    searchAborted = false;
}


void ComputerChessPlayer::SetTimeLimit ( INT32 hundredthsOfSeconds )
{
    maxlevel = NODES_ARRAY_SIZE/4 - 1;
    searchType = CCPST_TIMED_SEARCH;
    searchAborted = false;
    timeLimit = (hundredthsOfSeconds >= 10) ? hundredthsOfSeconds : 10; // enforce minimum reasonable think time: 0.1 seconds.
    stopTime = ChessTime() + timeLimit;
}


void ComputerChessPlayer::SetMaxNodesEvaluated ( UINT32 _maxNodesEvaluated )
{
    if ( _maxNodesEvaluated < 100 )
        ChessFatal ( "Max nodes evaluated was too small" );

    maxlevel = NODES_ARRAY_SIZE/4 - 1;
    searchType = CCPST_MAXEVAL_SEARCH;
    searchAborted = false;
    maxNodesEvaluated = _maxNodesEvaluated;
}


void ComputerChessPlayer::AbortSearch()
{
    searchAborted = true;
}


bool ComputerChessPlayer::CheckTimeLimit()
{
    if ( searchAborted )
        return true;

    if ( searchType == CCPST_MAXEVAL_SEARCH )
    {
        if ( evaluated >= maxNodesEvaluated )
            return searchAborted = true;
    }
    if ( searchType == CCPST_TIMED_SEARCH )
    {
        if ( ++timeCheckCounter >= timeCheckLimit )
        {
            timeCheckCounter = 0;
            INT32 now = ChessTime();

            if ( now >= stopTime )
            {
                if ( extendSearchFlag )
                {
                    // See if the expected score got "worse" suddenly...
                    INT32 deltaScore = INT32(expectedScoreNow) - INT32(expectedScorePrev);

                    if ( computerPlayingWhite )
                        deltaScore = -deltaScore;

                    if ( deltaScore > 60 )
                    {
                        // Things are worse than we thought... cancel timed
                        // search and finish with fixed depth search instead.

                        revertTimeLimit = timeLimit;  // will restore later
                        SetSearchDepth ( level );
                        return false;
                    }
                }

                if ( now - prevTime > 10 )
                    timeCheckLimit /= 2;

                return searchAborted = true;
            }
            else if ( now - prevTime < 5 )   // less than 0.05 sec elapsed?
                timeCheckLimit += 100;

            prevTime = now;
        }
    }

    return false;
}


void ComputerChessPlayer::SetSearchBias ( SCORE newSearchBias )
{
    if ( newSearchBias >= 0 )
        searchBias = newSearchBias;
    else
        ChessFatal ( "Negative search bias not allowed" );
}


/*
    $Log: search.cpp,v $
    Revision 1.12  2009/09/16 16:49:34  Don.Cross
    Added support for the WinBoard/xboard "memory" command to set hash table sizes.

    Revision 1.11  2008/11/22 16:27:32  Don.Cross
    Massive overhaul to xchenard: now supports pondering.
    This required very complex changes, and will need a lot of testing before release to the public.
    Pondering support made it trivial to implement the "hint" command, so I added that too.
    Pondering required careful attention to every possible command we could receive from xboard,
    so now I have started explicitly sending back errors for any command we don't understand.
    I finally figured out how to get rid of the annoying /WP64 warning from the Microsoft C++ compiler.

    Revision 1.10  2008/11/12 17:44:44  Don.Cross
    Replaced ComputerChessPlayer::WhiteDbEndgame and ComputerChessPlayer::BlackDbEndgame
    with a single method ComputerChessPlayer::FindEndgameDatabaseMove.
    Replaced complicated ad hoc logic for captures and promotions changing the situation in endgame searches
    with generic solution that uses feedback from prior endgame database calculations.
    This allows us to accurately calculate optimal moves and accurately report how long it will take to
    checkmate the opponent.  The extra file I/O required in the search does not appear to slow things down too badly.

    Revision 1.9  2008/11/08 16:54:39  Don.Cross
    Fixed a bug in WinChen.exe:  Pondering continued even when the opponent resigned.
    Now we abort the pondering search when informed of an opponent's resignation.

    Revision 1.8  2006/03/19 20:27:12  dcross
    Still trying to find bug that causes Chenard to occasionally delay a long time when first using experience tree.
    All I have found so far is what looked like a negative number being returned by ChessRandom,
    but I cannot reproduce it.  So I just stuck in ChessFatal calls to see if it ever happens again.
    Also found a case where we open the experience tree twice by creating a redundant LearnTree object.
    I removed the unnecessary redundancy, thinking maybe trying to open the file twice is causing some
    kind of delay.  So far this is just shooting in the dark!

    Revision 1.7  2006/02/19 23:25:06  dcross
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

    Revision 1.6  2006/01/25 19:09:13  dcross
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

    Revision 1.5  2006/01/18 19:58:13  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.4  2005/11/23 22:52:55  dcross
    Ported stdio version of Chenard to SUSE Linux:
    1. Needed #include <string.h> in chenga.cpp.
    2. Needed temporary Move object in InsertPrediction in search.cpp, because function takes Move &.
    3. Have to turn SUPPORT_LINUX_INTERNET off for the time being, because I lost lichess.h, dang it!

    Revision 1.3  2005/11/23 21:30:31  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:43  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!


         Revision history:

    1993 August 30 [Don Cross]
         Changing pointers to references in the interfaces where
         appropriate.

    1993 October 16 [Don Cross]
         Fixed bug where I had a MakeBlackMove() paired with
         UnmakeWhiteMove().

    1993 October 18 [Don Cross]
         Added the 'searchBias' stuff.  This is an attempt to allow
         some randomization of move selection.

    1993 October 23 [Don Cross]
         Implemented timed search.
         Added check for draw by repetition of moves:
         {W+, B+, W-, B-} plies repeated 3 times.

    1993 October 24 [Don Cross]
         Added call to ChessBoard::IsDefiniteDraw() to WhiteSearch() and
         BlackSearch() to make sure that draws by repetition are found
         anywhere in the search.

         Created a MoveList::AddMove(Move) function, and changed the
         pg.AddMove() line of code in the searchBias conditional to
         use it instead.  The old way was clobbering the bestmove.score,
         so searches were continuing even after a forced win was found.

    1993 October 25 [Don Cross]
         Making the ComputerChessPlayer::CheckTimeLimit() member function
         adaptive to the processor speed.  It does this by increasing
         the timeCheckLimit until successive calls to ChessTime() always
         report a difference of at least 0.1 seconds.

    1994 January 15 [Don Cross]
         Added ComputerChessPlayer::AbortSearch().
         This required a little reworking of logic in the CheckTimeLimit
         member function too.

    1994 January 22 [Don Cross]
         Adding support for variable eval functions.

    1994 February 2 [Don Cross]
         Split off from search.cpp to add BestPath stuff.

    1994 February 7 [Don Cross]
         Improving search behavior for time limits, and preparing
         to add search windows.

    1994 February 14 [Don Cross]
         Made quiescence search call regular search when a side is
         in check.

    1994 February 24 [Don Cross]
         Added "killer history" idea.

    1994 March 1 [Don Cross]
         Added hash function and code to help find repeated board positions.

    1994 March 2 [Don Cross]
         Adding conditional code to dump search to a text file.
         Need to tell the compiler to define DUMP_SEARCH to do this.

    1996 July 27 [Don Cross]
         Adding experiment with looking at moves which cause check in the
         quiescence search.

    1996 August 4 [Don Cross]
         Adding code to recycle best path from previous search so that
         we can start at a deeper search.

    1996 August 8 [Don Cross]
         Adding "obvious move" code to shorten search when one move is
         much better than all others after a certain fraction of the
         total search time.

    1996 August 23 [Don Cross]
         Replacing old memory-based learning tree with disk-based
         class LearnTree.

    1996 August 26 [Don Cross]
         Extending quiescence search when own side is in check to a
         full width search at that depth, with a cutoff extended
         depth of 'ESCAPE_CHECK_DEPTH' to avoid infinite recursion
         from perpetual checks.

         Bugs in extended quiescence search which cause infinite recursion,
         which in turn uncovered bugs with checking array indices in best
         path stuff.  Fixed 'em!

    1999 January 21 [Don Cross]
         Adding ability for ComputerChessPlayer to extend a timed search
         when the expected min-max score gets unexpectedly worse from
         one level to the next.

    1999 January 24 [Don Cross]
         Extended search was setting maxlevel to level+1 when delta-score
         was greater than 120.  This turned out to cause too extreme a
         variation in think time.  Now it sets to level always, causing
         the existing search depth to be finished in hopes of finding a
         better move.
         Made expectedScoreNow be updated in WhiteSearchRoot and BlackSearchRoot
         after each top-level branch search is completed, instead of in
         GetBlackMove and GetWhiteMove after each level is completed.
         Before, two sibling searhes (same full-width depth but differing
         moves) could set expectedSearchNow and expectedSearchPrev to
         similar values, even though both were a negative surprise compared
         to the previous search.  This would cause the search to not be
         extended when it really should have been.

    1999 January 26 [Don Cross]
         Made tactical benchmark score 20% faster using transposition tables!

    1999 January 27 [Don Cross]
         Made search even faster by careful short-circuiting of minmax
         when node retrieved from transposition table meets certain
         window criteria.
         For some reason, the default search bias for a ComputerChessPlayer
         object was still zero instead of one.  This has been fixed.
         Interestingly, this improved the benchmark score further, leaving
         me to believe that a lot of my previous optimzation attempts were
         possibly skewed.  (Need to re-try various combinations of parameters
         to see if Chenard has been mis-tuned!)

    1999 January 29 [Don Cross]
         Added AdjustCheckmateScore().  This is called whenever the
         transposition table suggests a move to short-circuit recursion
         and that move's score indicates a forced mate.
         I realized that the win-postponement score modifier would
         not always work to ensure that checkmates are executed as quickly
         as possible without it.
         This function modifies the score to correct for the depth at which
         the move was originally found with respect to the depth that it was
         found again by transposition.

    1999 February 5 [Don Cross]
         Modified the search's use of transposition tables.
         Now we do not short-circuit the search with transposition
         when there is any indication that a repeated position exists
         on the board.  This ensures that the search will find forcible
         draws by repetition.

    1999 February 11 [Don Cross]
         Adding support for "search repetition shortcut", which is an attempt
         to (a) speed up the search, and more importantly (b) to expedite
         finding/avoiding forced draws during the search.  The idea is that
         whenever any board position occurs more than once in a given search
         path, that the position should be scored as an immediate draw.

    1999 February 14 [Don Cross]
         I have reason to believe that occasional Chenard crashes might be
         due to the "pretty good move" randomizer code passing 0 to
         ChessRandom().  I am adding code to make sure that this never
         happens.

    1999 April 7 [Don Cross]
         Adding support for loading gene(s) from disk for the computer
         player(s).  The files 'black.gen' and 'white.gen' are loaded if
         either or both exist into the respective computer player(s).
         If either file does not exist, the default gene is used, just
         as always before.

    1999 July 21 [Don Cross]
         Changing tolerance for extended search from 30 to 60.

    2001 January 3 [Don Cross]
         Now use endgame eval when lone king confronted by bishops+knights>=2.
         (Before, only rook(s) and/or queen(s) invoked endgame eval.)

    2001 January 10 [Don Cross]
         Wow!  Fixed bug in search code that must have been there a long time.
         The bestpath depth needed to be chopped before any possibility
         of the function returning, otherwise bogus moves could end up
         being copied into later searches.  This only showed up during
         tree trainer, and I don't know why, other than the 90-second think
         times.

    2001 January 13 [Don Cross]
         Adding support for thinking on opponent's turn.
         This will be handled by the ChessUI abstract class, because
         some UIs will support multi-threading and some won't, and
         the ones that do will do so in OS-specific ways.

    2001 January 17 [Don Cross]
         In InsertPrediction(), needed to copy score from actual search
         to top move so that it gets displayed properly.
         Disabled mate-in-n prediction to UI for opp time thinkers.

*/

