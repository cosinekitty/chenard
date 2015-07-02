/*==========================================================================

     wbrdbuf.cpp  -  Copyright (C) 1993-2005 by Don Cross

     Contains BoardDisplayBuffer for Win32 GUI version of Chenard.

==========================================================================*/

#include "chess.h"
#include "winchess.h"
#include "winguich.h"
#include "resource.h"

int RawBitmapDX = 40;
int RawBitmapDY = 40;
int BitmapScaleFactor = 1;
int BitmapScaleDenom = 1;
int ChessBoardSize = 0;    // 0=small, 1=medium, 2=large

int PieceFont_DX [NUM_PIECE_FONTS] = {40, 48, 48};
int PieceFont_DY [NUM_PIECE_FONTS] = {40, 48, 48};


void NewBoardSize ( int newSize )
{
    if ( newSize < 0 || newSize > 2 )
    {
        newSize = 0;
    }

    ChessBoardSize = newSize;
    switch (ChessBoardSize)
    {
    case 1:   BitmapScaleFactor = 3;  BitmapScaleDenom = 2;  break;
    case 2:   BitmapScaleFactor = 2;  BitmapScaleDenom = 1;  break;
    default:  BitmapScaleFactor = 1;  BitmapScaleDenom = 1;  break;
    }
}


BoardDisplayBuffer::BoardDisplayBuffer():
    bitmapsLoadedFlag ( 0 ),
    whiteViewFlag ( true ),
    algebraicCoordWhiteViewFlag ( true ),
    readingMoveFlag ( false ),
    gotSource ( false ),
    moveSource ( 0 ),
    moveDest ( 0 ),
    selX ( -1 ),
    selY ( -1 ),
    hiliteSourceX ( -1 ),
    hiliteSourceY ( -1 ),
    hiliteDestX ( -1 ),
    hiliteDestY ( -1 ),
    hiliteKeyX ( 3 ),
    hiliteKeyY ( 3 ),
    pieceFont ( PIECE_FONT_TILBURG ),
    algebraicRank('\0'),
    tempHDC ( 0 )
{
    for ( int x=0; x < 8; x++ )
    {
        for ( int y=0; y < 8; y++ )
        {
            board[x][y] = EMPTY;
            changed[x][y] = false;
        }
    }

    RawBitmapDX = PieceFont_DX [pieceFont];
    RawBitmapDY = PieceFont_DY [pieceFont];
}


BoardDisplayBuffer::~BoardDisplayBuffer()
{
    unloadBitmaps();
}


void BoardDisplayBuffer::unloadBitmaps()
{
    if ( bitmapsLoadedFlag )
    {
        bitmapsLoadedFlag = 0;
        if ( wpw != NULL )  {  DeleteObject ( wpw );  wpw = NULL;  }
        if ( wnw != NULL )  {  DeleteObject ( wnw );  wnw = NULL;  }
        if ( wbw != NULL )  {  DeleteObject ( wbw );  wbw = NULL;  }
        if ( wrw != NULL )  {  DeleteObject ( wrw );  wrw = NULL;  }
        if ( wqw != NULL )  {  DeleteObject ( wqw );  wqw = NULL;  }
        if ( wkw != NULL )  {  DeleteObject ( wkw );  wkw = NULL;  }
        if ( wpb != NULL )  {  DeleteObject ( wpb );  wpb = NULL;  }
        if ( wnb != NULL )  {  DeleteObject ( wnb );  wnb = NULL;  }
        if ( wbb != NULL )  {  DeleteObject ( wbb );  wbb = NULL;  }
        if ( wrb != NULL )  {  DeleteObject ( wrb );  wrb = NULL;  }
        if ( wqb != NULL )  {  DeleteObject ( wqb );  wqb = NULL;  }
        if ( wkb != NULL )  {  DeleteObject ( wkb );  wkb = NULL;  }
        if ( bpw != NULL )  {  DeleteObject ( bpw );  bpw = NULL;  }
        if ( bnw != NULL )  {  DeleteObject ( bnw );  bnw = NULL;  }
        if ( bbw != NULL )  {  DeleteObject ( bbw );  bbw = NULL;  }
        if ( brw != NULL )  {  DeleteObject ( brw );  brw = NULL;  }
        if ( bqw != NULL )  {  DeleteObject ( bqw );  bqw = NULL;  }
        if ( bkw != NULL )  {  DeleteObject ( bkw );  bkw = NULL;  }
        if ( bpb != NULL )  {  DeleteObject ( bpb );  bpb = NULL;  }
        if ( bnb != NULL )  {  DeleteObject ( bnb );  bnb = NULL;  }
        if ( bbb != NULL )  {  DeleteObject ( bbb );  bbb = NULL;  }
        if ( brb != NULL )  {  DeleteObject ( brb );  brb = NULL;  }
        if ( bqb != NULL )  {  DeleteObject ( bqb );  bqb = NULL;  }
        if ( bkb != NULL )  {  DeleteObject ( bkb );  bkb = NULL;  }
        if ( qmw != NULL )  {  DeleteObject ( qmw );  qmw = NULL;  }
        if ( qmb != NULL )  {  DeleteObject ( qmb );  qmb = NULL;  }
        if ( ew != NULL )   {  DeleteObject ( ew  );  ew = NULL;   }
        if ( eb != NULL )   {  DeleteObject ( eb  );  eb = NULL;   }
    }
}



void BoardDisplayBuffer::loadBitmaps ( HINSTANCE progInstance )
{
    if ( bitmapsLoadedFlag )
        return;

    if ( pieceFont == PIECE_FONT_TILBURG )
    {
        // White pieces on white background...
        wpw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_T_WP_W) );
        wnw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_T_WN_W) );
        wbw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_T_WB_W) );
        wrw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_T_WR_W) );
        wqw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_T_WQ_W) );
        wkw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_T_WK_W) );

        // White pieces on black background...
        wpb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_T_WP_B) );
        wnb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_T_WN_B) );
        wbb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_T_WB_B) );
        wrb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_T_WR_B) );
        wqb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_T_WQ_B) );
        wkb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_T_WK_B) );

        // Black pieces on white background...
        bpw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_T_BP_W) );
        bnw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_T_BN_W) );
        bbw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_T_BB_W) );
        brw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_T_BR_W) );
        bqw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_T_BQ_W) );
        bkw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_T_BK_W) );

        // Black pieces on black background...
        bpb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_T_BP_B) );
        bnb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_T_BN_B) );
        bbb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_T_BB_B) );
        brb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_T_BR_B) );
        bqb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_T_BQ_B) );
        bkb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_T_BK_B) );

        // Special bitmaps...
        qmw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_T_X_W) );
        qmb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_T_X_B) );
        ew  = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_LARGE_EMPTY_W) );
        eb  = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_LARGE_EMPTY_B) );
    }
    else if ( pieceFont == PIECE_FONT_SKAK )
    {
        // White pieces on white background...
        wpw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_S_WP_W) );
        wnw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_S_WN_W) );
        wbw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_S_WB_W) );
        wrw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_S_WR_W) );
        wqw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_S_WQ_W) );
        wkw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_S_WK_W) );

        // White pieces on black background...
        wpb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_S_WP_B) );
        wnb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_S_WN_B) );
        wbb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_S_WB_B) );
        wrb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_S_WR_B) );
        wqb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_S_WQ_B) );
        wkb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_S_WK_B) );

        // Black pieces on white background...
        bpw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_S_BP_W) );
        bnw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_S_BN_W) );
        bbw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_S_BB_W) );
        brw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_S_BR_W) );
        bqw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_S_BQ_W) );
        bkw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_S_BK_W) );

        // Black pieces on black background...
        bpb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_S_BP_B) );
        bnb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_S_BN_B) );
        bbb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_S_BB_B) );
        brb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_S_BR_B) );
        bqb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_S_BQ_B) );
        bkb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_S_BK_B) );

        // Special bitmaps...
        qmw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_T_X_W) );
        qmb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_T_X_B) );
        ew  = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_LARGE_EMPTY_W) );
        eb  = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_LARGE_EMPTY_B) );
    }
    else
    {
        pieceFont = PIECE_FONT_ORIGINAL;
        // White pieces on white background...
        wpw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_O_WP_W) );
        wnw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_O_WN_W) );
        wbw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_O_WB_W) );
        wrw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_O_WR_W) );
        wqw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_O_WQ_W) );
        wkw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_O_WK_W) );

        // White pieces on black background...
        wpb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_O_WP_B) );
        wnb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_O_WN_B) );
        wbb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_O_WB_B) );
        wrb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_O_WR_B) );
        wqb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_O_WQ_B) );
        wkb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_O_WK_B) );

        // Black pieces on white background...
        bpw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_O_BP_W) );
        bnw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_O_BN_W) );
        bbw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_O_BB_W) );
        brw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_O_BR_W) );
        bqw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_O_BQ_W) );
        bkw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_O_BK_W) );

        // Black pieces on black background...
        bpb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_O_BP_B) );
        bnb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_O_BN_B) );
        bbb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_O_BB_B) );
        brb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_O_BR_B) );
        bqb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_O_BQ_B) );
        bkb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_O_BK_B) );

        // Special bitmaps...
        qmw = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_O_X_W) );
        qmb = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_O_X_B) );
        ew  = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_SMALL_EMPTY_W) );
        eb  = LoadBitmap ( progInstance, MAKEINTRESOURCE(IDB_SMALL_EMPTY_B) );
    }

    RawBitmapDX = PieceFont_DX [pieceFont];
    RawBitmapDY = PieceFont_DY [pieceFont];
    bitmapsLoadedFlag = 1;
}


void BoardDisplayBuffer::changePieceFont ( int newPieceFont )
{
    if ( newPieceFont != pieceFont )
    {
        unloadBitmaps();
        pieceFont = newPieceFont;
        loadBitmaps (global_hInstance);
    }
}


// The following function comes from Chapter 13 of Charles Petzold's
// book "Programming Windows 3.1".

void DrawBitmap (
    HDC hdc,
    HBITMAP hBitmap,
    short xStart,
    short yStart )
{
    HDC hdcMem = CreateCompatibleDC(hdc);
    SelectObject ( hdcMem, hBitmap );
    SetMapMode ( hdcMem, GetMapMode(hdc) );

    BITMAP bm;
    GetObject ( hBitmap, sizeof(BITMAP), &bm );
    POINT ptSize;
    ptSize.x = bm.bmWidth;
    ptSize.y = bm.bmHeight;
    DPtoLP ( hdc, &ptSize, 1 );

    POINT ptOrg;
    ptOrg.x = 0;
    ptOrg.y = 0;
    DPtoLP ( hdcMem, &ptOrg, 1 );

    StretchBlt (
        hdc, xStart, yStart,
        (BitmapScaleFactor * ptSize.x) / BitmapScaleDenom,
        (BitmapScaleFactor * ptSize.y) / BitmapScaleDenom,
        hdcMem, ptOrg.x, ptOrg.y,
        ptSize.x, ptSize.y,
        SRCCOPY );

    DeleteDC (hdcMem);
}


void DrawOffboard ( HDC hdc, int x, int y, bool whiteViewFlag )
{
    if ( !whiteViewFlag )
    {
        x = 7 - x;
        y = 7 - y;
    }

    RECT rect;
    rect.left   = SQUARE_SCREENX1(x);
    rect.right  = SQUARE_SCREENX2(x)+1;
    rect.top    = SQUARE_SCREENY1(y);
    rect.bottom = SQUARE_SCREENY2(y)+1;

    HBRUSH hbrush = CreateSolidBrush (CHESS_BACKGROUND_COLOR);
    HBRUSH oldBrush = (HBRUSH)SelectObject ( hdc, hbrush );
    FillRect ( hdc, &rect, hbrush );
    SelectObject ( hdc, oldBrush );
    DeleteObject ( hbrush );
}


void BoardDisplayBuffer::drawSquare (
    HDC hdc,
    SQUARE square,
    int x, int y )
{
    HBITMAP hbm;
    HBRUSH  oldbrush;
    HPEN    hpen;
    HPEN    oldpen;
    int whiteSquare = (x + y) & 1;

    switch ( square )
    {
    case EMPTY:       hbm = whiteSquare ? ew  : eb;   break;

    case WPAWN:       hbm = whiteSquare ? wpw : wpb;  break;
    case WKNIGHT:     hbm = whiteSquare ? wnw : wnb;  break;
    case WBISHOP:     hbm = whiteSquare ? wbw : wbb;  break;
    case WROOK:       hbm = whiteSquare ? wrw : wrb;  break;
    case WQUEEN:      hbm = whiteSquare ? wqw : wqb;  break;
    case WKING:       hbm = whiteSquare ? wkw : wkb;  break;

    case BPAWN:       hbm = whiteSquare ? bpw : bpb;  break;
    case BKNIGHT:     hbm = whiteSquare ? bnw : bnb;  break;
    case BBISHOP:     hbm = whiteSquare ? bbw : bbb;  break;
    case BROOK:       hbm = whiteSquare ? brw : brb;  break;
    case BQUEEN:      hbm = whiteSquare ? bqw : bqb;  break;
    case BKING:       hbm = whiteSquare ? bkw : bkb;  break;

    case OFFBOARD:    DrawOffboard(hdc,x,y,whiteViewFlag);  return;

    default:          hbm = whiteSquare ? qmw : qmb;
    }

    short xStart, yStart;

    if ( whiteViewFlag )
    {
        xStart = SQUARE_SCREENX1 ( x );
        yStart = SQUARE_SCREENY1 ( y );
    }
    else
    {
        xStart = SQUARE_SCREENX1 ( 7 - x );
        yStart = SQUARE_SCREENY1 ( 7 - y );
    }

    DrawBitmap ( hdc, hbm, xStart, yStart );

    if ( selX == x && selY == y )
    {
        const int inward = 4;
        const int curve = 10;

        int xLeft = xStart + inward;
        int yTop  = yStart + inward;

        int xRight = xStart + CHESS_BITMAP_DX - inward;
        int yBottom = yStart + CHESS_BITMAP_DY - inward;

        oldbrush = (HBRUSH) SelectObject ( hdc, GetStockObject(NULL_BRUSH) );
        hpen = CreatePen ( PS_DOT, 0, RGB(127,0,0) );
        oldpen = (HPEN) SelectObject ( hdc, hpen );
        RoundRect ( hdc, xLeft, yTop, xRight, yBottom, curve, curve );
        SelectObject ( hdc, oldpen );
        SelectObject ( hdc, oldbrush );
        DeleteObject ( hpen );
    }

    extern bool Global_HiliteMoves;
    if ( Global_HiliteMoves )
    {
        if (hiliteDestX == x && hiliteDestY == y)
        {
            oldbrush = (HBRUSH) SelectObject ( hdc, GetStockObject(NULL_BRUSH) );
            hpen = CreatePen ( PS_SOLID, 0, RGB(128,128,0) );
            oldpen = (HPEN) SelectObject ( hdc, hpen );

            const int inward = 0;
            int xLeft   =  xStart + inward;
            int yTop    =  yStart + inward;
            int xRight  =  xStart + CHESS_BITMAP_DX - inward;
            int yBottom =  yStart + CHESS_BITMAP_DY - inward;
            Rectangle ( hdc, xLeft, yTop, xRight, yBottom );

            SelectObject ( hdc, oldpen );
            SelectObject ( hdc, oldbrush );
            DeleteObject ( hpen );
        }
    }

    if (readingMoveFlag && (hiliteKeyX == x) && (hiliteKeyY == y))
    {
        // Display keyboard move selector...

        oldbrush = (HBRUSH) SelectObject ( hdc, GetStockObject(NULL_BRUSH) );
        hpen = CreatePen ( PS_SOLID, 0, RGB(128,128,0) );
        oldpen = (HPEN) SelectObject ( hdc, hpen );

        const int inward = 3;
        int xLeft   =  xStart + inward;
        int yTop    =  yStart + inward;
        int xRight  =  xStart + CHESS_BITMAP_DX - inward;
        int yBottom =  yStart + CHESS_BITMAP_DY - inward;
        Rectangle ( hdc, xLeft, yTop, xRight, yBottom );

        SelectObject ( hdc, oldpen );
        SelectObject ( hdc, oldbrush );
        DeleteObject ( hpen );
    }

    changed[x][y] = false;
}


void BoardDisplayBuffer::touchSquare (int x, int y)
{
    if (DisplayCoordsValid(x,y))
    {
        changed[x][y] = true;
        freshenSquare (x, y);
    }
}


void BoardDisplayBuffer::selectSquare ( int x, int y )
{
    touchSquare (selX, selY);

    selX = x;
    selY = y;

    touchSquare (selX, selY);
}


void BoardDisplayBuffer::keySelectSquare (int x, int y)
{
    touchSquare (hiliteKeyX, hiliteKeyY);

    hiliteKeyX = x;
    hiliteKeyY = y;

    touchSquare (hiliteKeyX, hiliteKeyY);
}


void BoardDisplayBuffer::keyMove (int dx, int dy)
{
    if (readingMoveFlag)
    {
        if (DisplayCoordsValid (hiliteKeyX, hiliteKeyY))
        {
            if (!whiteViewFlag)
            {
                dx = -dx;
                dy = -dy;
            }
            if (DisplayCoordsValid (hiliteKeyX + dx, hiliteKeyY + dy))
            {
                keySelectSquare (hiliteKeyX + dx, hiliteKeyY + dy);
            }
        }
    }
}


void BoardDisplayBuffer::informMoveCoords (
    int sourceX, int sourceY,
    int destX,   int destY )
{
    hiliteSourceX  =  sourceX;
    hiliteSourceY  =  sourceY;
    hiliteDestX    =  destX;
    hiliteDestY    =  destY;
}


void BoardDisplayBuffer::drawVector ( HDC hdc )
{
    if ( vx1 != -1 )
    {
        MoveToEx ( hdc, vx1, vy1, NULL );
        LineTo ( hdc, vx2, vy2 );
    }
}


void BoardDisplayBuffer::draw (
    HDC hdc,
    int minx, int maxx,
    int miny, int maxy )
{
    if ( maxx < 0 || minx > 7 || maxy < 0 || miny > 7 )  return;
    if ( minx < 0 )  minx = 0;
    if ( maxx > 7 )  maxx = 7;
    if ( miny < 0 )  miny = 0;
    if ( maxy > 7 )  maxy = 7;

    if ( !whiteViewFlag )
    {
        minx = 7 - minx;
        maxx = 7 - maxx;
        miny = 7 - miny;
        maxy = 7 - maxy;

        // swap 'em

        int t = minx;
        minx = maxx;
        maxx = t;

        t = miny;
        miny = maxy;
        maxy = t;
    }

    loadBitmaps (global_hInstance);

    for ( int y=miny; y <= maxy; y++ )
    {
        for ( int x=minx; x <= maxx; x++ )
        {
            // Put the bitmap in the presentation space given.
            drawSquare ( hdc, board[x][y], x, y );
        }
    }

    HBRUSH oldbrush = (HBRUSH) SelectObject ( hdc, GetStockObject(NULL_BRUSH) );
    HPEN oldpen = (HPEN) SelectObject ( hdc, GetStockObject(BLACK_PEN) );

    Rectangle (
        hdc,
        SQUARE_SCREENX1(0)-1, SQUARE_SCREENY1(7)-1,
        SQUARE_SCREENX2(7)+2, SQUARE_SCREENY2(0)+2 );

    SelectObject ( hdc, oldpen );
    SelectObject ( hdc, oldbrush );
}


void BoardDisplayBuffer::setSquareContents ( int x, int y, SQUARE s )
{
    if (DisplayCoordsValid(x,y))
    {
        board[x][y] = s;
        changed[x][y] = true;
    }
}


void BoardDisplayBuffer::update ( const ChessBoard &b )
{
    for ( int x=0; x < 8; x++ )
    {
        for ( int y=0; y < 8; y++ )
        {
            SQUARE s = b.GetSquareContents ( x, y );

            if ( s != board[x][y] )
            {
                board[x][y] = s;
                changed[x][y] = true;
            }
        }
    }
}


void BoardDisplayBuffer::startReadingMove ( bool whiteIsMoving )
{
    readingMoveFlag = true;
    gotSource = false;
    moverIsWhite = whiteIsMoving;
}


bool BoardDisplayBuffer::isReadingMove() const
{
    return readingMoveFlag;
}


void BoardDisplayBuffer::squareSelectedNotify ( int x, int y )
{
    int beepFreq;

    if ( x<0 || x>7 || y<0 || y>7 )
        return;   // It's off the Vulcan board!!!!

    int ofs = OFFSET ( 2+x, 2+y );

    if ( readingMoveFlag )
    {
        int sideMask = moverIsWhite ? WHITE_MASK : BLACK_MASK;
        bool looksValid = false;

        if ( !gotSource )
        {
            if ( board[x][y] & sideMask )
            {
                looksValid = true;
                beepFreq = 1000;
                gotSource = true;
                moveSource = ofs;
                selectSquare ( x, y );
            }
        }
        else
        {
            looksValid = true;
            beepFreq = 1100;
            moveDest = ofs;
            readingMoveFlag = false;
            deselectSquare();
        }

        if ( looksValid )
        {
            // Put stuff here to do for all valid-looking square selections!

            ChessBeep ( beepFreq, 50 );
        }
    }
}


void BoardDisplayBuffer::copyMove ( int &source, int &dest )
{
    source = moveSource;
    dest = moveDest;
}


void BoardDisplayBuffer::setView ( bool newWhiteView )
{
    whiteViewFlag = newWhiteView;
}


void BoardDisplayBuffer::freshenSquare ( int x, int y )
{
    RECT rect;

    if ( !whiteViewFlag )
    {
        x = 7 - x;
        y = 7 - y;
    }

    rect.left   = SQUARE_SCREENX1(x);
    rect.top    = SQUARE_SCREENY1(y);
    rect.right  = SQUARE_SCREENX2(x);
    rect.bottom = SQUARE_SCREENY2(y);

    InvalidateRect ( HwndMain, &rect, FALSE );
}


void BoardDisplayBuffer::freshenBoard()
{
    updateAlgebraicCoords();

    RECT rect;
    rect.left   =   SQUARE_SCREENX1(0);
    rect.top    =   SQUARE_SCREENY1(7);
    rect.right  =   SQUARE_SCREENX2(7);
    rect.bottom =   SQUARE_SCREENY2(0);

    InvalidateRect ( HwndMain, &rect, FALSE );
}


void BoardDisplayBuffer::updateAlgebraicCoords()
{
    if ( algebraicCoordWhiteViewFlag != whiteViewFlag )
    {
        algebraicCoordWhiteViewFlag = whiteViewFlag;
        char coordString[2] = {0, 0};
        for ( int coord = 0; coord < 8; ++coord )
        {
            int view = whiteViewFlag ? coord : (7 - coord);

            coordString[0] = coord + '1';
            ChessDisplayTextBuffer::SetText ( view + STATIC_ID_COORD_RANK_BASE, coordString );

            coordString[0] = coord + 'a';
            ChessDisplayTextBuffer::SetText ( view + STATIC_ID_COORD_FILE_BASE, coordString );
        }
    }
}


void BoardDisplayBuffer::sendAlgebraicChar(char c)
{
    // This method is called whenever the user presses 'a'..'h' or '1'..'8'.
    // It allows squares to be selected by typing pairs of characters,
    // calling into the same code that already gets called when a square
    // on the chess board is clicked with the mouse.
    if ((c >= 'a') && (c <= 'h'))
    {
        algebraicRank = c;
    }
    else if ((c >= '1') && (c <= '8'))
    {
        if (algebraicRank != '\0')
        {
            squareSelectedNotify(algebraicRank - 'a', c - '1');
            algebraicRank = '\0';
        }
    }
}


BoardDisplayBuffer TheBoardDisplayBuffer;


/*
    $Log: wbrdbuf.cpp,v $
    Revision 1.6  2006/01/18 19:58:17  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.5  2005/11/27 20:40:09  dcross
    1. No longer allow keyboard selector square to move or be visible when it is not a human player's turn.
    2. Jumped version number from 1.043 to 1.045, because the 1.044 source was lost (dammit).

    Revision 1.4  2005/11/26 19:16:25  dcross
    Added support for using the keyboard to make moves.

    Revision 1.3  2005/11/23 21:30:32  dcross
    Removed all references to intersrv.com, because that ISP no longer exists!
    Changed references to Chenard web site and my email address to cosinekitty.com.
    (Still need to fix cosinekitty.com/chenard.html.)

    Revision 1.2  2005/11/23 21:14:44  dcross
    1. Added cvs log tag at end of each source file.
    2. Moved old manual revision history after cvs log tag.
    3. Made sure each source file has extra blank line at end so gcc under Linux won't fuss!


         Revision history:

    1999 February 12 [Don Cross]
         Updated coding style.

    1999 January 11 [Don Cross]
         Adding new capability: hilighting dest square of all moves.
         Adding user control to enable/disable this, along with
         control over whether to animate opponent moves.

    1999 January 3 [Don Cross]
         Adding support for displaying algebraic coordinates around board.

    1997 January 30 [Don Cross]
         Adding "Skak" font.

    1996 July 29 [Don Cross]
         Started porting from OS/2 to Win32.

    1994 February 9 [Don Cross]
         Adding visual feedback for selecting squares.

*/

