/*
    gamefile.cpp  -  Don Cross, January 2006.

    C++ classes to abstract the process to load chess moves from
    various kinds of files:  old chenard GAM, PGN, or
    Yahoo game listing email text.
*/

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

#include "chess.h"
#include "gamefile.h"

//-----------------------------------------------------------------------------------------------

bool ScanAlgebraic (char file, char rank, int &offset)
{
    if ((file >= 'a') && (file <= 'h') && (rank >= '1') && (rank <= '8'))
    {
        offset = OFFSET ((file - 'a') + 2, (rank - '1') + 2);
        return true;
    }
    else
    {
        offset = 0;     // This is so caller does not need to check return value (can check offset instead)
        return false;
    }
}

//-----------------------------------------------------------------------------------------------

tChessMoveStream *tChessMoveStream::OpenFileForRead (const char *filename)
{
    // factory function for auto-detecting file format
    tChessMoveStream *stream = NULL;
    FILE *infile = NULL;
    char line [256];

    // To make life simple, we assume it must be Chenard GAM format if
    // it ends with the extension ".gam"...
    const char *ext = strrchr (filename, '.');

    if (0 == stricmp (ext, ".gam"))
    {
        infile = fopen (filename, "rb");
        if (infile)
        {
            stream = new tChessMoveFile_GAM (infile);
        }
    }
    else
    {
        infile = fopen (filename, "rt");
        if (infile)
        {
            // Now we need to peek through the file to see what kind of file it is...

            while (fgets (line, sizeof(line), infile))
            {
                if (line[0] == '[')
                {
                    // e.g. '[Event "?"]'
                    rewind (infile);    // do this so object we create can read moves from the file
                    stream = new tChessMoveFile_PGN (infile);
                    break;
                }
                else if (line[0] == ';')
                {
                    // e.g. ';Title: Yahoo! Chess Game'
                    rewind (infile);    // do this so object we create can read moves from the file
                    stream = new tChessMoveFile_Yahoo (infile);
                    break;
                }
            }
        }
    }

    if (stream == NULL)
    {
        if (infile)
        {
            fclose (infile);
            infile = NULL;
        }
    }

    return stream;
}

//-----------------------------------------------------------------------------------------------

bool tChessMoveFile_GAM::GetNextMove (Move &move, bool &GameReset)
{
    GameReset = false;
    return ::ReadFromFile (infile, move);
}

//-----------------------------------------------------------------------------------------------

bool tChessMoveFile_PGN::GetNextMove (Move &move, bool &GameReset)
{
    char            movestr [1+MAX_MOVE_STRLEN];
    UnmoveInfo      unmove;
    PGN_FILE_STATE  state;
    PgnExtraInfo    info;

    GameReset = false;

    if (GetNextPgnMove (infile, movestr, state, info))
    {
        if (state == PGN_FILE_STATE_NEWGAME)
        {
            if (info.fen[0])
            {
                return false;       // this API does not support edited positions!
            }
            else
            {
                GameReset = true;
                board.Init();
            }
        }

        if (board.ScanMove (movestr, move))
        {
            board.MakeMove (move, unmove);
            return true;
        }
    }

    return false;
}

//-----------------------------------------------------------------------------------------------

bool tChessMoveFile_Yahoo::GetNextMove (Move &move, bool &GameReset)
{
    int          test_move_number = 0;
    char        *move_string = NULL;
    UnmoveInfo   unmove;

    GameReset = false;

    if (board.WhiteToMove())
    {
        // Read lines until we find one that begins with the expected move number.
        // Typical example:  "29. c7-c6+ e6xe7"
        while (fgets (line, sizeof(line), infile))
        {
            num_scanned = sscanf (line, "%d. %s %s", &test_move_number, white_move_string, black_move_string);
            if (num_scanned >= 2)
            {
                if (test_move_number == move_number)
                {
                    // We found the desired move.
                    move_string = white_move_string;
                    break;
                }
            }
        }
    }
    else
    {
        if (num_scanned == 3)
        {
            move_string = black_move_string;
        }
    }

    if (move_string)
    {
        int msource = 0;
        int mdest   = 0;

        // Special cases:   e2-e4  f3xf6  o-o  o-o-o  e5xg4+  f5-h5++
        if (0 == memcmp (move_string, "o-o", 3))
        {
            // We don't know whether it is o-o or o-o-o yet, but we know the king is moving.
            // But is it White's king or Black's king?
            if (board.WhiteToMove())
            {
                msource = OFFSET(6,2);
            }
            else
            {
                msource = OFFSET(6,9);
            }
            if (move_string[3]=='-' && move_string[4]=='o')
            {
                mdest = msource + (2*WEST);     // o-o-o
            }
            else
            {
                mdest = msource + (2*EAST);     // o-o
            }
        }
        else
        {
            // not castling, so expect normal coords.
            if (strlen(move_string) >= 5)
            {
                if ((move_string[2] == '-') || (move_string[2] == 'x'))
                {
                    ScanAlgebraic (move_string[0], move_string[1], msource);
                    ScanAlgebraic (move_string[3], move_string[4], mdest  );
                }
            }
        }

        if (msource && mdest)
        {
            int     tsource, tdest;
            MoveList    ml;
            board.GenMoves (ml);
            for (int i=0; i < ml.num; ++i)
            {
                SQUARE prom = ml.m[i].actualOffsets (board, tsource, tdest);
                if ((tsource == msource) && (tdest == mdest))
                {
                    // Stupid Yahoo game format does not indicate pawn promotion piece!
                    // So we ignore promotions to anything other than queen.
                    if ((prom == EMPTY) || (prom == WQUEEN) || (prom == BQUEEN))
                    {
                        move = ml.m[i];
                        board.MakeMove (move, unmove);
                        if (board.WhiteToMove())
                        {
                            ++move_number;
                        }
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

//-----------------------------------------------------------------------------------------------

void SavePortableGameNotation (
    FILE        *outfile,
    ChessBoard  &refboard,
    const char  *whiteName,
    const char  *blackName )
{
    ChessBoard  board;  // we need a fresh board, starting at beginning of game, to generate correct move notation.

    /*
        http://www.saremba.de/chessgml/standards/pgn/pgn-complete.htm
        http://en.wikipedia.org/wiki/Portable_Game_Notation

        The following Seven Tag Roster (STR) is mandatory:

           1. Event (the name of the tournament or match event)
           2. Site (the location of the event)
           3. Date (the starting date of the game)
           4. Round (the playing round ordinal of the game)
           5. White (the player of the white pieces)
           6. Black (the player of the black pieces)
           7. Result (the result of the game)
    */


    fprintf (outfile, "[Event \"?\"]\n");
    fprintf (outfile, "[Site \"?\"]\n");

    time_t  now = time(NULL);
    tm *date = localtime(&now);
    fprintf (outfile, "[Date \"%04d.%02d.%02d\"]\n", (date->tm_year + 1900), (date->tm_mon + 1), date->tm_mday);

    fprintf (outfile, "[Round \"-\"]\n");

    fprintf (outfile, "[White \"%s\"]\n", whiteName);
    fprintf (outfile, "[Black \"%s\"]\n", blackName);

    if (refboard.HasBeenEdited())
    {
        // We need to save the initial position for this board...
        char *initialFEN = refboard.GetInitialForsythEdwardsNotation();
        board.SetForsythEdwardsNotation (initialFEN);       // Must start local board in same state as after any local edits in refboard.
        fprintf (outfile, "[SetUp \"1\"]\n");
        fprintf (outfile, "[FEN \"%s\"]\n", initialFEN);
        delete[] initialFEN;
    }
    else
    {
        fprintf (outfile, "[SetUp \"0\"]\n");
    }

    const char *ResultString = NULL;
    if (refboard.GameIsOver())
    {
        if (refboard.CurrentPlayerCanMove())
        {
            ResultString = "1/2-1/2";   // assume draw by repetition or 50-move rule
        }
        else
        {
            if (refboard.CurrentPlayerInCheck())
            {
                if (refboard.WhiteToMove())
                {
                    ResultString = "0-1";   // Black has checkmated White
                }
                else
                {
                    ResultString = "1-0";   // White has checkmated Black
                }
            }
            else
            {
                ResultString = "1/2-1/2";   // stalemate
            }
        }
    }
    else
    {
        ResultString = "*";
    }
    assert (ResultString != NULL);
    fprintf (outfile, "[Result \"%s\"]\n", ResultString);

    // A single blank line separates the STR and the movetext section...
    fprintf (outfile, "\n");

    char numstr [32];
    char movestr [1 + MAX_MOVE_STRLEN];
    int  column = 0;
    int  moveNumber = 0;
    size_t  length;

    for (int i = refboard.InitialPlyNumber(); i < refboard.GetCurrentPlyNumber(); ++i)
    {
        UnmoveInfo unmove;
        Move move = refboard.GetPastMove(i);
        FormatChessMove (board, move, movestr);
        if (board.BlackToMove())
        {
            // Black's turn... no prefix string...
            numstr[0] = '\0';
            length = 0;
        }
        else
        {
            // White's turn... prepend move number...
            length = sprintf (numstr, "%d. ", ++moveNumber);
        }
        length += (int) strlen(movestr);
        if (column + length > 79)
        {
            column = 0;
            fprintf (outfile, "\n");
        }
        else
        {
            if (column > 0)
            {
                column += fprintf (outfile, " ");
            }
        }
        column += fprintf (outfile, "%s%s", numstr, movestr);
        board.MakeMove (move, unmove);
    }

    // The result string must appear at the end of every game!
    // Use the same word-wrap algorithm as the rest of the move text...
    length = strlen(ResultString);
    if (column + length > 79)
    {
        column = 0;
        fprintf (outfile, "\n");
    }
    else
    {
        column += fprintf (outfile, " ");
    }
    column += fprintf (outfile, "%s", ResultString);

    fprintf (outfile, "\n");    // A single blank line terminates the movetext.
}

//-----------------------------------------------------------------------------------------------

bool SaveGamePGN (ChessBoard &board, const char *filename)
{
    bool saved = false;
    FILE *outfile = fopen (filename, "wt");
    if (outfile)
    {
        SavePortableGameNotation (outfile, board);
        saved = true;
        fclose (outfile);
        outfile = NULL;
    }
    return saved;
}

//---------------------------------------------------------------------------------------------------------------------------------

inline bool IsPgnSymbolChar (char c)
{
    return isalnum(c) || (c == '_') || (c == '+') || (c == '#') || (c == '=') || (c == ':') || (c == '-') || (c == '/') || (c == '$');
}

const int MAX_PGN_TOKEN_LENGTH = 300;   // I believe the spec allows for little more than 255 characters in a token.

enum PGN_TOKEN_TYPE
{
    PGNTOKEN_UNDEFINED,
    PGNTOKEN_SYNTAX_ERROR,
    PGNTOKEN_EOF,
    PGNTOKEN_SYMBOL,
    PGNTOKEN_STRING,
    PGNTOKEN_PUNCTUATION
};

#define tokencopy(c)    {if(index < MAX_PGN_TOKEN_LENGTH) {token[index++] = (c);} else {token[0] = '\0'; return PGNTOKEN_SYNTAX_ERROR;}}

PGN_TOKEN_TYPE ScanPgnToken (FILE *file, char token [1 + MAX_PGN_TOKEN_LENGTH])
{
    // See section 7 in:  http://www.chessclub.com/help/PGN-spec

    PGN_TOKEN_TYPE type = PGNTOKEN_UNDEFINED;
    int index;
    int c;

skip_white_space:
    index = 0;
    token[0] = '\0';

    // Skip any leading white space...
    for(;;)
    {
        c = fgetc (file);
        if (c == EOF)
        {
            return PGNTOKEN_EOF;   // did not find any non-whitespace before end of file; give up.
        }
        else if (!isspace(c))
        {
            break;
        }
    }

    // Determine whether this character is a token by itself, or whether there may be further characters...
    if (IsPgnSymbolChar (c))
    {
        tokencopy (c);
        type = PGNTOKEN_SYMBOL;
        for(;;)
        {
            c = fgetc(file);
            if (c == EOF)
            {
                break;      // valid end of symbol
            }
            else
            {
                if (IsPgnSymbolChar(c))
                {
                    tokencopy (c);
                }
                else
                {
                    ungetc (c, file);   // push back character for next token scan
                    break;
                }
            }
        }
    }
    else if (c == '"')
    {
        /*
            http://www.chessclub.com/help/PGN-spec  says:
                A string token is a sequence of zero or more printing characters delimited by a
                pair of quote characters (ASCII decimal value 34, hexadecimal value 0x22).  An
                empty string is represented by two adjacent quotes.  (Note: an apostrophe is
                not a quote.)  A quote inside a string is represented by the backslash
                immediately followed by a quote.  A backslash inside a string is represented by
                two adjacent backslashes.  Strings are commonly used as tag pair values (see
                below).  Non-printing characters like newline and tab are not permitted inside
                of strings.  A string token is terminated by its closing quote.  Currently, a
                string is limited to a maximum of 255 characters of data.
        */

        // Note that we do NOT retain the opening and closing quotes in our token.
        // We also correct for backslash-escaped characters inside the string.
        bool backslashEscape = false;

        type = PGNTOKEN_STRING;
        for(;;)     // keep going until we find an unescaped double-quote terminator...
        {
            c = fgetc(file);
            if (c == EOF)
            {
                token[0] = '\0';
                return PGNTOKEN_SYNTAX_ERROR;   // unterminated string constant
            }
            else
            {
                if (backslashEscape)
                {
                    // Treat this character literally, no matter what it is...
                    tokencopy (c);
                    backslashEscape = false;
                }
                else
                {
                    if (c == '"')
                    {
                        break;      // end of string!
                    }
                    else if (c == '\\')
                    {
                        backslashEscape = true;
                    }
                    else
                    {
                        tokencopy (c);
                    }
                }
            }
        }
    }
    else if (c == ';')
    {
        // This is the beginning of a comment that spans to the end of this current line...
        for(;;)
        {
            c = fgetc(file);
            if (c == EOF)
            {
                return PGNTOKEN_EOF;
            }
            else if (c == '\r' || c == '\n')
            {
                goto skip_white_space;      // start all over looking for the beginning of a token
            }
        }
    }
    else if (c == '{')
    {
        // This is the beginning of a comment that does not end until a close brace is encountered.
        // PGN brace comments are not nestable, so we don't need to count nested brace levels...
        for(;;)
        {
            c = fgetc(file);
            if (c == EOF)
            {
                return PGNTOKEN_EOF;
            }
            else if (c == '}')
            {
                goto skip_white_space;      // start all over looking for the beginning of a token
            }
        }
    }
    else if (c == '(')
    {
        // http://www.saremba.de/chessgml/standards/pgn/pgn-complete.htm
        // 8.2.5: Movetext RAV (Recursive Annotation Variation)
        //     "An RAV (Recursive Annotation Variation) is a sequence of movetext
        //      containing one or more moves enclosed in parentheses.
        //      An RAV is used to represent an alternative variation.
        //      The alternate move sequence given by an RAV is one that
        //      may be legally played by first unplaying the move that appears
        //      immediately prior to the RAV.
        //      Because the RAV is a recursive construct, it may be nested."
        //
        // We skip all annotations.
        int nesting = 1;
        while (nesting > 0)
        {
            c = fgetc(file);
            switch (c)
            {
            case EOF:
                return PGNTOKEN_EOF;

            case '(':
                ++nesting;
                break;

            case ')':
                --nesting;
                break;
            }
        }
        goto skip_white_space;  // start all over looking for the beginning of a token
    }
    else
    {
        // All other characters are tokens by themselves...
        type = PGNTOKEN_PUNCTUATION;
        tokencopy (c);
    }

    token[index] = '\0';
    assert (type != PGNTOKEN_UNDEFINED);
    return type;
}

#undef tokencopy

//---------------------------------------------------------------------------------------------------------------------------------

bool IsPgnTagNameWeCareAbout (const char *tag, int &tagIndex)
{
    static const char * const ImportantTags[] =         // be careful with the order of these strings: their indexes are returned in tagIndex!
    {
        "FEN",              // tagIndex == 0
        "WhiteElo",         // tagIndex == 1
        "BlackElo",         // tagIndex == 2
        "Result",           // tagIndex == 3
        0
    };

    for (int i=0; ImportantTags[i]; ++i)
    {
        if (0 == stricmp (tag, ImportantTags[i]))
        {
            tagIndex = i;
            return true;
        }
    }

    tagIndex = -1;
    return false;
}

//---------------------------------------------------------------------------------------------------------------------------------


bool GetNextPgnMove (
    FILE            *file,
    char             movestr[1+MAX_MOVE_STRLEN],
    PGN_FILE_STATE  &state,
    PgnExtraInfo    &info )
{
    char    token [1 + MAX_PGN_TOKEN_LENGTH];
    memset (&info, 0, sizeof(PgnExtraInfo));

    if (movestr == NULL)
    {
        state = PGN_FILE_STATE_INVALID_PARAMETER;
        return false;
    }

    movestr[0] = '\0';

    if (file == NULL)
    {
        state = PGN_FILE_STATE_INVALID_PARAMETER;
        return false;
    }

    state = PGN_FILE_STATE_SAMEGAME;

    for (;;)
    {
        PGN_TOKEN_TYPE type = ScanPgnToken(file, token);
        if (type == PGNTOKEN_EOF)
        {
            state = PGN_FILE_STATE_EOF;
            break;
        }
        else if (type == PGNTOKEN_SYNTAX_ERROR)
        {
            state = PGN_FILE_STATE_SYNTAX_ERROR;
            break;
        }
        else if (type == PGNTOKEN_SYMBOL)
        {
            if (isalpha(token[0]))
            {
                // A valid move token will always begin with a letter of the alphabet.
                size_t length = strlen(token);
                if (length >= 1 && length <= 7)
                {
                    strcpy(movestr, token);
                    return true;
                }
                else
                {
                    state = PGN_FILE_STATE_SYNTAX_ERROR;
                    break;
                }
            }
            else
            {
                // Look for special game terminators: "1-0", "0-1", "1/2-1/2".
                if (0 == strcmp(token, "1-0") || 0 == strcmp(token, "0-1") || 0 == strcmp(token, "1/2-1/2"))
                {
                    state = PGN_FILE_STATE_GAMEOVER;
                    break;
                }
            }
        }
        else if (type == PGNTOKEN_STRING)
        {
            // Invalid token type for this context!
            state = PGN_FILE_STATE_SYNTAX_ERROR;
            break;
        }
        else if (type == PGNTOKEN_PUNCTUATION)
        {
            if (0 == strcmp(token, "["))
            {
                state = PGN_FILE_STATE_NEWGAME;     // We are starting a whole new game.

                // A tag must contain exactly 4 tokens: "[" Symbol String "]".
                type = ScanPgnToken(file, token);
                if (type != PGNTOKEN_SYMBOL)
                {
                    state = PGN_FILE_STATE_SYNTAX_ERROR;
                    break;
                }

                int tagIndex;
                if (IsPgnTagNameWeCareAbout(token, tagIndex))
                {
                    type = ScanPgnToken(file, token);
                    if (type != PGNTOKEN_STRING)
                    {
                        state = PGN_FILE_STATE_SYNTAX_ERROR;
                        break;
                    }

                    switch (tagIndex)
                    {
                    case 0:     // FEN
                        strncpy(info.fen, token, sizeof(info.fen));
                        break;

                    case 1:     // WhiteELO
                        info.whiteElo = atoi(token);
                        break;

                    case 2:     // BlackELO
                        info.blackElo = atoi(token);
                        break;

                    case 3:     // Result
                        if (0 == strcmp(token, "1-0"))
                        {
                            info.result = PGNRESULT_WHITE_WON;
                        }
                        else if (0 == strcmp(token, "0-1"))
                        {
                            info.result = PGNRESULT_BLACK_WON;
                        }
                        else if (0 == strcmp(token, "1/2-1/2"))
                        {
                            info.result = PGNRESULT_DRAW;
                        }
                        else
                        {
                            info.result = PGNRESULT_UNKNOWN;
                        }
                        break;
                    }

                    type = ScanPgnToken(file, token);
                    if (0 != strcmp(token, "]"))
                    {
                        state = PGN_FILE_STATE_SYNTAX_ERROR;
                        break;
                    }
                }
                else
                {
                    // I have run into some software that screws up some tags
                    // with extra quote marks, e.g.:   [Black ""Atak_62.exe"   "]
                    // For tags I don't care about, I am going to skip over everything until I find the end of the line.
                    // I am going to avoid looking for ']', in case some joker puts that character inside the string!
                    for (;;)
                    {
                        int c = fgetc(file);
                        if (c == EOF)
                        {
                            state = PGN_FILE_STATE_SYNTAX_ERROR;
                            goto bail_out;
                        }
                        if (c == '\n')
                        {
                            break;
                        }
                    }
                }
            }
            else if (0 == strcmp(token, "*"))
            {
                // Symbol for the end of the listing of an unfinished game...
                state = PGN_FILE_STATE_GAMEOVER;
                break;
            }
        }
    }

bail_out:
    movestr[0] = '\0';
    return false;
}


/*
    $Log: gamefile.cpp,v $
    Revision 1.13  2008/12/01 23:08:53  Don.Cross
    Norm Pollock announced on the WinBoard forum that he has updated his PGN database:
        http://www.open-aurec.com/wbforum/viewtopic.php?t=49522
        http://www.hoflink.com/~npollock/chess.html
    This is just what I was looking for, so that I could beef up chenard.trx (training file).
    I modified the PGN scanner to recognize WhiteElo, BlackElo, and Result tags.
    I am using WhiteElo and BlackElo to decide whether a game is trustworthy enough to pretend like Chenard thought
    of the move itself, so that it will play that move in actual games, instead of just as a reference point for opponent moves.
    I am currently in the process of slurping an enormous amount of openings into chenard.trx.
    I decided to move GetNextPgnMove from board.cpp to gamefile.cpp, where it really belongs.

    Revision 1.12  2008/11/13 15:29:05  Don.Cross
    Fixed bug: user can now Undo/Redo moves after loading a game from a PGN file.

    Revision 1.11  2008/11/13 14:43:11  Don.Cross
    Minor correction to PGN file output: wrap result indicator with the same rules as the rest of the move text.

    Revision 1.10  2008/11/12 23:06:07  Don.Cross
    Massive refactoring of the concept of an 'editedFlag' in ChessBoard.
    Now instead of just saying the board was edited, we will always remember the most recent position
    after which there are no more pseudo-moves representing edit actions.
    This allows us to save/load PGN files based on edited positions.
    More work needs to be done:
    - Need to allow move undo/redo to work in edited positions.
    - Because the game history is stored now inside the ChessBoard, it seems silly to have an external copy also!
    - Move numbers should be based on FEN when PGN is saved.
    In general, this is a scary change, so it needs more careful testing before being published.

    Revision 1.9  2008/11/12 20:01:34  Don.Cross
    Completely rewrote the PGN file loader to properly scan tokens.
    This allows it to better handle finding the end-of-game markers, and will allow
    adding more advanced features in the future if desired.
    When saving to a PGN file, the code was failing to write an end-of-game result marker,
    but this is required by the PGN spec.

    Revision 1.8  2008/11/02 21:04:27  Don.Cross
    Building xchenard for Win32 console mode now, so I can debug some stuff.
    Tweaked code to make Microsoft compiler warnings go away: deprecated functions and implicit type conversions.
    Checking in with version set to 1.51 for WinChen.

    Revision 1.7  2006/01/28 21:44:16  dcross
    1. Updated auto-save feature to use PGN format instead of old GAM format.
       This means we are sacrificing the ability to auto-save edited games, but I don't care.
    2. Added a feature in class ChessGame: if the file 'game.counter' exists, auto-save
       to file based on integer counter it contains, and increment that counter in the file.
       This is so I no longer have to manually make up unique filenames and remember to
       save files when Chenard is in "combat mode".
    3. Reworked PGN save functions so they can be shared by Win32 and portable versions.
       This requires all versions of Chenard to define functions GetWhitePlayerString and
       GetBlackPlayerString, so the players can be recorded in the PGN header.

    Revision 1.6  2006/01/20 21:12:40  dcross
    Renamed "loadpgn" command to "import", because now it can read in
    PGN, Yahoo, or GAM formats.  To support PGN files that contain multiple
    game listings, I had to add a bool& parameter to GetMove method.
    Caller should check this and reset the board if needed.

    Revision 1.5  2006/01/18 19:58:45  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.4  2006/01/15 19:56:40  dcross
    Fixed oopsie that made us unable to scan "o-o-o" properly in a Yahoo game listing.

    Revision 1.3  2006/01/15 15:22:52  dcross
    Removed garbage commented includes

    Revision 1.2  2006/01/14 22:21:00  dcross
    Added support for analyzing PGN games in addition to existing support for GAM and Yahoo formats.

    Revision 1.1  2006/01/14 17:13:06  dcross
    Added new class library for auto-detecting chess game file format and reading moves from it.
    Now using this new class for "portable.exe -a" game analysis.
    Still need to add support for PGN files:  now it supports only GAM and Yahoo formats.
    Also would be nice to go back and make File/Open in winchen.exe use this stuff.

*/

