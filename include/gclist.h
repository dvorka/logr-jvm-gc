/*
 * gclist.h
 *
 * Author: Martin Dvorak
 *
 * ToDo: 
 *
 */

#ifndef __GCLIST_H
 #define __GCLIST_H

 #include "bool.h"
 #include "debuglog.h"
 #include "gcoptions.h"



 //- struct ListItem --------------------------------------------------------



 class ListItem /*fold00*/
 {
  public:
   ListItem  *Next,
	     *Last;

   virtual ~ListItem( void ) {};
 };
 /*FOLD00*/



 //- class List -------------------------------------------------------------

 class List /*fold00*/
 {
  public:
   ListItem  *Head;
   ListItem  *Tail;	        	// service head and tail are empty
					// nodes
   int 	     NrOfItems;		        // data items ( Head, Tail not in )

   List( void );
    #ifdef GC_ENABLE_SYSTEM_HEAP
     void     *operator new(size_t s);
     void     operator  delete(void *p);
    #endif
    void     Insert( ListItem  *InsertedItem ) { InsertBeforeTail( InsertedItem ); }
    void     InsertAfterHead( ListItem  *InsertedItem  ); // InsertedItem is inserted => not copied!
    void     InsertBeforeTail( ListItem  *InsertedItem  ); // InsertedItem is inserted => not copied!
    void     Paste( ListItem  *InsHead, ListItem  *InsTail, int NrOfNewItems ) { PasteAtTail( InsHead, InsTail, NrOfNewItems ); };
    void     PasteAtTail( ListItem  *InsHead, ListItem  *InsTail, int NrOfNewItems );
    void     Delete( ListItem  *DelItem );           // DelItem points to node in queue which should be deleted
    void     Get( ListItem  *Item ); // take item out of list but DO NOT delete
    ListItem *Next( ListItem  *Item );
    ListItem *Last( ListItem  *Item );
    ListItem *GetHead( void );
    ListItem *GetTail( void );
    void     Destroy( void );       // destroys every data node
    void     LeaveContent( void );  // make list empty, nodes not destroyed
    bool     Empty( void );

    void     Push( ListItem  *PushedItem ) { InsertBeforeTail( PushedItem ); }
    ListItem *Pop( void );		// pop it but DO NOT delete item

   ~List();

 };
 /*FOLD00*/


#endif

//- EOF ---------------------------------------------------------------------------
