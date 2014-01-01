/*
 * gc.cc
 *
 * Author: Dvorka
 *
 * ToDo:
 */

#include "gc.h"

#include <pthread.h>
#include <stdlib.h>
#include <sys/cdefs.h>
#include <unistd.h>

#include "bool.h"
#include "debuglog.h"
#include "gcfinalize.h"
#include "gcheap.h"
#include "gcollector.h"
#include "gcreference.h"
#include "gcsupport.h"
#include "heapobjects.h"
#include "reference.h"



//- Externs --------------------------------------------------------------------

#ifdef GC_ENABLE_SYSTEM_HEAP
 extern GcHeap          *systemHeap;            // JVM inner heap
 extern bool            systemHeapEnabled;
#endif

extern GcHeap           *logrHeap;

extern ReferenceVectors *logrHeapRefs;

extern GarbageCollector *logrHeapGC;

extern InstRef          *nullInstRef;
extern StatRef          *nullStatRef;

//-----------------------------------------------------------------------------

int  bitField[] = { /*fold00*/
                   0x00000001,
                   0x00000002,
                   0x00000004,
                   0x00000008,
                   0x00000010,
                   0x00000020,
                   0x00000040,
                   0x00000080,
                   0x00000100,
                   0x00000200,
                   0x00000400,
                   0x00000800,
                   0x00001000,
                   0x00002000,
                   0x00004000,
                   0x00008000,
                   0x00010000,
                   0x00020000,
                   0x00040000,
                   0x00080000,
                   0x00100000,
                   0x00200000,
                   0x00400000,
                   0x00800000,
                   0x01000000,
                   0x02000000,
                   0x04000000,
                   0x08000000,
                   0x10000000,
                   0x20000000,
                   0x40000000,
                   0x80000000
                  };
 /*FOLD00*/


//-----------------------------------------------------------------------------



bool isBitSet( int bit, char *bitmap ) /*fold00*/
{
  //         i/32               i%32
  if( bitmap[bit>>5] & bitField[bit&0x0000001F] )
  return 1;
 else
  return 0;
}
 /*FOLD00*/


//-----------------------------------------------------------------------------



void traceStatReferences() /*fold00*/
{
 DBGprintf(DBG_GCTRACE,"-Begin-> traceStatReferences");

 int      i,
          b;                    // for bitmap tracing
 int      size;
 int      bitmapWords;
 ListItem *item;
 StatRef  *sR;
 dword    *bitmap;
 void     *hPtr;


  // +---------------------------+
  // | Go through StatRef vector |
  // +---------------------------+

  item= logrHeapRefs->statRefVecs->GetHead();

  while( item )
  {
   // for each vector
   size = ((StatRefVecsItem*)item)->lng; DBGprintf(DBG_GCTRACE," ----> StatRef vector %p size %d",item,size);



   for( i=0; i<size; i++ )
   {
    // check if is valid
    if( ((StatRefVecsItem*)item)->vector[i].state != REF_NOT_VALID )
    {
     // reference is valid -> check type
     switch( ((StatRefVecsItem*)item)->vector[i].type )
     {
      case STATREF_VOID:
        //DBGprintf(DBG_GCTRACE," %d -> VOID: refCount %d",i,((StatRefVecsItem*)item)->vector[i].refCount);
       break;
      case STATREF_FIXED_DATA:
        DBGprintf(DBG_GCTRACE," %d -> FIXED_DATA:",i); DBGsection(DBG_GCTRACE,(printStatRef(&(((StatRefVecsItem*)item)->vector[i]))));
       break;
      case STATREF_RUNTIME_DATA:
        DBGprintf(DBG_GCTRACE," %d -> RUNTIME_DATA:",i); DBGsection(DBG_GCTRACE,(printStatRef(&(((StatRefVecsItem*)item)->vector[i]))));

                sR = &(((StatRefVecsItem*)item)->vector[i]);
                lockStatRef(sR);                       // runtime data locked



                // +-----------------+
                // | Instance bitmap |
                // +-----------------+

                DBGprintf(DBG_GCTRACE," Tracing instance bitmap...");

                hPtr   = sR->ptr;                      // *hPtr*       (is never NULL)
                bitmap = (dword*)
                         (
                          (byte*)hPtr                                     // RD
                          +
                          ((RuntimeDataHeader*)hPtr)->instanceDataBitmap  // offset
                         );


                // get number of words in instance
                bitmapWords = ((RuntimeDataHeader*)hPtr)->instanceDataCount;

                DBGprintf(DBG_GCTRACE," ################# ");
                DBGprintf(DBG_GCTRACE," # words    %d",bitmapWords);
                DBGprintf(DBG_GCTRACE," # bitmap   %p",bitmap);
                DBGprintf(DBG_GCTRACE," # bitOff   %d",((RuntimeDataHeader*)hPtr)->instanceDataBitmap);
                DBGprintf(DBG_GCTRACE," ################# ");

                for( b=0; b<bitmapWords; b++ )
                {

                 // Bitmap structure:
                 //
                 // 31 <- 0            bitmap is array of integers
                 // ---------
                 // | | | | |  0
                 // ---------
                 // | | | | |  ||
                 // ---------  \/
                 // | | | | |
                 // ...
                
                 //         i/32             i%32
                 if( bitmap[b>>5] & bitField[b&0x0000001F] )   // check if bit is set
                  DBGprintf(DBG_GCTRACE," *");
                 else // it is primitive type
                  DBGprintf(DBG_GCTRACE," -");
                
                }

                DBGprintf(DBG_GCTRACE," ...tracing instance bitmap");



                // +--------------------+
                // | Static data bitmap |
                // +--------------------+

                DBGprintf(DBG_GCTRACE," Tracing static data bitmap...");

                hPtr   = sR->ptr;                      // *hPtr*       (is never NULL)
                bitmap = (dword*)
                         (
                          (byte*)hPtr                                   // RD
                          +
                          ((RuntimeDataHeader*)hPtr)->staticDataBitmap  // offset
                         );


                // get number of words in static data
                bitmapWords = ((RuntimeDataHeader*)hPtr)->staticDataCount;

                DBGprintf(DBG_GCTRACE," ################# ");
                DBGprintf(DBG_GCTRACE," # words    %d",bitmapWords);
                DBGprintf(DBG_GCTRACE," # bitmap   %p",bitmap);
                DBGprintf(DBG_GCTRACE," # bitOff   %d",((RuntimeDataHeader*)hPtr)->staticDataBitmap);
                DBGprintf(DBG_GCTRACE," ################# ");

                for( b=0; b<bitmapWords; b++ )
                {

                 // Bitmap structure:
                 //
                 // 31 <- 0            bitmap is array of integers
                 // ---------
                 // | | | | |  0
                 // ---------
                 // | | | | |  ||
                 // ---------  \/
                 // | | | | |
                 // ...
                
                 //         i/32             i%32
                 if( bitmap[b>>5] & bitField[b&0x0000001F] )
                  DBGprintf(DBG_GCTRACE," *");
                 else // it is primitive type
                  DBGprintf(DBG_GCTRACE," -");
                
                }

                DBGprintf(DBG_GCTRACE," ...tracing static data bitmap");



                unlockStatRef(sR);

       break;
      case STATREF_ARRAY_HASHTABLE:
        DBGprintf(DBG_GCTRACE," %d -> HASHTABLE_DATA:",i); DBGsection(DBG_GCTRACE,(printStatRef(&(((StatRefVecsItem*)item)->vector[i]))));
       break;
      case STATREF_CLASSLOADER_HASHTABLE:
        DBGprintf(DBG_GCTRACE," %d -> CLASSLOADER_HASHTABLE:",i); DBGsection(DBG_GCTRACE,(printStatRef(&(((StatRefVecsItem*)item)->vector[i]))));
       break;
      case STATREF_INTERFACE_METHOD_TABLE:
        DBGprintf(DBG_GCTRACE," %d -> METHOD_TABLE:",i); DBGsection(DBG_GCTRACE,(printStatRef(&(((StatRefVecsItem*)item)->vector[i]))));
       break;
      default:
        DBGprintf(DBG_GCTRACE," -type-> ?");
     } // switch

    } // else reference NOT valid
   }

   item=logrHeapRefs->statRefVecs->Next(item);
  }



 DBGprintf(DBG_GCTRACE,"-End-> traceStatReferences");
}
 /*FOLD00*/


//-----------------------------------------------------------------------------



void colorizeInstanceRecursively( InstRef *iR ) /*fold00*/
// - this function traces:
//    i)  instance itself
//    ii) static data of coresponding class
// - reference to instance is NOT locked
{
 DBGprintf(DBG_GCTRACE,"-Begin-> colorizeInstance");
 DBGsection(DBG_GCTRACE,(printInstRef(iR)));


 int     i;
 int     bitmapWords;

 void    *iPtr;         // iPtr = iR->ptr
 dword   *instance;     // instance itself inside iPtr
 StatRef *sR;           // reference to RuntimeData from iR->ptr
 void    *sPtr;         // sPtr = sR->ptr
 dword   *bitmap;       // bitmap in sPtr
 dword   *staticData;   // address of static data
 InstRef *instanceField,// field loaded from instance
         *staticField;  // static data field



 #ifdef GC_CHECK_INSTANCE_VALIDATION
  checkInstanceValidation(iR);
 #endif



 lockInstRef(iR);

 iPtr=iR->ptr;
 
 if( iPtr==NULL        // check if it's empty reference (size==0,ptr==NULL)
     ||                // or
     iR==nullInstRef   // it is nullInstRef
   )
 {
  DBGprintf(DBG_GCTRACE," Colorized -> RED       iR->ptr==NULL");

  iR->color = GC_RED;  // mark it as done 

  unlockInstRef(iR);
  return;
 }



 // reference should be worked out -> because work on it is in progress
 // -> mark it as ORANGE
 // instance is traced -> can be marked as RED
 iR->color = GC_ORANGE;

 DBGprintf(DBG_GCTRACE,"+---------------------------------------------------------------");
 DBGprintf(DBG_GCTRACE,"| Work on %p is in progress -> marking it as ORANGE...",iR);




 // ------------------
 // | trace instance |
 // ------------------

 instance = (dword*)((byte*)iPtr+sizeof(InstanceHeader));

 // ireference is locked -> pointer to RuntimeData can be taken (dwords)
 sR = ((InstanceHeader*)iPtr)->runtimeData;

 lockStatRef(sR);                       // runtime data locked

 sPtr   = sR->ptr;

 // get the address of bitmap from RuntimeData
 bitmap = (dword*)
          (
           (byte*)sPtr                                     // RD
           +
           ((RuntimeDataHeader*)sPtr)->instanceDataBitmap  // offset
          );

 // get number of words in instance
 bitmapWords = ((RuntimeDataHeader*)sPtr)->instanceDataCount;



 DBGprintf(DBG_GCTRACE,"   +- Recursively traced instance -----------------");
 DBGprintf(DBG_GCTRACE,"   | InstRef  %p",iR);
 DBGprintf(DBG_GCTRACE,"   | instance %p",iPtr);
 DBGprintf(DBG_GCTRACE,"   | RDRef    %p",sR);
 DBGprintf(DBG_GCTRACE,"   | RD       %p",sPtr);
 DBGprintf(DBG_GCTRACE,"   | bmpOff   %d",((RuntimeDataHeader*)sPtr)->instanceDataBitmap);
 DBGprintf(DBG_GCTRACE,"   | bmp      %p",bitmap);
 DBGprintf(DBG_GCTRACE,"   | bmpWords %d",bitmapWords);
 DBGprintf(DBG_GCTRACE,"   +------------------");



 DBGprintf(DBG_GCTRACE,"    Tracing instance bitmap...");

 for( i=0; i<bitmapWords; i++ )
 {

  // Bitmap structure:
  //
  // 31 <- 0            bitmap is array of integers
  // ---------
  // | | | | |  0
  // ---------
  // | | | | |  ||
  // ---------  \/
  // | | | | |
  // ...

  //         i/32             i%32
  if( bitmap[i>>5] & bitField[i&0x0000001F] )
  {
   // field is reference -> dive deeper (recursive call)
   DBGprintf(DBG_GCTRACE,"    * Colorizer");

   // !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!!

   // load copy of field in instance (it can be locked using -1)

   instanceField = readRefField(((InstRef**)(&(instance[i]))));

   // !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!!

   // check the color of the instance, dive if it is BLUE or GREEN
   if(
      instanceField->color == GC_BLUE
       ||
      instanceField->color == GC_GREEN
     )
   {
    DBGprintf(DBG_GCTRACE," ->> Colorize %p DOWN",instanceField);

     colorizeInstanceRecursively( instanceField );

    DBGprintf(DBG_GCTRACE," <<- Colorize %p UP",instanceField);
   }
   else // is RED or ORANGE
   {
    DBGprintf(DBG_GCTRACE," Colorized (RED) || working on (ORANGE)");
   }

  }
  // else it is primitive type
  else
   DBGprintf(DBG_GCTRACE," - Colorizer");

 }

 DBGprintf(DBG_GCTRACE,"    ... tracing instance bitmap");



 // +--------+
 // | Static |
 // +--------+

 // get the address of static data
 staticData = (dword*)
               (
                (byte*)sPtr                                   // RD
                +
                ((RuntimeDataHeader*)sPtr)->staticDataOffset  // offset
               );

 // get the address of bitmap from RuntimeData
 bitmap = (dword*)
          (
           (byte*)sPtr                                  // RD
           +
           ((RuntimeDataHeader*)sPtr)->staticDataBitmap // offset
          );

 // get number of words in instance
 bitmapWords = ((RuntimeDataHeader*)sPtr)->staticDataCount;



 DBGprintf(DBG_GCTRACE,"   +- Recursively traced static bitmap -----------------");
 DBGprintf(DBG_GCTRACE,"   | statOff  %d",((RuntimeDataHeader*)sPtr)->staticDataOffset);
 DBGprintf(DBG_GCTRACE,"   | static   %p",staticData);
 DBGprintf(DBG_GCTRACE,"   | bmpOff   %d",((RuntimeDataHeader*)sPtr)->staticDataBitmap);
 DBGprintf(DBG_GCTRACE,"   | bmp      %p",bitmap);
 DBGprintf(DBG_GCTRACE,"   | bmpWords %d",bitmapWords);
 DBGprintf(DBG_GCTRACE,"   +------------------");




 // +--------------------+
 // | java.langClass     |
 // +--------------------+

 DBGprintf(DBG_GCTRACE,"    Tracing java.langClass instance for this class...");

 if( ((RuntimeDataHeader*)sPtr)->javaLangClassRef == nullInstRef )
 {
  DBGprintf(DBG_GCTRACE,"    --> reference to java.lang.Class of this instance is NULL...");
 }
 else
 {
  // trace java.lang.Class recursively
  if(
     staticField->color == GC_BLUE
      ||
     staticField->color == GC_GREEN
    )
  {
   DBGprintf(DBG_GCTRACE," ->> Colorize java.lang.Class %p",iR);
         colorizeInstanceRecursively( ((RuntimeDataHeader*)sPtr)->javaLangClassRef );
   DBGprintf(DBG_GCTRACE," <<- Colorize java.lang.Class %p",iR);
  }
  else
   DBGprintf(DBG_GCTRACE," --> java.lang.Class instance already in process (RED|ORANGE)");
 }

 DBGprintf(DBG_GCTRACE,"    ...tracing java.langClass instance for this class");



 // +--------------------+
 // | Static data bitmap |
 // +--------------------+

 DBGprintf(DBG_GCTRACE,"    Tracing static data bitmap...");

 for( i=0; i<bitmapWords; i++ )
 {

  // Bitmap structure:
  //
  // 31 <- 0            bitmap is array of integers
  // ---------
  // | | | | |  0
  // ---------
  // | | | | |  ||
  // ---------  \/
  // | | | | |
  // ...

  //         i/32             i%32
  if( bitmap[i>>5] & bitField[i&0x0000001F] )
  {
   // field is reference -> dive deeper (recursive call)
   DBGprintf(DBG_GCTRACE,"    *  Static colorizer");

   // !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!!

   // load copy of field in static data (it can be locked using -1)

   staticField = readRefField(((InstRef**)(&(staticData[i]))));
                 
   // !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!!

   // check the color of the instance, dive if it is BLUE or GREEN
   if(
      staticField->color == GC_BLUE
       ||
      staticField->color == GC_GREEN
     )
   {
    DBGprintf(DBG_GCTRACE," ->> Colorize static %p",staticField);

     colorizeInstanceRecursively( staticField );

    DBGprintf(DBG_GCTRACE," <<- Colorize static %p",staticField);
   }
   else // is RED or ORANGE
   {
    DBGprintf(DBG_GCTRACE," Static field colorized (RED) || working on (ORANGE)");
   }

  }
  // else it is primitive type
  else
   DBGprintf(DBG_GCTRACE,"    - Static colorizer");

 }


 DBGprintf(DBG_GCTRACE,"    ... tracing static data bitmap");



 // instance is traced -> can be marked as RED
 iR->color = GC_RED;

 DBGprintf(DBG_GCTRACE,"| Reference %p traced -> marked as RED...",iR);
 DBGprintf(DBG_GCTRACE,"+---------------------------------------------------------------");



 unlockStatRef(sR);
 unlockInstRef(iR);

 DBGprintf(DBG_GCTRACE,"-End-> colorizeInstance");
}
 /*FOLD00*/


//-----------------------------------------------------------------------------



void instanceCollection() /*fold00*/
// - makes one collection of instances, after collection GREEN references
//   can be recycled
{
 int     i;
 InstRef *iR;

 // +------------+
 // | RUNLEVEL 0 |
 // +------------+

 DBGprintf(DBG_GCTRACE,"-Begin-> instanceCollection");

 logrHeapRefs->color = GC_GREEN;
 gcOrMask            = GC_NOMARK;



 // +------------+
 // | RUNLEVEL 1 |
 // +------------+

 DBGprintf(DBG_GCTRACE,"Entering RUNLEVEL 1   ...   GREEN/heapRefCount/finalizerflags");

 // Initialization:
 //  - mark each reference to instance as GREEN
 //  - set heapRefCount to 0
 //  - unset flags FINALIZER_CANDIDATE and FINALIZER_READY_TO_DIE used 
 //    in finalization
 ListItem *item= logrHeapRefs->instRefVecs->GetHead();
 int      size;



 while( item )
 {
  // go through one vector
  size = ((InstRefVecsItem*)item)->lng;

  // it is not important if reference is valid or not -> mark each (it's fast)

  for( i=0; i<size; i++ )
  {
   iR=&(((InstRefVecsItem*)item)->vector[i]);

   // GREEN
   iR->color          = GC_GREEN;
   // heap ref count
   iR->heapRefCount   = 0;
   // unset flags used in finalization
   iR->finalizerFlags &= ~FINALIZER_CANDIDATE;
   iR->finalizerFlags &= ~FINALIZER_READY_TO_DIE;
  }

  item=logrHeapRefs->instRefVecs->Next(item);
 }



 // +------------+
 // | RUNLEVEL 2 |
 // +------------+

 DBGprintf(DBG_GCTRACE,"Entering RUNLEVEL 2   ...   collection");

 // Switch on gcIsRunning
 gcIsRunning         = TRUE;

 gcOrMask            = GC_MARK;
 logrHeapRefs->color = GC_BLUE;



 // Trace recursively each root instance
 item= logrHeapRefs->instRefVecs->GetHead();

 while( item )
 {
  // one vector
  size = ((InstRefVecsItem*)item)->lng;

  for( i=0; i<size; i++ )
  {
   // check if is valid
   if( ((InstRefVecsItem*)item)->vector[i].state != REF_NOT_VALID )
   {
    // reference is valid -> test if it's root pointer
    if( ((InstRefVecsItem*)item)->vector[i].refCount > 0 )
    {
     DBGprintf(DBG_GCTRACE," * ROOT refCount %d  ... vector[%i]",((InstRefVecsItem*)item)->vector[i].refCount,i); // root pointer



     // if instance is RED -> already done -> do not dive
     if( ((InstRefVecsItem*)item)->vector[i].color == GC_RED )
     {
      DBGprintf(DBG_GCTRACE,"   --> ROOT already RED");
     }
     else
     {
      // trace recursively instance
      colorizeInstanceRecursively( &(((InstRefVecsItem*)item)->vector[i]) );
     }

    }
    else
    {
     DBGprintf(DBG_GCTRACE," - VALID nonRoot ... vector[%i]",i); // valid but not root
    }
   }
  }

  item=logrHeapRefs->instRefVecs->Next(item);
 }

 // Switch off gcIsRunning
 gcIsRunning = FALSE;



 // +------------+
 // | RUNLEVEL 3 |
 // +------------+

 DBGprintf(DBG_GCTRACE,"Entering RUNLEVEL 3   ...   touched rest");

 logrHeapRefs->color = GC_RED;
 gcOrMask            = GC_NOMARK;



 // Colorize touched rest: BLUE references
 item= logrHeapRefs->instRefVecs->GetHead();

 while( item )
 {
  // one vector
  size = ((InstRefVecsItem*)item)->lng;

  for( i=0; i<size; i++ )
  {
   // * DEBUG * DEBUG * DEBUG * DEBUG * DEBUG * DEBUG * DEBUG * DEBUG *
   if( ((InstRefVecsItem*)item)->vector[i].color == GC_ORANGE )
    WORD_OF_DEATH("ORANGE reference...");
   // * DEBUG * DEBUG * DEBUG * DEBUG * DEBUG * DEBUG * DEBUG * DEBUG *

   // check if is valid
   if( ((InstRefVecsItem*)item)->vector[i].state != REF_NOT_VALID )
   {
    // reference is valid -> test if it's touched reference
    if( ((InstRefVecsItem*)item)->vector[i].color == GC_BLUE )
    {
     // trace recursively instance
     colorizeInstanceRecursively( &(((InstRefVecsItem*)item)->vector[i]) );
    }
   }
  }

  item=logrHeapRefs->instRefVecs->Next(item);
 }



 // +------------------------------------------------------+
 // | Garbage collection done:                             |
 // |  RED   ... reachable references                      |
 // |  GREEN ... unreachable references (can be discarted) |
 // +------------------------------------------------------+

 DBGprintf(DBG_GCTRACE,"Result of colorization:");
 DBGprintf(DBG_GCTRACE,"+------------------+");
 DBGprintf(DBG_GCTRACE,"| DEAD REFERENCES: |");
 DBGprintf(DBG_GCTRACE,"+------------------+---------------------------------------------");
 DBGsection(DBG_GCTRACE,logrHeapRefs->iWalker(GC_GREEN));
 DBGprintf(DBG_GCTRACE,"+------------------+");
 DBGprintf(DBG_GCTRACE,"| LIVE REFERENCES: |");
 DBGprintf(DBG_GCTRACE,"+------------------+---------------------------------------------");
 DBGsection(DBG_GCTRACE,logrHeapRefs->iWalker(GC_RED));



 // +-------------+
 // | RUNLEVEL iv |
 // +-------------+

 DBGprintf(DBG_GCTRACE,"Entering RUNLEVEL IV   ...   finalization");

 logrHeapRefs->color = GC_RED;
 gcOrMask            = GC_MARK;



 // actualize heapRefCount for GREEN references (heapRefCount==0 now - it
 // was done during RUNLEVEL 1)
 item= logrHeapRefs->instRefVecs->GetHead();

 while( item )
 {
  // one vector
  size = ((InstRefVecsItem*)item)->lng;

  for( i=0; i<size; i++ )
  {
   // check if is valid
   if( ((InstRefVecsItem*)item)->vector[i].state != REF_NOT_VALID
       &&
       ((InstRefVecsItem*)item)->vector[i].color == GC_GREEN
     )
   {
    // make heapRefCount++ in each succesor, 
    // mark it only to depth 1 (non recursive trace)

    universalInstanceTracer(
                            &(((InstRefVecsItem*)item)->vector[i]),
                            GC_TRACER_HEAP_REF_COUNT
                           );

    // this reference is candidate for finalization -> prepare flag
    ((InstRefVecsItem*)item)->vector[i].finalizerFlags |= FINALIZER_CANDIDATE;
   }
  }

  item=logrHeapRefs->instRefVecs->Next(item);
 }



 #ifdef GC_ENABLE_GC
  // everything is prepared -> make finalization === summon finalizer thread
  pthread_t *finalizerThread;

  if( (finalizerThread=new pthread_t)==NULL )
   WORD_OF_DEATH("cann't allocate finalizer thread...")

  if( pthread_create( finalizerThread,
                      NULL,             // default attributes
                      finalizerBody,    // thread routine
                      NULL              // arguments
                     )
     )
    WORD_OF_DEATH("cann't run finalizer thread...")

  if( pthread_join( *finalizerThread, NULL ) )
   WORD_OF_DEATH("cann't join finalizer thread...")
 #endif

 #ifdef GC_SYNC_GC
  runSyncFinalize();
 #endif

 // finalization done: wake up *thread* waiting from runFinalization();
 logrHeapGC->finalizationFinished();



 // +-------------+
 // | RUNLEVEL 0 |
 // +-------------+

 logrHeapRefs->color = GC_GREEN;
 gcOrMask            = GC_NOMARK;

 DBGprintf(DBG_GCTRACE,"-End-> instanceCollection");

 // instance collection cycle done: wake up thread waiting from gc();
 logrHeapGC->collectionFinished();
}
 /*FOLD00*/


//- gCollectorBody ------------------------------------------------------------



// Colors during garbage collection:
//
//  new references:
//
//        RUNLEVEL       color
//       ----------------------
//           0           GREEN
//           1           GREEN
//           2           BLUE
//           3           RED
//           iv          RED
//
//  get/put action:
//
//        RUNLEVEL       color
//       ----------------------
//           0           .
//           1           .
//           2           or GC_MARK     ... GREEN -> BLUE
//           3           .
//           iv          or GC_MARK     ... GREEN -> BLUE



void *gcBody( void *attr ) /*fold00*/
{
 DBGprintf(DBG_GC,"<- GC ------------------------------------------------------------->");
 DBGprintf(DBG_GC," Starting GC daemon: pid %i, ppid %i, gCollector %p", getpid(),getppid(),attr);
 DBGprintf(DBG_GC,"<- GC ------------------------------------------------------------->");

 GarbageCollector *GC = (GarbageCollector*)attr; // instance of GC



 // GC loop
 while( !GC->termSignal )
 {
  DBGprintf(DBG_GC,"-Begin-> GC LOOP");

  //- Check suspend signal ----------------------------------------------------

  if( GC->suspendSignal )
  {
   DBGprintf(DBG_GC,"GCsuspend...");

   // at this time narcosis must be down
   DBGprintf(2000," GC deaemon pid %i going to be suspended...",getpid());
   LOCK_MUTEX(GC->narcosis);

   DBGprintf(2000," GC deaemon pid %i resumed...",getpid());
   UNLOCK_MUTEX(GC->narcosis);

   // tells that it's ready to be suspended in next cycle
   GC->suspendSignal=FALSE;
  }



  //- garbage work ------------------------------------------------------------



  instanceCollection();         // colorize reachable references
                                // and finalize everything what's possible


  //- garbage work ------------------------------------------------------------



  DBGprintf(DBG_GC,"-End-> GC LOOP");
 } // while( !termSignal )



 DBGprintf(DBG_GC,"<- GC ------------------------------------>");
 DBGprintf(DBG_GC,"   GC daemon pid %i dispelled...",getpid());
 DBGprintf(DBG_GC,"<- GC ------------------------------------>");

 pthread_exit(NULL);
}
 /*FOLD00*/


//- EOF -----------------------------------------------------------------------
