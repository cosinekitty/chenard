/*==========================================================================

      uistdio.cpp   -   Copyright (C) 1993-2005 by Don Cross

      Contains the stdio.h portable chess user interface class
      (ChessUI_stdio).

==========================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chess.h"
#include "uistdio.h"
#include "lrntree.h"

#if SUPPORT_LINUX_INTERNET
    #include "lichess.h"
#endif

const char * LogFilename = "gamelog.txt";

const char *WhitePlayerString = "?";
const char *BlackPlayerString = "?";

const char *GetWhitePlayerString()  // used by SavePortableGameNotation()
{
    return WhitePlayerString;
}

const char *GetBlackPlayerString() // used by SavePortableGameNotation()
{
    return BlackPlayerString;
}


ChessUI_stdio::ChessUI_stdio():
    ChessUI(),
    whitePlayerType ( PT_UNKNOWN ),
    blackPlayerType ( PT_UNKNOWN ),
    showScores ( false ),
    rotateBoard ( false ),
    beepFlag ( false ),
    numPlayersCreated ( 0 ),
    whitePlayer ( 0 ),
    blackPlayer ( 0 ),
    boardDisplayType ( BDT_STANDARD )
{
    remove ( LogFilename );
}


ChessUI_stdio::~ChessUI_stdio()
{
}


ChessPlayer *ChessUI_stdio::CreatePlayer ( ChessSide side )
{
    const char *sideName = NULL;
    const char **PlayerString = NULL;

    switch ( side )
    {
        case SIDE_WHITE:
            sideName = "White";
            PlayerString = &WhitePlayerString;
            break;

        case SIDE_BLACK:
            sideName = "Black";
            PlayerString = &BlackPlayerString;
            break;

        default:
            ChessFatal ( "Invalid call to ChessUI_stdio::CreatePlayer()" );
            break;
    }

#if SUPPORT_LINUX_INTERNET
    printf ( "Should %s be played by Human, Computer, or Internet (h,c,i)? ", sideName );
#else
    printf ( "Should %s be played by Human or Computer (h,c)? ", sideName );
#endif

    fflush(stdout);

    ChessPlayer *NewPlayer = 0;
    char UserString[80];

    while ( fgets ( UserString, sizeof(UserString), stdin ) )
    {
        if ( UserString[0] == 'h' || UserString[0] == 'H' )
        {
            if ( side == SIDE_WHITE )
                whitePlayerType = PT_HUMAN;
            else
                blackPlayerType = PT_HUMAN;

            NewPlayer = new HumanChessPlayer ( *this );
            *PlayerString = "Human";
            break;
        }
        else if ( UserString[0] == 'c' || UserString[0] == 'C' )
        {
            if ( side == SIDE_WHITE )
                whitePlayerType = PT_COMPUTER;
            else
                blackPlayerType = PT_COMPUTER;

            ComputerChessPlayer *GoodGuy = new ComputerChessPlayer ( *this );
            NewPlayer = GoodGuy;
            if ( GoodGuy )
            {
                *PlayerString = "Computer";
                printf ( "Enter think time in seconds [10]: " );
                fflush(stdout);
                UserString[0] = '\0';
                if (!fgets (UserString, sizeof(UserString), stdin)) {
                    // I added this to get around a compiler warning in Linux for not checking return value of fgets().
                    strcpy (UserString, "10");      // apply default if for some reason we can't read line from stdin
                }

                if ( UserString[0] == 'p' )
                {
                    int PliesDeep = 0;
                    if ( sscanf ( UserString+1, "%d", &PliesDeep ) != 1 )
                    {
                        PliesDeep = 3;
                    }
                    GoodGuy->SetSearchDepth ( PliesDeep );
                    GoodGuy->SetSearchBias ( 1 );
                }
                else
                {
                    double ThinkTime = 0;
                    if ( !(sscanf ( UserString, "%lf", &ThinkTime ) == 1 &&
                           ThinkTime >= 0.1) )
                    {
                        ThinkTime = 10;
                    }
                    GoodGuy->SetTimeLimit ( INT32(ThinkTime * 100) );
                    GoodGuy->SetSearchBias ( 1 );
                    GoodGuy->setExtendedSearchFlag ( true );
                }
            }
            break;
        }
#if SUPPORT_LINUX_INTERNET
        else if ( UserString[0] == 'i' || UserString[0] == 'I' )
        {
            *PlayerString = "Remote Player";
            if ( side == SIDE_WHITE )
                whitePlayerType = PT_INTERNET;
            else
                blackPlayerType = PT_INTERNET;

            InternetChessPlayer *icp = new InternetChessPlayer (*this);
            NewPlayer = icp;

            int result = icp->serverConnect (side);
            if ( result )
                break;
            else
                ChessFatal ( "Error connecting to remote client!" );
        }
        else
        {
            printf ( "\n??? Please enter 'h', 'c', or 'i': " );
        }
#else
        else
        {
            printf ( "\n??? Please enter 'h' or 'c': " );
        }
#endif  // SUPPORT_LINUX_INTERNET
    }

    if ( !NewPlayer )
        ChessFatal ( "Failure to create a ChessPlayer object!" );

    if ( ++numPlayersCreated == 2 )
    {
        if ( whitePlayerType!=PT_HUMAN && blackPlayerType!=PT_HUMAN )
            showScores = true;
    }

    if ( side == SIDE_WHITE )
        whitePlayer = NewPlayer;
    else
        blackPlayer = NewPlayer;

    return NewPlayer;
}


static void TrimString ( char *s )
{
    while ( *s )
    {
        if ( *s == '\r' || *s == '\n' )
            *s = '\0';
        else
            ++s;
    }
}


void ChessUI_stdio::EnableCombatMode()
{
    rotateBoard = true;
    beepFlag = true;
    showScores = true;
    boardDisplayType = BDT_LARGE_COLOR;

    if ( whitePlayerType == PT_HUMAN )
        ((HumanChessPlayer *)whitePlayer)->setAutoSingular (true);

    if ( blackPlayerType == PT_HUMAN )
        ((HumanChessPlayer *)blackPlayer)->setAutoSingular (true);
}


bool ChessUI_stdio::ReadMove (
    ChessBoard  &board,
    int         &source,
    int         &dest,
    SQUARE      &promIndex )
{
    promIndex = 0;      // indicate either (a) no promotion, or (b) desire callback for asking for promotion

    if ( beepFlag )
    {
        fputc ( 7, stderr );
        fflush (stderr);
    }

    char s[200];
    char Prompt[80];
    int newTimeLimit = 0;
    Move move;      // ignored

    sprintf ( Prompt, "%s move> ", board.WhiteToMove() ? "White" : "Black" );
    printf ( "%s", Prompt );

    while ( fgets ( s, sizeof(s), stdin ) )
    {
        char temp[200];

        TrimString(s);

        if ( ParseFancyMove (s, board, source, dest, promIndex, move) )
        {
            return true;
        }
        else if (0==strcmp(s,"quit") || 0==strcmp(s,"exit"))    // forces an immediate draw
        {
            LearnTree tree;
            tree.learnFromGame ( board, SIDE_NEITHER );
            return false;
        }
        else if ( strcmp ( s, "resign" ) == 0 )   // important for teaching
        {
            ChessSide winner = board.WhiteToMove() ? SIDE_BLACK : SIDE_WHITE;
            LearnTree tree;
            tree.learnFromGame ( board, winner );
            return false;
        }
        else if ( strcmp ( s, "iwin" ) == 0 )   // important for teaching
        {
            ChessSide winner = board.WhiteToMove() ? SIDE_WHITE : SIDE_BLACK;
            LearnTree tree;
            tree.learnFromGame ( board, winner );
            return false;
        }
        else if ( strcmp ( s, "clear" ) == 0 )
        {
            board.ClearEverythingButKings();
            DrawBoard ( board );
        }
        else if ( strcmp ( s, "new" ) == 0 )
        {
            board.Init();
            DrawBoard ( board );
        }
        else if ( strcmp ( s, "beep" ) == 0 )
        {
            beepFlag = !beepFlag;
            printf ( ">>> Beep is %s.\n", beepFlag ? "ON" : "OFF" );
        }
        else if ( strcmp ( s, "rotate" ) == 0 )
        {
            rotateBoard = !rotateBoard;
            DrawBoard ( board );
        }
        else if ( sscanf ( s, "load %s", temp ) == 1 )
        {
            bool imWhite = board.WhiteToMove();
            if ( LoadGame(board,temp) )
            {
                if ( board.WhiteToMove() != imWhite )
                {
                    // Make a NULL move to get things rolling.
                    // (kind of like "pass")

                    source = dest = 0;
                }
                printf ( "Successfully loaded game from file '%s'\n", temp );
                DrawBoard ( board );
                return true;
            }
            else
            {
                fprintf ( stderr,
                    "Error opening file '%s' for read!\x07\n",
                    temp );
            }
        }
        else if ( sscanf ( s, "save %s", temp ) == 1 )
        {
            if ( SaveGame ( board, temp ) )
            {
                printf ( "Game successfully saved to file '%s'\n", temp );
            }
            else
            {
                fprintf ( stderr,
                    "Error opening file '%s' for write!\x07\n",
                    temp );
            }
        }
        else if ( strcmp ( s, "scores" ) == 0 )
        {
            showScores = !showScores;
            printf ( "Display of min-max scores and best path info now %s.\n",
                (showScores ? "ENABLED" : "DISABLED") );
        }
        else if ( strcmp ( s, "undo" ) == 0 )
        {
            if ( Global_MoveUndoPath.undo(board) )
                DrawBoard(board);
        }
        else if ( strcmp ( s, "redo" ) == 0 )
        {
            if ( Global_MoveUndoPath.redo(board) )
                DrawBoard(board);
        }
        else if ( strcmp ( s, "combat" ) == 0 )
        {
            EnableCombatMode();
            DrawBoard (board);
            printf ( "READY FOR COMBAT!\n" );
        }
        else if ( sscanf ( s, "time %d", &newTimeLimit ) == 1 )
        {
            if ( newTimeLimit > 0 )
            {
                if ( whitePlayerType==PT_COMPUTER && whitePlayer )
                {
                    ComputerChessPlayer *cp =
                        (ComputerChessPlayer *)whitePlayer;
                    cp->SetTimeLimit ( INT32(100) * INT32(newTimeLimit) );
                    printf ( "Set White's search time limit to %d seconds\n",
                        newTimeLimit );
                }

                if ( blackPlayerType==PT_COMPUTER && blackPlayer )
                {
                    ComputerChessPlayer *cp =
                        (ComputerChessPlayer *)blackPlayer;
                    cp->SetTimeLimit ( INT32(100) * INT32(newTimeLimit) );
                    printf ( "Set Black's search time limit to %d seconds\n",
                        newTimeLimit );
                }
            }
            else
            {
                printf ( "!!! Ignoring invalid time limit\n" );
            }
        }
        else if ( sscanf ( s, "edit %[a-zA-Z1-8.]", temp ) == 1
            && strlen(temp)==3 )
        {
            int x = temp[1] - 'a';
            int y = temp[2] - '1';
            if ( x >= 0 && x <= 7 && y >= 0 && y <= 7 )
            {
                SQUARE square = 0xFFFFFFFF;
                switch ( temp[0] )
                {
                    case 'P':   square = WPAWN;     break;
                    case 'N':   square = WKNIGHT;   break;
                    case 'B':   square = WBISHOP;   break;
                    case 'R':   square = WROOK;     break;
                    case 'Q':   square = WQUEEN;    break;
                    case 'K':   square = WKING;     break;
                    case 'p':   square = BPAWN;     break;
                    case 'n':   square = BKNIGHT;   break;
                    case 'b':   square = BBISHOP;   break;
                    case 'r':   square = BROOK;     break;
                    case 'q':   square = BQUEEN;    break;
                    case 'k':   square = BKING;     break;
                    case '.':   square = EMPTY;     break;
                }

                if ( square != 0xFFFFFFFF )
                {
                    board.SetSquareContents ( square, x, y );
                    Move edit;
                    edit.dest = dest = SPECIAL_MOVE_EDIT | ConvertSquareToNybble(square);
                    edit.source = source = OFFSET(x+2,y+2);
                    board.SaveSpecialMove ( edit );
                    return true;
                }
                else
                {
                    printf ( "Invalid square contents in edit command\n" );
                }
            }
            else
            {
                printf ( "Invalid coordinates in edit command\n" );
            }
        }
        else if ( strcmp ( s, "color" ) == 0 )
        {
            boardDisplayType = Next (boardDisplayType);
            DrawBoard (board);
        }
        else
        {
            printf ( "??? " );
        }

        printf ( "%s", Prompt );
        fflush ( stdout );
        fflush ( stdin );
    }

    return false;
}


void ChessUI_stdio::DrawBoard ( const ChessBoard &board )
{
    switch ( boardDisplayType )
    {
        case BDT_LARGE_COLOR:
            DrawBoard_LargeColor (board);
            break;

        case BDT_STANDARD:
        default:
            DrawBoard_Standard (board);
    }
}



void ChessUI_stdio::DrawBoard_Standard ( const ChessBoard &board )
{
    bool whiteView = false;
    if ( board.WhiteToMove() )
        whiteView = whitePlayerType==PT_HUMAN;
    else
        whiteView = blackPlayerType!=PT_HUMAN;

    if ( rotateBoard )
        whiteView = !whiteView;

    int x, y;

    char fen_buffer[256];
    char *fen = board.GetForsythEdwardsNotation (fen_buffer, sizeof(fen_buffer));
    if (fen) {
        printf("\nFEN = %s\n", fen);
    } else {
        printf("\n!!! FEN FAILURE !!!\n");
    }

    printf ( "    +--------+\n" );

    if ( whiteView )
    {
        for ( y=7; y >= 0; y-- )
        {
            printf ( "    |" );
            for ( x=0; x <= 7; x++ )
            {
                SQUARE square = board.GetSquareContents ( x, y );
                printf ( "%c", PieceRepresentation(square) );
            }

            printf ( "|%d\n", y + 1 );
        }
        printf ( "    +--------+\n" );
        printf ( "     abcdefgh\n\n\n" );
    }
    else
    {
        for ( y=0; y <= 7; y++ )
        {
            printf ( "    |" );
            for ( x=7; x >= 0; x-- )
            {
                SQUARE square = board.GetSquareContents ( x, y );
                printf ( "%c", PieceRepresentation(square) );
            }

            printf ( "|%d\n", y + 1 );
        }
        printf ( "    +--------+\n" );
        printf ( "     hgfedcba\n\n\n" );
    }
}


void ChessUI_stdio::RecordMove (
    ChessBoard  &board,
    Move         move,
    INT32        thinkTime )
{
    ClearScreen();

    // Update the MoveUndoPath data structure with this move...
    UnmoveInfo unmove;
    board.MakeMove ( move, unmove );
    Global_MoveUndoPath.resetFromBoard (board);
    board.UnmakeMove ( move, unmove );

    char movestr [MAX_MOVE_STRLEN + 1];
    FormatChessMove ( board, move, movestr );
    int whiteMove = board.WhiteToMove();
    printf ( "\n%s%s  (%0.2lf seconds)\n",
        whiteMove ? "White: " : "Black: ",
        movestr,
        double(thinkTime) / 100.0 );

    FILE *log = fopen ( LogFilename, "at" );
    if ( log )
    {
        static unsigned MoveNumber = 1;
        if ( whiteMove )
            fprintf ( log, "%4u. %-20s", MoveNumber++, movestr );
        else
            fprintf ( log, "%s\n", movestr );

        fclose (log);
    }
    else
    {
        fprintf ( stderr,
            "*** Warning:  Cannot append to file '%s'\n", LogFilename );
    }
}


void ChessUI_stdio::DisplayMove ( ChessBoard &, Move )
{
}


void ChessUI_stdio::ReportEndOfGame ( ChessSide winner )
{
    switch ( winner )
    {
        case SIDE_WHITE:
            printf ( "White wins.\n" );
            break;

        case SIDE_BLACK:
            printf ( "Black wins.\n" );
            break;

        case SIDE_NEITHER:
            printf ( "Game over.\n" );
            break;

        default:
            printf ( "The game is over, and I don't know why!\n" );
    }
}


void ChessUI_stdio::NotifyUser ( const char *message )
{
    printf ( ">>> %s [press Enter] ", message );
    fflush (stdout);
    char junk [256];
    if ( fgets ( junk, sizeof(junk), stdin ) ) {
        // Do nothing.  This 'if' statement exists solely to get rid of a Linux gcc compiler warning.
    }
}


static int ConvertCharToPromPiece ( char x )
{
    switch ( x )
    {
        case 'b':
        case 'B':
            return B_INDEX;

        case 'n':
        case 'N':
            return N_INDEX;

        case 'r':
        case 'R':
            return R_INDEX;

        case 'q':
        case 'Q':
            return Q_INDEX;
    }

    return 0;
}


static int AskPawnPromote()
{
    char UserString[80];

    printf ( "Promote pawn to what kind of piece (Q,R,B,N)? " );
    fflush(stdout);

    while ( fgets ( UserString, sizeof(UserString), stdin ) )
    {
        int piece = ConvertCharToPromPiece ( UserString[0] );
        if ( piece )
            return piece;

        printf ( "Try again (Q,R,B,N): " );
        fflush(stdout);
    }

    ChessFatal ( "Hit EOF on stdin in AskPawnPromote()" );
    return 0;   // make the compiler happy
}



SQUARE ChessUI_stdio::PromotePawn ( int, ChessSide )
{
    return AskPawnPromote();
}


void ChessUI_stdio::DisplayCurrentMove (
    const ChessBoard &b,
    Move              m,
    int               level )
{
    char moveString [MAX_MOVE_STRLEN+1];
    FormatChessMove ( b, m, moveString );
    if ( showScores )
    {
        printf (
            "\rdepth=%d  s=%6d m=[%s]               ",
            level,
            m.score,
            moveString );
    }
    else
    {
        printf (
            "\rdepth=%d  m=[%s]               ",
            level,
            moveString );
    }

    fflush(stdout);
}


void ChessUI_stdio::DisplayBestMoveSoFar (
    const ChessBoard &b,
    Move  m,
    int   level )
{
    char moveString [MAX_MOVE_STRLEN+1];
    FormatChessMove ( b, m, moveString );

    if ( showScores )
    {
        printf ( "\rBest so far: depth=%d  score=%d  move=[%s]              \n",
                level,
                m.score,
                moveString );
    }
    else
    {
        printf ( "\rBest so far: depth=%d  move=[%s]                \n",
                level,
                moveString );
    }

    fflush (stdout);
}


void ChessUI_stdio::DisplayBestPath (
    const ChessBoard &_board,
    const BestPath &path )
{
    if ( showScores )
    {
        char moveString [MAX_MOVE_STRLEN + 1];
        ChessBoard board = _board;

        printf ( "{ " );
        for ( int i=0; i <= path.depth; i++ )
        {
            Move move = path.m[i];
            UnmoveInfo unmove;

            FormatChessMove ( board, move, moveString );
            board.MakeMove ( move, unmove );

            if ( i > 0 )
            {
                printf ( ", " );
            }
            printf ( "%s", moveString );
        }
        printf ( " }\n\n" );
    }
}


void ChessUI_stdio::PredictMate ( int numMoves )
{
   printf ( "Mate in %d\n", numMoves );
}


void ChessUI_stdio::ReportComputerStats (
    INT32   /*thinkTime*/,
    UINT32  nodesVisited,
    UINT32  nodesEvaluated,
    UINT32  nodesGenerated,
    int     /*fwSearchDepth*/,
    UINT32  /*vis*/ [NODES_ARRAY_SIZE],
    UINT32  /*gen*/ [NODES_ARRAY_SIZE] )
{
    printf (
        "\ngenerated=%lu, visited=%lu, evaluated=%lu\n",
        (unsigned long) nodesGenerated,
        (unsigned long) nodesVisited,
        (unsigned long) nodesEvaluated 
    );
}


void ChessFatal ( const char *message )
{
    fprintf ( stderr, "Fatal chess error: %s\n", message );
    exit(1);
}


const int  ANSIMODE_WonB    =  0;
const int  ANSIMODE_WonW    =  1;
const int  ANSIMODE_BonB    =  2;
const int  ANSIMODE_BonW    =  3;


static void SetAnsiMode ( int newMode, int &oldMode )
{
    if ( newMode != oldMode )
    {
        oldMode = newMode;
        printf ( "\x1b[m" );
        switch ( newMode )
        {
            case ANSIMODE_WonB:
                printf ( "\x1b[38m\x1b[45m" );
                break;

            case ANSIMODE_WonW:
                printf ( "\x1b[38m\x1b[46m" );
                break;

            case ANSIMODE_BonB:
                printf ( "\x1b[30m\x1b[45m" );
                break;

            case ANSIMODE_BonW:
                printf ( "\x1b[30m\x1b[46m" );
        }
    }
}


void ChessUI_stdio::DrawBoard_LargeColor ( const ChessBoard &board )
{
    const int idx = 7;  // number of cols in each image
    const int idy = 4;  // number of rows in each image

    static const char *imageR[] =
    { " _ _ _ ",
      " [___] ",
      "  | |  ",
      " [___] " };

    static const char *imageN[] =
    { "  _^^  ",
      " <_  | ",
      "   / | ",
      "  <__| " };

    static const char *imageB[] =
    { "   ^   ",
      "  (/)  ",
      "   U   ",
      "  /_\\  " };

    static const char *imageQ[] =
    { " vvOvv ",
      "  \\^/  ",
      "  | |  ",
      " {===} " };

    static const char *imageK[] =
    { "  _+_  ",
      " {ooo} ",
      "  >-<  ",
      " (___) " };

    static const char *imageP[] =
    { "       ",
      "   O   ",
      "   T   ",
      "  /_\\  " };

    static const char *imageE[] =
    { "       ",
      "       ",
      "       ",
      "       " };


    int currentMode = -1;
    bool whiteView = false;
    if ( board.WhiteToMove() )
        whiteView = whitePlayerType==PT_HUMAN;
    else
        whiteView = blackPlayerType!=PT_HUMAN;

    if ( rotateBoard )
        whiteView = !whiteView;

    for ( int yy=7; yy >= 0; --yy )
    {
        int y = whiteView ? yy : (7 - yy);
        for ( int row=0; row < idy; ++row )
        {
            printf ( " %c ", (row == idy/2) ? (y + '1') : ' ' );
            for ( int xx=0; xx <= 7; ++xx )
            {
                int x = whiteView ? xx : (7 - xx);
                SQUARE piece = board.GetSquareContents(x,y);

                int squareIsWhite = (x + y) & 1;
                int pieceIsWhite = piece & WHITE_MASK;

                const char **image = imageE;
                switch ( piece )
                {
                    case WPAWN:   case BPAWN:    image = imageP;   break;
                    case WKNIGHT: case BKNIGHT:  image = imageN;   break;
                    case WBISHOP: case BBISHOP:  image = imageB;   break;
                    case WROOK:   case BROOK:    image = imageR;   break;
                    case WQUEEN:  case BQUEEN:   image = imageQ;   break;
                    case WKING:   case BKING:    image = imageK;   break;
                }

                const char *raster = image[row];

                for ( int col=0; col < idx; ++col )
                {
                    int newMode = pieceIsWhite ?
                        (squareIsWhite ? ANSIMODE_WonW : ANSIMODE_WonB) :
                        (squareIsWhite ? ANSIMODE_BonW : ANSIMODE_BonB);

                    SetAnsiMode ( newMode, currentMode );
                    printf ( "%c", raster[col] );
                }
            }
            printf ( "\x1b[m\n" );
            currentMode = -1;
        }
    }

    printf ( "\n   " );
    int start = whiteView ? 'a' : 'h';
    int stop  = whiteView ? 'h' : 'a';
    int delta = whiteView ? 1 : -1;
    for ( int x = start; x != delta + stop; x += delta )
    {
        for ( int col=0; col < idx; ++col )
            printf ( "%c", (col == idx/2) ? x : ' ' );
    }
    printf ( "\n" );
    fflush ( stdout );
}



BoardDisplayType Next ( BoardDisplayType displayType )
{
    switch ( displayType )
    {
        case BDT_STANDARD:
            return BDT_LARGE_COLOR;

        case BDT_LARGE_COLOR:
            return BDT_STANDARD;
    }

    return BDT_STANDARD;
}


void ChessUI_stdio::ClearScreen()
{
#if 0
    printf ( "\x1b[2J" );
    fflush (stdout);
#endif
}


void ChessUI_stdio::ReportSpecial ( const char *message )
{
    printf ( "%s\n", message );
}


/*
    $Log: uistdio.cpp,v $
    Revision 1.11  2009/09/23 20:09:03  Don.Cross
    No algorithm change: just made tweaks to get rid of Linux gcc compiler warnings:
    1. Check return values of fgets().
    2. Typecast printf() args to explicit 'unsigned long' values.

    Revision 1.10  2008/11/24 11:21:58  Don.Cross
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

    Revision 1.9  2008/11/13 17:38:29  Don.Cross
    Fixed latent buffer overflow vulnerability in ChessUI_stduio::ReadMove.
    Made buffers bigger also so that longer filenames may be entered.

    Revision 1.8  2008/11/06 22:25:06  Don.Cross
    Now ParseFancyMove accepts either PGN or longmove (e7e8q) style notation only.
    We tolerate of check '+' or checkmate '#' characters being absent or extraneous in the input.
    The former is especially important because I often see PGN notation where '+' is missing on checking moves.
    To allow for PGN input, I had to allow avoidance of the PromotePawn callback, so other code was changed as well.
    Version numbers are now always constently yyyy.mm.dd across all builds of Chenard!

    Revision 1.7  2008/10/28 00:46:43  Don.Cross
    Fixed a warning issued in Linux by g++ about casting a string constant to a non-const char pointer.

    Revision 1.6  2006/01/28 21:44:16  dcross
    1. Updated auto-save feature to use PGN format instead of old GAM format.
       This means we are sacrificing the ability to auto-save edited games, but I don't care.
    2. Added a feature in class ChessGame: if the file 'game.counter' exists, auto-save
       to file based on integer counter it contains, and increment that counter in the file.
       This is so I no longer have to manually make up unique filenames and remember to
       save files when Chenard is in "combat mode".
    3. Reworked PGN save functions so they can be shared by Win32 and portable versions.
       This requires all versions of Chenard to define functions GetWhitePlayerString and
       GetBlackPlayerString, so the players can be recorded in the PGN header.

    Revision 1.5  2006/01/18 19:58:15  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.4  2005/12/31 21:37:02  dcross
    1. Added the ability to generate Forsyth-Edwards Notation (FEN) string, representing the state of the game.
    2. In stdio version of chess UI, display FEN string every time the board is printed.

    Revision 1.3  2005/11/23 21:30:31  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:43  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!


          Revision history:

    1999 January 24 [Don Cross]
         Adding ChessUI_stdio::ReportSpecial(const char *).

    1999 January 16 [Don Cross]
         Cloning support for undo and redo (from Win32 GUI Chenard).
         Adding ChessUI_stdio::NotifyUser().
         Adding support for automatically making move for user when
         he has only one legal move.
         Adding 'beep' toggle option for sounding bell after computer
         makes a move.

    1999 January 15 [Don Cross]
         Adding large, ANSI-color version of chess board display.

    1998 December 28 [Don Cross]
         Now allow fractional number of seconds of think time.

    1996 July 23 [Don Cross]
         Added ReportTranspositionStats member function.

    1996 July 21 [Don Cross]
         Added "time" command to change ComputerChessPlayer time limit
         in middle of game.

    1996 July 20 [Don Cross]
         Added the "rotate" command to show the board rotated on the screen.

    1995 December 30 [Don Cross]
         Fixed bug in ChessUI_stdio::DisplayBestPath.
         Was not displaying path[depth]; instead, was
         stopping at depth-1.

    1995 July 13 [Don Cross]
         Improved display of current move and best move so far.

    1995 April 17 [Don Cross]
         Added the ability to enable/disable showing eval scores from UI.
         By default, this ability will be disabled (ChessUI_stdio::showScores == false),
         so that opponents cannot glean unfair information from the UI.
         For debug purposes, though, it can be enabled.

    1995 March 26 [Don Cross]
         Added the command line option "-s <MaxTimePerMove>", which causes Chenard to play
         a game against itself and save the result in a unique file with filename of the
         form "SELFxxxx.TXT", where xxxx is a four-digit decimal expression of a
         positive integer.  (Leading zeroes are used when necessary.)

    1995 February 26 [Don Cross]
         Added the following UI commands: new, clear, edit.
         new:    Resets the board to start-of-game.
         clear:  Removes every piece from the board except the two kings.
         edit:   Allows a piece to be placed or removed from a single square on the board.

    1995 February 12 [Don Cross]
         Added support for showing both white's and black's view
         of the board.

    1994 November 28 [Don Cross]
         Added simple game log functionality.

    1993 August 30 [Don Cross]
         Changing pointers to references in the interfaces where
         appropriate.

    1993 October 25 [Don Cross]
         Using timed search instead of fixed full-width search.

*/

