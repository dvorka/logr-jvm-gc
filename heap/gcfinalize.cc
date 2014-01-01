/*
 * gcfinalize.cc
 *
 * Author: Dvorka
 *
 */

#include "gcfinalize.h"

#include "exception.h"
#ifdef GC_FINALIZER_CALL
 #include "lni.h"
 #include "fictive.h"
#endif
#include "gclist.h"
#include "gcollector.h"
#include "gcreference.h"
#include "referencetypes.h"
#ifdef GC_RELEASE
 #include "utils.h"
#endif



//- Externs -------------------------------------------------------------------

extern ReferenceVectors *logrHeapRefs;

extern GarbageCollector *logrHeapGC;

extern int              bitField[];

//-----------------------------------------------------------------------------



/* void internalFinalize( InstRef *iR ) */ /*fold00*/
#define internalFinalize( IR )                                                \
/* - remove reference from internal JVM structures using finalizerFlags */    \
do                                                                            \
 {                                                                            \
                                                                              \
  if( (IR)->finalizerFlags & FINALIZER_STRING_INTERN )                        \
  {                                                                           \
   DBGprintf(DBG_GCTRACE,"Calling string intern finalizer...");               \
                                                                              \
   char *excInfo=NULL;                                                        \
                                                                              \
   removeInternStringRepresentation( &excInfo, (IR) );                        \
                                                                              \
   if( excInfo )                                                              \
    free(excInfo);                                                            \
                                                                              \
   (IR)->finalizerFlags &= ~FINALIZER_STRING_INTERN;                          \
  }                                                                           \
                                                                              \
 } while(0)
 /*FOLD00*/


//-----------------------------------------------------------------------------



/* void javaFinalize( InstRef iR, RtExceptionInfo *excInfo ) */ /*FOLD00*/
#define javaFinalize( IR, EXCINFO )                                           \
do                                                                            \
 {                                                                            \
                                                                              \
  /* +----------------------------------------+ */                            \
  /* | Java finalizer can be called only ONCE | */                            \
  /* +----------------------------------------+ */                            \
                                                                              \
  if( (IR)->finalizerFlags & FINALIZER_JAVA )                                 \
  {                                                                           \
   DBGprintf(DBG_GCTRACE," Invoking Java finalizer %p...",(IR));              \
                                                                              \
   /* call finalizer */                                                       \
   lniInvokeVirtual(&beginEnv,M_JAVA_LANG_OBJECT_FINALIZE,(IR));              \
                                                                              \
   /* exception is ignored */                                                 \
   deleteSynchException((EXCINFO));                                           \
                                                                              \
   /* finalizer done */                                                       \
   (IR)->finalizerFlags &= ~FINALIZER_JAVA;                                   \
  }                                                                           \
                                                                              \
} while(0)
 /*FOLD00*/


//-----------------------------------------------------------------------------



void universalInstanceTracer( InstRef *iR, int action, RtExceptionInfo *excInfo ) /*FOLD00*/
// - this function traces:
//    i) instance itself
//   (static data of coresponding class *NOT* traced)
{
 switch( action )
 {
  case GC_TRACER_FINALIZER_CANDIDATE:
        DBGprintf(DBG_GCTRACE,"-Begin-> unset FINALIZE_CANDIDATE flag \n Unset FINALIZER_CANDIDATE in:"); DBGsection(DBG_GCTRACE,(printInstRef(iR)));
       break;

  case GC_TRACER_HEAP_REF_COUNT:
        DBGprintf(DBG_GCTRACE,"-Begin-> heapRefCount trace \n Counting heapRefCount:"); DBGsection(DBG_GCTRACE,(printInstRef(iR)));
       break;
  
  case GC_TRACER_FINALIZE_RECURSIVELY:
        DBGprintf(DBG_GCTRACE,"-Begin-> recursive finalization \n Finalizing instance:"); DBGsection(DBG_GCTRACE,(printInstRef(iR)));
       break;

  default:
        WORD_OF_DEATH("universalInstanceTracer: unknown option")
 }



 int     i;
 int     bitmapWords;

 void    *iPtr;         // iPtr = iR->ptr
 dword   *instance;     // instance itself inside iPtr
 StatRef *sR;           // reference to RuntimeData from iR->ptr
 void    *sPtr;         // sPtr = sR->ptr
 dword   *bitmap;       // bitmap in sPtr
 InstRef *instanceField;// reference field from instance



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

  DBGprintf(DBG_GCTRACE,"Bottom finalizer reference              (ptr==NULL)");

  // case GC_TRACER_FINALIZE_RECURSIVELY:
  //  - trace not possible:             iPtr==NULL
  //  - finalization not possible       iPtr==NULL (-> RD not accessible)

  if( action == GC_TRACER_FINALIZER_CANDIDATE
      ||
      action == GC_TRACER_FINALIZE_RECURSIVELY
    )
  {
   // unset candidate flag
   iR->finalizerFlags &= ~FINALIZER_CANDIDATE;
   iR->finalizerFlags &= ~FINALIZER_JAVA;
   iR->finalizerFlags &= ~FINALIZER_STRING_INTERN;
  }

  unlockInstRef(iR);
  return;
 }



 // +----------------+
 // | trace instance |
 // +----------------+

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



 DBGprintf(DBG_GCTRACE,"+- Recursive universal action --------------------");
 DBGprintf(DBG_GCTRACE,"| InstRef  %p",iR);
 DBGprintf(DBG_GCTRACE,"| ->ptr    %p",iPtr);
 DBGprintf(DBG_GCTRACE,"| instance %p",instance);
 DBGprintf(DBG_GCTRACE,"| RDRef    %p",sR);
 DBGprintf(DBG_GCTRACE,"| RD       %p",sPtr);
 DBGprintf(DBG_GCTRACE,"| bmpOff   %d",((RuntimeDataHeader*)sPtr)->instanceDataBitmap);
 DBGprintf(DBG_GCTRACE,"| bmp      %p",bitmap);
 DBGprintf(DBG_GCTRACE,"| bmpWords %d",bitmapWords);
 DBGprintf(DBG_GCTRACE,"+---------------------");



 switch( action )
 {
  case GC_TRACER_FINALIZER_CANDIDATE:
        DBGprintf(DBG_GCTRACE," Tracing instance bitmap for FINALIZE_CANDIDATE unset ...");
       break;
  case GC_TRACER_HEAP_REF_COUNT:
        DBGprintf(DBG_GCTRACE," Tracing instance bitmap for heapRefCount...");
       break;
  case GC_TRACER_FINALIZE_RECURSIVELY:

        // first finalize itself then check color:
        //  i)  if color changed
        //          -> resurrection of instance occured
        //          -> unset own FINALIZER_CANDIDATE
        //          -> unset FINALIZER_CANDIDATE recursively
        //             === for each field dive deeper
        //  ii) else
        //          -> unset own FINALIZER_CANDIDATE
        //          -> set own FINALIZER_READY_TO_DIE
        //          -> finalize recursively
        //             === on each field call this function

        // self finalization
        // I know that:
        //  - reference is valid
        //  - FINALIZER_CANDIDATE is set
        //  - color is GREEN

        if( iR->finalizerFlags & FINALIZER_JAVA ) // not finalized yet
        {
         DBGprintf(DBG_GCTRACE," *SELF invoke Java finalizer*");

         #ifdef GC_FINALIZER_CALL
          javaFinalize( iR, excInfo );
         #endif
        }
        else
        { DBGprintf(DBG_GCTRACE," Reference %p already Java finalized...",iR); }

        // finalization done 
        iR->finalizerFlags &= ~FINALIZER_CANDIDATE;

        // if color NOT changed during self finalization then call finalizers
        // recursively for each GREEN reference in instance. If reference 
        // is BLUE -> becomes valid, then only unset FINALIZER_CANDIDATE but 
        // do not call finalizer (instance stays as usable as before 
        // finalizing process)
        if( iR->color != GC_GREEN )
        {
         // unset CANDIDATE flag recursively
         DBGprintf(DBG_GCTRACE," ->> unset");
          universalInstanceTracer(iR,GC_TRACER_FINALIZER_CANDIDATE);
         DBGprintf(DBG_GCTRACE," <<- unset");

         unlockStatRef(sR);
         unlockInstRef(iR);

         return;
        }
        // else color not changed -> finalize instance -> then die

        iR->finalizerFlags |= FINALIZER_READY_TO_DIE;



        DBGprintf(DBG_GCTRACE," Tracing instance bitmap recursive finalization...");
       break;
  default:
        WORD_OF_DEATH("universalInstanceTracer: unknown option")
 }



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
   // !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!!

   // load copy of field in instance (it can be locked using -1)

   instanceField = readRefField(((InstRef**)(&(instance[i]))));

   // !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!! !!!

   switch( action )
   {
    case GC_TRACER_FINALIZER_CANDIDATE:

        // field is reference -> if reference has FINALIZER_CANDIDATE flag set
        //                       then unset it and dive deeper, else up

        if( instanceField->finalizerFlags & FINALIZER_CANDIDATE )
        {
         instanceField->finalizerFlags &= ~FINALIZER_CANDIDATE;

         // unset CANDIDATE flag recursively
         DBGprintf(DBG_GCTRACE," ->> unset");
          universalInstanceTracer(instanceField,GC_TRACER_FINALIZER_CANDIDATE);
         DBGprintf(DBG_GCTRACE," <<- unset");

        }

       break;

    case GC_TRACER_HEAP_REF_COUNT:

        // field is reference -> mark it's immediate succesor
        DBGprintf(DBG_GCTRACE," r -> %p->heapRefCount++ ... %d -> %d",
                              instanceField,
                              instanceField->heapRefCount,
                              instanceField->heapRefCount+1
                 );

        instanceField->heapRefCount++;

       break;

    case GC_TRACER_FINALIZE_RECURSIVELY:

        // check reference field properties, if OK call recursivery

        if( instanceField->state          != REF_NOT_VALID
            &&
            instanceField->finalizerFlags & FINALIZER_CANDIDATE
          )
        {

         if( instanceField->color == GC_GREEN ) // finalize it recursively
         {
          DBGprintf(DBG_GCTRACE," ->>");
           universalInstanceTracer(instanceField,GC_TRACER_FINALIZE_RECURSIVELY);
          DBGprintf(DBG_GCTRACE," <<-");
         }
         else // it's BLUE reference -> unset FINALIZER_CANDIDATE
         {
          // unset CANDIDATE flag recursively
          DBGprintf(DBG_GCTRACE," ->> unset");
           universalInstanceTracer(instanceField,GC_TRACER_FINALIZER_CANDIDATE);
          DBGprintf(DBG_GCTRACE," <<- unset");
         }
        }

       break;

    default:
        WORD_OF_DEATH("universalInstanceTracer: unknown option")
   }
  }
  else // it is primitive type
   { DBGprintf(DBG_GCTRACE," ."); }

 } // for



 unlockStatRef(sR);
 unlockInstRef(iR);



 switch( action )
 {
  case GC_TRACER_FINALIZER_CANDIDATE:
        DBGprintf(DBG_GCTRACE," ... tracing instance bitmap for FINALIZE_CANDIDATE");
        DBGprintf(DBG_GCTRACE,"-End-> unset FINALIZE_CANDIDATE flag");
       break;

  case GC_TRACER_HEAP_REF_COUNT:
        DBGprintf(DBG_GCTRACE," ... tracing instance bitmap for heapRefCount");
        DBGprintf(DBG_GCTRACE,"-End-> heapRefCount trace");
       break;

  case GC_TRACER_FINALIZE_RECURSIVELY:
        DBGprintf(DBG_GCTRACE," ... tracing instance bitmap for heapRefCount");
        DBGprintf(DBG_GCTRACE,"-End-> recursive finalization");
       break;
  default:
        WORD_OF_DEATH("universalInstanceTracer: unknown option")
 }

}
 /*FOLD00*/


//-----------------------------------------------------------------------------



void finalize( RtExceptionInfo *excInfo ) /*FOLD00*/
// - attr unused
// - in RUNLEVEL 3
//    new ... RED
//    get ... X
// - in RUNLEVEL IV
//    new ... RED
//    get ... GREEN -> BLUE
{
 ListItem *item;
 int      size,
          i;
 InstRef  *iR;           // cached reference I am working on for speed up

 // +------------------------+
 // | already in RUNLEVEL IV |
 // +------------------------+



 // + - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
 // : make references with heapRefCount==0 (finalizer roots)  :
 // +  - - - - - - - - - - - - - - - - - - - - - - - - - - - - +

 DBGprintf(DBG_GCTRACE,"Finalizing ROOT heapRefCount references...");

 item= logrHeapRefs->instRefVecs->GetHead();

 while( item )
 {
  size = ((InstRefVecsItem*)item)->lng;

  for( i=0; i<size; i++ )
  {
   iR= &(((InstRefVecsItem*)item)->vector[i]); // speed up

   if( iR->state         != REF_NOT_VALID
       &&
       iR->heapRefCount  == 0
       &&
       iR->finalizerFlags & FINALIZER_CANDIDATE
     )
   {
    // it's root reference

    if( iR->color == GC_GREEN ) // finalize it recursively
    {
     DBGprintf(DBG_GCTRACE," ->> ROOT finalize");
      universalInstanceTracer(iR,GC_TRACER_FINALIZE_RECURSIVELY);
     DBGprintf(DBG_GCTRACE," <<- ROOT finalize");
    }
    else // it's BLUE reference -> unset FINALIZER_CANDIDATE
    {
     // unset CANDIDATE flag recursively
     DBGprintf(DBG_GCTRACE," ->> unset finalize");
      universalInstanceTracer(iR,GC_TRACER_FINALIZER_CANDIDATE);
     DBGprintf(DBG_GCTRACE," <<- unset finalize");
    }
   }
  }
  item=logrHeapRefs->instRefVecs->Next(item);
 }



 // + - - - - - - - - - - - - - - - - - - +
 // : make references with heapRefCount>0 :
 // + - - - - - - - - - - - - - - - - - - +

 DBGprintf(DBG_GCTRACE,"Finalizing CYCLIC heapRefCount references...");

 // here is done nonrecursive finalization (heapRefCount!=0 which were not
 // finalized are found and made)

 item= logrHeapRefs->instRefVecs->GetHead();

 while( item )
 {
  // one vector
  size = ((InstRefVecsItem*)item)->lng;

  // Test of flag FINALIZER_CANDIDATE is enought because in BLUE and
  // deep GREEN references was this flag unset during ROOT make
  for( i=0; i<size; i++ )
  {
   iR= &(((InstRefVecsItem*)item)->vector[i]); // speed up

   if( iR->state != REF_NOT_VALID
       &&
       iR->finalizerFlags & FINALIZER_CANDIDATE
     )
   {

    DBGprintf(DBG_GCTRACE,"Java finalizer...");

    #ifdef GC_FINALIZER_CALL
     javaFinalize( iR, excInfo );
    #else
     // mark instance as it was done
     iR->finalizerFlags &= ~FINALIZER_JAVA;
    #endif

    // unset candidate flag
    iR->finalizerFlags &= ~FINALIZER_CANDIDATE;
    // set die flag
    iR->finalizerFlags |= FINALIZER_READY_TO_DIE;
   }
  }
  item=logrHeapRefs->instRefVecs->Next(item);
 }



 // + - - - - - - - - - - - - - - +
 // : search for BLUE references  :
 // + - - - - - - - - - - - - - - +
 DBGprintf(DBG_GCTRACE,"Finalizer is now searching for BLUE references...");

 // if BLUE reference is found, collection becomes invalid
 // ->
 // anything will be discarted

 item= logrHeapRefs->instRefVecs->GetHead();

 while( item )
 {
  // one vector
  size = ((InstRefVecsItem*)item)->lng;

  for( i=0; i<size; i++ )
  {
   if( ((InstRefVecsItem*)item)->vector[i].state != REF_NOT_VALID
       &&
       ((InstRefVecsItem*)item)->vector[i].color == GC_BLUE
     )
    {
     DBGprintf(DBG_GCTRACE," Touched reference %p found -> anything will be discarted",&(((InstRefVecsItem*)item)->vector[i]));

     // reference was touched during finalizer execution
     //  ->
     // collection becomes invalid
     //  ->
     // return

     return;
    }
  }
  item=logrHeapRefs->instRefVecs->Next(item);
 }



 // +---------------------+
 // | InstRef DESTRUCTION |
 // +---------------------+

 DBGprintf(DBG_GCTRACE,"Finalizer does InstRef DESTRUCTION...");

  item= logrHeapRefs->instRefVecs->GetHead();

  while( item )
  {
   // one vector
   size = ((InstRefVecsItem*)item)->lng;

   for( i=0; i<size; i++ )
   {
    if( ((InstRefVecsItem*)item)->vector[i].state != REF_NOT_VALID )
    {
     if( ((InstRefVecsItem*)item)->vector[i].finalizerFlags & FINALIZER_READY_TO_DIE )
     {
      // Remove reference from internal JVM structures using finalizerFlags

     if(
        ((InstRefVecsItem*)item)->vector[i].refCount
        ||
        ((InstRefVecsItem*)item)->vector[i].lockCount
       )
     {
      fprintf(stderr,"\n\n\n Buggy Frydlatko reference:");
      printInstRef((&(((InstRefVecsItem*)item)->vector[i])));
      WORD_OF_DEATH("travelling to Italy...")
     }



      DBGprintf(DBG_GCTRACE,"Intern finalizer of:");
      DBGsection(DBG_GCTRACE,(printInstRef(&(((InstRefVecsItem*)item)->vector[i]))));

      #ifdef GC_INTERN_RELEASE
       internalFinalize( &(((InstRefVecsItem*)item)->vector[i]) );
      #else
       // mark instance as it was done
       (((InstRefVecsItem*)item)->vector[i]).finalizerFlags &= ~FINALIZER_STRING_INTERN;
      #endif

      #ifndef GC_FAKE_COLLECTION
       // update GC stat
       logrHeapGC->huntedInstances++;
       logrHeapGC->huntedBytes+= ((InstRefVecsItem*)item)->vector[i].size;

       deleteInstRef(&(((InstRefVecsItem*)item)->vector[i]));
      #endif

      DBGprintf(DBG_GCTRACE,"\n GC: object ptr %p, size %iB destroyed...",&(((InstRefVecsItem*)item)->vector[i]),((InstRefVecsItem*)item)->vector[i].size);
     }
    }
   }

   item=logrHeapRefs->instRefVecs->Next(item);
  }



  // increase number of finished collections
  logrHeapGC->iterations++;

  // verbose GC message
  if( logrHeapGC->verbose )
   logrHeapGC->printInfo();

}
 /*FOLD00*/


//-----------------------------------------------------------------------------



void runSyncFinalize() /*fold00*/
{
 DBGprintf(DBG_GC,"<----------------------------------------------------------------------->");
 DBGprintf(DBG_GC,"   Starting *sync* finalizer");
 DBGprintf(DBG_GC,"<----------------------------------------------------------------------->");



 RtExceptionInfo *excInfo;

 #ifdef GC_FINALIZER_CALL
  excInfo=(RtExceptionInfo *)GETSPECIFIC(keyRtExceptionInfo);
 #endif

 // +-----------------------------------------------+
 // | Java thread initialized -> finalize instances |
 // +-----------------------------------------------+
 finalize(excInfo);



 DBGprintf(DBG_GC,"<----------------------------------------------------------------------->");
 DBGprintf(DBG_GC,"   *sync* finalizer finished...");
 DBGprintf(DBG_GC,"<----------------------------------------------------------------------->");
}
 /*FOLD00*/


//-----------------------------------------------------------------------------



void *finalizerBody( void *attr ) /*fold00*/
{
 DBGprintf(DBG_GC,"<----------------------------------------------------------------------->");
 DBGprintf(DBG_GC,"   Starting finalizer thread: pid %i, ppid %i", getpid(),getppid());
 DBGprintf(DBG_GC,"<----------------------------------------------------------------------->");

 RtExceptionInfo excInfo;

 excInfo.flags = NO_EXCEPTION;
 excInfo.asynchCount = 0;
 excInfo.synchExc = nullInstRef;
 COND_INIT(&excInfo.suspendCondVar);
 COND_INIT(&excInfo.sleepCondVar);
 excInfo.waitOn = nullInstRef;
 excInfo.interrupted = INTERRUPT_NO;
 excInfo.threadId = pthread_self();
 excInfo.prev = &excInfo;
 excInfo.next = &excInfo;

 #if defined(GC_FINALIZER_CALL) || defined(GC_INTERN_RELEASE)
  SETSPECIFIC(keyRtExceptionInfo, &excInfo);
 #endif


 // +-----------------------------------------------+
 // | Java thread initialized -> finalize instances |
 // +-----------------------------------------------+
 finalize(&excInfo);



 COND_DESTROY(&excInfo.suspendCondVar);
 COND_DESTROY(&excInfo.sleepCondVar);

 #if defined(GC_FINALIZER_CALL) || defined(GC_INTERN_RELEASE)
  SETSPECIFIC(keyRtExceptionInfo, NULL);
 #endif



 DBGprintf(DBG_GC,"<----------------------------------------->");
 DBGprintf(DBG_GC,"   Finalizer thread pid %i finished...",getpid());
 DBGprintf(DBG_GC,"<----------------------------------------->");

 pthread_exit(NULL);
}
 /*FOLD00*/


//- EOF -----------------------------------------------------------------------
