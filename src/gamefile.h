/*
    gamefile.h  -  Don Cross, January 2006.

    C++ classes to abstract the process to load chess moves from
    various kinds of files:  old chenard GAM, PGN, or
    Yahoo game listing email text.
*/


bool ReadFromFile ( FILE *, Move & );   // recycled old function

enum PGNRESULT
{
    PGNRESULT_UNKNOWN = 0,
    PGNRESULT_WHITE_WON,
    PGNRESULT_BLACK_WON,
    PGNRESULT_DRAW
};

struct PgnExtraInfo
{
    char        fen [256];      // set to "" if game started normally.
    PGNRESULT   result;
    int         whiteElo;       // set to 0 if White's ELO is not specified in the PGN file
    int         blackElo;       // set to 0 if Black's ELO is not specified in the PGN file
};

bool GetNextPgnMove (
    FILE            *file,
    char             movestr[1+MAX_MOVE_STRLEN],
    PGN_FILE_STATE  &state,
    PgnExtraInfo    &info       // will be zeroed out, then members will be set if new game is found.  may be set with EOF return as special case, if game has no moves.
);


class tChessMoveStream
{
public:
    static tChessMoveStream *OpenFileForRead (const char *filename);    // factory function for auto-detecting file format

    virtual ~tChessMoveStream()
    {
    }

    virtual bool GetNextMove(Move &, PGN_FILE_STATE &) = 0;
};


class tChessMoveFile: public tChessMoveStream
{
public:
    tChessMoveFile(FILE *_infile):
        infile(_infile)
    {
    }

    virtual ~tChessMoveFile()
    {
        if (infile)
        {
            fclose (infile);
            infile = NULL;
        }
    }

protected:
    FILE        *infile;
    ChessBoard   board;
};


class tChessMoveFile_PGN: public tChessMoveFile
{
public:
    tChessMoveFile_PGN(FILE *_infile):
        tChessMoveFile(_infile)
    {
    }

    virtual bool GetNextMove(Move &, PGN_FILE_STATE &);
};


void SavePortableGameNotation (
    FILE        *outfile,
    ChessBoard  &refboard,
    const char  *whiteName,
    const char  *blackName
);


const char *GetWhitePlayerString();     // defined on per-project basis
const char *GetBlackPlayerString();     // defined on per-project basis


inline void SavePortableGameNotation (FILE *outfile, ChessBoard &board)
{
    SavePortableGameNotation (
        outfile,
        board,
        GetWhitePlayerString(),
        GetBlackPlayerString()
    );
}


bool SaveGamePGN (
    ChessBoard  &board,
    const char  *filename,
    const char  *whiteName,
    const char  *blackName
);


bool SaveGamePGN (
    ChessBoard  &board,
    const char  *filename
);


/*
    $Log: gamefile.h,v $
    Revision 1.7  2008/12/01 23:08:53  Don.Cross
    Norm Pollock announced on the WinBoard forum that he has updated his PGN database:
        http://www.open-aurec.com/wbforum/viewtopic.php?t=49522
        http://www.hoflink.com/~npollock/chess.html
    This is just what I was looking for, so that I could beef up chenard.trx (training file).
    I modified the PGN scanner to recognize WhiteElo, BlackElo, and Result tags.
    I am using WhiteElo and BlackElo to decide whether a game is trustworthy enough to pretend like Chenard thought
    of the move itself, so that it will play that move in actual games, instead of just as a reference point for opponent moves.
    I am currently in the process of slurping an enormous amount of openings into chenard.trx.
    I decided to move GetNextPgnMove from board.cpp to gamefile.cpp, where it really belongs.

    Revision 1.6  2008/11/12 23:06:07  Don.Cross
    Massive refactoring of the concept of an 'editedFlag' in ChessBoard.
    Now instead of just saying the board was edited, we will always remember the most recent position
    after which there are no more pseudo-moves representing edit actions.
    This allows us to save/load PGN files based on edited positions.
    More work needs to be done:
    - Need to allow move undo/redo to work in edited positions.
    - Because the game history is stored now inside the ChessBoard, it seems silly to have an external copy also!
    - Move numbers should be based on FEN when PGN is saved.
    In general, this is a scary change, so it needs more careful testing before being published.

    Revision 1.5  2006/01/28 21:44:16  dcross
    1. Updated auto-save feature to use PGN format instead of old GAM format.
       This means we are sacrificing the ability to auto-save edited games, but I don't care.
    2. Added a feature in class ChessGame: if the file 'game.counter' exists, auto-save
       to file based on integer counter it contains, and increment that counter in the file.
       This is so I no longer have to manually make up unique filenames and remember to
       save files when Chenard is in "combat mode".
    3. Reworked PGN save functions so they can be shared by Win32 and portable versions.
       This requires all versions of Chenard to define functions GetWhitePlayerString and
       GetBlackPlayerString, so the players can be recorded in the PGN header.

    Revision 1.4  2006/01/20 21:12:40  dcross
    Renamed "loadpgn" command to "import", because now it can read in
    PGN, Yahoo, or GAM formats.  To support PGN files that contain multiple
    game listings, I had to add a bool& parameter to GetMove method.
    Caller should check this and reset the board if needed.

    Revision 1.3  2006/01/18 19:58:11  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.2  2006/01/14 22:21:00  dcross
    Added support for analyzing PGN games in addition to existing support for GAM and Yahoo formats.

    Revision 1.1  2006/01/14 17:13:06  dcross
    Added new class library for auto-detecting chess game file format and reading moves from it.
    Now using this new class for "portable.exe -a" game analysis.
    Still need to add support for PGN files:  now it supports only GAM and Yahoo formats.
    Also would be nice to go back and make File/Open in winchen.exe use this stuff.

*/
