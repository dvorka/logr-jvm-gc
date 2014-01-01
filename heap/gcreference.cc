/*
 * gcreference.cc
 *
 * Author: Dvorka
 *
 * ToDo: 
 *  - create free list -> macro a soupnout do konstruktoru
 *  - oznacit referenci, ze neni pouzita tagem do refcount 0xFFFFFF,
 *    aby se v tom gc orientoval
 */
#include "gcreference.h"

#include <stdlib.h>
#include <sys/cdefs.h>

#include "debuglog.h"
#include "exception.h"
#include "gcheap.h"
#include "gcsupport.h"
#include "reference.h"



//- Globals -------------------------------------------------------------------

// JVM inner heap
#ifdef GC_ENABLE_SYSTEM_HEAP
 extern GcHeap           *systemHeap;
 extern bool             systemHeapEnabled;
#endif



//- struct StatRefVecsItem methods --------------------------------------------



StatRefVecsItem::StatRefVecsItem( int lng ) /*fold00*/
{
 if( (vector = (StatRef*)logrAlloc(sizeof(StatRef)*lng))==NULL )
  WORD_OF_DEATH("unable to allocate StatRef vector...");

 this->lng= lng;           // init member
 lng      = this->lng-1;   // used in cycle below

 // create free list:
 //  free list is linked through StatRef.ptr pointers and finished with
 //  NULL pointer
 for( int i=0; i<lng; i++ )
 {
  vector[i].state= REF_NOT_VALID;
  vector[i].ptr  = (void *)&(vector[i+1]);
 }

 vector[lng].state= REF_NOT_VALID;
 vector[lng].ptr  = NULL;
}

//-----------------------------------------------------------------------------

#ifdef GC_ENABLE_SYSTEM_HEAP

void *StatRefVecsItem::operator new(size_t s) /*fold00*/
{
 if( systemHeapEnabled )
  return systemHeap->alloc(s);
 else
  return ::new byte[s];
}

//----------------------------------------------------------------------------

void StatRefVecsItem::operator delete(void *p) /*fold00*/
{
 if( systemHeapEnabled )
  systemHeap->free(p);
 else
  ::delete p;
}

#endif

//----------------------------------------------------------------------------

StatRefVecsItem::~StatRefVecsItem() /*fold00*/
{
 logrFree(vector);
}
 /*FOLD00*/


//- struct InstRefVecsItem methods --------------------------------------------------



InstRefVecsItem::InstRefVecsItem( int lng ) /*fold00*/
{
 if( (vector = (InstRef*)logrAlloc(sizeof(InstRef)*lng))==NULL )
  WORD_OF_DEATH("unable to create InstRef vector...");

 int i;

 // prepare synchro, other fields are initialized in ::get
 for( i=0; i<lng; i++ )
 {
  if( pthread_cond_init(  &(vector[i].syncCond), NULL ) )  // if !=0 then err
   WORD_OF_DEATH("cann't create condition...");
  if( pthread_mutex_init( &(vector[i].syncMutex), NULL ) ) // if !=0 then err
   WORD_OF_DEATH("cann't create mutex...");
 }

 this->lng= lng;   // init member
 lng      = this->lng-1; // used in cycle below

 // create free list
 for( i=0; i<lng; i++ )
 {
  vector[i].state= REF_NOT_VALID;
  vector[i].ptr  = (void *)&(vector[i+1]);
 }

 vector[lng].state      =REF_NOT_VALID;
 vector[lng].ptr        =NULL;
}

//-----------------------------------------------------------------------------

#ifdef GC_ENABLE_SYSTEM_HEAP

void *InstRefVecsItem::operator new(size_t s) /*fold00*/
{
 if( systemHeapEnabled )
  return systemHeap->alloc(s);
 else
  return ::new byte[s];
}

//----------------------------------------------------------------------------

void InstRefVecsItem::operator delete(void *p) /*fold00*/
{
 if( systemHeapEnabled )
  systemHeap->free(p);
 else
  ::delete p;
}

#endif

//-----------------------------------------------------------------------------

InstRefVecsItem::~InstRefVecsItem() /*fold00*/
{
 // destroy synchro
 for( int i=0; i<lng; i++ )
 {
  pthread_cond_destroy( &(vector[i].syncCond) );
  pthread_mutex_destroy( &(vector[i].syncMutex) );
 }
 // destroy vector
 logrFree(vector);
}
 /*FOLD00*/


//- class ReferenceVectors methods --------------------------------------------



ReferenceVectors::ReferenceVectors() /*fold00*/
{
 
 sVectorSize     = GCREFVECS_START_SIZE; // size of the first vector
 sVectorIncrement= GCREFVECS_INCREMENT;  // size of vector when old overflow
 iVectorSize     = GCREFVECS_START_SIZE;
 iVectorIncrement= GCREFVECS_INCREMENT;  

 // create basic vectors
 StatRefVecsItem *sItem;
 InstRefVecsItem *iItem;

 statRefVecs=new List;
  if( (sItem = new StatRefVecsItem(sVectorSize))==NULL )
   WORD_OF_DEATH("unable to create StatRef vector...")
  else
   statRefVecs->Insert(sItem);

 instRefVecs=new List;
  if( (iItem = new InstRefVecsItem(iVectorSize))==NULL )
   WORD_OF_DEATH("unable to create InstRef vector...")
  else
  instRefVecs->Insert(iItem);

 // init free lists
 sFL=sItem->vector;
 iFL=iItem->vector;

 // init synchro
 FLASH_LOCK_INIT(sShield);
 FLASH_LOCK_INIT(iShield);

 color = GC_GREEN;
}

//-----------------------------------------------------------------------------

#ifdef GC_ENABLE_SYSTEM_HEAP

void *ReferenceVectors::operator new(size_t s) /*fold00*/
{
 if( systemHeapEnabled )
  return systemHeap->alloc(s);
 else
  return ::new byte[s];
}

//----------------------------------------------------------------------------

void ReferenceVectors::operator delete(void *p) /*fold00*/
{
 if( systemHeapEnabled )
  systemHeap->free(p);
 else
  ::delete p;
}

#endif

//-----------------------------------------------------------------------------

StatRef *ReferenceVectors::getStatRef() /*fold00*/
{
 FLASH_LOCK(sShield);

 if(sFL==NULL) // vector overflows -> create new vector and add it into Flist
 {
  StatRefVecsItem *item = new StatRefVecsItem(sVectorIncrement);

  if( item==NULL )
   return NULL;
  else;
   statRefVecs->Insert(item);
  
  sFL=item->vector;             // free list (it's correct because iFL == NULL )
 }

 // get the reference
 StatRef *sr=sFL;               // get the first free reference
  sFL=(StatRef*)sFL->ptr;       // remove reference from free list

 sr->color=color;               // GC color

 FLASH_UNLOCK(sShield);

 DBGprintf(DBG_GCREF," get StatRef %p",sr);

 return sr;
}

//-----------------------------------------------------------------------------

InstRef *ReferenceVectors::getInstRef() /*fold00*/
{
 FLASH_LOCK(iShield);

 if(iFL==NULL) // vector overflows -> create new vector and add it into Flist
 {
  InstRefVecsItem *item = new InstRefVecsItem(iVectorIncrement);

  if( item==NULL )
   return NULL;
  else;
   instRefVecs->Insert(item);
  
  iFL=item->vector;             // free list (it's correct because iFL == NULL )
 }

 // get the reference
 InstRef *ir=iFL;               // get the first free reference
  iFL=(InstRef*)iFL->ptr;       // remove reference from free list

 ir->color       =color;        // GC color
 ir->heapRefCount=0;            // GC color

 FLASH_UNLOCK(iShield);

 DBGprintf(DBG_GCREF," get InstRef %p",ir);

 return ir;
}

//-----------------------------------------------------------------------------

void ReferenceVectors::putStatRef( StatRef *sr ) /*fold00*/
// - release reference and put it to free list
{
 FLASH_LOCK(sShield);

  // put reference to free list after head
  sr->ptr=sFL;
   sFL=sr;

 FLASH_UNLOCK(sShield);
}

//-----------------------------------------------------------------------------

void ReferenceVectors::putInstRef( InstRef *ir ) /*fold00*/
// - release reference and put it to free list
{
 FLASH_LOCK(iShield);

  // put reference to free list after head
  ir->ptr=iFL;
   iFL=ir;

 FLASH_UNLOCK(iShield);
}

//-----------------------------------------------------------------------------

void ReferenceVectors::iWalker() /*fold00*/
{
 fprintf(stderr,"\n\n- InstRefs walker ----------------------------------------- Begin -\n");

 ListItem *item= instRefVecs->GetHead();
 int      size,
          i;

 while( item )
 {
  size = ((InstRefVecsItem*)item)->lng;

  for( i=0; i<size; i++ )                             // one vector
  {
   if( ((InstRefVecsItem*)item)->vector[i].state != REF_NOT_VALID )
    printInstRef( (&(((InstRefVecsItem*)item)->vector[i])) );
  }

  item = instRefVecs->Next(item);
 }

 fprintf(stderr,"\n\n- InstRefs walker ----------------------------------------- End -\n");
}

//-----------------------------------------------------------------------------

void ReferenceVectors::iWalker( int color ) /*fold00*/
{
 fprintf(stderr,"\n\n- InstRefs walker ----------------------------------------- Begin -\n");

 ListItem *item= instRefVecs->GetHead();
 int      size,
          i;

 while( item )
 {
  size = ((InstRefVecsItem*)item)->lng;

  for( i=0; i<size; i++ )                             // one vector
  {
   if( ((InstRefVecsItem*)item)->vector[i].state != REF_NOT_VALID
       &&
       ((InstRefVecsItem*)item)->vector[i].color == color
     )
    printInstRef( (&(((InstRefVecsItem*)item)->vector[i])) );
  }

  item = instRefVecs->Next(item);
 }

 fprintf(stderr,"\n\n- InstRefs walker ----------------------------------------- End -\n");
}

//-----------------------------------------------------------------------------

void ReferenceVectors::sWalker() /*fold00*/
{
 fprintf(stderr,"\n\n- StatRefs walker ----------------------------------------- Begin -\n");

 ListItem *item= statRefVecs->GetHead();
 int      size,
          i;

 while( item )
 {
  size = ((StatRefVecsItem*)item)->lng;

  for( i=0; i<size; i++ )                             // one vector
  {
   if( ((StatRefVecsItem*)item)->vector[i].state != REF_NOT_VALID )
    printStatRef( (&(((StatRefVecsItem*)item)->vector[i])) );
  }

  item = statRefVecs->Next(item);
 }

 fprintf(stderr,"\n\n- StatRefs walker ------------------------------------------- End -\n");
}

//-----------------------------------------------------------------------------

void ReferenceVectors::sWalker( int color ) /*fold00*/
{
 fprintf(stderr,"\n\n- StatRefs walker ----------------------------------------- Begin -\n");

 ListItem *item= statRefVecs->GetHead();
 int      size,
          i;

 while( item )
 {
  size = ((StatRefVecsItem*)item)->lng;

  for( i=0; i<size; i++ )                             // one vector
  {
   if( ((StatRefVecsItem*)item)->vector[i].state != REF_NOT_VALID
       &&
       ((StatRefVecsItem*)item)->vector[i].color == color
     )
    printStatRef( (&(((StatRefVecsItem*)item)->vector[i])) );
  }

  item = statRefVecs->Next(item);
 }

 fprintf(stderr,"\n\n- StatRefs walker ------------------------------------------- End -\n");
}

//-----------------------------------------------------------------------------

void ReferenceVectors::walker() /*fold00*/
{
 iWalker();
 sWalker();
}

//-----------------------------------------------------------------------------

ReferenceVectors::~ReferenceVectors() /*fold00*/
{
 DBGprintf(DBG_GCREF," ref ptrs: sr %p ir %p",statRefVecs,instRefVecs);
 delete statRefVecs;
 delete instRefVecs;
}
 /*FOLD00*/


//- EOF -----------------------------------------------------------------------
