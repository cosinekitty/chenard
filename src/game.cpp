/*=========================================================================

     game.cpp  -  Copyright (C) 1993-2005 by Don Cross

     Contains class ChessGame.

=========================================================================*/

#include <string.h>
#include <stdio.h>

#include "chess.h"
#include "gamefile.h"
#include "lrntree.h"


bool ReadFromFile ( FILE *f, Move &m )
{
    int data [4];

    data[0] = fgetc(f);
    data[1] = fgetc(f);
    data[2] = fgetc(f);
    data[3] = fgetc(f);

    if ( data[3] == EOF )
        return false;

    m.source = BYTE (data[0]);
    m.dest   = BYTE (data[1]);
    m.score  = INT16 (data[2] | (data[3] << 8)); 

    return true;   
}


bool WriteToFile ( FILE *f, const Move &m )
{
    if ( fputc(m.source,f) == EOF )
        return false;

    if ( fputc(m.dest,f) == EOF )
        return false;

    if ( fputc(m.score & 0xff, f) == EOF )
        return false;

    if ( fputc(m.score >> 8, f) == EOF )
        return false;

    return true; 
}


//------------------------------------------------------------------


ChessGame::ChessGame ( ChessBoard &Board, ChessUI &Ui ):
    board ( Board ),
    ui ( Ui ),
    autoSave_Filename ( 0 )
{
    whitePlayer = ui.CreatePlayer ( SIDE_WHITE );
    blackPlayer = ui.CreatePlayer ( SIDE_BLACK );
}

ChessGame::ChessGame ( 
    ChessBoard &_board, 
    ChessUI &_ui, 
    bool /*_whiteIsHuman*/, 
    bool /*_blackIsHuman*/,
    ChessPlayer *_whitePlayer,
    ChessPlayer *_blackPlayer ):
        board ( _board ),
        whitePlayer ( _whitePlayer ),
        blackPlayer ( _blackPlayer ),
        ui ( _ui ),
        autoSave_Filename ( 0 )
{
}


ChessGame::~ChessGame()
{
    if ( autoSave_Filename )
    {
        delete[] autoSave_Filename;
        autoSave_Filename = 0;
    }

    if ( whitePlayer )
    {
        delete whitePlayer;
        whitePlayer = 0;
    }

    if ( blackPlayer )
    {
        delete blackPlayer;
        blackPlayer = 0;
    }
}


bool ChessGame::AutoSaveToFile ( const char *filename )
{
    if ( autoSave_Filename )
        delete[] autoSave_Filename;

    if ( !filename )
    {
        // Interpret this as 'stop saving to file'
        autoSave_Filename = 0;
    }
    else
    {
        autoSave_Filename = new char [ strlen(filename) + 1 ];
        if ( autoSave_Filename )
            strcpy ( autoSave_Filename, filename );
        else
            return false;   // out of memory!
    }

    return true;
}


void ChessGame::DetectAutoSaveFile()
{
    // Undocumented feature that helps me gather game listings for tree trainer...
    // If the file game.counter exists, open it, scan for game number, increment,
    // and make auto-save to the file <counter>.pgn...
    const char *CounterFileName = "game.counter";
    FILE *file = fopen (CounterFileName, "rt");
    if (file) {
        int counter;
        if (1 == fscanf (file, "%d", &counter)) {
            char GameFileName [64];
            sprintf (GameFileName, "%05d.pgn", counter);
            if (AutoSaveToFile (GameFileName)) {
                fclose (file);
                file = fopen (CounterFileName, "wt");
                if (file) {
                    fprintf (file, "%d\n", ++counter);
                }
            }
        }

        if (file) {
            fclose (file);
            file = NULL;
        }
    }
}


void ChessGame::Play()
{
    if ( sizeof(INT16) != 2 || sizeof(INT32) != 4 )
    {
        // It's OK if compiler reports an "unreachable code" warning...
        ChessFatal ( "Integer types are wrong size!" );
    }

    MoveList     ml;
    Move         move;
    UnmoveInfo   unmove;

    if ( !whitePlayer || !blackPlayer )
        ChessFatal ( "Chess player(s) not initialized!" );

    move.dest = SPECIAL_MOVE_NULL;   // to assist in checking for board edit

    for(;;)
    {
        ui.DrawBoard ( board );

        if (board.GetCurrentPlyNumber() == 0) {     // handle starting multiple games
            DetectAutoSaveFile();   // every time we start a new game, increment game number
        } else {
            if (autoSave_Filename) {
                SaveGamePGN ( board, autoSave_Filename );
            }
        }

        if ( board.WhiteToMove() )
        {
            board.GenWhiteMoves ( ml );
            if ( ml.num == 0 )
            {
                // The game is over...

                whitePlayer->InformGameOver (board);
                ChessSide winner = board.WhiteInCheck() ? SIDE_BLACK : SIDE_NEITHER;
                ui.ReportEndOfGame ( winner );
                LearnTree tree;
                tree.learnFromGame ( board, winner );
                return;
            }
            else if ( !move.ShouldIgnore() && board.IsDefiniteDraw() )
            {
                whitePlayer->InformGameOver (board);
                ui.ReportEndOfGame ( SIDE_NEITHER );
                LearnTree tree;
                tree.learnFromGame ( board, SIDE_NEITHER );
                return;
            }

            INT32 timeSpent = 0;
            if ( !whitePlayer->GetMove ( board, move, timeSpent ) )
            {
                blackPlayer->InformResignation();
                ui.Resign ( SIDE_WHITE, whitePlayer->QueryQuitReason() );
                LearnTree tree;
                tree.learnFromGame ( board, SIDE_BLACK );
                return;
            }

            if ( !move.ShouldIgnore() )
            {
                ui.RecordMove ( board, move, timeSpent );
                board.MakeWhiteMove ( move, unmove, true, true );
            }
        }
        else   // Black's turn to move...
        {
            board.GenBlackMoves ( ml );
            if ( ml.num == 0 )
            {
                // The game is over...

                blackPlayer->InformGameOver (board);
                ChessSide winner = board.BlackInCheck() ? SIDE_WHITE : SIDE_NEITHER;
                ui.ReportEndOfGame ( winner );
                LearnTree tree;
                tree.learnFromGame ( board, winner );
                return;
            }
            else if ( !move.ShouldIgnore() && board.IsDefiniteDraw() )
            {
                blackPlayer->InformGameOver (board);
                ui.ReportEndOfGame ( SIDE_NEITHER );
                LearnTree tree;
                tree.learnFromGame ( board, SIDE_NEITHER );
                return;
            }

            INT32 timeSpent = 0;
            if ( !blackPlayer->GetMove ( board, move, timeSpent ) )
            {
                whitePlayer->InformResignation();
                ui.Resign ( SIDE_BLACK, blackPlayer->QueryQuitReason() );
                LearnTree tree;
                tree.learnFromGame ( board, SIDE_WHITE );
                return;
            }

            if ( !move.ShouldIgnore() )
            {
                ui.RecordMove ( board, move, timeSpent );
                board.MakeBlackMove ( move, unmove, true, true );
            }
        }
    }
}


bool SaveGame ( const ChessBoard &board, const char *filename )
{
    FILE *gameFile = fopen ( filename, "wb" );
    if ( gameFile )
    {
        UINT32 n = board.GetCurrentPlyNumber();
        for ( UINT32 i=0; i < n; i++ )
        {
            Move m = board.GetPastMove(i);
            WriteToFile ( gameFile, m );
        }
        fclose ( gameFile );
    }
    else
        return false;

    return true;
}



bool LoadGame ( ChessBoard &board, const char *filename )
{
    FILE *gameFile = fopen ( filename, "rb" );
    if ( gameFile )
    {
        board.Init();
        Move move;
        for ( UINT32 i=0; ReadFromFile(gameFile,move); ++i )
        {
            if ( (move.dest & SPECIAL_MOVE_MASK) == SPECIAL_MOVE_EDIT )
            {
                board.EditCommand ( move );
                board.SaveSpecialMove ( move );
            }
            else
            {
                UnmoveInfo unmove;
                board.MakeMove ( move, unmove );
            }
        }
        fclose ( gameFile );
    }
    else
        return false;

    return true;
}


/*
    $Log: game.cpp,v $
    Revision 1.6  2006/01/28 22:39:36  dcross
    Fixed bug in new auto-saver code:  need to switch to new game file every time
    I start a new game, not just when ChessGame::Play is entered.  So now I check
    to see if the board has been reset every time, and if so, check for game.counter.

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
    
    1993 July 14 [Don Cross]
         Changed constructor to use initializer list.
    
    1993 August 30 [Don Cross]
         Changing pointers to references in the interfaces where
         appropriate.
    
    1993 October 19 [Don Cross]
         Added calls to ChessBoard::IsDefiniteDraw() to end the
         game automatically when an obvious draw has occurred.
    
    1993 October 21 [Don Cross]
         Added the ChessUI::RecordMove() function and
         narrowed the meaning of ChessUI::DisplayMove().
    
    1993 October 22 [Don Cross]
         Adding runtime check for sizes of standard integer data types.
    
    1994 January 15 [Don Cross]
         Added check for SPECIAL_MOVE_NULL, which causes the
         move returned by GetMove to be ignored.  This is useful
         for resetting the game, and might be useful for other things.
    
    1994 January 16 [Don Cross]
         Added logic to fix problem with causing an instant draw
         and ending the game when the board is edited into a drawn
         position.
    
    1994 January 17 [Don Cross]
         Changed check for SPECIAL_MOVE_NULL to a call to Move::ShouldIgnore()
    
    1994 February 9 [Don Cross]
         Changed ChessPlayer::GetMove() to return time spent thinking.
         This way, the ComputerChessPlayer can internally exclude time
         spent animating the move.
    
    1995 March 26 [Don Cross]
         Added new constructor ChessGame::ChessGame ( ChessBoard &, ChessUI &, bool, bool ).
         This constructor refrains from creating the players, but is told what
         kind of players to expect.
    
    1995 July 13 [Don Cross]
         Added ChessGame::~ChessGame.
         Added ChessGame::AutoSaveToFile.
    
    1995 December 30 [Don Cross]
         Made ChessGame::~ChessGame delete blackPlayer and whitePlayer.
    
    1996 August 23 [Don Cross]
         Replacing old memory-based learning tree with disk-based 
         class LearnTree.
    
    1997 March 8 [Don Cross]
         Causing a resignation to issue a negative feedback for continuation
         in the learning tree.
    
    1999 January 4 [Don Cross]
         Updated coding style.
    
    1999 January 11 [Don Cross]
         Added calls to ChessPlayer::InformResignation() to notify
         remote opponent of loss of connection or resignation.
    
    1999 January 15 [Don Cross]
         Adding ReadFromFile(FILE *,Move &) and 
         WriteToFile(FILE *,const Move &) functions to allow portable
         file I/O for Move structures.  This eliminates any danger
         of endianness or structure packing non-portability.
*/

