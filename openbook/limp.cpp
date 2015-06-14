/*==========================================================================

       limp.cpp  -  Don Cross, September 1993.

       Contains class LinkedImplementation.
       This is a helper class for the List<> class template.
       Together, they can make an LList<>.

==========================================================================*/

// Standard includes
#include <iostream>
#include <stdlib.h>

using namespace std;

// DDCLIB includes
#include "contain.h"


LinkedImplementation::LinkedImplementation():
   numItemsInList ( 0 ),
   front ( 0 ),
   back ( 0 ),
   current ( 0 ),
   currentIndex ( 0 )
{
}


LinkedImplementation::~LinkedImplementation()
{
   if ( numItemsInList > 0 )
   {
      // This is a fatal error because class LinkedImplementation
      // does not know how to destroy the items it contains.
      // (It knows of them only via void pointers).

      cerr << "Fatal error in LinkedImplementation::~LinkedImplementation():" << endl;
      cerr << "List was not cleaned up by user before deletion!" << endl;
      exit(1);
   }
}


void *LinkedImplementation::operator[] ( unsigned index ) const
{
   LinkedImplementation *me = (LinkedImplementation *) this;
   node *x = me->seek(index);
   if ( !x )
   {
      cerr << "Fatal error in LinkedImplementation::operator[]" << endl;
      cerr << "me->seek returned NULL" << endl << endl;
      exit(1);
   }

   if ( !(x->item) )
   {
      cerr << "Fatal error in LinkedImplementation::operator[]" << endl;
      cerr << "x != NULL but x->item == NULL" << endl;
   }

   return x->item;
}


LinkedImplementation::node *LinkedImplementation::seek ( unsigned index )
{
   if ( index >= numItemsInList )
   {
      cerr << "Fatal error in LinkedImplementation::seek(" <<
              index << "): subscript out of range!" << endl;

      exit(1);
   }

   if ( !current )
   {
      // If we get here, it means that we need to reset our "indexing cache"

      current = front;
      currentIndex = 0;
   }

   // Seek forward if necessary...
   while ( currentIndex < index )
   {
      ++currentIndex;
      current = current->next;
   }

   // Seek backward if necessary...
   while ( currentIndex > index )
   {
      --currentIndex;
      current = current->prev;
   }

   return current;
}


DDCRET LinkedImplementation::insert ( unsigned index, void *newItem )
{
   if ( !newItem )
   {
       cerr << "Fatal error in LinkedImplementation::insert()" << endl;
       cerr << "newItem == NULL" << endl << endl;
       exit(1);
   }

   node *newNode = new node ( newItem );

   if ( !newNode )
   {
      return DDC_OUT_OF_MEMORY;
   }

   if ( index == 0 )
   {
      // prepend to list...

      newNode->next = front;
      if ( front )
      {
         front = front->prev = newNode;
      }
      else
      {
         front = back = newNode;
      }
   }
   else if ( index == numItemsInList )
   {
      // append to list...

      newNode->prev = back;
      if ( back )
      {
         back = back->next = newNode;
      }
      else
      {
         back = newNode;
      }
   }
   else
   {
      // It goes somewhere in the middle...

      seek(index);

      newNode->next = current;
      newNode->prev = current->prev;
      current->prev = newNode->prev->next = newNode;
   }

   current = newNode;
   currentIndex = index;
   ++numItemsInList;

   return DDC_SUCCESS;
}


void *LinkedImplementation::remove ( unsigned index )
{
   void *itemPointer = seek(index)->item;

   if ( current->prev )
   {
      current->prev->next = current->next;
   }
   else
   {
      front = current->next;
   }

   if ( current->next )
   {
      current->next->prev = current->prev;
   }
   else
   {
      back = current->prev;
   }

   --numItemsInList;
   delete current;
   current = 0;
   return itemPointer;
}


/*--- end of file limp.cpp ---*/
