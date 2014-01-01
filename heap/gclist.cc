/*
 * gclist.cc
 *
 * Author: Dvorka
 *
 * ToDo: 
 */

#include "gclist.h"

#include <stdlib.h>

// JVM inner heap
#ifdef GC_ENABLE_SYSTEM_HEAP
 #include "gcheap.h"
 extern GcHeap           *systemHeap;
 extern bool             systemHeapEnabled;
#endif


// print info when hunting bugs: 2005

/*
 * Replace: DBGprintf with //DBGprintf a vice versa
 */

//- class List methods -------------------------------------------------------

List::List( void ) /*fold00*/
{
 //DBGprintf(2005,"List() ");

 Head=new ListItem;
 Tail=new ListItem;

 if( Head==NULL || Tail==NULL )
 {
  printf("\n Error: not enough memory in %s line %d...", __FILE__, __LINE__ );
  exit(0);
 }

 // init
 NrOfItems=0;
 Head->Last=Tail->Next=NULL;
 Head->Next=Tail;
 Tail->Last=Head;

 //DBGprintf(2005,"--> this %p head %p tail %p", this, Head, Tail );
}

//----------------------------------------------------------------------------

#ifdef GC_ENABLE_SYSTEM_HEAP

void *List::operator new(size_t s) /*fold00*/
{
 if( systemHeapEnabled )
  return systemHeap->alloc(s);
 else
  return ::new byte[s];
}

//----------------------------------------------------------------------------

void List::operator delete(void *p) /*fold00*/
{
 if( systemHeapEnabled )
  systemHeap->free(p);
 else
  ::delete p;
}

#endif

//----------------------------------------------------------------------------

void List::InsertAfterHead( ListItem  *InsertedItem ) /*fold00*/
// Inserted item is inserted not copied!
{
 if( InsertedItem )
 {
  Head->Next->Last=InsertedItem;

  InsertedItem->Next=Head->Next;
  InsertedItem->Last=Head;

  Head->Next=InsertedItem;

  NrOfItems++;

  //DBGprintf(2005,"  Item %p inserted,", InsertedItem );
 }
}

//----------------------------------------------------------------------------

void List::InsertBeforeTail( ListItem  *InsertedItem ) /*fold00*/
// Inserted item is inserted not copied!
{
 if( InsertedItem )
 {
  InsertedItem->Next=Tail;
  InsertedItem->Last=Tail->Last;

  Tail->Last->Next=InsertedItem;
  Tail->Last=InsertedItem;

  NrOfItems++;

  //DBGprintf(2005,"  Item %p inserted,", InsertedItem );
 }
}

//----------------------------------------------------------------------------

void List::PasteAtTail( ListItem  *InsHead, ListItem  *InsTail, int NrOfNewItems ) /*fold00*/
{
 Tail->Last->Next=InsHead;
 InsHead->Last=Tail->Last;

 InsTail->Next=Tail;
 Tail->Last=InsTail;

 NrOfItems+=NrOfNewItems;
}

//----------------------------------------------------------------------------

void List::Delete( ListItem  *DelItem ) /*fold00*/
// DelItem points to node in queue which should be deleted
{
 if( DelItem && DelItem!=Head && DelItem!=Tail )
 {
  DelItem->Last->Next=DelItem->Next;
  DelItem->Next->Last=DelItem->Last;

  // must be used delete, because program evides new instances
  // and if farfree( DelItem ); is used after return 0 of main
  // it says Null pointer assignment...
  delete DelItem;

  NrOfItems--;

  //DBGprintf(2005,"  Item %p deleted,", DelItem );
 }
}

//----------------------------------------------------------------------------

void List::Get( ListItem  *Item ) /*fold00*/
// take Item out of list but DO NOT delete
{
 if( Item && Item!=Head && Item!=Tail )
 {
  Item->Last->Next=Item->Next;
  Item->Next->Last=Item->Last;

  NrOfItems--;

  //DBGprintf(2005,"  Item %p taken out,", Item );
 }
}

//----------------------------------------------------------------------------

ListItem  *List::Next( ListItem  *Item ) /*fold00*/
// if next is tail returns NULL
{
 if( Item->Next==Tail )
  return NULL;
 else
  return
   Item->Next;
}

//----------------------------------------------------------------------------

ListItem  *List::Last( ListItem  *Item ) /*fold00*/
// if last is head returns NULL
{
 if( Item->Last==Head )
  return NULL;
 else
  return Item->Last;
}

//----------------------------------------------------------------------------

ListItem  *List::GetHead( void ) /*fold00*/
// get node head which contains data ( it's different from Head! )
// if no data node in queue => returns NULL
{
 if( Head->Next==Tail )
  return NULL;
 else
  return Head->Next;
}

//----------------------------------------------------------------------------

ListItem  *List::GetTail( void ) /*fold00*/
// if no data node in queue => returns NULL
{
 if( Head->Next==Tail )
  return NULL;
 else
  return Tail->Last;
}

//----------------------------------------------------------------------------

void List::Destroy( void ) /*fold00*/
{
 while( Head->Next!=Tail ) Delete( Head->Next );

 NrOfItems=0;

 //DBGprintf(2005," Destroy List...");

 return;
}

//----------------------------------------------------------------------------

void List::LeaveContent( void ) /*fold00*/
{
 NrOfItems=0;

 Head->Last=Tail->Next=NULL;
 Head->Next=Tail;
 Tail->Last=Head;
}

//----------------------------------------------------------------------------

bool List::Empty( void ) /*fold00*/
{
 if( Head->Next==Tail )
  return TRUE;
 else
  return FALSE;
}

//----------------------------------------------------------------------------

ListItem  *List::Pop( void ) /*fold00*/
{
 ListItem  *PopItem;

 if( Head->Next != Tail )
 {
  PopItem=Tail->Last;

  // change pointers
  Tail->Last->Last->Next=Tail;
  Tail->Last		=Tail->Last->Last;


  //DBGprintf(2005," POP item %p ( not deleted )", PopItem );

  NrOfItems--;

  return PopItem;
 }
 else
 {
  //DBGprintf(2005,"\n stack empty");

  return NULL;
 }
}

//----------------------------------------------------------------------------

List::~List( void ) /*fold00*/
{
 //DBGprintf(2005," ~List() this %p, items %u", this, NrOfItems );

 Destroy();

 delete Head;
 delete Tail;
}
 /*FOLD00*/

//- EOF ---------------------------------------------------------------------------
