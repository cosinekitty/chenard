/*
    ttychess.cpp  -  Don Cross  -  http://cosinekitty.com/chenard

    A version of Chenard designed especially for TTY devices.
*/

#include <Windows.h>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include "chess.h"

const char *SideName(ChessSide side)
{
    switch (side)
    {
    case SIDE_WHITE:    return "White";
    case SIDE_BLACK:    return "Black";
    default:            return "? ? ?";
    }
}

const char *BoolSideName(bool whiteToMove)
{
    return SideName(whiteToMove ? SIDE_WHITE : SIDE_BLACK);
}

class Terminal
{
private:
    HANDLE  hComPort;
    bool    echo;
    bool    allUpperOutput;

    bool IsComPortOpen() const
    {
        return (hComPort != INVALID_HANDLE_VALUE) && (hComPort != NULL);
    }

public:
    Terminal()
        : hComPort(INVALID_HANDLE_VALUE)
        , echo(false)
        , allUpperOutput(true)
    {
    }

    ~Terminal()
    {
        close();
    }

    void setUpperCase(bool upcase)
    {
        allUpperOutput = upcase;
    }

    void close()
    {
        if (IsComPortOpen())
        {
            CloseHandle(hComPort);
            hComPort = INVALID_HANDLE_VALUE;
        }
    }

    void print(char c)
    {
        if (allUpperOutput)
        {
            c = toupper(c);
        }
        fputc(c, stdout);
        if (IsComPortOpen())
        {
            DWORD numBytesWritten = 0;
            WriteFile(hComPort, &c, 1, &numBytesWritten, NULL);
        }
    }

    void print(const std::string& s)
    {
        const size_t n = s.length();
        for (size_t i=0; i < n; ++i)
        {
            print(s[i]);
        }
    }

    void printline()
    {
        print('\r');
        print('\n');
        fflush(stdout);
    }

    void printline(const std::string& s)
    {
        print(s);
        printline();
    }

    void printInteger(int n)
    {
        char s[30];
        sprintf(s, "%d", n);
        print(s);
    }

    void printChessTime(int centiseconds)
    {
        char s[60];
        if (centiseconds < 0)
        {
            print('-');
            centiseconds = -centiseconds;
        }
        sprintf(s, "%d.%02d seconds", centiseconds / 100, centiseconds % 100);
        print(s);
    }

    std::string readline()
    {
        const int max = 120;
        char line[max];
        if (IsComPortOpen())
        {
            // If the com port is open, read from it.
            int index = 0;
            while (true)
            {
                DWORD numBytesRead = 0;
                char c = 0;
                if (ReadFile(hComPort, &c, 1, &numBytesRead, NULL) && (numBytesRead == 1))
                {
                    if (echo)
                    {
                        print(c);
                    }

                    if ((c >= 'A') && (c <= 'Z'))
                    {
                        // convert to lower case (otherwise move coordinates cannot be parsed)
                        c = (c + 'a') - 'A';
                    }

                    if (c == '\r')
                    {
                        if (echo)
                        {
                            print('\n');
                        }
                        line[index] = '\0';
                        return line;
                    }
                    else if (c != '\n')
                    {
                        if (index < max-1)
                        {
                            line[index++] = c;
                        }
                    }
                }
                else
                {
                    close();
                    throw "Com port read failure.";
                }
            }
        }
        else
        {
            // If com port is closed, read from the local keyboard.
            if (fgets(line, sizeof(line), stdin))
            {
                // Remove carriage return and/or line feed at end.
                for (int i=0; i < max && line[i]; ++i)
                {
                    if (line[i] == '\r' || line[i] == '\n')
                    {
                        line[i] = '\0';
                        break;
                    }
                    if (line[i] >= 'A' && line[i] <= 'Z')
                    {
                        line[i] = (line[i] + 'a') - 'A';    // convert to lower case
                    }
                }
                line[max-1] = '\0';
                return line;
            }
        }

        // lost communication - not much we can do
        throw "END OF INPUT";
    }

    void OpenComPort(const char *comport, int baud, bool enableEcho)
    {
        echo = enableEcho;

        hComPort = CreateFileA(
            comport,
            GENERIC_READ|GENERIC_WRITE,
            0,    // cannot share the COM port                        
            0,    // security  (None)                
            OPEN_EXISTING,
            0,    // no flags
            0);   // no templates

        if (IsComPortOpen())
        {
            DCB dcb = {0};
            if (!GetCommState(hComPort, &dcb))
            {
                close();    // must close before throwing, or we will try to print the error too!
                throw "GetCommState failed.";
            }
            dcb.BaudRate = baud;
            dcb.ByteSize = 8;
            dcb.fParity = 0;    // no parity
            dcb.StopBits = 2;   // 2 stop bits
            if (!SetCommState(hComPort, &dcb))
            {
                close();    // must close before throwing, or we will try to print the error too!
                throw "SetCommState failed.";
            }
            printf("Opened and initialized %s\n", comport);
        }
        else
        {
            throw "Cannot open serial port.";
        }
    }
};

Terminal TheTerminal;

std::string WhiteName = "WHITE";
std::string BlackName = "BLACK";

const char *GetWhitePlayerString()
{
    return WhiteName.c_str();
}

const char *GetBlackPlayerString()
{
    return BlackName.c_str();
}

int ChessRandom(int n)   // returns random value between 0 and n-1
{
    static bool isFirstTime = true;
    if (isFirstTime)
    {
        isFirstTime = false;
        srand((unsigned)time(NULL));
    }
    if (n > 1)
    {
        return rand() % n;
    }
    return 0;
}

void ChessFatal ( const char *message )
{
    throw message;
}

bool IsFirstPrompt = true;          // Have we yet offered help?
bool AutoPrintBoard = false;        // Should we automatically print the board when it is the human's turn?
int  ComputerThinkTime = 100;       // Max computer think time, in centiseconds.

class ChessUI_TTY: public ChessUI
{
private:
    ChessSide            humanSide;              // Which side the human is playing (SIDE_WHITE or SIDE_BLACK).
    ComputerChessPlayer *computer;
    HumanChessPlayer    *human;
    ChessGame           *game;

public:
    ChessUI_TTY()
        : humanSide(SIDE_NEITHER)
        , computer(NULL)
        , human(NULL)
        , game(NULL)
    {
    }

    void SetGame (ChessGame *theGame)
    {
        game = theGame;
    }

    virtual ChessPlayer *CreatePlayer(ChessSide side)
    {
        while (humanSide == SIDE_NEITHER)
        {
            TheTerminal.print("Which side do you want to play (w, b, quit)? ");
            std::string choice = TheTerminal.readline();
            if (choice == "w")
            {
                humanSide = SIDE_WHITE;
            }
            else if (choice == "b")
            {
                humanSide = SIDE_BLACK;
            }
            else if (choice == "quit")
            {
                throw "Chess program exited.";
            }
        }

        if (side == humanSide)
        {
            human = new HumanChessPlayer(*this);
            return human;
        }
        else
        {
            computer = new ComputerChessPlayer(*this);
            computer->SetTimeLimit(ComputerThinkTime);
            computer->SetSearchBias(1);
            computer->setResignFlag(false);
            return computer;
        }
    }

    // NOTE:  The following is called only when there is a
    //        checkmate, stalemate, or draw by attrition.
    //        The function is NOT called if a player resigns.
    virtual void ReportEndOfGame(ChessSide winner)
    {
        switch (winner)
        {
        case SIDE_WHITE:
            TheTerminal.printline("White wins.");
            break;

        case SIDE_BLACK:
            TheTerminal.printline("Black wins.");
            break;

        case SIDE_NEITHER:
            TheTerminal.printline("Draw game.");
            break;
        }
    }

    virtual void Resign(ChessSide iGiveUp, QuitGameReason reason)
    {
        switch (reason)
        {
        case qgr_resign:
            switch (iGiveUp)
            {
            case SIDE_WHITE:
                TheTerminal.printline("White resigns.");
                break;

            case SIDE_BLACK:
                TheTerminal.printline("Black resigns.");
                break;
            }
            break;

        case qgr_lostConnection:
            TheTerminal.printline("Lost connection.");
            break;

        case qgr_startNewGame:
            TheTerminal.printline("Starting new game.");
            break;
        }
    }

    void PrintHelp()
    {
        static const char * const help[] =
        {
        //   Assume TTY has 72 columns.  Let's try to keep everything to 70 max.
        //            1         2         3         4         5         6         7
        //   1234567890123456789012345678901234567890123456789012345678901234567890
            "Enter a move using file letter, rank number for source and target.",
            "For example, White moving king pawn forward two squares: e2e4.",
            "To castle, just move the king two squares. Example: e1g1.",
            "",
            "You may also enter any of the following commands:",
            "board   Print the board now (see 'hide', 'show').",
            "help    Print this help text.",
            "hide    Print the board only when asked (see 'board', 'show').",
            "new     Start a new game.",
            "quit    Exit the game.",
            "show    Print the board every time it is your turn.",
            "swap    Swap sides with the computer.",
            "time    Adjust computer's think time (difficulty).",
            NULL
        };

        TheTerminal.printline();
        for (int i=0; help[i]; ++i)
        {
            TheTerminal.printline(help[i]);
        }
        TheTerminal.printline();
    }

    //--------------------------------------------------------------
    //  The following function should return true if the Move
    //  was read successfully, or false to abort the game.
    //  The move does not have to be legal; if an illegal move
    //  is returned, the caller will repeatedly call until a
    //  legal move is obtained.
    //--------------------------------------------------------------
    virtual bool ReadMove ( 
        ChessBoard  &board, 
        int         &source, 
        int         &dest, 
        SQUARE      &promPieceIndex )   // set to 0 (P_INDEX) if no promotion, or call to PromotePawn is needed; set to N_INDEX..Q_INDEX to specify promotion.
    {
        promPieceIndex = P_INDEX;

        bool boardWasAlreadyPrinted = AutoPrintBoard;

        // accept either of the following two formats: e2e4, g7g8q
        while (true)
        {
            if (IsFirstPrompt)
            {
                IsFirstPrompt = false;
                TheTerminal.printline("Enter 'help' for info and more options.");
            }
            TheTerminal.print("Your move? ");
            std::string line = TheTerminal.readline();

            // =================================================================
            // REMEMBER:  Update "PrintHelp()" every time you add a command!
            // =================================================================

            if (line == "board")
            {
                ReallyDrawBoard(board);
                boardWasAlreadyPrinted = true;
            }
            else if (line == "help")
            {
                PrintHelp();
            }
            else if (line == "new")
            {                
                if (human)
                {
                    human->SetQuitReason(qgr_startNewGame);     // suppress saying that the player resigned
                }
                return false;   // Start a new game.
            }
            else if (line == "quit")
            {
                throw "Chess program exited.";
            }
            else if (line == "show")
            {
                AutoPrintBoard = true;
                if (!boardWasAlreadyPrinted)
                {
                    ReallyDrawBoard(board);
                    boardWasAlreadyPrinted = true;
                }
            }
            else if (line == "swap")
            {
                if (game)
                {
                    humanSide = OppositeSide(humanSide);
                    if (humanSide == SIDE_WHITE)
                    {
                        game->SetPlayers(human, computer);
                    }
                    else
                    {
                        game->SetPlayers(computer, human);
                    }
                    TheTerminal.print("You are now playing as ");
                    TheTerminal.print(SideName(humanSide));
                    TheTerminal.printline(".");

                    source = 0;
                    dest = SPECIAL_MOVE_NULL;
                    return true;
                }
            }
            else if (line == "time")
            {
                // Adjust computer's think time.
                TheTerminal.print("The computer's max think time is now ");
                TheTerminal.printChessTime(ComputerThinkTime);
                TheTerminal.printline(".");
                TheTerminal.print("Enter new think time (0.1 .. 600): ");
                std::string line = TheTerminal.readline();
                double thinkTime = atof(line.c_str());
                if ((thinkTime >= 0.1) && (thinkTime < 600.0))
                {
                    ComputerThinkTime = static_cast<int>(100.0*thinkTime + 0.5);    // round seconds to integer centiseconds
                    if (computer)
                    {
                        computer->SetTimeLimit(ComputerThinkTime);
                    }
                }
                else
                {
                    TheTerminal.printline("Invalid think time - ignoring.");
                }
            }
            else
            {
                Move move;  // ignored
                if (ParseFancyMove(line.c_str(), board, source, dest, promPieceIndex, move))
                {
                    return true;
                }
            }
        }
    }

    //--------------------------------------------------------------
    //  Called whenever a player needs to be asked which
    //  piece a pawn should be promoted to.
    //  Must return one of the following values:
    //      Q_INDEX, R_INDEX, B_INDEX, N_INDEX
    //--------------------------------------------------------------
    virtual SQUARE PromotePawn(int pawnDest, ChessSide side)
    {
        while (true)
        {
            TheTerminal.print("Promote pawn to Q, R, N, B? ");
            std::string answer = TheTerminal.readline();
            if (answer == "q")
            {
                return Q_INDEX;
            }
            else if (answer == "r")
            {
                return R_INDEX;
            }
            else if (answer == "n")
            {
                return N_INDEX;
            }
            else if (answer == "b")
            {
                return B_INDEX;
            }
        }
    }

    //--------------------------------------------------------------
    //  The following member function is called bracketing
    //  when a ComputerChessPlayer object is deciding upon a move.
    //  The parameter 'entering' is true when the computer is
    //  starting to think, and false when the thinking is done.
    //--------------------------------------------------------------
    virtual void ComputerIsThinking(bool entering, ComputerChessPlayer &) 
    {
    }

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
    virtual void DisplayMove(ChessBoard &board, Move move)
    {
    }

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
    virtual void RecordMove(ChessBoard &board, Move move, INT32 thinkTime)
    {
        TheTerminal.print(BoolSideName(board.WhiteToMove()));
        TheTerminal.print(" move #");
        TheTerminal.printInteger((board.GetCurrentPlyNumber()/2) + 1);
        TheTerminal.print(": ");

        int source, dest;
        SQUARE prom = move.actualOffsets(board, source, dest);
        char sourceFile = XPART(source) - 2 + 'a';
        char sourceRank = YPART(source) - 2 + '1';
        char destFile   = XPART(dest)   - 2 + 'a';
        char destRank   = YPART(dest)   - 2 + '1';
        TheTerminal.print(sourceFile);
        TheTerminal.print(sourceRank);
        TheTerminal.print(destFile);
        TheTerminal.print(destRank);
        if (prom)
        {
            char pchar = '?';
            int pindex = UPIECE_INDEX(prom);
            switch (pindex)
            {
                case Q_INDEX:   pchar = 'Q';    break;
                case R_INDEX:   pchar = 'R';    break;
                case B_INDEX:   pchar = 'B';    break;
                case N_INDEX:   pchar = 'N';    break;
            }
            TheTerminal.print(pchar);
        }

        // Temporarily make the move, so we can display what the board
        // looks like after the move has been made.
        // We also use this to determine if the move causes check,
        // checkmate, stalemate, etc.
        UnmoveInfo unmove;
        board.MakeMove(move, unmove);

        // Report check, checkmate, draws.
        bool inCheck = board.CurrentPlayerInCheck();
        bool canMove = board.CurrentPlayerCanMove();
        if (canMove)
        {
            if (inCheck)
            {
                TheTerminal.print('+');
            }
        }
        else
        {
            // The game is over!
            if (inCheck)
            {
                if (board.WhiteToMove())
                {
                    // White just got checkmated.
                    TheTerminal.print("# 0-1");
                }
                else
                {
                    // Black just got checkmated.
                    TheTerminal.print("# 1-0");
                }
            }
            else
            {
                // The game is a draw (could be stalemate, could be by repetition, insufficient material, ...)
                TheTerminal.print(" 1/2-1/2");
            }
        }

        TheTerminal.printline();

        // Unmake the move, because the caller will make the move itself.
        board.UnmakeMove(move, unmove);
    }

    //----------------------------------------------------------------
    //   The following function is called when a method other than
    //   searching has been used to choose a move, e.g.
    //   opening book or experience database.
    //----------------------------------------------------------------
    virtual void ReportSpecial ( const char *msg )  {}

    virtual void ReportComputerStats (
        INT32   thinkTime,
        UINT32  nodesVisited,
        UINT32  nodesEvaluated,
        UINT32  nodesGenerated,
        int     fwSearchDepth,
        UINT32  vis [NODES_ARRAY_SIZE],
        UINT32  gen [NODES_ARRAY_SIZE] )
    {
    }

    //--------------------------------------------------------------//
    //  The following function should display the given ChessBoard  //
    //  to the user.                                                //
    //  In a GUI, this means updating the board display to the      //
    //  given board's contents.                                     //
    //  In TTY-style UI, this means printing out a new board.       //
    //--------------------------------------------------------------//
    virtual void DrawBoard(const ChessBoard &board)
    {
        // Drawing the board is VERY slow on a teletype.
        // Only draw the board right before the human player's turn,
        // and then only if auto-printing of the board is enabled.
        if (AutoPrintBoard)
        {
            if (board.WhiteToMove() == (humanSide == SIDE_WHITE))
            {
                ReallyDrawBoard(board);
            }
        }
    }

    void ReallyDrawBoard(const ChessBoard &board)
    {
        // Show the board from the human player's point of view.
        char startFile, startRank;
        int deltaFile, deltaRank;
        if (humanSide == SIDE_WHITE)
        {
            // We draw the board with White (rank 1) at the bottom, so we start with rank 8.
            startFile = 'a';
            startRank = '8';
            deltaFile = +1;
            deltaRank = -1;
        }
        else
        {
            // We draw the board with Black (rank 8) at the bottom, so we start with rank 1.
            startFile = 'h';
            startRank = '1';
            deltaFile = -1;
            deltaRank = +1;
        }

        for (int y=0, rank=startRank; y < 8; ++y, rank += deltaRank)
        {
            TheTerminal.print(static_cast<char>(rank));
            TheTerminal.print("   ");
            for (int x=0, file=startFile; x < 8; ++x, file += deltaFile)
            {
                const SQUARE square = board.GetSquareContents(file - 'a', rank - '1');
                const char *text;
                switch (square)
                {
                case EMPTY:     text = ". ";    break;

                case WPAWN:     text = "WP";    break;
                case WKNIGHT:   text = "WN";    break;
                case WBISHOP:   text = "WB";    break;
                case WROOK:     text = "WR";    break;
                case WQUEEN:    text = "WQ";    break;
                case WKING:     text = "WK";    break;

                case BPAWN:     text = "BP";    break;
                case BKNIGHT:   text = "BN";    break;
                case BBISHOP:   text = "BB";    break;
                case BROOK:     text = "BR";    break;
                case BQUEEN:    text = "BQ";    break;
                case BKING:     text = "BK";    break;

                default:        text = "??";    break;
                }

                TheTerminal.print(text);

                if (x < 7)      // no need for white space at end of line
                {
                    TheTerminal.print("   ");
                }
            }
            TheTerminal.printline();
            TheTerminal.printline();
        }

        // Print file letters at the bottom.
        TheTerminal.print("    ");
        for (int x=0, file=startFile; x < 8; ++x, file += deltaFile)
        {
            TheTerminal.print(static_cast<char>(file));
            if (x < 7)      // no need for white space at end of line
            {
                TheTerminal.print("    ");
            }
        }
        TheTerminal.printline();
        TheTerminal.printline();
    }

    //----------------------------------------------------------------
    //  The following member function displays a prompt and waits
    //  for acknowledgement from the user.
    //----------------------------------------------------------------
    virtual void NotifyUser ( const char *message )
    {
        TheTerminal.print(message);
    }

    virtual void DisplayBestMoveSoFar (
        const ChessBoard &,
        Move,
        int level )
    {
    }

    virtual void DisplayCurrentMove (
        const ChessBoard &,
        Move,
        int level )
    {
    }

    virtual void DisplayBestPath (
        const ChessBoard &,
        const BestPath & )  
    {
    }
};

void LoadIniFile(const char *iniFileName)
{
    FILE *infile = fopen(iniFileName, "rt");
    if (infile != NULL)
    {
        bool enableSerialPort = false;
        bool enableEcho = false;
        std::string portName = "COM1";
        int baud = 110;

        const int max = 128;
        char line [max];
        while (fgets(line, max, infile))
        {
            line[max-1] = '\0';

            // Truncate each line at cr, lf, or pound sign.
            // Find position of equal sign, if present.
            int eqindex = -1;
            int firstNonSpaceIndex = -1;
            int lastNonSpaceIndex = -1;
            for (int i=0; line[i]; ++i)
            {
                if (line[i]=='\n' || line[i]=='\r' || line[i]=='#')
                {
                    break;
                }
                if (line[i] == '=')
                {
                    eqindex = i;
                }
                if (!isspace(line[i]))
                {
                    if (firstNonSpaceIndex < 0)
                    {
                        firstNonSpaceIndex = i;
                    }
                    lastNonSpaceIndex = i;
                }
            }
            line[1+lastNonSpaceIndex] = '\0';
            if (firstNonSpaceIndex >= 0)
            {
                // Not a blank line.  Is its contents valid?
                if (eqindex >= 0)
                {
                    line[eqindex] = '\0';
                    const char *key = line + firstNonSpaceIndex;
                    const char *value = line + eqindex + 1;
                    if (0 == stricmp(key, "serial"))
                    {
                        enableSerialPort = !!atoi(value);
                    }
                    else if (0 == stricmp(key, "port"))
                    {
                        portName = value;
                    }
                    else if (0 == stricmp(key, "baud"))
                    {
                        baud = atoi(value);
                    }
                    else if (0 == stricmp(key, "echo"))
                    {
                        enableEcho = !!atoi(value);
                    }
                    else if (0 == stricmp(key, "upcase"))
                    {
                        TheTerminal.setUpperCase(!!atoi(value));
                    }
                    else
                    {
                        printf("ttychess.ini: Ignoring unknown key '%s'\n", key);
                    }
                }
                else
                {
                    printf("ttychess.ini: Ignoring line '%s'\n", line);
                }
            }
        }
        fclose(infile);
        infile = NULL;

        if (enableSerialPort)
        {
            printf("Opening serial port %s for %d baud ...\n", portName.c_str(), baud);
            TheTerminal.OpenComPort(portName.c_str(), baud, enableEcho);
        }
    }
    else
    {
        printf("Note: could not open file '%s'\n", iniFileName);
    }
}

int main(int argc, const char *argv[])
{
    int rc = 1;
    try 
    {
        LoadIniFile("ttychess.ini");
        TheTerminal.printline();
        TheTerminal.printline("Teletype chess program (2014.07.11) by Don Cross.");
        TheTerminal.printline("http://cosinekitty.com/chenard");
        while (true)
        {
            ChessBoard board;
            ChessUI_TTY ui;
            ChessGame game(board, ui);
            ui.SetGame(&game);
            game.Play();
            ui.SetGame(NULL);
        }
        rc = 0;
    }
    catch (const char *message)
    {
        TheTerminal.print(message);
        TheTerminal.print("\r\n");
    }
    TheTerminal.close();
 	return rc;
}
