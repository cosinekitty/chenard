/*
    Copyright (C) 1993-2008 by Don Cross.  All Rights Reserved.

    winchess.h  -  Resource file identifiers for Win32 GUI version of Chenard
*/

#include <windows.h>

#define CHENARD_ICON           1
#define DR_CHENARD_ICON        1
#define IDD_EDIT_SQUARE      102
#define DDC_MENU_RESOURCE      1
#define CB_RESTORE_GAME      522
#define RB_W_IS_SECONDS      118
#define RB_W_IS_PLIES        119
#define RB_B_IS_SECONDS      120
#define RB_B_IS_PLIES        121
#define TB_WBIAS             415
#define TB_BBIAS             414
#define CB_ENABLE_SOUNDS     513
#define IDD_DEFINE_PLAYERS   100
#define RB_WHUMAN            103
#define RB_WINTERNET         104
#define RB_WCOMPUTER         105
#define TB_WTIME             412
#define TB_BTIME             409
#define RB_BHUMAN            106
#define RB_BINTERNET         107
#define RB_BCOMPUTER         108
#define RB_W_NAMEDPIPE       109
#define RB_B_NAMEDPIPE       110
#define TB_EDIT_FEN          600

#define IDD_PROMOTE_PAWN     101
#define RB_QUEEN             201
#define RB_ROOK              202
#define RB_BISHOP            203
#define RB_KNIGHT            204

#define IDC_LOAD_FILENAME    301

#define IDD_REQUEST_SUGGESTION          167
#define IDD_DEFINE_PLAYERS2             168
#define IDD_EDIT_THINK_TIMES            169
#define IDD_ABOUT                       170
#define IDD_ENTER_FEN                   171
#define IDD_EDIT_BLUNDER_THRESHOLD      172

#define IDR_ACCELERATOR1                400

/*
    $Log: winchess.h,v $
    Revision 1.5  2008/11/17 18:47:07  Don.Cross
    WinChen.exe now has an Edit/Enter FEN function for setting a board position based on a FEN string.
    Tweaked ChessBoard::SetForsythEdwardsNotation to skip over any leading white space.
    Fixed incorrect Chenard web site URL in the about box.
    Now that computers are so much faster, reduced the

    Revision 1.4  2006/02/25 22:22:19  dcross
    Added support for connecting to Flywheel named pipe server as a Chenard chess player.
    This allows me to use my new Flywheel engine via the Chenard GUI.

    Revision 1.3  2005/11/23 21:30:32  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:44  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!

*/

