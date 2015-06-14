/*===========================================================================

	 contain.h  -  Don Cross, August 1993.

	 New container templates.

	 Revision history:

1994 January 27 [Don Cross]
	 Fixed bug in List<>::removeFromBack(): the index should
	 be numItems()-1, not numItems().

===========================================================================*/

#ifndef __DDC_CONTAIN_H
#define __DDC_CONTAIN_H

#include "ddc.h"

class LinkedImplementation;

template <class Item, class Impl>
class List
{
public:
   virtual ~List();

   void reset();

   unsigned numItems() const {return vpList.numItems();}

   Item & operator[] ( unsigned i ) const {return *(Item *)(vpList[i]);}

   Item removeFromFront();
   Item removeFromBack();
   Item remove ( unsigned i );

   DDCRET insert ( unsigned index, const Item &x )  // make new item at index
   {
      Item *newX = new Item(x);
      return vpList.insert ( index, newX );
   }

   DDCRET addToFront ( const Item &x )   {return insert ( 0, x );}
   DDCRET addToBack  ( const Item &x )   {return insert ( numItems(), x );}

private:
   Impl      vpList;     // an implementation of a list of (void *)
};


template <class Item, class Impl>
List<Item,Impl>::~List()
{
   reset();
}


template <class Item, class Impl>
void List<Item,Impl>::reset()
{
   unsigned n = vpList.numItems();

   while ( n-- > 0 )
   {
      Item *x = (Item *) ( vpList.remove(0) );
      delete x;
   }
}


template <class Item, class Impl>
Item List<Item,Impl>::removeFromFront()
{
   Item *xPtr = (Item *) ( vpList.remove(0) );
   Item x = *xPtr;
   delete xPtr;
   return x;
}


template <class Item, class Impl>
Item List<Item,Impl>::removeFromBack()
{
   Item *xPtr = (Item *) ( vpList.remove ( vpList.numItems() - 1 ) );
   Item x = *xPtr;
   delete xPtr;
   return x;
}


template <class Item, class Impl>
Item List<Item,Impl>::remove ( unsigned index )
{
   Item *xPtr = (Item *) ( vpList.remove ( index ) );
   Item x = *xPtr;
   delete xPtr;
   return x;
}


template <class Item>
class LList: public List <Item, LinkedImplementation>
{
public:
   LList()   {}
   ~LList()  {}

private:
};


class LinkedImplementation
{
public:
   LinkedImplementation();
   ~LinkedImplementation();

   unsigned numItems() const {return numItemsInList;}
   void *   operator [] ( unsigned index ) const;

   DDCRET   insert ( unsigned index, void *newItem );
   void    *remove ( unsigned index );

private:

   struct node
   {
      node   *next;
      node   *prev;
      void   *item;

      node ( void *initItem, node *initNext=0, node *initPrev=0 ):
         next(initNext), prev(initPrev), item(initItem)  {}
   };

   node     *seek ( unsigned newCurrentIndex );

   unsigned  numItemsInList;
   node     *front;
   node     *back;
   node     *current;
   unsigned  currentIndex;
};


#endif // __DDC_CONTAIN_H

/*--- end of file contain.h ---*/
