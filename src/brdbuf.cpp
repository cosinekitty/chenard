/*==========================================================================

     brdbuf.cpp  -  Copyright (C) 1993-1996 by Don Cross
     email: dcross@intersrv.com
     WWW:   http://www.intersrv.com/~dcross/

     Contains BoardDisplayBuffer for OS/2 PM version of NewChess.

==========================================================================*/

#define INCL_PM
#define INCL_DOSPROCESS
#define INCL_GPIPRIMITIVES
#include <os2.h>

#include "chess.h"
#include "os2chess.h"
#include "os2pmch.h"


BoardDisplayBuffer::BoardDisplayBuffer():
   bitmapsLoadedFlag ( 0 ),
   whiteViewFlag ( true ),
   readingMoveFlag ( false ),
   gotSource ( false ),
   moveSource ( 0 ),
   moveDest ( 0 ),
   selX ( -1 ),
   selY ( -1 )
{
   for ( int x=0; x < 8; x++ )
   {
      for ( int y=0; y < 8; y++ )
      {
         board[x][y] = EMPTY;
         changed[x][y] = false;
      }
   }
}


BoardDisplayBuffer::~BoardDisplayBuffer()
{
}


void BoardDisplayBuffer::loadBitmaps ( HPS hps )
{
   // White pieces on white background...
   wpw = GpiLoadBitmap ( hps, 0, WPawn_W, 0, 0 );
   wnw = GpiLoadBitmap ( hps, 0, WKnight_W, 0, 0 );
   wbw = GpiLoadBitmap ( hps, 0, WBishop_W, 0, 0 );
   wrw = GpiLoadBitmap ( hps, 0, WRook_W, 0, 0 );
   wqw = GpiLoadBitmap ( hps, 0, WQueen_W, 0, 0 );
   wkw = GpiLoadBitmap ( hps, 0, WKing_W, 0, 0 );

   // White pieces on black background...
   wpb = GpiLoadBitmap ( hps, 0, WPawn_B, 0, 0 );
   wnb = GpiLoadBitmap ( hps, 0, WKnight_B, 0, 0 );
   wbb = GpiLoadBitmap ( hps, 0, WBishop_B, 0, 0 );
   wrb = GpiLoadBitmap ( hps, 0, WRook_B, 0, 0 );
   wqb = GpiLoadBitmap ( hps, 0, WQueen_B, 0, 0 );
   wkb = GpiLoadBitmap ( hps, 0, WKing_B, 0, 0 );

   // Black pieces on white background...
   bpw = GpiLoadBitmap ( hps, 0, BPawn_W, 0, 0 );
   bnw = GpiLoadBitmap ( hps, 0, BKnight_W, 0, 0 );
   bbw = GpiLoadBitmap ( hps, 0, BBishop_W, 0, 0 );
   brw = GpiLoadBitmap ( hps, 0, BRook_W, 0, 0 );
   bqw = GpiLoadBitmap ( hps, 0, BQueen_W, 0, 0 );
   bkw = GpiLoadBitmap ( hps, 0, BKing_W, 0, 0 );

   // Black pieces on black background...
   bpb = GpiLoadBitmap ( hps, 0, BPawn_B, 0, 0 );
   bnb = GpiLoadBitmap ( hps, 0, BKnight_B, 0, 0 );
   bbb = GpiLoadBitmap ( hps, 0, BBishop_B, 0, 0 );
   brb = GpiLoadBitmap ( hps, 0, BRook_B, 0, 0 );
   bqb = GpiLoadBitmap ( hps, 0, BQueen_B, 0, 0 );
   bkb = GpiLoadBitmap ( hps, 0, BKing_B, 0, 0 );

   // Special bitmaps...
   qmw = GpiLoadBitmap ( hps, 0, Qmark_W, 0, 0 );
   qmb = GpiLoadBitmap ( hps, 0, Qmark_B, 0, 0 );
   ew  = GpiLoadBitmap ( hps, 0, Empty_W, 0, 0 );
   eb  = GpiLoadBitmap ( hps, 0, Empty_B, 0, 0 );
}


void BoardDisplayBuffer::drawSquare ( HPS hps,
                                      SQUARE square,
                                      int x, int y )
{
   HBITMAP hbm;
   int whiteSquare = (x + y) & 1;

   switch ( NOINDEX(square) )
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

      default:          hbm = whiteSquare ? qmw : qmb;
   }

   POINTL ptl;

   if ( whiteViewFlag )
   {
      ptl.x = SQUARE_SCREENX1 ( x );
      ptl.y = SQUARE_SCREENY1 ( y );
   }
   else
   {
      ptl.x = SQUARE_SCREENX1 ( 7 - x );
      ptl.y = SQUARE_SCREENY1 ( 7 - y );
   }

   WinDrawBitmap ( hps, hbm, 0, &ptl,
                   CLR_NEUTRAL, CLR_BACKGROUND, DBM_NORMAL );

   if ( selX == x && selY == y )
   {
      const int inward = 4;
      const int curve = 10;

      POINTL p1;
      p1.x = ptl.x + inward;
      p1.y = ptl.y + inward;

      POINTL p2;
      p2.x = ptl.x + CHESS_BITMAP_DX - inward;
      p2.y = ptl.y + CHESS_BITMAP_DY - inward;

      GpiSetColor ( hps, CLR_RED );
      GpiMove ( hps, &p1 );
      GpiBox ( hps, DRO_OUTLINE, &p2, curve, curve );
   }

   changed[x][y] = false;
}


void BoardDisplayBuffer::selectSquare ( int x, int y )
{
   if ( selX>=0 && selX<=7 && selY>=0 && selY<=7 )
   {
      changed[selX][selY] = true;
      freshenSquare ( selX, selY );
   }

   selX = x;
   selY = y;

   if ( selX>=0 && selX<=7 && selY>=0 && selY<=7 )
   {
      changed[selX][selY] = true;
      freshenSquare ( selX, selY );
   }
}


void BoardDisplayBuffer::draw ( HPS hps,
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
      int t;

      minx = 7 - minx;
      maxx = 7 - maxx;
      miny = 7 - miny;
      maxy = 7 - maxy;

      // swap 'em

      t = minx;
      minx = maxx;
      maxx = t;

      t = miny;
      miny = maxy;
      maxy = t;
   }

   if ( !bitmapsLoadedFlag )
   {
      loadBitmaps(hps);
      bitmapsLoadedFlag = 1;
   }

   for ( int y=miny; y <= maxy; y++ )
   {
      for ( int x=minx; x <= maxx; x++ )
      {
         // Put the bitmap in the presentation space given.
         drawSquare ( hps, board[x][y], x, y );
      }
   }
}


void BoardDisplayBuffer::setSquareContents ( int x, int y, SQUARE s )
{
   if ( x>=0 && x<=7 && y>=0 && y<=7 )
   {
      board[x][y] = NOINDEX(s);
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
            board[x][y] = NOINDEX(s);
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
   {
      return;   // It's off the Vulcan board!!!!
   }

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
   RECTL rect;

   if ( !whiteViewFlag )
   {
      x = 7 - x;
      y = 7 - y;
   }

   rect.xLeft   = SQUARE_SCREENX1(x);
   rect.yBottom = SQUARE_SCREENY1(y);
   rect.xRight  = SQUARE_SCREENX2(x);
   rect.yTop    = SQUARE_SCREENY2(y);

   WinInvalidateRect ( global_hwndClient, &rect, 1 );
}


void BoardDisplayBuffer::freshenBoard()
{
   RECTL rect;

   rect.xLeft   = SQUARE_SCREENX1(0);
   rect.yBottom = SQUARE_SCREENY1(0);
   rect.xRight  = SQUARE_SCREENX2(7);
   rect.yTop    = SQUARE_SCREENY2(7);

   WinInvalidateRect ( global_hwndClient, &rect, 1 );
}


BoardDisplayBuffer TheBoardDisplayBuffer;



/*
    $Log: brdbuf.cpp,v $
    Revision 1.2  2006/01/18 19:58:10  dcross
    I finally got around to scrubbing out silly cBOOLEAN, cFALSE, and cTRUE.
    Now use C++ standard bool, true, false (none of which existed when I started this project).

    Revision 1.1  2005/11/25 19:47:26  dcross
    Recovered lots of old chess source files from the dead!
    Found these on CDROM marked "14 September 1999".
    Added cvs log tag and moved old revision history after that.


     Revision history:

1994 February 9 [Don Cross]
     Adding visual feedback for selecting squares.

*/

