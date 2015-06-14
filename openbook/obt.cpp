/*===========================================================================

      obt.cpp  -  Copyright (C) 1993-2008 by Don Cross
      email: cosinekitty@hotmail.com
      WWW:   http://cosinekitty.com/chenard

      Opening Book Translator for Dr. Chenard.

      Translates 'chenard.obt' into 'openbook.cpp'.

===========================================================================*/

// Standard includes...

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#ifdef _MSC_VER
	// Microsoft C++ compiler.
	// Disable warnings about deprecated functions like sprintf(), sscanf(), etc.
	// I would like to fix these, but no equivalent exists in Linux that I can tell.
	// Maybe some day I will make macros that expand to sprintf_s in Microsoft, or sprintf in Linux.
	#pragma warning (disable: 4996)
#endif // _MSC_VER

// DDCLIB includes...

#include "contain.h"


//------------------------------------------------------------------------


class IntegerSink
{
public:
   IntegerSink ( FILE *_outfile ):
      outfile ( _outfile ),
      col ( 0 ),
      num ( 0 )
      {}

   void writeDec ( unsigned long data );
   void writeHex ( unsigned long data );

private:
   FILE *outfile;
   int   col;
   long  num;
};


void IntegerSink::writeHex ( unsigned long data )
{
   if ( num++ > 0 )
   {
      col += fprintf ( outfile, "," );
      if ( col > 70 )
      {
         col = 0;
         fprintf ( outfile, "\n" );
      }
   }

   col += fprintf ( outfile, "0x%lx", data );
}


void IntegerSink::writeDec ( unsigned long data )
{
   if ( num++ > 0 )
   {
      col += fprintf ( outfile, "," );
      if ( col > 70 )
      {
         col = 0;
         fprintf ( outfile, "\n" );
      }
   }

   col += fprintf ( outfile, "%lu", data );
}


//------------------------------------------------------------------------

struct Branch;
typedef Branch *BranchPtr;

struct Branch
{
   unsigned short    move;      // a move in the library
   char              isBad;     // Dr. Chenard should avoid making this move
   LList<BranchPtr>  replies;   // all possible replies to the move

   unsigned long querySize() const;   // returns how big representation is
   void write ( IntegerSink & );
};


unsigned long Branch::querySize() const
{
   unsigned long size = 3;   // up front overhead: size + move + reply count

   for ( unsigned i=0; i < replies.numItems(); i++ )
   {
      BranchPtr r = replies[i];
      size += r->querySize();   // count + branchlist size
   }

   return size;
}

void WriteBranchList ( LList<BranchPtr> &blist, IntegerSink &isink );

void Branch::write ( IntegerSink &isink )
{
   isink.writeDec ( querySize() );

   unsigned long moveCode = (unsigned long) move;
   if ( isBad )
   {
      moveCode |= 0x10000;
   }

   isink.writeHex ( moveCode );

   WriteBranchList ( replies, isink );
}


//------------------------------------------------------------------------



const char *inFilename  = "chenard.obt";    // renamed from "openbook.dat" for CVS
const char *outFilename = "openbook.cpp";

void ReadData ( const char *filename, FILE *infile, LList<BranchPtr> & );
void WriteLibrary ( const char *filename, FILE *outfile, LList<BranchPtr> & );


int main()
{
   printf ( "Opening Book Translator - Don Cross, February 1994\n" );

   FILE *infile = fopen ( inFilename, "rt" );
   if ( !infile )
   {
      fprintf ( stderr, "Error opening file '%s' for read\n", inFilename );
      return 1;
   }

   FILE *outfile = fopen ( outFilename, "wt" );
   if ( !outfile )
   {
      fprintf ( stderr, "Error opening file '%s' for write\n", outFilename );
      fclose(infile);
      return 1;
   }

   LList<BranchPtr> library;

   ReadData ( inFilename, infile, library );
   fclose ( infile );

   WriteLibrary ( outFilename, outfile, library );
   fclose ( outfile );

   printf ( "Done!\n" );

   return 0;  // Success
}


int StripComments ( char *line )  // returns whether something interesting
{
   int foundSomething = 0;

   while ( *line )
   {
      if ( *line == ';' || *line == '\n' )
      {
         *line = '\0';
      }
      else
      {
         if ( !isspace(*line) )
         {
            foundSomething = 1;
         }
         ++line;
      }
   }

   return foundSomething;
}


void ReadData ( const char *inFilename,
                FILE *infile,
                LList<BranchPtr> &library )
{
   printf ( "Reading from file '%s'\n", inFilename );

   char line [128];
   LList<BranchPtr> *current = 0;

   while ( fgets ( line, sizeof(line), infile ) )
   {
      if ( StripComments ( line ) )
      {
         if ( line[0] == '[' )
         {
            // We have found the start of a new opening continuation

            current = &library;
            printf ( "processing %s\n", line );
         }
         else
         {
            // We have found the next move in the current continuation

            if ( !current )
            {
               fprintf ( stderr,
                         "Error:  move '%s' is not in a continuation!\n",
                         line );

               exit(1);
            }

            int len = (int) strlen(line);
            int isValid = 0;

            if ( len == 4 || (len == 5 && line[4] == '?') )
            {
               int x1 = line[0] - 'a';
               int y1 = line[1] - '1';
               int x2 = line[2] - 'a';
               int y2 = line[3] - '1';

               if ( x1 >= 0 && x1 <= 7 &&
                    y1 >= 0 && y1 <= 7 &&
                    x2 >= 0 && x2 <= 7 &&
                    y2 >= 0 && y2 <= 7 )
               {
                  isValid = 1;
                  unsigned short source = (x1+2) + (y1+2)*12;
                  unsigned short dest   = (x2+2) + (y2+2)*12;
                  unsigned short move   = (source << 8) | dest;

                  // First, see if this move is already there...
                  Branch *b = 0;

                  for ( unsigned j=0; j < current->numItems(); j++ )
                  {
                     Branch *x = (*current)[j];
                     if ( x->move == move )
                     {
                        b = x;
                        break;
                     }
                  }

                  if ( !b )
                  {
                     // This branch wasn't found, so create it!

                     b = new Branch;
                     if ( !b )
                     {
                        fprintf ( stderr, "Error: out of memory!\n" );
                        exit(1);
                     }

                     b->isBad = (line[4] == '?');
                     b->move  = move;

                     if ( current->addToBack ( b ) != DDC_SUCCESS )
                     {
                        fprintf ( stderr, "Error adding branch to node!\n" );
                        exit(1);
                     }
                  }

                  current = &(b->replies);
               }
            }

            if ( !isValid )
            {
               fprintf ( stderr,
                         "Error:  invalid move syntax: '%s'\n",
                         line );
               exit(1);
            }
         }
      }
   }
}


void WriteBranchList ( LList<BranchPtr> &blist, IntegerSink &isink )
{
   unsigned long n = blist.numItems();

   isink.writeDec ( n );          // number of branches in node
   for ( unsigned i=0; i < n; i++ )
   {
      blist[i]->write ( isink );
   }
}


void WriteLibrary ( const char *outFilename,
                    FILE *outfile,
                    LList<BranchPtr> &library )
{
   fprintf ( outfile, "// %s  -  Generated by OBT.EXE\n\n", outFilename );
   fprintf ( outfile, "unsigned long OpeningData[] = {\n" );

   IntegerSink isink ( outfile );

   WriteBranchList ( library, isink );

   fprintf ( outfile, "};\n\n/*--- end of file %s ---*/\n", outFilename );
}


/*
    $Log: obt.cpp,v $
    Revision 1.3  2008/11/03 22:53:11  Don.Cross
    Added Open Book Translator (obt.exe) to Visual Studio solution.
    Made tweaks so that code compiles without warnings now.

    Revision 1.2  2008/05/03 16:53:24  Don.Cross
    Batch files for zipping up source code.
    Updated contact info in obt.cpp.

    Revision 1.1  2005/11/25 18:30:04  dcross
    Managed to bring back Chenard's open book translator from the dead!
    Found this on CDROM marked "14 September 1999", and fixed compile problems.
    Renamed openbook.dat to chenard.obt so that I won't have any problems with CVS
    thinking it is a binary file.

*/
