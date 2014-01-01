/*
 * gcheap.cc
 *
 * Author: Dvorka
 *
 * ToDo: 
 */

#include "gcheap.h"

#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/cdefs.h>
#include <sys/stat.h>
#include <unistd.h>

#include "debuglog.h"
#include "gcsupport.h"
#include "math.h"



// JVM inner heap
#ifdef GC_ENABLE_SYSTEM_HEAP
 extern GcHeap           *systemHeap;
 extern bool             systemHeapEnabled;
#endif



//- Hash function -------------------------------------------------------------

// +-------------------------------+
// | int  gcHeap::hash( int size ) |
// +-------------------------------+
 /*fold00*/
// Example:
//  size==18 -> 10010
//  size |= slack                  ... the smallest allocateble block
//                                    for simplifity here is slack==size==18
//  bits in long: 0 ... 0 10010
//                  27      5          (27+5=32b)
//  -> h=27
//  -> hash returns # of 0 bits in front of the first set bit

// Example:(how EAIgnored is calculated)
//  heapSize==15
//  -> the biggest allocatable area is in interval <16,31>
//
//  31 ...  11111           -> 5 bits used
//            5
//
//  -> 32b - 5b = 27 upper bits can be ignored ( 0 ... 0 ????? general block)
//                                                  27     5
//  ( recounted whenever new EA created )

// Rem:
// because I am interesting only in the highest bit set, I can set the lowest
// "block" of bits so the asm becomes faster because of no cmp operation

#undef HASH_VALUE
#undef IGNORED_BITS
#undef CHUNKS_SIZE

 // hash returns interval where chunk belongs (*not bigger*)
 #define GCHEAP_BELONG_HASH(HASH_VALUE,IGNORED_BITS,CHUNK_SIZE)                                                                  \
 __asm__ __volatile__ (                                                                                                   \
                        "movl %2, %%edx                    \n" /* save size to edx - will be destroyed */                 \
                        "orl  $"GCHEAP_SLACK_LIMITS",%%edx \n" /* "block" into size */                                    \
                        "movl %1, %%ecx                    \n" /* ignored bits to ecx */                                  \
                        "movl %%ecx, %%eax                 \n" /* init h with ignored bits (h is stored in eax) */        \
                        "shll %%cl, %%edx                  \n" /* skip big (shift size) (cl from ecx is correct) */       \
                                                                                                                          \
                        "1:                                \n"                                                            \
                        "  shll $1, %%edx                  \n" /* shift size by 1 */                                      \
                        "  jc 0f                           \n" /* check carry */                                          \
                        "  incl %%eax                      \n" /* inc h */                                                \
                        "  jmp 1b                          \n" /* and again */                                            \
                        "0:                                \n"                                                            \
                        "  incl %%eax                      \n" /* !!! ID++; clasical hash returns chunks where items are bigger -> move to smaller (in asm) */ \
                        "  movl %%eax, %0                  \n" /* h from register to mem */                               \
                                                                                                                          \
			: "=g" (HASH_VALUE)                                                                               \
			: "g" (IGNORED_BITS),                                                                             \
                          "g" (CHUNK_SIZE)                     /* contains size */                                        \
                        : "eax", "ecx", "edx", "memory"                                                                   \
                      );

 // hash returns interval where chunk *are* bigger (ID_BELONG++ = ID_BIGGER)
 #define GCHEAP_BIGGER_HASH(HASH_VALUE,IGNORED_BITS,CHUNK_SIZE)                                                           \
 __asm__ __volatile__ (                                                                                                   \
                        "movl %2, %%edx                    \n" /* save size to edx - will be destroyed */                 \
                        "orl  $"GCHEAP_SLACK_LIMITS",%%edx \n" /* "block" into size */                                    \
                        "movl %1, %%ecx                    \n" /* ignored bits to ecx */                                  \
                        "movl %%ecx, %%eax                 \n" /* init h with ignored bits (h is stored in eax) */        \
                        "shll %%cl, %%edx                  \n" /* skip big (shift size) (cl from ecx is correct) */       \
                                                                                                                          \
                        "1:                                \n"                                                            \
                        "  shll $1, %%edx                  \n" /* shift size by 1 */                                      \
                        "  jc 0f                           \n" /* check carry */                                          \
                        "  incl %%eax                      \n" /* inc h */                                                \
                        "  jmp 1b                          \n" /* and again */                                            \
                        "0:                                \n"                                                            \
                        "  movl %%eax, %0                  \n" /* h from register to mem */                               \
                                                                                                                          \
			: "=g" (HASH_VALUE)                                                                               \
			: "g" (IGNORED_BITS),                                                                             \
                          "g" (CHUNK_SIZE)                     /* contains size */                                        \
                        : "eax", "ecx", "edx", "memory"                                                                   \
                      );
 /*FOLD00*/


//- class EAsItem methods -----------------------------------------------------



EAsItem::EAsItem( int size, byte *EA ) /*fold00*/
{
 this->size=size; // this->size contains size of usable heap area
                  //  +
                  // 2 fake heap terminators

 this->EA   = EA;
}

//----------------------------------------------------------------------------

bool EAsItem::create( int size, GcHeap *heap ) /*fold00*/
// - static function
// - creates new EA and adds it into heap->EAs
// - create is needed by systemHeap. When systemHeap is full, then is
//   stretched. In this time is systemHeap full, so there is no available 
//   memory for EAsItem instance.
//      Here is created EA which is add into free list. From this time
//   is available for allocations so EAsItem instance can be created.
{
 byte *newEA;

 size+=2;         // add space for EA terminators
                  // this->size contains size of usable heap area
                  //  +
                  // 2 fake heap terminators

 if( (newEA=new byte[size]) == NULL )
  WORD_OF_DEATH("not enough system memory to efemere area...");

 // *** for debug ***
#if 0
 #ifndef GC_RELEASE
  int j;
  for(j=0; j<size; j++) newEA[j]=0xAA;
 #endif
#endif
 // *** for debug ***



 // allocation was successful so now update heap structures
 // (it's used bu createFreeChunk)
 heap->EAsCount++;          // increase number of efemere areas
 heap->heapSizeSI+= size-2; // omit termitors

 // recount bits and maxAllocatableChunk count ignored bits (see hash())
 int helpEAIgnoredBits= 32,
     i                = size-2;
 while(i)
 {
  i >>= 1;
  helpEAIgnoredBits--;
 }
 if( helpEAIgnoredBits<heap->EAIgnoredBits ) // try to ignore the less bits
 {
  heap->EAIgnoredBits=helpEAIgnoredBits;

  // the last interval which is used
  heap->lastUsedInterval = heap->EAIgnoredBits+1;

  // maximum size of allocatable chunk by EAIgnoredBits
  heap->maxAllocatableChunk= (int)(pow(2,32-heap->EAIgnoredBits)-1.0);
 }
 // else stays the same



 // make flag terminators - the first and the last byte in heap
 // these bytes must be set in order to correct run of defragmentation
 newEA[0]=newEA[size-1]=GCHEAP_CHUNK_FULL;

 // +------------------------------------------------------------------------+
 // | create chunk and insert this chunk into freeList (without terminators) |
 // +------------------------------------------------------------------------+
 // - heap->createFreeChunk( size-2, newEA+1 ); ------------------------------

     int  ID,
          hsize = size-2;
     byte *hEA  = newEA+1;

     DBGprintf(DBG_GCHEAP," -Begin-> createFreeChunk %p size %i(0x%x)",hEA,hsize,hsize);



     GCHEAP_BELONG_HASH(ID,heap->EAIgnoredBits,hsize)



     *(int*)(hEA+GCHEAP_SIZE)=
     *(int*)(hEA+hsize - (sizeof(byte)+sizeof(int))) = hsize;  // sizes



     // linked list operations must be protected
     FLASH_LOCK(heap->freeList[ID].shield);

        // format chunk (protected because of defragmentation routine)
        *hEA = hEA[hsize-1] = GCHEAP_CHUNK_FREE; // tags

        // ptr 'last'
        *((dword*)(hEA+GCHEAP_LAST))
        =
        (dword)(heap->freeList+ID);

        dword *np = ((dword*)(((byte*)(&(heap->freeList[ID])))+GCHEAP_NEXT)); // forsage

        // ptr 'next'                                           (NULL OK)
        *((dword*)(hEA+GCHEAP_NEXT))
        =
        *np;
        // 'last' of the old first chunk
        if( *np ) // np != NULL
        *((dword*)(*np+GCHEAP_LAST))
        =
        (dword)hEA;
        // freeList fakeNext
        *np
        =
        (dword)hEA;

     FLASH_UNLOCK(heap->freeList[ID].shield);

   DBGprintf(DBG_GCHEAP," -End-> createFreeChunk %p size %i(0x%x), inserted into ID %i",hEA,hsize,hsize,ID);

 // - heap->createFreeChunk( size-2, newEA+1 ); ------------------------------



 // -> from now I can allocate in stretched heap



 // * create item in EAs list *
 ListItem *EAListItem = new EAsItem(size,newEA);

 if( !EAListItem )  // newEA == NULL
 {
  delete newEA;
  return FALSE;     // unable to create new efemere area === to stretch heap
 }
 else
  heap->EAs->Insert( EAListItem );



 return TRUE;
}

//----------------------------------------------------------------------------

#ifdef GC_ENABLE_SYSTEM_HEAP

void *EAsItem::operator new(size_t s) /*fold00*/
{
 if( systemHeapEnabled )
  return systemHeap->alloc(s);
 else
  return ::new byte[s];
}

//----------------------------------------------------------------------------

void EAsItem::operator delete(void *p) /*fold00*/
{
 if( systemHeapEnabled )
  systemHeap->free(p);
 else
  ::delete p;
}

#endif

//----------------------------------------------------------------------------

void EAsItem::dump( int where, int handle ) /*fold00*/
{
 int i;

 switch( where )
 {
  case GCHEAP_DUMP_2_STDERR:
        for(i=0; i<25; i++)
         fprintf(stderr,"%2i ",i+1);
        fprintf(stderr,"\n");
        for(i=0; i<25; i++)
         fprintf(stderr,"---");
        fprintf(stderr,"\n");

        for(i=0; i<size; i++)
        {
         fprintf(stderr,"%2x ",EA[i]);

         if(!((i+1)%25))
          fprintf(stderr,"[%i]\n",i+1);
        }
        fprintf(stderr,"\n");
       break;
  case GCHEAP_DUMP_2_FILE:
        write(handle,(void *)EA,size);
       break;
 }
}

//----------------------------------------------------------------------------

void EAsItem::walker() /*fold00*/
// - checks heap and writes result
{
 byte *p=EA;
 bool full;
 char sFull[2]; sFull[1]=0;
 int  chunkSize;
 int  off=0;     // offset in EA



 // check heap terminators
 if( p[0]!=GCHEAP_CHUNK_FULL )
 {
    fprintf(stderr,"\n Error:\n   heap walker: L heap terminator is bad...");
    return;
 }
 if( p[size-1]!=GCHEAP_CHUNK_FULL )
 {
    fprintf(stderr,"\n Error:\n   heap walker: R heap terminator is bad...");
    return;
 }



 fprintf(stderr,"\n+- Heap walker ---------------------------+");
 fprintf(stderr,"\n|    Full  PhySize    Ptr             Off |");
 fprintf(stderr,"\n+-----------------------------------------+");

 p++;              // fake full flag
 off++;

 while( off < size-1 ) // -1 because the last tag is fake
 {

  // check one chunk
  full=FALSE;

  // L tag
  if( p[GCHEAP_TAG]==GCHEAP_CHUNK_FULL )
  {
   full=TRUE;
   sFull[0]='X';
  }
  else
  {
   if( p[GCHEAP_TAG]==GCHEAP_CHUNK_FREE )
   {
    full=FALSE;
    sFull[0]='.';
   }
   else
   {
    fprintf(stderr,"\n Error: bad node in heap: %p, cause Ltag",p);
    fprintf(stderr,"\n|    %s %7i       %10p  %7i  |",sFull,chunkSize,p,off);
    return;
   }
  }

  // chunkSize
  chunkSize = *(int*)(p+GCHEAP_SIZE);

  if( (chunkSize < 0)
       ||
      (off+chunkSize) > size
    )
  {
    fprintf(stderr,"\n Error: bad node in heap: %p, cause: Lsize",p);
    fprintf(stderr,"\n|    %s %7i       %10p  %7i  |",sFull,chunkSize,p,off);
    return;
  }

  // check the right side of the chunk
  // RTAG
  if( full && (*(byte*)(p+chunkSize-1)==GCHEAP_CHUNK_FULL) )
  ;
  else
  {
   if( !full && (*(byte*)(p+chunkSize-1)==GCHEAP_CHUNK_FREE) )
   ;
   else
   {
     fprintf(stderr,"\n Error: bad node in heap: %p, cause: Rtag is different from Ltag",p);
     fprintf(stderr,"\n|    %s %7i       %10p  %7i  |",sFull,chunkSize,p,off);
     return;
   }
  }

  // check things specific for free chunk
  if( !full )
  {

   // 1) Rsize
   if( (*(int*)(p+GCHEAP_SIZE)) != (*(int*)(p+chunkSize-1 - sizeof(int))) )
   {
    fprintf(stderr,"\n Error: bad node in heap: %p, cause: Rsize is different from LSize",p);
    fprintf(stderr,"\n|    %s %7i       %10p  %7i  |",sFull,chunkSize,p,off);
    return;
   }

   // 2) free list pointers
    // check: p->Last->Next == p
    if( *((dword*)(p+GCHEAP_LAST)) ) // != NULL
    {
     if( (byte*)*((dword*)(((byte*)(*((dword*)(p+GCHEAP_LAST))))+GCHEAP_NEXT))
         !=
         p
       )
     {
      fprintf(stderr,"\n Error: bad node in heap: %p, cause: free list last->next pointer invalid",p);
      fprintf(stderr,"\n|    %s %7i       %10p  %7i  |",sFull,chunkSize,p,off);
      return;
     }
    }


    // check:  p->Next->Last == p
    if( *((dword*)(p+GCHEAP_NEXT)) ) // != NULL
    {
     if( (byte*)*((dword*)(((byte*)(*((dword*)(p+GCHEAP_NEXT))))+GCHEAP_LAST))
         !=
         p
       )
     {
      fprintf(stderr,"\n Error: bad node in heap: %p, cause: free list next->last pointer invalid",p);
      fprintf(stderr,"\n|    %s %7i       %10p  %7i  |",sFull,chunkSize,p,off);
      return;
     }
    }
  }



  // print info
  fprintf(stderr,"\n|    %s %7i       %10p  %7i  |",sFull,chunkSize,p,off);

  off+=chunkSize;
  p+=chunkSize;

 } // while

 fprintf(stderr,"\n+- Heap walker ---------------------------+");
}

//----------------------------------------------------------------------------

bool EAsItem::check() /*fold00*/
// - heap check, if error occured 
//   -> caller dumps heap content into file, writes bad EA and
//   sends invalid memory reference signal 11 -> *core* is dumped
{
 byte *p=EA;
 bool full;
 char sFull[2]; sFull[1]=0; sFull[0]='?';
 int  chunkSize;
 int  off=0;     // offset in EA



 // check heap terminators
 if( p[0]!=GCHEAP_CHUNK_FULL )
 {
    fprintf(stderr,"\n Error: L heap terminator is bad...");
    return FALSE;
 }
 if( p[size-1]!=GCHEAP_CHUNK_FULL )
 {
    fprintf(stderr,"\n Error: R heap terminator is bad...");
    return FALSE;
 }

 p++;            // fake full flag
 off++;

 while( off < size-1 ) // -1 because the last tag is fake
 {

  // check one chunk
  full=FALSE;

  // L tag
  if( p[GCHEAP_TAG]==GCHEAP_CHUNK_FULL )
  {
   full=TRUE;
   sFull[0]='X';
  }
  else
  {
   if( p[GCHEAP_TAG]==GCHEAP_CHUNK_FREE )
   {
    full=FALSE;
    sFull[0]='.';
   }
   else
   {
    fprintf(stderr,"\n Error: bad node in heap: %p, cause Ltag",p);
    fprintf(stderr,"\n     %s %7i        %p %7i",sFull,chunkSize,p,off);
    return FALSE;
   }
  }

  // chunkSize
  chunkSize = *(int*)(p+GCHEAP_SIZE);

  if( (chunkSize < 0)
       ||
      (off+chunkSize) > size
    )
  {
    fprintf(stderr,"\n Error: bad node in heap: %p, cause: Lsize",p);
    fprintf(stderr,"\n     %s %7i        %p %7i",sFull,chunkSize,p,off);
    return FALSE;
  }

  // check the right side of the chunk
  // RTAG
  if( full && (*(byte*)(p+chunkSize-1)==GCHEAP_CHUNK_FULL) )
  ;
  else
  {
   if( !full && (*(byte*)(p+chunkSize-1)==GCHEAP_CHUNK_FREE) )
   ;
   else
   {
     fprintf(stderr,"\n Error: bad node in heap: %p, cause: Rtag is different from Ltag",p);
     fprintf(stderr,"\n     %s %7i        %p %7i",sFull,chunkSize,p,off);
     return FALSE;
   }
  }

  // check things specific for free chunk
  if( !full )
  {

   // 1) Rsize
   if( (*(int*)(p+GCHEAP_SIZE)) != (*(int*)(p+chunkSize-1 - sizeof(int))) )
   {
    fprintf(stderr,"\n Error: bad node in heap: %p, cause: Rsize is different from LSize",p);
    fprintf(stderr,"\n     %s %7i        %p %7i",sFull,chunkSize,p,off);
    return FALSE;
   }

   // 2) free list pointers
    // check: p->Last->Next == p
    if( *((dword*)(p+GCHEAP_LAST)) ) // != NULL
    {
     if( (byte*)*((dword*)(((byte*)(*((dword*)(p+GCHEAP_LAST))))+GCHEAP_NEXT))
         !=
         p
       )
     {
      fprintf(stderr,"\n Error: bad node in heap: %p, cause: free list last->next pointer invalid",p);
      fprintf(stderr,"\n     %s %7i        %p %7i",sFull,chunkSize,p,off);
      return FALSE;
     }
    }


    // check:  p->Next->Last == p
    if( *((dword*)(p+GCHEAP_NEXT)) ) // != NULL
    {
     if( (byte*)*((dword*)(((byte*)(*((dword*)(p+GCHEAP_NEXT))))+GCHEAP_LAST))
         !=
         p
       )
     {
      fprintf(stderr,"\n Error: bad node in heap: %p, cause: free list next->last pointer invalid",p);
      fprintf(stderr,"\n     %s %7i        %p %7i",sFull,chunkSize,p,off);
      return FALSE;
     }
    }
  }

  off+=chunkSize;
  p+=chunkSize;

 } // while

 return TRUE;
}

//----------------------------------------------------------------------------

bool EAsItem::fillFree(byte pattern) /*fold00*/
{
 byte *p=EA;
 bool full;
 char sFull[2]; sFull[1]=0; sFull[0]='?';
 int  chunkSize;
 int  off=0;     // offset in EA
 int  j;



 // check heap terminators
 if( p[0]!=GCHEAP_CHUNK_FULL )
 {
    fprintf(stderr,"\n Error: L heap terminator is bad...");
    return FALSE;
 }
 if( p[size-1]!=GCHEAP_CHUNK_FULL )
 {
    fprintf(stderr,"\n Error: R heap terminator is bad...");
    return FALSE;
 }

 p++;            // fake full flag
 off++;

 while( off < size-1 ) // -1 because the last tag is fake
 {

  // check one chunk
  full=FALSE;

  // L tag
  if( p[GCHEAP_TAG]==GCHEAP_CHUNK_FULL )
  {
   full=TRUE;
   sFull[0]='X';
  }
  else
  {
   if( p[GCHEAP_TAG]==GCHEAP_CHUNK_FREE )
   {
    full=FALSE;
    sFull[0]='.';
   }
   else
   {
    fprintf(stderr,"\n Error: bad node in heap: %p, cause Ltag",p);
    fprintf(stderr,"\n     %s %7i        %p %7i",sFull,chunkSize,p,off);
    return FALSE;
   }
  }

  // chunkSize
  chunkSize = *(int*)(p+GCHEAP_SIZE);

  if( (chunkSize < 0)
       ||
      (off+chunkSize) > size
    )
  {
    fprintf(stderr,"\n Error: bad node in heap: %p, cause: Lsize",p);
    fprintf(stderr,"\n     %s %7i        %p %7i",sFull,chunkSize,p,off);
    return FALSE;
  }

  // check the right side of the chunk
  // RTAG
  if( full && (*(byte*)(p+chunkSize-1)==GCHEAP_CHUNK_FULL) )
  ;
  else
  {
   if( !full && (*(byte*)(p+chunkSize-1)==GCHEAP_CHUNK_FREE) )
   ;
   else
   {
     fprintf(stderr,"\n Error: bad node in heap: %p, cause: Rtag is different from Ltag",p);
     fprintf(stderr,"\n     %s %7i        %p %7i",sFull,chunkSize,p,off);
     return FALSE;
   }
  }

  // check things specific for free chunk
  if( !full )
  {

   // 1) Rsize
   if( (*(int*)(p+GCHEAP_SIZE)) != (*(int*)(p+chunkSize-1 - sizeof(int))) )
   {
    fprintf(stderr,"\n Error: bad node in heap: %p, cause: Rsize is different from LSize",p);
    fprintf(stderr,"\n     %s %7i        %p %7i",sFull,chunkSize,p,off);
    return FALSE;
   }

   // 2) free list pointers
    // check: p->Last->Next == p
    if( *((dword*)(p+GCHEAP_LAST)) ) // != NULL
    {
     if( (byte*)*((dword*)(((byte*)(*((dword*)(p+GCHEAP_LAST))))+GCHEAP_NEXT))
         !=
         p
       )
     {
      fprintf(stderr,"\n Error: bad node in heap: %p, cause: free list last->next pointer invalid",p);
      fprintf(stderr,"\n     %s %7i        %p %7i",sFull,chunkSize,p,off);
      return FALSE;
     }
    }


    // check:  p->Next->Last == p
    if( *((dword*)(p+GCHEAP_NEXT)) ) // != NULL
    {
     if( (byte*)*((dword*)(((byte*)(*((dword*)(p+GCHEAP_NEXT))))+GCHEAP_LAST))
         !=
         p
       )
     {
      fprintf(stderr,"\n Error: bad node in heap: %p, cause: free list next->last pointer invalid",p);
      fprintf(stderr,"\n     %s %7i        %p %7i",sFull,chunkSize,p,off);
      return FALSE;
     }
    }

   //+-----------------+
   //| fill free chunk |
   //+-----------------+
   for( j=0; j<chunkSize-GCHEAP_FREE_SERVINFO; j++ )
    p[GCHEAP_FREE_PREFIX+j]=pattern;

  }

  off+=chunkSize;
  p+=chunkSize;

 } // while

 return TRUE;
}

//----------------------------------------------------------------------------

bool EAsItem::checkFillFree(byte pattern) /*fold00*/
{
 byte *p=EA;
 bool full;
 char sFull[2]; sFull[1]=0; sFull[0]='?';
 int  chunkSize;
 int  off=0;     // offset in EA
 int  j;



 // check heap terminators
 if( p[0]!=GCHEAP_CHUNK_FULL )
 {
    fprintf(stderr,"\n Error: L heap terminator is bad...");
    return FALSE;
 }
 if( p[size-1]!=GCHEAP_CHUNK_FULL )
 {
    fprintf(stderr,"\n Error: R heap terminator is bad...");
    return FALSE;
 }

 p++;            // fake full flag
 off++;

 while( off < size-1 ) // -1 because the last tag is fake
 {

  // check one chunk
  full=FALSE;

  // L tag
  if( p[GCHEAP_TAG]==GCHEAP_CHUNK_FULL )
  {
   full=TRUE;
   sFull[0]='X';
  }
  else
  {
   if( p[GCHEAP_TAG]==GCHEAP_CHUNK_FREE )
   {
    full=FALSE;
    sFull[0]='.';
   }
   else
   {
    fprintf(stderr,"\n Error: bad node in heap: %p, cause Ltag",p);
    fprintf(stderr,"\n     %s %7i        %p %7i",sFull,chunkSize,p,off);
    return FALSE;
   }
  }

  // chunkSize
  chunkSize = *(int*)(p+GCHEAP_SIZE);

  if( (chunkSize < 0)
       ||
      (off+chunkSize) > size
    )
  {
    fprintf(stderr,"\n Error: bad node in heap: %p, cause: Lsize",p);
    fprintf(stderr,"\n     %s %7i        %p %7i",sFull,chunkSize,p,off);
    return FALSE;
  }

  // check the right side of the chunk
  // RTAG
  if( full && (*(byte*)(p+chunkSize-1)==GCHEAP_CHUNK_FULL) )
  ;
  else
  {
   if( !full && (*(byte*)(p+chunkSize-1)==GCHEAP_CHUNK_FREE) )
   ;
   else
   {
     fprintf(stderr,"\n Error: bad node in heap: %p, cause: Rtag is different from Ltag",p);
     fprintf(stderr,"\n     %s %7i        %p %7i",sFull,chunkSize,p,off);
     return FALSE;
   }
  }

  // check things specific for free chunk
  if( !full )
  {

   // 1) Rsize
   if( (*(int*)(p+GCHEAP_SIZE)) != (*(int*)(p+chunkSize-1 - sizeof(int))) )
   {
    fprintf(stderr,"\n Error: bad node in heap: %p, cause: Rsize is different from LSize",p);
    fprintf(stderr,"\n     %s %7i        %p %7i",sFull,chunkSize,p,off);
    return FALSE;
   }

   // 2) free list pointers
    // check: p->Last->Next == p
    if( *((dword*)(p+GCHEAP_LAST)) ) // != NULL
    {
     if( (byte*)*((dword*)(((byte*)(*((dword*)(p+GCHEAP_LAST))))+GCHEAP_NEXT))
         !=
         p
       )
     {
      fprintf(stderr,"\n Error: bad node in heap: %p, cause: free list last->next pointer invalid",p);
      fprintf(stderr,"\n     %s %7i        %p %7i",sFull,chunkSize,p,off);
      return FALSE;
     }
    }


    // check:  p->Next->Last == p
    if( *((dword*)(p+GCHEAP_NEXT)) ) // != NULL
    {
     if( (byte*)*((dword*)(((byte*)(*((dword*)(p+GCHEAP_NEXT))))+GCHEAP_LAST))
         !=
         p
       )
     {
      fprintf(stderr,"\n Error: bad node in heap: %p, cause: free list next->last pointer invalid",p);
      fprintf(stderr,"\n     %s %7i        %p %7i",sFull,chunkSize,p,off);
      return FALSE;
     }
    }

   //+--------------------------+
   //| check fill in free chunk |
   //+--------------------------+
   for( j=0; j<chunkSize-GCHEAP_FREE_SERVINFO; j++ )
   {
    if( p[GCHEAP_FREE_PREFIX+j] != pattern )
    {
     fprintf(stderr,"\n Error: bad node in heap: %p, cause: fill corrupted",p);
     fprintf(stderr,"\n     %s %7i        %p %7i",sFull,chunkSize,p,off);
     return FALSE;
    }
   }

  }

  off+=chunkSize;
  p+=chunkSize;

 } // while

 return TRUE;
}

//----------------------------------------------------------------------------

bool EAsItem::isPtr(byte *ptr) /*fold00*/
// - checks if ptr points to heap
{
 byte *p=EA;

 if( p<=ptr && ptr<(p+size) )
  return TRUE;
 else
  return FALSE;
}

//----------------------------------------------------------------------------

EAsItem::~EAsItem() /*fold00*/
{
 if( EA ) // in strethed system heap -> EA is deleted manually
  delete EA;
}
 /*FOLD00*/


//- class GcHeap methods ------------------------------------------------------



GcHeap::GcHeap( int ms=GCHEAP_MS_DEFAULT, int mx=GCHEAP_MX_DEFAULT ) /*fold00*/
// - ms size is size of the first efemere area
// - baseSize is decreased by 2 because of defragmentation terminators
//   in the first and the last byte of the heap
{
 // create free list vector
 if( (freeList
      =
     (FreeListItem*)logrAlloc( sizeof(FreeListItem)*GCHEAP_NROFFREELISTS))
     ==
     NULL
   )
  WORD_OF_DEATH("not enough system memory for free list...")
 // Initialize array:
 // when creating array of object then constructor behaviour is undefined
 // some compilators call them, others don't -> here it si done manually
 for(int i=0;i<GCHEAP_NROFFREELISTS;i++)
 {
  *((byte*) (((byte*)(&freeList[i]))+GCHEAP_TAG)) =GCHEAP_CHUNK_FULL;
  *((dword*)(((byte*)(&freeList[i]))+GCHEAP_SIZE))=0;
  *((dword*)(((byte*)(&freeList[i]))+GCHEAP_LAST))=0;
  *((dword*)(((byte*)(&freeList[i]))+GCHEAP_NEXT))=0;

  // initialize linked list protection lock
  FLASH_LOCK_INIT(freeList[i].shield);
 }


 // create the first efemere area
 this->ms        = ms;
 this->mx        = mx;
 EAsCount        = 0;
 heapSizeSI      = 0;
 allocatedBytes  = 0;
 allocatedBytesSI= 0;
 EAIgnoredBits   = 32;

 // create the list of efemere areas, and insert the first efemere area into
 EAs = new List;
 // add FULL service to alloc chunk with size ms
 if( !EAsItem::create(ms+GCHEAP_FULL_SERVINFO,this) ) // ==NULL
 {
  // unable to create efemere area
  WORD_OF_DEATH("not enough system memory for base EA...")
 }

 // heap stretch synchro init
 FLASH_LOCK_INIT(heapStretchShield);

 DBGprintf(DBG_GCHEAP," -Begin-> GCHEAP() info section");
  DBGsection(DBG_GCHEAP,(info()));
  DBGsection(DBG_GCHEAP,(walker()));
  DBGsection(DBG_GCHEAP,(listingFL()));
 DBGprintf(DBG_GCHEAP," -End-> GCHEAP() info section");
}

//----------------------------------------------------------------------------

#ifdef GC_ENABLE_SYSTEM_HEAP

void *GcHeap::operator new(size_t s) /*fold00*/
{
 if( systemHeapEnabled )
  return systemHeap->alloc(s);
 else
  return ::new byte[s];
}

//----------------------------------------------------------------------------

void GcHeap::operator delete(void *p) /*fold00*/
{
 if( systemHeapEnabled )
  systemHeap->free(p);
 else
  ::delete p;
}

#endif

//----------------------------------------------------------------------------

void *GcHeap::alloc( int logicalSize ) /*fold00*/
// - * IS REENTRANT *
// - get chunk from heap === allocate memory
// - ptr points to tag byte, voidPtr points to data
// - size: physical size of chunk, logicalSize: size of data in chunk
// - chunk is found, formated and then addr of whole chunk is returned
//   (ptr points to tag)
// - synchro:
//   - get chunk is protected inside get. Here working with chunk
//     which is in any list so no protection is needed. FULL tags are set
//     in protected get too -> no problems with defragmentation routine
// - size<GCHEAP_SLACK_LIMIT ... size is smaller than the smallest allocatable 
//   area -> make it longer, SAA in heap is chunk big enought for empty chunk.
//   It's because when this chunk is later deleted, it wouldn't be big enough
//   to put into it free service info



   #define CREATE_FULL_CHUNK(INTERVAL_ID)                                                   \
   /* now format chunk, if the fragment is big enough, make the new chunk */                \
   /* from fragment and put it into the free list                         */                \
                                                                                            \
   /* I have free chunk -> mark bytes as allocated */                                       \
   /* (used in heap overflow check)                */                                       \
   allocatedBytes  +=logicalSize;                                                           \
                                                                                            \
   fragmentSize = sizeFound-size;                                                           \
                                                                                            \
   if( fragmentSize > GCHEAP_SLACK_LIMIT) /* rest is big enough to be new free chunk */     \
   {                                                                                        \
    /* format chunk and set size */                                                         \
    *(byte*)(ptr+size-1)     = GCHEAP_CHUNK_FULL; /* end tag (begin tag done in getChunk) */\
    *(int*)(ptr+GCHEAP_SIZE) = size;              /* size                                 */\
                                                                                            \
    /* create free chunk from the rest and insert this chunk into freeList */               \
    /* createFreeChunk(int size, byte *ptr ) {...} */                                       \
    /* createFreeChunk(fragmentSize,ptr+size);     */                                       \
    /* - createFreeChunk ----------------------------------------------------------------*/ \
                                                                                            \
    byte *hPtr = ptr+size;                                                                  \
                                                                                            \
    GCHEAP_BELONG_HASH(INTERVAL_ID,EAIgnoredBits,fragmentSize)                              \
                                                                                            \
    DBGprintf(DBG_GCHEAP," hash=%i/%x ",INTERVAL_ID,INTERVAL_ID);                           \
                                                                                            \
    *(int*)(hPtr+GCHEAP_SIZE)=                                                              \
    *(int*)(hPtr+fragmentSize - (sizeof(byte)+sizeof(int))) = fragmentSize;                 \
                                                                                            \
    /* linked list operations must be protected */                                          \
    FLASH_LOCK(freeList[INTERVAL_ID].shield);                                               \
                                                                                            \
     /* format chunk (protected because of defragmentation routine) */                      \
     *hPtr = hPtr[fragmentSize-1] = GCHEAP_CHUNK_FREE; /* tags */                           \
                                                                                            \
     /* ptr 'last' */                                                                       \
     *((dword*)(hPtr+GCHEAP_LAST))                                                          \
     =                                                                                      \
     (dword)(freeList+INTERVAL_ID);                                                         \
                                                                                            \
     dword *np = ((dword*)(((byte*)(&freeList[INTERVAL_ID]))+GCHEAP_NEXT)); /* forsage */   \
                                                                                            \
     /* ptr 'next'                                           (NULL OK) */                   \
     *((dword*)(hPtr+GCHEAP_NEXT))                                                          \
     =                                                                                      \
     *np;                                                                                   \
     /* 'last' of the old first chunk */                                                    \
     if( *np ) /* np != NULL */                                                             \
      *((dword*)(*np+GCHEAP_LAST))                                                          \
      =                                                                                     \
      (dword)hPtr;                                                                          \
     /* freeList fakeNext */                                                                \
     *np                                                                                    \
     =                                                                                      \
     (dword)hPtr;                                                                           \
                                                                                            \
    FLASH_UNLOCK(freeList[INTERVAL_ID].shield);                                             \
                                                                                            \
    /* - createFreeChunk ----------------------------------------------------------------*/ \
   }                                                                                        \
   else                                                                                     \
   {                                                                                        \
    /* fragment is not big enough -> stays together with allocated space */                 \
    /* chunk is already formated from getChunk -> nothing to do          */                 \
    size       =sizeFound;                                                                  \
    logicalSize=size-GCHEAP_FULL_SERVINFO;                                                  \
   }                                                                                        \
                                                                                            \
   voidPtr=(void *)(ptr+GCHEAP_DATA); /* function returns pointer to data */                \
                                      /* -> skip service info */                            \
                                                                                            \
   allocatedBytesSI+=size;            /* now update physical alloc bytes */



{
 DBGprintf(DBG_GCHEAP," -Begin-> ALLOC log. size %i",logicalSize);
 #ifdef GC_CHECK_HEAP_INTEGRITY
  check();
 #endif

 again: // occured heap stretch in time of free chunk search

 if( (logicalSize+allocatedBytes)>mx )
 {
  DBGprintf(DBG_GCHEAP,"   Chunk exceeds heap *MX* %i cmd param -> returning NULL",mx);
  return NULL;
 }

 void *voidPtr;
 byte *ptr;
 int  size= logicalSize+GCHEAP_FULL_SERVINFO, // caller wants logicalSize bytes
                                              // for it's own usage -> I must
                                              // allocate space for service too
      sizeFound,
      fragmentSize,
      ID,            
      IDBackup,
      IDBelong,
      EAsCountCopy = EAsCount;                // for heap stretch check

 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 if( size<=maxAllocatableChunk ) // teoretically maximum allocatable chunk
 {

 // if size is smaller than the smallest allocatable area -> make it longer
 // SAA in heap is chunk big enought for empty chunk it is 16B
 // it is because when this chunk is later deleted, it won't be big enough
 // to put into it free service info
 if( size<GCHEAP_FREE_SERVINFO )
 {
  size       =GCHEAP_FREE_SERVINFO;
  logicalSize=GCHEAP_FREE_SERVINFO-GCHEAP_FULL_SERVINFO;
 }

 sizeFound=size;


 
 GCHEAP_BIGGER_HASH(ID,EAIgnoredBits,size)



 IDBackup=ID;           //DBGprintf(DBG_GCHEAP," hash=%i",ID);

 // +------------------------------------------------------------------------+
 // | 1) get takes out free chunk from given free list if it exists          |
 // |   (foundSize is physical chunk size)                                   |
 // +------------------------------------------------------------------------+
 for( ; ID>=lastUsedInterval; ID-- )
 {
  if( freeList[ID].get(sizeFound,ptr) ) // item found, if not sizeFound value is protected
  {

   DBGprintf(DBG_GCHEAP,"   freeList.get: %p size %i/%i(0x%x), from ID %i",ptr,size,sizeFound,sizeFound,ID);

    CREATE_FULL_CHUNK(ID)

   DBGprintf(DBG_GCHEAP," -End-> ALLOC log/phy %p/%p, log/phy size %i/%i",voidPtr,ptr,logicalSize,size);

   return voidPtr;
  }

                        // DBGprintf(DBG_GCHEAP,"   %i",ID);
 } // for



 IDBelong=ID-1;
 // +------------------------------------------------------------------------+
 // | 2) item was not found -> search in interval where low <= size <= high  |
 // +------------------------------------------------------------------------+
 if( freeList[IDBelong].searchFor(sizeFound,ptr) ) // search in belong interval
 {

  DBGprintf(DBG_GCHEAP,"   freeList.searchFor: %p size %i/%i(0x%x), from ID %i -> *search*",ptr,size,sizeFound,sizeFound,IDBelong);

   CREATE_FULL_CHUNK(IDBelong)

  DBGprintf(DBG_GCHEAP," -End-> ALLOC log/phy %p/%p, log/phy size %i/%i",voidPtr,ptr,logicalSize,size);

  return voidPtr;
 }

 } // if( size<=maxAllocatableChunk )
 // else chunk is too big -> new EA must be created



 // +------------------------------------------------------------------------+
 // | 3) efemere area is *FULL* -> if allowed create new efemere area        |
 // +------------------------------------------------------------------------+
 DBGprintf(DBG_GCHEAP,"   EA *FULL* -> creating new EA for chunk size %i",size);

 // newEASize =  min( max( size*10, ms ) mx-allocatedBytes )
 int newEASize = size*10;             // size already contains service info
  if( newEASize < ms )                newEASize= ms;
  if( newEASize > mx-allocatedBytes ) newEASize= mx-allocatedBytes;

 // check if size is big enought to contain wanted size
 if(size>newEASize)
 {
  DBGprintf(DBG_GCHEAP,"   Chunk %i exceeds heap *AVAIL* bytes %i -> returning NULL",size,mx-allocatedBytes);
  DBGprintf(DBG_GCHEAP," -End-> ALLOC");
  return NULL;
 }



 // create new efemere area and try to allocate again
 // +------------------------------------------------------------------------+
 // ! First create EA and *then* insert EA into EAs. EAs is list, which      !
 // ! allocates from systemHeap usually in time of creating new AE has *no*  !
 // ! available space -> here is created new EA which contains enought       !
 // ! of free space and then is in new free space allocated member of the    !
 // ! list which this efemere area contains.                                 !
 // !   Algorithm is correct because EAsItem() inserts new EA into free list !
 // ! so EA is immediately available, inserting into EAs is only for service !
 // ! playing of the heap                                                    !
 // +------------------------------------------------------------------------+

 FLASH_LOCK(heapStretchShield);

  if( EAsCountCopy!=EAsCount )
  // heap was stretched in time of search
  {
   DBGprintf(DBG_GCHEAP,"   ALLOC: nr of EAs changed -> trying again...");
   FLASH_UNLOCK(heapStretchShield);
   // try to alloc chunk again
   goto again;
  }

  if( (logicalSize+allocatedBytes)>mx )
  {
   DBGprintf(DBG_GCHEAP,"   Chunk exceeds heap *MX* %i cmd param -> returning NULL",mx);

   FLASH_UNLOCK(heapStretchShield);
   return NULL;
  }
  else
  {
   DBGprintf(DBG_GCHEAP,"   ALLOC: creating EA %i...",EAsCount);

   // create new EA and insert it into EAs
   if( !EAsItem::create(newEASize,this) )
   {
    DBGprintf(DBG_GCHEAP,"   ALLOC unable to create EA %i -> NULL...",EAsCount);

    FLASH_UNLOCK(heapStretchShield);
    return NULL;
   }

   DBGsection(DBG_GCHEAP,(info()));
   DBGsection(DBG_GCHEAP,(walker()));
   DBGsection(DBG_GCHEAP,(listingFL()));

   FLASH_UNLOCK(heapStretchShield);
  }


 // +------------------------------------------------------------------------+
 // | x) dive deeper (heap is now stretched)                                 |
 // +------------------------------------------------------------------------+
 DBGprintf(DBG_GCHEAP,"   ALLOC: diving deeper...");
 return alloc(logicalSize);

}

//----------------------------------------------------------------------------

void GcHeap::shrink( void *voidPtr, int newSize ) /*fold00*/
// - * IS REENTRANT *
//   (except allocatedBytes, but this variable is used only for info so 
//    short time inconsistency is not serious problem)
// - incoming pointer points to data - not to service info
{
 DBGprintf(DBG_GCHEAP," -Begin-> SHRINK log. ptr %p to log. size %i",voidPtr,newSize);
 #ifdef GC_CHECK_HEAP_INTEGRITY
  check();
 #endif

 byte *ptr=((byte *)voidPtr)-GCHEAP_DATA; // move from data to tag

 int oldSize,
     shrSize;                           // how many bytes I want to free

 newSize += GCHEAP_FULL_SERVINFO;       // add serv info to wanted size
 oldSize = *(int*)(ptr+GCHEAP_SIZE);    // get size
 shrSize = oldSize-newSize;             // how many bytes shrinked

 DBGprintf(DBG_GCHEAP,"   shrink %i -> %i (%i)",oldSize,newSize,shrSize);



 // if newly unused part is big enought -> create new free chunk
 if( shrSize > GCHEAP_FREE_SERVINFO )
 {
  // finish shrinked chunk with terminator
  *(byte*)(ptr+newSize-1) = GCHEAP_CHUNK_FULL;
  // set its new size
  *(int*)(ptr+GCHEAP_SIZE) = newSize;

  // set tag in free chunk
  *(byte*)(ptr+newSize) = GCHEAP_CHUNK_FULL; // points to first byto of new
  // and it's size
  *(int*)(ptr+newSize+GCHEAP_SIZE) = shrSize;
  // correct allocated bytes because free chunk is created from free space
  // (free has no effect on allocatedBytes)
  allocatedBytes+=shrSize-GCHEAP_FULL_SERVINFO;
  // allocatedBytesSI-=shrSize; // idempotent ... see below
  // new chunk will be created from free space using free() which formats it
  free((void *)(ptr+newSize+GCHEAP_DATA)); // pointer points to data in free
 }
 // else nothing to do

 allocatedBytes-=shrSize;       // update allocatedBytes
 // allocatedBytesSI+=shrSize;  // idempotent ... see above

 

 DBGprintf(DBG_GCHEAP," -End-> SHRINK");
}

//----------------------------------------------------------------------------

void *GcHeap::stretch( void *voidPtr, int newSize ) /*fold00*/
// - * IS REENTRANT *
//   (except allocatedBytes, but this variable is used only for info so 
//    short time inconsistency is not serious problem)
{
 DBGprintf(DBG_GCHEAP," -Begin-> STRETCH log. ptr %p to log. size %i",voidPtr,newSize);
 #ifdef GC_CHECK_HEAP_INTEGRITY
  check();
 #endif

 byte *ptr=((byte *)voidPtr)-GCHEAP_DATA, // move from data to tag
      *hp;

 int ID,                        
     oldSize,
     hSize,
     strSize;                           // how many bytes I want to add

 newSize += GCHEAP_FULL_SERVINFO;       // add serv info to wanted size
 oldSize = *(int*)(ptr+GCHEAP_SIZE);    // get size
 strSize = newSize-oldSize;             // how many bytes stretched

 DBGprintf(DBG_GCHEAP,"   stretch %i -> %i (%i)",oldSize,newSize,strSize);



 // +------------------------------------------------------------------------+
 // | 1) check if chunk has some free space inside                           |
 // +------------------------------------------------------------------------+
 if(strSize<=0) // I don't have to stretch, chunk is big enought
 {
  DBGprintf(DBG_GCHEAP," -End-> STRETCH");
  return voidPtr;
 }



 // +------------------------------------------------------------------------+
 // | 2) check if behind chunk is free chunk and is big enought              |
 // +------------------------------------------------------------------------+
 if( *(ptr+oldSize)==GCHEAP_CHUNK_FREE ) // left tag of the right neighbour
 {
  hSize = *(int*)(ptr+oldSize+GCHEAP_SIZE); // get size of right chunk

  DBGprintf(DBG_GCHEAP,"   -> right neighbour of stretched chunk is free: %i",hSize);

  if(hSize>=strSize)    // right chunk is big enought
  {
   // alloc the chunk
   hp= ptr+oldSize;     // pointer to chunk

   // get ID
   GCHEAP_BELONG_HASH(ID,EAIgnoredBits,hSize)

   // lock FL
   FLASH_LOCK(freeList[ID].shield);

    // check if chunk stays free ( was not used in time of locking )
    if( *(ptr+oldSize)!=GCHEAP_CHUNK_FREE ) // *hp
    {
     // unlock FL
     FLASH_UNLOCK(freeList[ID].shield);
    }
    else // OK - was not used
    {

     // no reason to set tags now, only delete chunk from free list

     // delete chunk from free FL (p points to delete chunk - initialized above)
      //  p->Last->Next=p->Next;
      if( *((dword*)(hp+GCHEAP_LAST)) ) // != NULL
       *((dword*)(((byte*)(*((dword*)(hp+GCHEAP_LAST))))+GCHEAP_NEXT)) // save value - do not use pointer
       =
       *((dword*)(hp+GCHEAP_NEXT));
      //  p->Next->Last=p->Last;
      if( *((dword*)(hp+GCHEAP_NEXT)) ) // != NULL
       *((dword*)(((byte*)(*((dword*)(hp+GCHEAP_NEXT))))+GCHEAP_LAST)) // save value - do not use pointer
        =
       *((dword*)(hp+GCHEAP_LAST));



     // if newly unused part is big enought -> create new free chunk
     if( (hSize-strSize) > GCHEAP_FREE_SERVINFO )
     {
      // finish stretched chunk with terminator
      *(byte*)(ptr+newSize-1) = GCHEAP_CHUNK_FULL;
      // set its new size
      *(int*) (ptr+GCHEAP_SIZE) = newSize;

      // set tags and size of created free chunk, other will be set in free
      *(byte*)(ptr+newSize) = GCHEAP_CHUNK_FULL; // points to chunk first byte
      hSize-=strSize; 
      // hSize now contains size of free chunk which creating
      *(int*) (ptr+newSize+GCHEAP_SIZE) = hSize;
      *(byte*)(ptr+newSize+hSize-1) = GCHEAP_CHUNK_FULL; // points to chunk last byte

      // correct allocatedBytes, because freeing already free chunk
      // (free has no effect on allocatedBytes)
      allocatedBytes  +=hSize-GCHEAP_FULL_SERVINFO;
      allocatedBytesSI+=hSize;

      // unlock FL
      FLASH_UNLOCK(freeList[ID].shield);

      // new chunk will be created from free space using free() which formats it
      free((void *)(ptr+newSize+GCHEAP_DATA)); // pointer points to data
     }
     else // space stays together with chunk -> only set the last tag
     {
      // hsize contains size of the right chunk, oldsize is size
      // of stretched chunk

      // link chunks -> finish right neighbour with tag
      *(byte*)(ptr+oldSize+hSize-1) = GCHEAP_CHUNK_FULL; // points to chunk last byte
      // and set size 
      *(int*) (ptr+GCHEAP_SIZE) = oldSize+hSize;

      // unlock FL
      FLASH_UNLOCK(freeList[ID].shield);
     }

     allocatedBytes  +=strSize; // update allocatedBytes & allocatedBytesSI
     allocatedBytesSI+=strSize;

     DBGprintf(DBG_GCHEAP," -End-> STRETCH");
     return voidPtr;
    } // new free chunk not created
  } // right chunk is NOT big enought
 } // else chunk is small -> cann't be used



 // +-------------------------------------------------------------------------+
 // | 3) alloc new chunk and copy it's content into new area, delete old chunk|
 // +-------------------------------------------------------------------------+
 if( (hp=(byte *)alloc(newSize))==NULL )
 {
  DBGprintf(DBG_GCHEAP,"   *UNABLE* to STRETCH chunk into %i -> size unchanged",newSize);
  DBGprintf(DBG_GCHEAP," -End-> STRETCH");
  return NULL;
 }

 // move to data (hp already is (it's output of alloc))
 ptr+=GCHEAP_DATA;

 hSize=oldSize-GCHEAP_FULL_SERVINFO;

 DBGprintf(DBG_GCHEAP,"   Stretch: copying chunk content into new area...");
 // copy content
 for( ID=0; ID<hSize; ID++ )
  hp[ID]=ptr[ID];

 // delete old chunk (point to data)
 free(ptr);

 DBGprintf(DBG_GCHEAP," -End-> STRETCH");
 return (void *)hp;
}

//----------------------------------------------------------------------------

void GcHeap::free( void *voidPtr ) /*fold00*/
// - * IS REENTRANT *
// - put chunk into the free list (size already set inside chunk)
//   === dealocate memory
// - ptr points to byte which becomes tag byte
// - size: physical size of the chunk
// - chunk is formated and inserted into the free list
//   (ptr points to tag)
// - synchronization:
//   - this function is called when deallocating ptr -> chunk is in any list
//     because was full so reading it's size is safe operation
// - size<GCHEAP_SLACK_LIMIT ... cann't occure because chunk have been
//   created by FLGet and there it is treaten
{
 DBGprintf(DBG_GCHEAP," -Begin-> FREE deleting logical ptr %p",voidPtr);

 #ifdef GC_CHECK_HEAP_INTEGRITY
  check();

  if( !isPtrRaw(voidPtr) )
   return;

  if( isFreeChunk(voidPtr) )
  {
   DBGprintf(DBG_GCHEAP,"--------------------------------------------------");
   DBGprintf(DBG_GCHEAP," Error: DOUBLE DELETE of logical pointer %p",voidPtr);
   DBGprintf(DBG_GCHEAP,"--------------------------------------------------");
   WORD_OF_DEATH("Bye!")
  }
 #endif

 byte *ptr;
 int  ID,                       // used by both in defrag and hash
      size;



 ptr  = ((byte *)voidPtr) - GCHEAP_DATA;        // move from data to tag
 size = *(int*)(ptr+GCHEAP_SIZE);               // get size

 DBGprintf(DBG_GCHEAP,"   free size: %i",size);


 // dodelat: zmenit az po fyzickem roztazeni === kdyz uz je misto dostupne
 allocatedBytes  -= (size-GCHEAP_FULL_SERVINFO);
 allocatedBytesSI= allocatedBytesSI-size;



 // free space defragmentation
 #ifdef GC_FREE_SPACE_DEFRAG

  byte *hp;   // helping pointer to neighbour
  int  hsize; // helping size of neighbour

  // defrag is made here to avoid deadlock. I will allocate free neighbours
  // (if exist). Full chunk is in any free list -> nothing locked -> safe

  // snift LEFT neighbour
  if( *(ptr-1)==GCHEAP_CHUNK_FREE ) // right tag of the left neighbour
  {
   DBGprintf(DBG_GCHEAP," -> defrag: left neighbour is free -> will be defragmented");

   // take out this chunk from free list - manually:
   //  - get chunk size
   //  - lock corresponding FL
   //  - mark chunk as FULL
   //  - delete chunk from FL (no search in FL, it's address is known)
   //  - unlock FL

   // get the size to get ID
   hsize = *(int*)(ptr-(sizeof(byte)+sizeof(int))); // get size (tag+size)

   DBGprintf(DBG_GCHEAP,"   defrag: lsize %i",hsize);

   hp= ptr-hsize;

   // get ID
   __asm__ __volatile__ (
                        "movl %2, %%edx                    \n" // save size to edx - will be destroyed
                        "orl  $"GCHEAP_SLACK_LIMITS",%%edx \n" // "block" into size
                        "movl %1, %%ecx                    \n" // ignored bits to ecx
                        "movl %%ecx, %%eax                 \n" // init h with ignored bits (h is stored in eax)
                        "shll %%cl, %%edx                  \n" // skip big (shift size) (cl from ecx is correct)

                        "1:                                \n"
                        "  shll $1, %%edx                  \n" // shift size by 1
                        "  jc 0f                           \n" // check carry
                        "  incl %%eax                      \n" // inc h
                        "  jmp 1b                          \n" // and again
                        "0:                                \n"
                        "  incl %%eax                      \n" // !!! ID++; clasical hash returns chunks where items are bigger -> move to smaller (in asm)
                        "  movl %%eax, %0                  \n" // h from register to mem

			: "=g" (ID)
			: "g" (EAIgnoredBits),
                          "g" (hsize)                          // contains size
                        : "eax", "ecx", "edx", "memory"
                      );

   // dodelat: zamknout a pak otestovat, je to rychlejsi, zde je to vyhodnejsi
   //

   // lock FL
   FLASH_LOCK(freeList[ID].shield);

   // check if chunk stays free ( was NOT used in time of locking )
   if( *(ptr-1)!=GCHEAP_CHUNK_FREE )
   {
    // unlock FL
    FLASH_UNLOCK(freeList[ID].shield);
   }
   else
   {

   // dodelat: odstran  predelavani chunku, dam full a vzapeti free

   // mark chunk as full
   *(hp)=
   *(ptr-1)   =GCHEAP_CHUNK_FULL;

   // delete chunk from free FL (p points to delete chunk - initialized above)
    //  p->Last->Next=p->Next;
    if( *((dword*)(hp+GCHEAP_LAST)) ) // != NULL
     *((dword*)(((byte*)(*((dword*)(hp+GCHEAP_LAST))))+GCHEAP_NEXT)) // save value - do not use pointer
     =
     *((dword*)(hp+GCHEAP_NEXT));
    //  p->Next->Last=p->Last;
    if( *((dword*)(hp+GCHEAP_NEXT)) ) // != NULL
     *((dword*)(((byte*)(*((dword*)(hp+GCHEAP_NEXT))))+GCHEAP_LAST)) // save value - do not use pointer
      =
     *((dword*)(hp+GCHEAP_LAST));

   // unlock FL
   FLASH_UNLOCK(freeList[ID].shield);

   // * change structures === link free chunks *
   ptr =  hp;
   size+= hsize;

   } // WAS used in time of locking
  }



  // snift RIGHT neighbour (the same way)
  if( *(ptr+size)==GCHEAP_CHUNK_FREE ) // left tag of the right neighbour
  {
   DBGprintf(DBG_GCHEAP," -> defrag: right neighbour of %i is free",size);

   hp= ptr+size;

   // take out this chunk from free list - manually:
   //  - get chunk size
   //  - lock corresponding FL
   //  - mark chunk as FULL
   //  - delete chunk from FL (no search in FL, it's address is known)
   //  - unlock FL

   // get the size to get ID
   hsize = *(int*)(hp+GCHEAP_SIZE);       // get size of deleted chunk

   DBGprintf(DBG_GCHEAP," -> defrag: rsize %i",hsize);

   // get ID
   __asm__ __volatile__ (
                        "movl %2, %%edx                    \n" // save size to edx - will be destroyed
                        "orl  $"GCHEAP_SLACK_LIMITS",%%edx \n" // "block" into size
                        "movl %1, %%ecx                    \n" // ignored bits to ecx
                        "movl %%ecx, %%eax                 \n" // init h with ignored bits (h is stored in eax)
                        "shll %%cl, %%edx                  \n" // skip big (shift size) (cl from ecx is correct)

                        "1:                                \n"
                        "  shll $1, %%edx                  \n" // shift size by 1
                        "  jc 0f                           \n" // check carry
                        "  incl %%eax                      \n" // inc h
                        "  jmp 1b                          \n" // and again
                        "0:                                \n"
                        "  incl %%eax                      \n" // !!! ID++; clasical hash returns chunks where items are bigger -> move to smaller (in asm)
                        "  movl %%eax, %0                  \n" // h from register to mem

			: "=g" (ID)
			: "g" (EAIgnoredBits),
                          "g" (hsize)                     // contains size
                        : "eax", "ecx", "edx", "memory"
                      );
   // lock FL
   FLASH_LOCK(freeList[ID].shield);

   // check if chunk stays free ( was NOT used in time of locking )
   if( *(ptr+size)!=GCHEAP_CHUNK_FREE )
   {
    // unlock FL
    FLASH_UNLOCK(freeList[ID].shield);
   }
   else
   {

   // mark chunk as full
   hp[0]=
   hp[hsize-1]=GCHEAP_CHUNK_FULL;

   // delete chunk from free FL (p points to delete chunk - initialized above)
    //  p->Last->Next=p->Next;
    if( *((dword*)(hp+GCHEAP_LAST)) ) // != NULL
     *((dword*)(((byte*)(*((dword*)(hp+GCHEAP_LAST))))+GCHEAP_NEXT)) // save value - do not use pointer
     =
     *((dword*)(hp+GCHEAP_NEXT));
    //  p->Next->Last=p->Last;
    if( *((dword*)(hp+GCHEAP_NEXT)) ) // != NULL
     *((dword*)(((byte*)(*((dword*)(hp+GCHEAP_NEXT))))+GCHEAP_LAST)) // save value - do not use pointer
      =
     *((dword*)(hp+GCHEAP_LAST));

   // unlock FL
   FLASH_UNLOCK(freeList[ID].shield);

   // * change structures === link free chunks *
   size+= hsize;

   } // WAS used in time of locking
  }

 #endif

  // insert chunk into free list (behind head)
  // for hashing is always used size *with servinfo*, chunk becomes head

  // save the size (defragmentation can change the size)
  *(int*)(ptr+GCHEAP_SIZE)=            
  *(int*)(ptr+size - 1 - sizeof(int)) = size; 
  
 __asm__ __volatile__ (
                        "movl %2, %%edx                    \n" // save size to edx - will be destroyed
                        "orl  $"GCHEAP_SLACK_LIMITS",%%edx \n" // "block" into size
                        "movl %1, %%ecx                    \n" // ignored bits to ecx
                        "movl %%ecx, %%eax                 \n" // init h with ignored bits (h is stored in eax)
                        "shll %%cl, %%edx                  \n" // skip big (shift size) (cl from ecx is correct)

                        "1:                                \n"
                        "  shll $1, %%edx                  \n" // shift size by 1
                        "  jc 0f                           \n" // check carry
                        "  incl %%eax                      \n" // inc h
                        "  jmp 1b                          \n" // and again
                        "0:                                \n"
                        "  incl %%eax                      \n" // !!! ID++; clasical hash returns chunks where items are bigger -> move to smaller (in asm)
                        "  movl %%eax, %0                  \n" // h from register to mem

			: "=g" (ID)
			: "g" (EAIgnoredBits),
                          "g" (size)                     // contains size 
                        : "eax", "ecx", "edx", "memory"
                      );



 DBGprintf(DBG_GCHEAP," hash=%i",ID); // DBGprintf(DBG_GCHEAP," h=%i/%x, bits ignored %i, size 0x%x / %i",ID,ID,EAIgnoredBits,size,size);

 FLASH_LOCK(freeList[ID].shield);

  // set tags - protected because of defragmentation routine
  ptr[GCHEAP_TAG]=ptr[size-1]=GCHEAP_CHUNK_FREE;

  // ptr 'last'
  *((dword*)(ptr+GCHEAP_LAST)) = (dword)(freeList+ID);
  // ptr 'next'                                           (NULL OK)
  *((dword*)(ptr+GCHEAP_NEXT))
   =
  *((dword*)(((byte*)(&freeList[ID]))+GCHEAP_NEXT));
  // 'last' of the old first chunk
  if(  *((dword*)(((byte*)(&freeList[ID]))+GCHEAP_NEXT)) ) // != NULL
   *((dword*)((*((dword*)(((byte*)(&freeList[ID]))+GCHEAP_NEXT)))+GCHEAP_LAST))
    =
   (dword)ptr;
  // freeList fakeNext
  *((dword*)(((byte*)(&freeList[ID]))+GCHEAP_NEXT)) = (dword)ptr;

 FLASH_UNLOCK(freeList[ID].shield);



 DBGprintf(DBG_GCHEAP," -End-> FREE chunk %p size %i, inserted into ID %i",ptr,size,ID);
}

//----------------------------------------------------------------------------

void GcHeap::destroy() /*fold00*/
// This method is used if instance of heap is *SYSTEM HEAP*. If destroying
// system heap do it this way:
//
//      systemHeap->destroy();
//      systemHeapEnabled=FALSE;
//      delete systemHeap;
//
// This obscure algorithm is required because heap can be stretched typically
// when systemHeapEnabled==TRUE. So destroy kills all EAs except the first
// one in EAs list.
//      The rest of system heap structuctures is done in time of start when
// systemHeapEnabled==TRUE.
{  
 DBGprintf(DBG_GCHEAP," -Begin-> DESTROY");

 ListItem *item = EAs->Next(EAs->GetHead()), // move to the first efemere area
          *hItem;
 byte     *hEA;
 int      i=1;  // skip the first EA

 while( item )
 {
  DBGsection(DBG_GCHEAP,(info()));
  DBGsection(DBG_GCHEAP,(walker()));

  hItem = EAs->Next(item);              // store next item (item will be del)

  hEA = ((EAsItem*)(item))->EA;         // delete efemere area manually,
  ((EAsItem*)(item))->EA = NULL;        // because I need it would be deleted
                                        // before after. (list destructor)
                                        // first deletes EA and then node
                                        // -> now it looks like EA would be
                                        // deleted

  DBGprintf(DBG_GCHEAP,"   Destroying EA %i, list node %p, &EA %p -> next EA %p...",i++,item,hEA,hItem);
  EAs->Delete(item);                    // delete item from *list*

  delete hEA;                           // now is time to delete efemere area

  item = hItem;                         // use backup
 }

 DBGprintf(DBG_GCHEAP," -End-> DESTROY");
}

//----------------------------------------------------------------------------

bool GcHeap::checkChunk(void *voidPtr) /*fold00*/
// - checks chunk
// - gets logical pointer
{
 DBGprintf(DBG_GCHEAP," -Begin-> CHECK_CHUNK logical ptr %p",voidPtr);

 byte *p = ((byte *)voidPtr)-GCHEAP_DATA;       // move from data to tag

 bool full;
 char sFull[2]; sFull[1]=0; sFull[0]='?';
 int  chunkSize;



 // check one chunk
 full=FALSE;

  // L tag
  if( p[GCHEAP_TAG]==GCHEAP_CHUNK_FULL )
  {
   full=TRUE;
   sFull[0]='X';
  }
  else
  {
   if( p[GCHEAP_TAG]==GCHEAP_CHUNK_FREE )
   {
    full=FALSE;
    sFull[0]='.';
   }
   else
   {
    fprintf(stderr,"\n Error: bad node in heap: %p, cause Ltag",p);
    fprintf(stderr,"\n     %s %7i        %p ?",sFull,chunkSize,p);
    return FALSE;
   }
  }

  // chunkSize
  chunkSize = *(int*)(p+GCHEAP_SIZE);

  if( chunkSize < 0 )
  {
    fprintf(stderr,"\n Error: bad node in heap: %p, cause: Lsize",p);
    fprintf(stderr,"\n     %s %7i        %p ?",sFull,chunkSize,p);
    return FALSE;
  }

  // check the right side of the chunk
  // RTAG
  if( full && (*(byte*)(p+chunkSize-1)==GCHEAP_CHUNK_FULL) )
  ;
  else
  {
   if( !full && (*(byte*)(p+chunkSize-1)==GCHEAP_CHUNK_FREE) )
   ;
   else
   {
     fprintf(stderr,"\n Error: bad node in heap: %p, cause: Rtag is different from Ltag",p);
     fprintf(stderr,"\n     %s %7i        %p ?",sFull,chunkSize,p);
     return FALSE;
   }
  }

  // check things specific for free chunk
  if( !full )
  {

   // 1) Rsize
   if( (*(int*)(p+GCHEAP_SIZE)) != (*(int*)(p+chunkSize-1 - sizeof(int))) )
   {
    fprintf(stderr,"\n Error: bad node in heap: %p, cause: Rsize is different from LSize",p);
    fprintf(stderr,"\n     %s %7i        %p ?",sFull,chunkSize,p);
    return FALSE;
   }

   // 2) free list pointers
    // check: p->Last->Next == p
    if( *((dword*)(p+GCHEAP_LAST)) ) // != NULL
    {
     if( (byte*)*((dword*)(((byte*)(*((dword*)(p+GCHEAP_LAST))))+GCHEAP_NEXT))
         !=
         p
       )
     {
      fprintf(stderr,"\n Error: bad node in heap: %p, cause: free list last->next pointer invalid",p);
      fprintf(stderr,"\n     %s %7i        %p ?",sFull,chunkSize,p);
      return FALSE;
     }
    }


    // check:  p->Next->Last == p
    if( *((dword*)(p+GCHEAP_NEXT)) ) // != NULL
    {
     if( (byte*)*((dword*)(((byte*)(*((dword*)(p+GCHEAP_NEXT))))+GCHEAP_LAST))
         !=
         p
       )
     {
      fprintf(stderr,"\n Error: bad node in heap: %p, cause: free list next->last pointer invalid",p);
      fprintf(stderr,"\n     %s %7i        %p ?",sFull,chunkSize,p);
      return FALSE;
     }
    }
  }



 DBGprintf(DBG_GCHEAP," -End-> CHECK_CHUNK");
 return TRUE;
}

//----------------------------------------------------------------------------

bool GcHeap::isPtrRaw(void *voidPtr) /*fold00*/
// takes ptr to service info, checks if pointer points to heap
{
 DBGprintf(DBG_GCHEAP," -Begin-> IS_PTR ptr %p",voidPtr);

 int      i;
 ListItem *item;



 i=0;
 item=EAs->GetHead();

 while( item )
 {
  DBGprintf(DBG_GCHEAP,"  --> Efemere area # %i:",i++);

  if( ((EAsItem *)item)->isPtr((byte*)voidPtr) )
   return TRUE;

  item=EAs->Next(item);
 }

 DBGsection(DBG_GC,(printf("%c",7)));
 DBGprintf(DBG_GC,"<-------------------------------------------------->");
 DBGprintf(DBG_GC,"  isPtrRaw: pointer %p is *not* mine",voidPtr);
 DBGprintf(DBG_GC,"<-------------------------------------------------->");

 return FALSE;
}

//----------------------------------------------------------------------------

bool GcHeap::isPtr(void *voidPtr) /*fold00*/
// takes ptr to data, checks if pointer points to heap
{
 DBGprintf(DBG_GCHEAP," -Begin-> IS_PTR logical ptr %p",voidPtr);

 return isPtrRaw((((byte*)voidPtr)-GCHEAP_DATA));
}

//----------------------------------------------------------------------------

bool GcHeap::isFreeChunkRaw(void *voidPtr) /*fold00*/
// gets pointer to service info
{
 DBGprintf(DBG_GCHEAP," -Begin-> IS_FREE_CHUNK ptr %p",voidPtr);

 if( *((byte *)voidPtr) == GCHEAP_CHUNK_FREE )
  return TRUE;
 else
  return FALSE;

 DBGprintf(DBG_GCHEAP," -End-> IS_FREE_CHUNK");
}

//----------------------------------------------------------------------------

bool GcHeap::isFreeChunk(void *voidPtr) /*fold00*/
// gets pointer to data
{
 DBGprintf(DBG_GCHEAP," -Begin-> IS_FREE_CHUNK logical ptr %p",voidPtr);

 byte *p = ((byte *)voidPtr)-GCHEAP_DATA;       // move from data to tag

 if( *((byte *)p) == GCHEAP_CHUNK_FREE )
  return TRUE;
 else
  return FALSE;

 DBGprintf(DBG_GCHEAP," -End-> IS_FREE_CHUNK");
}

//----------------------------------------------------------------------------

bool GcHeap::isFullChunkRaw(void *voidPtr) /*fold00*/
// gets pointer to service info
{
 DBGprintf(DBG_GCHEAP," -Begin-> IS_FULL_CHUNK ptr %p",voidPtr);

  return !isFreeChunkRaw(voidPtr);

 DBGprintf(DBG_GCHEAP," -End-> IS_FULL_CHUNK");
}

//----------------------------------------------------------------------------

bool GcHeap::isFullChunk(void *voidPtr) /*fold00*/
// gets pointer to data
{
 DBGprintf(DBG_GCHEAP," -Begin-> IS_FULL_CHUNK logical ptr %p",voidPtr);

  return !isFreeChunk(voidPtr);

 DBGprintf(DBG_GCHEAP," -End-> IS_FULL_CHUNK");
}

//----------------------------------------------------------------------------

void GcHeap::dumpChunk(void *voidPtr) /*fold00*/
// dumps chunk
{
 DBGprintf(DBG_GCHEAP," -Begin-> DUMP_CHUNK logical ptr %p",voidPtr);

 char *buf = "CHUNK.dump";
 byte *ptr = ((byte *)voidPtr)-GCHEAP_DATA;     // move from data to tag
 int  size = *(int*)(ptr+GCHEAP_SIZE),
      handle;



 // create name for dump file
 if( (handle=open( buf,
                   O_CREAT|O_RDWR|O_TRUNC,
                   S_IRWXU|S_IRWXG|S_IRWXO
                 )
     )
      <
     0
   )
  return;

  write(handle,(void *)ptr,size);

  close(handle);

 DBGprintf(DBG_GCHEAP," -End-> DUMP_CHUNK");
}

//----------------------------------------------------------------------------

void GcHeap::dump( int where ) /*fold00*/
{
 int      i,
          handle;
 ListItem *item;
 char     buf[17];

 switch( where )
 {
  case GCHEAP_DUMP_2_STDERR:
        // go through EAs and dump each efemere area
        DBGprintf(DBG_GCHEAP,"- Heap Dump -----------------------------------------");

        i=0;
        item=EAs->GetHead();

        while( item )
        {
         DBGprintf(DBG_GCHEAP,"--> Efemere area # %i:",i++);

         ((EAsItem *)item)->dump();
         item=EAs->Next(item);
        }

        DBGprintf(DBG_GCHEAP,"- Heap Dump -----------------------------------------");
       break;
  case GCHEAP_DUMP_2_FILE:
        i=0;
        item=EAs->GetHead();

        while( item )
        {
         // create name for dump file
         buf[0]=0;
         sprintf(buf,"EA%i.dump",i++);

         if( (handle=open( buf,
                           O_CREAT|O_RDWR|O_TRUNC,
                           S_IRWXU|S_IRWXG|S_IRWXO
                         )
             )
             <
             0
           )
          return;

          ((EAsItem *)item)->dump(GCHEAP_DUMP_2_FILE,handle);
         close(handle);

         item=EAs->Next(item);
        }
       break;
 }
}

//----------------------------------------------------------------------------

void GcHeap::info() /*fold00*/
// heapsize service, heap size, allocated, free, biggestEA, #ea, ignbits, int
{
 fprintf(stderr ,"\n+- Heap info: --------------------"
                 "\n| Heap size start (ms) : %i"
                 "\n| Heap size limit (mx) : %i"
                 "\n| Allocated            : %i"
                 "\n| Free                 : %i"
                 "\n| EA count             : %i"
                 "\n| Heap size service    : %i"
                 "\n| Allocated service    : %i"
                 "\n| EAIgnoredBits        : %i"
                 "\n| lastUsedInterval     : %i"
                 "\n| MaxAlloc by bits     : %i"
                 "\n+---------------------------------\n\n",
                  ms,
                  mx,
                  allocatedBytes,
                  mx-allocatedBytes,
                  EAsCount,
                  heapSizeSI,
                  allocatedBytesSI,
                  EAIgnoredBits,
                  lastUsedInterval,
                  maxAllocatableChunk
        );
}

//----------------------------------------------------------------------------

int  GcHeap::coreLeft() /*fold00*/
{
 return (mx-allocatedBytes);
}

//----------------------------------------------------------------------------

int  GcHeap::totalMem() /*fold00*/
{
 return mx;
}

//----------------------------------------------------------------------------

void GcHeap::listingFL() /*fold00*/
// - lists content of free lists
{
 int i;

 fprintf(stderr,"\n\n- FreeList lister -----------------------------------------");

 for(i=0;i<GCHEAP_NROFFREELISTS;i++)
 {
  fprintf(stderr,"\nFL %i:",i);
  freeList[i].listing();
 }

 fprintf(stderr,"\n- FreeList lister -----------------------------------------\n\n");
}

//----------------------------------------------------------------------------

void GcHeap::walker() /*fold00*/
{
 // go through EAs and dump each efemere area
 fprintf(stderr,"\n\n- Heap walker --------------------------------------------\n");

 int      i=0;
 ListItem *item=EAs->GetHead();

 while( item )
 {
  fprintf(stderr,"\nEA %i",i++);

  ((EAsItem *)item)->walker();
  item=EAs->Next(item);
 }

 fprintf(stderr,"\n\n- Heap walker ----------------------------------------------\n\n");
}

//----------------------------------------------------------------------------

bool GcHeap::check() /*fold00*/
// - heap check, if error occured 
//   -> dumps heap content into file, writes bad EA and
//   sends invalid memory reference signal 11 -> *core* is dumped
{
 int      i=0;
 ListItem *item=EAs->GetHead();

 while( item )
 {

  if( !((EAsItem *)item)->check() )
  {
    fprintf(stderr,"\n Error:\n  heap check: efemere area #%i corrupted\n",i);
    dump(GCHEAP_DUMP_2_FILE);
    // self send segmentation violation signal
    i=kill(getpid(),SIGSEGV);
    for(;;); // loop
  }

  item=EAs->Next(item);
  i++;
 }

 return TRUE;
}

//----------------------------------------------------------------------------

bool GcHeap::fillFree(byte pattern) /*fold00*/
{
 int      i=0;
 ListItem *item=EAs->GetHead();

 while( item )
 {

  if( !((EAsItem *)item)->fillFree(pattern) )
  {
    fprintf(stderr,"\n Error:\n  fill free: efemere area #%i corrupted\n",i);
    dump(GCHEAP_DUMP_2_FILE);
    // self send segmentation violation signal
    i=kill(getpid(),SIGSEGV);
    for(;;); // loop
  }

  item=EAs->Next(item);
  i++;
 }

 return TRUE;
}

//----------------------------------------------------------------------------
                                
bool GcHeap::checkFillFree(byte pattern) /*fold00*/
{
 int      i=0;
 ListItem *item=EAs->GetHead();

 while( item )
 {

  if( !((EAsItem *)item)->checkFillFree(pattern) )
  {
    fprintf(stderr,"\n Error:\n  check fill free: efemere area #%i corrupted\n",i);
    dump(GCHEAP_DUMP_2_FILE);
    // self send segmentation violation signal
    i=kill(getpid(),SIGSEGV);
    for(;;); // loop
  }

  item=EAs->Next(item);
  i++;
 }

 return TRUE;
}
                                
//----------------------------------------------------------------------------

bool GcHeap::checkFillFreeChunk(void *voidPtr,byte pattern) /*fold00*/
// - gets logical pointer
// - checks free chunks and it's fill
{
 DBGprintf(DBG_GCHEAP," -Begin-> CHECK_FILL_FREE_CHUNK logical ptr %p",voidPtr);

 byte *p = ((byte *)voidPtr)-GCHEAP_DATA;       // move from data to tag

 bool full;
 char sFull[2]; sFull[1]=0; sFull[0]='?';
 int  chunkSize;
 int  j;


 // check one chunk
 full=FALSE;

  // L tag
  if( p[GCHEAP_TAG]==GCHEAP_CHUNK_FREE )
  {
   full=FALSE;
   sFull[0]='.';
  }
  else
  {
   fprintf(stderr,"\n Error: bad node in heap: %p, cause Ltag",p);
   fprintf(stderr,"\n     %s %7i        %p ?",sFull,chunkSize,p);
   return FALSE;
  }

  // chunkSize
  chunkSize = *(int*)(p+GCHEAP_SIZE);

  if( chunkSize < 0 )
  {
    fprintf(stderr,"\n Error: bad node in heap: %p, cause: Lsize",p);
    fprintf(stderr,"\n     %s %7i        %p ?",sFull,chunkSize,p);
    return FALSE;
  }

  // check the right side of the chunk
  // R tag
  if( !full && (*(byte*)(p+chunkSize-1)==GCHEAP_CHUNK_FREE) )
  ;
  else
  {
   fprintf(stderr,"\n Error: bad node in heap: %p, cause: Rtag is different from Ltag",p);
   fprintf(stderr,"\n     %s %7i        %p ?",sFull,chunkSize,p);
   return FALSE;
  }

  // check things specific for free chunk
  if( !full )
  {

   // 1) Rsize
   if( (*(int*)(p+GCHEAP_SIZE)) != (*(int*)(p+chunkSize-1 - sizeof(int))) )
   {
    fprintf(stderr,"\n Error: bad node in heap: %p, cause: Rsize is different from LSize",p);
    fprintf(stderr,"\n     %s %7i        %p ?",sFull,chunkSize,p);
    return FALSE;
   }

   // 2) free list pointers
    // check: p->Last->Next == p
    if( *((dword*)(p+GCHEAP_LAST)) ) // != NULL
    {
     if( (byte*)*((dword*)(((byte*)(*((dword*)(p+GCHEAP_LAST))))+GCHEAP_NEXT))
         !=
         p
       )
     {
      fprintf(stderr,"\n Error: bad node in heap: %p, cause: free list last->next pointer invalid",p);
      fprintf(stderr,"\n     %s %7i        %p ?",sFull,chunkSize,p);
      return FALSE;
     }
    }


    // check:  p->Next->Last == p
    if( *((dword*)(p+GCHEAP_NEXT)) ) // != NULL
    {
     if( (byte*)*((dword*)(((byte*)(*((dword*)(p+GCHEAP_NEXT))))+GCHEAP_LAST))
         !=
         p
       )
     {
      fprintf(stderr,"\n Error: bad node in heap: %p, cause: free list next->last pointer invalid",p);
      fprintf(stderr,"\n     %s %7i        %p ?",sFull,chunkSize,p);
      return FALSE;
     }
    }

    //+--------------------------+
    //| check fill in free chunk |
    //+--------------------------+
    for( j=0; j<chunkSize-GCHEAP_FREE_SERVINFO; j++ )
    {
     if( p[GCHEAP_FREE_PREFIX+j] != pattern )
     {
      fprintf(stderr,"\n Error: bad node in heap: %p, cause: fill corrupted",p);
      fprintf(stderr,"\n     %s %7i        %p",sFull,chunkSize,p);
      return FALSE;
     }
    }
  }



 DBGprintf(DBG_GCHEAP," -End-> CHECK_FILL_FREE_CHUNK");
 return TRUE;
}

//----------------------------------------------------------------------------

GcHeap::~GcHeap() /*fold00*/
{
 DBGprintf(DBG_GCHEAP,"-Begin-> GcHeap DEstructor");

 DBGsection(DBG_GCHEAP,(info()));
 DBGsection(DBG_GCHEAP,(walker()));

 delete EAs;
 logrFree(freeList);

 DBGprintf(DBG_GCHEAP,"-End-> GcHeap DEstructor");
}
 /*FOLD00*/


//- class FreeListItem methods ------------------------------------------------



bool FreeListItem::get( int &size, byte *&p ) /*fold00*/
// - size: service info included -> size of the whole chunk
// - take out the chunk of size >= 'size' and return it's address and size
// - 'size' is always <= all chunks in list (see hash()) so any of chunks
//   can be taken
// - operations directly with chunks are correct because of the same
//   header (see struct FreeListItem)
// - if not found size and ptr stays the same
{
 p = (byte*)*((dword*)(((byte*)this)+GCHEAP_NEXT));

 if( p ) // p != NULL ... list is NOT empty
 {

  FLASH_LOCK(shield);
              
   // check if in time of locking list was not changed
   if( !p ) // p==NULL
   {
    FLASH_UNLOCK(shield);
    return FALSE;
   }

   // take out the first item from free list and format this chunk

   // format: mark tags
   *p = p[*((int*)(p+GCHEAP_SIZE))-1] = GCHEAP_CHUNK_FULL;

   dword *np = ((dword*)(p+GCHEAP_NEXT)); // forsage

   // delete chunk from free list
    // Because the first chunk from the list is deleted I know that
    // p->Last->Next = p->Next;
    // ==
    // this->next    = p->Next;
    *((dword*)(((byte*)this)+GCHEAP_NEXT))
    =
    *np;
    //  p->Next->Last = p->Last;
    if( *np ) // != NULL
     *((dword*)(((byte*)(*np))+GCHEAP_LAST)) // save value - do not use pointer
     =
     *((dword*)(p+GCHEAP_LAST));

   size=*(int*)(p+GCHEAP_SIZE);

  FLASH_UNLOCK(shield);

  return TRUE;
 }
 else
  return FALSE;
}

//----------------------------------------------------------------------------
     
bool FreeListItem::searchFor( int &size, byte *&p ) /*fold00*/
// - size: service info included -> size of the whole chunk
// - find a chunk of size >= 'size' and return it's address and size
// - searching in interval where lowLimit <= size <= highLimit
// - if not found size and ptr is protected
{
 p= (byte*) *((dword*)(((byte*)this)+GCHEAP_NEXT));

 FLASH_LOCK(shield);

  DBGprintf(DBG_GCHEAP,"Inside searchFor:");

  while( p ) // p != NULL
  {
                        //DBGprintf(DBG_GCHEAP," -> %p, %i",p,*(int*)(p+GCHEAP_SIZE));

   if( size <= *(int*)(p+GCHEAP_SIZE) )
   {
    DBGprintf(DBG_GCHEAP," found: %i",*(int*)(p+GCHEAP_SIZE));

    // mark tags
    *p=p[*(int*)(p+GCHEAP_SIZE)-1]=GCHEAP_CHUNK_FULL;

    dword *np = ((dword*)(p+GCHEAP_NEXT)); // forsage
    dword *lp = ((dword*)(p+GCHEAP_LAST)); // forsage

    // delete chunk from free list
     //  p->Last->Next=p->Next;
     if( *lp ) // != NULL
      *((dword*)(((byte*)(*lp))+GCHEAP_NEXT)) // save value - do not use pointer
      =
      *np;
     //  p->Next->Last=p->Last;
     if( *np ) // != NULL
      *((dword*)(((byte*)(*np))+GCHEAP_LAST)) // save value - do not use pointer
       =
      *lp;

    // set size
    size=*(int*)(p+GCHEAP_SIZE);
 
    FLASH_UNLOCK(shield);

    return TRUE;
   }

   p=(byte*)*((dword*)(p+GCHEAP_NEXT));
  }

  FLASH_UNLOCK(shield);

 return FALSE;
}

//----------------------------------------------------------------------------


//#define PRECISE_LISTING

void FreeListItem::listing() /*fold00*/
{
 byte *p= (byte*) *((dword*)(((byte*)this)+GCHEAP_NEXT));

 #ifdef PRECISE_LISTING
  fprintf(stderr,"\n Listing: (this %p chunks %p pseudoLast %p pseudoSize %i)",
                       this,
                       (void*)*((dword*)(((byte*)this)+GCHEAP_NEXT)),
                       (void*)*((dword*)(((byte*)this)+GCHEAP_LAST)),
                       (void*)*((dword*)(((byte*)this)+GCHEAP_SIZE))
           );
 #endif


  while( p != NULL )
  {
    fprintf(stderr,"\n -> %p, %i \t\t\tlast %p next %p",
              p,
              *(int*)(p+GCHEAP_SIZE),
              (void*) *(int*)(p+GCHEAP_LAST),
              (void*) *(int*)(p+GCHEAP_NEXT)
             );

   p=(byte*)*(dword*)(p+GCHEAP_NEXT);
  }
}

#undef PRECISE_LISTING
 /*FOLD00*/


//- EOF -----------------------------------------------------------------------
