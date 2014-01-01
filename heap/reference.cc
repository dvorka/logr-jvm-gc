/*
 * reference.cc
 *
 * Author: Dvorka
 *
 * ToDo: 
 */

#include "reference.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/cdefs.h>

#include "asmdefines.h"
#include "exception.h"
#include "gcollector.h"
#include "gcheap.h"
#include "gclist.h"   
#include "gcreference.h"
#include "javaparse.h"



//-----------------------------------------------------------------------------
//                                GLOBALS
//-----------------------------------------------------------------------------

// heap for Logr/Java classes
GcHeap            *logrHeap;

ReferenceVectors  *logrHeapRefs;

GarbageCollector  *logrHeapGC;

int               gcIsRunning= FALSE;     // AddInstRef() & Get/PutRefField()
int               gcOrMask   = GC_NOMARK;



// JVM inner heap
#ifdef GC_ENABLE_SYSTEM_HEAP
 GcHeap           *systemHeap;
 bool             systemHeapEnabled=FALSE;
#endif

//-----------------------------------------------------------------------------
//                                 NULL
//-----------------------------------------------------------------------------
/*
 * nullRefs definition
 */

StatRef *nullStatRef;
InstRef *nullInstRef;

//-----------------------------------------------------------------------------
//                             DEBUG RECORDING
//-----------------------------------------------------------------------------
/*
 * reference debug is in standalone file
 */

#include "gcdebug.cc"

//-----------------------------------------------------------------------------
//                                INIT&CLOSE
//-----------------------------------------------------------------------------

void checkAndFixCmdArgs() /*FOLD00*/
{

 // start heapsize

 if( cmdargs.startmem<GCHEAP_MS_MIN )
 {
  fprintf(stderr,"\n Error:"
                 "\n  specified init heapsize too small, was stretched: %d -> "GCHEAP_MS_MINS,
                 cmdargs.startmem );
  cmdargs.startmem = GCHEAP_MS_MIN;
 }

 if( cmdargs.startmem>GCHEAP_MS_MAX )
 {
  fprintf(stderr,"\n Error:"
                 "\n  specified init heapsize too big, was shrinked: %d -> "GCHEAP_MS_MAXS,
                 cmdargs.startmem );
  cmdargs.startmem = GCHEAP_MS_MAX;
 }



 // maximal heapsize

 if( cmdargs.maxheap<GCHEAP_MX_MIN )
 {
  fprintf(stderr,"\n Error:"
                 "\n  specified maximum heapsize too small, was stretched: %d -> "GCHEAP_MX_MINS,
                 cmdargs.maxheap );
  cmdargs.maxheap = GCHEAP_MS_MIN;
 }

 if( cmdargs.maxheap>GCHEAP_MX_MAX )
 {
  fprintf(stderr,"\n Error:"
                 "\n  specified maximum heapsize too big, was shrinked: %d -> "GCHEAP_MX_MAXS,
                 cmdargs.maxheap );
  cmdargs.maxheap = GCHEAP_MX_MAX;
 }

}

//-----------------------------------------------------------------------------

int referenceInitialize(void) /*FOLD00*/
// - if OK returns 0, else errorcode
{
 DBGprintf(DBG_REF,"Initializing *RELEASE* reference...");

 // initialize JVM heap
 #ifdef GC_ENABLE_SYSTEM_HEAP
 // TS
 if( (systemHeap = new GcHeap(1111111,300000000))==NULL )
   WORD_OF_DEATH("unable to create system heap...")
  else
   systemHeapEnabled=TRUE;

  DBGprintf(DBG_REF,"+------------------------------------------+");
  DBGprintf(DBG_REF,"| Reference -INIT-: SYSTEM heap enabled... |");
  DBGprintf(DBG_REF,"+------------------------------------------+");
 #endif



 // init debug recording
 DBGsection(DBG_REF,(recordList = new List));



 // create logr heap and references
 #ifdef GC_RELEASE

  checkAndFixCmdArgs();

  if( (logrHeap = new GcHeap(cmdargs.startmem,cmdargs.maxheap))==NULL )
   WORD_OF_DEATH("unable to create logrHeap...");

 #else

  if( (logrHeap = new GcHeap(500000,16000000))==NULL )
   WORD_OF_DEATH("unable to create logrHeap...");

 #endif



 if( (logrHeapRefs=new ReferenceVectors())==NULL )
  return JAVA__LANG__OUT_OF_MEMORY_ERROR;

 // Initialize nullStatRef and nullInstRef
 nullInstRef=newNamedInstRef(0,"nullInstRef");
  nullInstRef->finalizerFlags|=FINALIZER_VALID; // clear validation flag
 nullStatRef=newNamedStatRef(0,"nullStatRef");
 if( !nullStatRef || !nullInstRef )
   WORD_OF_DEATH("unable to create nullInstref/nullStatRef...")
 else
 {
  nullInstRef->finalizerFlags &= ~FINALIZER_JAVA;                                   \
 }



 // create garbage collector service structure
 if( (logrHeapGC = new GarbageCollector())==NULL )
  WORD_OF_DEATH("unable to create garbage collector...")

 // GC daemon is summonized in time when internal JVM structures
 // are initialized using gcRunAtJvmStartup()



 DBGprintf(DBG_REF,"*RELEASE* reference initialized...");
 return 0;
}

//-----------------------------------------------------------------------------

int referenceCleanup(void) /*FOLD00*/
// - if OK returns 0, else errorcode
{
 DBGprintf(DBG_REF,"DeInitializing *RELEASE* reference...");



 // GC deinitialization already done using gcShutdown()



 // print GC info
 DBGprintf(DBG_REF,"+- Garbage collector statistic: -------------------------------------------");
  DBGsection(DBG_REF,(logrHeapGC->printInfo()));
 DBGprintf(DBG_REF,"+--------------------------------------------------------------------------");
 // delete garbage collector service structure
 delete logrHeapGC;



 // delete debug recording
 DBGsection(DBG_REF,(delete (recordList)));



 // delete logr heap
 delete logrHeapRefs;
 delete logrHeap;



 #ifdef GC_ENABLE_SYSTEM_HEAP
  systemHeap->destroy();        // see gcHeap.cc for motivation
  systemHeapEnabled=FALSE;
  delete systemHeap;
 #endif

 DBGprintf(DBG_REF,"*RELEASE* reference deinitialized...");
 return 0;
}
 /*FOLD00*/
//-----------------------------------------------------------------------------
//                                   HEAP
//-----------------------------------------------------------------------------

void *logrAlloc( size_t s ) /*FOLD00*/
{
 #ifdef GC_ENABLE_SYSTEM_HEAP
  if(systemHeapEnabled)
   return systemHeap->alloc((int)s); // Klinger cast
  else
 #endif
   return malloc(s);
}

//-----------------------------------------------------------------------------

void logrFree( void *p ) /*fold00*/
{
 #ifdef GC_ENABLE_SYSTEM_HEAP
  if(systemHeapEnabled)
   systemHeap->free(p);
  else
 #endif
   free(p);
}

//-----------------------------------------------------------------------------

void referenceHeapDump(int heapName) /*fold00*/
// dump heap content into EA*.dump files
{
 switch( heapName )
 {
  case GC_LOGR_HEAP:
        logrHeap->dump(GCHEAP_DUMP_2_FILE);
        break;
  #ifdef GC_ENABLE_SYSTEM_HEAP
  case GC_SYSTEM_HEAP:
        if(systemHeapEnabled)
         systemHeap->dump(GCHEAP_DUMP_2_FILE);
        break;
  #endif
 }

}

//-----------------------------------------------------------------------------

void referenceHeapInfo(int heapName) /*fold00*/
// print info about heap
{
 switch( heapName )
 {
  case GC_LOGR_HEAP:
        logrHeap->info();
       break;
  #ifdef GC_ENABLE_SYSTEM_HEAP
  case GC_SYSTEM_HEAP:
        if(systemHeapEnabled)
         systemHeap->info();
        break;
  #endif
 }
}

//-----------------------------------------------------------------------------

void referenceHeapWalker(int heapName) /*fold00*/
// Logr heap walker
{
 switch( heapName )
 {
  case GC_LOGR_HEAP:
        logrHeap->walker();
       break;
  #ifdef GC_ENABLE_SYSTEM_HEAP
  case GC_SYSTEM_HEAP:
        if(systemHeapEnabled)
         systemHeap->walker();
        break;
  #endif
 }
}

//-----------------------------------------------------------------------------

int referenceHeapCheck(int heapName) /*fold00*/
// Logr heap walker
{
 switch( heapName )
 {
  case GC_LOGR_HEAP:
        return logrHeap->check();
  #ifdef GC_ENABLE_SYSTEM_HEAP
  case GC_SYSTEM_HEAP:
        if(systemHeapEnabled)
         return systemHeap->check();
        else
         return TRUE;
  #endif
 }
 return TRUE;
}

//-----------------------------------------------------------------------------

int referenceHeapCoreLeft(int heapName) /*fold00*/
// print how many bytes can be allocated
{
 switch( heapName )
 {
  case GC_LOGR_HEAP:
         return logrHeap->coreLeft();
  #ifdef GC_ENABLE_SYSTEM_HEAP
  case GC_SYSTEM_HEAP:
        if(systemHeapEnabled)
         return systemHeap->coreLeft();
        else
         return 0;
  #endif
 }

 return 0;
}

//-----------------------------------------------------------------------------

int referenceHeapTotalMem(int heapName) /*fold00*/
// print how many bytes can be allocated
{
 switch( heapName )
 {
  case GC_LOGR_HEAP:
         return logrHeap->totalMem();
  #ifdef GC_ENABLE_SYSTEM_HEAP
  case GC_SYSTEM_HEAP:
        if(systemHeapEnabled)
         return systemHeap->totalMem();
        else
         return 0;
  #endif
 }

 return 0;
}

//-----------------------------------------------------------------------------

int referenceHeapCheckChunk(int heapName, void *p) /*fold00*/
// chunk check TRUE if OK
{
 switch( heapName )
 {
  case GC_LOGR_HEAP:
         return logrHeap->checkChunk(p);
  #ifdef GC_ENABLE_SYSTEM_HEAP
  case GC_SYSTEM_HEAP:
        if(systemHeapEnabled)
         return systemHeap->checkChunk(p);
        else
         return 0;
  #endif
 }
 return TRUE;
}

//-----------------------------------------------------------------------------

void referenceHeapDumpChunk(int heapName, void *p) /*fold00*/
// dump into CHUNK.dump
{
 switch( heapName )
 {
  case GC_LOGR_HEAP:
         logrHeap->dumpChunk(p);
  #ifdef GC_ENABLE_SYSTEM_HEAP
  case GC_SYSTEM_HEAP:
        if(systemHeapEnabled)
         systemHeap->dumpChunk(p);
  #endif
 }
}

//-----------------------------------------------------------------------------

int referenceHeapFillFree(int heapName,byte pattern) /*fold00*/
{
 switch( heapName )
 {
  case GC_LOGR_HEAP:
         return logrHeap->fillFree(pattern);
  #ifdef GC_ENABLE_SYSTEM_HEAP
  case GC_SYSTEM_HEAP:
        if(systemHeapEnabled)
         return systemHeap->fillFree(pattern);
  #endif
 }

 return FALSE;
}

//-----------------------------------------------------------------------------

int referenceHeapCheckFillFree(int heapName,byte pattern) /*fold00*/
{
 switch( heapName )
 {
  case GC_LOGR_HEAP:
         return logrHeap->checkFillFree(pattern);
  #ifdef GC_ENABLE_SYSTEM_HEAP
  case GC_SYSTEM_HEAP:
        if(systemHeapEnabled)
         return systemHeap->checkFillFree(pattern);
  #endif
 }

 return FALSE;
}

//-----------------------------------------------------------------------------

int referenceHeapCheckFillFreeChunk(int heapName,void *p,byte pattern) /*fold00*/
{
 switch( heapName )
 {
  case GC_LOGR_HEAP:
         return logrHeap->checkFillFreeChunk(p,pattern);
  #ifdef GC_ENABLE_SYSTEM_HEAP
  case GC_SYSTEM_HEAP:
        if(systemHeapEnabled)
         return systemHeap->checkFillFreeChunk(p,pattern);
  #endif
 }

 return FALSE;
}
 /*FOLD00*/
//-----------------------------------------------------------------------------
//                              STAT REF
//-----------------------------------------------------------------------------

/* void NEW_NAMED_STAT_REF( char *NAME ) */ /*fold00*/
#ifdef PEDANT_REF_DEBUG
#define NEW_NAMED_STAT_REF(NAME)                                                 \
    StatRef *sR;                                                                 \
    \
    /* get static reference */                                                   \
    if( (sR=logrHeapRefs->getStatRef())==NULL )                                  \
    return NULL;                                                                 \
    \
    if( size==0 )                                                                \
    {                                                                            \
    sR->ptr=NULL;                                                                \
    }                                                                            \
    else                                                                         \
    {                                                                            \
    if (!(sR->ptr=malloc((size_t) size)))   {                                    \
        logrHeapRefs->putStatRef(sR);                                            \
        return NULL;                                                             \
    }                                                                            \
    }                                                                            \
    \
    sR->size     = size;                                                         \
    sR->refCount = 1;                                                            \
    sR->lockCount= 1;                                                            \
    sR->type     = STATREF_VOID;                                                 \
    sR->fieldLock= 0;                                                            \
    if( NAME )                                                                   \
    sR->name = strdup(NAME);                                                     \
    else                                                                         \
    sR->name = NULL;                                                             \
    \
    sR->finalizerFlags= FINALIZER_VOID;                                          \
    \
    sR->state = REF_VALID; /* from now GC can work with this reference */
#else
#define NEW_NAMED_STAT_REF(NAME)                                                 \
    StatRef *sR;                                                                 \
    \
    /* get static reference */                                                   \
    if( (sR=logrHeapRefs->getStatRef())==NULL )                                  \
    return NULL;                                                                 \
    \
    if( size==0 )                                                                \
    {                                                                            \
    sR->ptr=NULL;                                                                \
    }                                                                            \
    else                                                                         \
    {                                                                            \
    if( (sR->ptr=logrHeap->alloc(size))==NULL )                                  \
    {                                                                            \
    logrHeapRefs->putStatRef(sR);                                                \
    return NULL;                                                                 \
    }                                                                            \
    }                                                                            \
    \
    sR->size     = size;                                                         \
    sR->refCount = 1;                                                            \
    sR->lockCount= 1;                                                            \
    sR->type     = STATREF_VOID;                                                 \
    sR->fieldLock= 0;                                                            \
    if( NAME )                                                                   \
    sR->name = strdup(NAME);                                                     \
    else                                                                         \
    sR->name = NULL;                                                             \
    \
    sR->finalizerFlags= FINALIZER_VOID;                                          \
    \
    sR->state = REF_VALID; /* from now GC can work with this reference */
#endif
//-----------------------------------------------------------------------------

StatRef *coreNewStatRef(int size) /*fold00*/
{
 char *name=NULL;

 NEW_NAMED_STAT_REF( name )



 #ifdef GC_CHECK_HEAP_INTEGRITY
  logrHeap->check();
 #endif

 #ifndef GC_PURE_MACRO_REFERENCE
  DBGsection(DBG_REF,(referenceDebugManage(S_WAS_BORN,sR,NULL,0)));
 #endif

 return sR;
}

//-----------------------------------------------------------------------------

StatRef *coreNewNamedStatRef(int size,char *name) /*fold00*/
{
 NEW_NAMED_STAT_REF( name )



 #ifdef GC_CHECK_HEAP_INTEGRITY
  logrHeap->check();
 #endif

 #ifndef GC_PURE_MACRO_REFERENCE
  DBGsection(DBG_REF,(referenceDebugManage(S_WAS_BORN,sR,NULL,0)));
 #endif

 return sR;
}

//-----------------------------------------------------------------------------

void coreSetStatRefName( StatRef *sR, char *name ) /*fold00*/
{
 if(sR->name!=NULL)
  free(sR->name);

 if(name==NULL)
  sR->name=NULL;
 else
  sR->name=strdup(name);
}

//-----------------------------------------------------------------------------

char *coreGetStatRefName( StatRef *sR ) /*fold00*/
{
 return sR->name;
}

//-----------------------------------------------------------------------------

void coreDeleteStatRef( StatRef *sR ) /*fold00*/
{
 #ifndef GC_PURE_MACRO_REFERENCE
  DBGsection(DBG_REF,(referenceDebugManage(S_DELETE,sR,NULL,0)));
 #endif

 if(sR->ptr)
#ifdef PEDANT_REF_DEBUG
     free(sR->ptr);
#else
     logrHeap->free(sR->ptr);      // delete chunk
#endif

 if(sR->name)
  free(sR->name);

 logrHeapRefs->putStatRef(sR);  // return reference to system

 sR->state = REF_NOT_VALID;     // from now GC can work with this reference

 #ifdef GC_CHECK_HEAP_INTEGRITY
  logrHeap->check();
 #endif
}

//-----------------------------------------------------------------------------

void coreSetStatRefType(StatRef *sR, int type) /*fold00*/
// - sets type of StatRef -> see reference.h
{
 #ifndef GC_PURE_MACRO_REFERENCE
  DBGsection(DBG_REF,(referenceDebugManageCore(S_SET_TYPE,sR,type,NULL,0)));
 #endif

 sR->type=type;
}

//-----------------------------------------------------------------------------

void coreStretchStatRef( StatRef *sR, int size ) /*fold00*/
{
 #ifndef GC_PURE_MACRO_REFERENCE
  DBGsection(DBG_REF,(referenceDebugManageCore(S_REALLOC,sR,size,NULL,0)));
 #endif

 if( sR->size > size )
  WORD_OF_DEATH("attempt to make shrink using stretch.")
 else // OK make stretch
 {
  gcLockStatRef( sR );

#ifdef PEDANT_REF_DEBUG
  void *p = realloc (sR->ptr,size);
#else
  void *p = logrHeap->stretch(sR->ptr,size);
#endif

   if( p!=NULL )
   {
    sR->size=size;
    sR->ptr=p;
   }

  gcUnlockStatRef( sR );
 }

 #ifdef GC_CHECK_HEAP_INTEGRITY
  logrHeap->check();
 #endif
}

//-----------------------------------------------------------------------------

void coreShrinkStatRef( StatRef *sR, int size ) /*fold00*/
// - if size==0 -> reference is deleted
// - in case of performance chunk in heap is not shrinked
{
 #ifndef GC_PURE_MACRO_REFERENCE
  DBGsection(DBG_REF,(referenceDebugManageCore(S_REALLOC,sR,size,NULL,0)));
 #endif

 if( !size )                   // if size is shrinked to 0 -> delete reference
 {
#ifdef PEDANT_REF_DEBUG
     free(sR->ptr);
#else
     logrHeap->free(sR->ptr);     // delete chunk inside heap
#endif

  logrHeapRefs->putStatRef(sR);// return reference to system

  sR->state = REF_NOT_VALID;   // from now GC can work with this reference
 }
 else // !size
 {
  if( sR->size < size )
   WORD_OF_DEATH("attempt to make stretch using shrink.")
  else // OK make shrink
  {
   gcLockStatRef( sR );

#ifdef PEDANT_REF_DEBUG
   realloc(sR->ptr,size);
#else
   logrHeap->shrink(sR->ptr,size); // delete chunk inside heap
#endif

    sR->size=size;

   gcUnlockStatRef( sR );
  }
 }

 #ifdef GC_CHECK_HEAP_INTEGRITY
  logrHeap->check();
 #endif
}

//-----------------------------------------------------------------------------

void coreReallocStatRef( StatRef *sR, int size ) /*fold00*/
{
 #ifndef GC_PURE_MACRO_REFERENCE
  DBGsection(DBG_REF,(referenceDebugManageCore(S_REALLOC,sR,size,NULL,0)));
 #endif

 if(size==0)
 {
  coreDeleteStatRef(sR);
  return;
 }

 if(sR==NULL)
 {
  sR=coreNewStatRef(size);
  return;
 }

 if( sR->size < size ) // stretch
  coreStretchStatRef(sR,size);
 else                  // shrink
  coreShrinkStatRef(sR,size);

 #ifdef GC_CHECK_HEAP_INTEGRITY
  logrHeap->check();
 #endif
}

//-----------------------------------------------------------------------------

void corePrintStatRef( StatRef *sR ) /*fold00*/
{
 fprintf(stderr,"\n StatRef %p:"
                "\n  ptr       %p"
                "\n  size      %i"
                "\n  refCount  %i"
                "\n  lockCount %i"
                "\n  name      %s"
                "\n  type      %i"
                "\n  color     %i ... red 5, orange 6, green 3, blue 7"
                "\n  flags     %i"
                "\n  state     %i"
                "\n"
                "\n"
                ,
                sR,
                sR->ptr,
                sR->size,
                sR->refCount,
                sR->lockCount,
                sR->name,
                sR->type,
                sR->color,
                sR->finalizerFlags,
                sR->state
          );
}


 /*FOLD00*/
//-----------------------------------------------------------------------------
//                               INSTREF
//-----------------------------------------------------------------------------

/* void NEW_NAMED_INST_REF( char *NAME ) */ /*fold00*/
#ifdef PEDANT_REF_DEBUG
#define NEW_NAMED_INST_REF(NAME)                                                 \
    InstRef *iR;                                                                 \
    \
    /* get instance reference, contains synchronization tools check */           \
    if( (iR=logrHeapRefs->getInstRef())==NULL )                                  \
    return NULL;                                                                 \
    \
    if( size==0 )                                                                \
    {                                                                            \
    iR->ptr=NULL;                                                                \
    }                                                                            \
    else                                                                         \
    {                                                                            \
    if (!(iR->ptr=malloc((size_t) size)))   {                                    \
        logrHeapRefs->putInstRef(iR);                                            \
        return NULL;                                                             \
    }\
    }                                                                            \
    \
    iR->size         = size;                                                     \
    iR->refCount     = 1+1;                                                      \
    iR->lockCount    = 1;                                                        \
    iR->monitorOwner = 0;                                                        \
    iR->monitorCount = 0;                                                        \
    iR->fieldLock    = 0;                                                        \
    if( NAME )                                                                   \
    iR->name = strdup(NAME);                                                     \
    else                                                                         \
    iR->name = NULL;                                                             \
    \
    iR->finalizerFlags= FINALIZER_JAVA;                                          \
    \
    iR->state         = REF_VALID; /* from now GC can work with this reference */
#else
#define NEW_NAMED_INST_REF(NAME)                                                 \
    InstRef *iR;                                                                 \
    \
    /* get instance reference, contains synchronization tools check */           \
    if( (iR=logrHeapRefs->getInstRef())==NULL )                                  \
    return NULL;                                                                 \
    \
    if( size==0 )                                                                \
    {                                                                            \
    iR->ptr=NULL;                                                                \
    }                                                                            \
    else                                                                         \
    {                                                                            \
    if( (iR->ptr=logrHeap->alloc(size))==NULL )                                  \
    {                                                                            \
    logrHeapRefs->putInstRef(iR);                                                \
    return NULL;                                                                 \
    }                                                                            \
    }                                                                            \
    \
    iR->size         = size;                                                     \
    iR->refCount     = 1+1;                                                      \
    iR->lockCount    = 1;                                                        \
    iR->monitorOwner = 0;                                                        \
    iR->monitorCount = 0;                                                        \
    iR->fieldLock    = 0;                                                        \
    if( NAME )                                                                   \
    iR->name = strdup(NAME);                                                     \
    else                                                                         \
    iR->name = NULL;                                                             \
    \
    iR->finalizerFlags= FINALIZER_JAVA;                                          \
    \
    iR->state         = REF_VALID; /* from now GC can work with this reference */
#endif
//-----------------------------------------------------------------------------

InstRef *coreNewInstRef(int size) /*fold00*/
{
 char *name=NULL;

 NEW_NAMED_INST_REF( name )



 #ifdef GC_CHECK_HEAP_INTEGRITY
  logrHeap->check();
 #endif

 #ifndef GC_PURE_MACRO_REFERENCE
  DBGsection(DBG_REF,(referenceDebugManage(I_WAS_BORN,iR,NULL,0)));
 #endif

 return iR;
}

//-----------------------------------------------------------------------------

InstRef *coreNewNamedInstRef(int size,char *name) /*fold00*/
{
 NEW_NAMED_INST_REF( name )



 #ifdef GC_CHECK_HEAP_INTEGRITY
  logrHeap->check();
 #endif

 #ifndef GC_PURE_MACRO_REFERENCE
  DBGsection(DBG_REF,(referenceDebugManage(I_WAS_BORN,iR,NULL,0)));
 #endif

 return iR;
}

//-----------------------------------------------------------------------------

void coreSetInstRefName( StatRef *iR, char *name ) /*fold00*/
{
 if(iR->name!=NULL)
  free(iR->name);

 if(name==NULL)
  iR->name=NULL;
 else
  iR->name=strdup(name);
}

//-----------------------------------------------------------------------------

char *coreGetInstRefName( InstRef *iR ) /*fold00*/
{
 return iR->name;
}

//-----------------------------------------------------------------------------

void coreDeleteInstRef( InstRef *iR ) /*fold00*/
{
 #ifndef GC_PURE_MACRO_REFERENCE
  DBGsection(DBG_REF,(referenceDebugManage(I_DELETE,iR,NULL,0)));
 #endif

 if(iR->ptr)
#ifdef PEDANT_REF_DEBUG
     free(iR->ptr);
#else
 logrHeap->free(iR->ptr);      // delete chunk
#endif

 if(iR->name)
  free(iR->name);

 logrHeapRefs->putInstRef(iR);  // return reference to system

 iR->state    =REF_NOT_VALID;   // from now GC can work with this reference

 #ifdef GC_CHECK_HEAP_INTEGRITY
  logrHeap->check();
 #endif
}

//-----------------------------------------------------------------------------

void coreStretchInstRef( InstRef *iR, int size ) /*fold00*/
{
 #ifndef GC_PURE_MACRO_REFERENCE
  DBGsection(DBG_REF,(referenceDebugManageCore(I_REALLOC,iR,size,NULL,0)));
 #endif

 if( iR->size > size )
  WORD_OF_DEATH("attempt to make shrink using stretch.")
 else // OK make stretch
 {
  gcLockInstRef( iR );

#ifdef PEDANT_REF_DEBUG
  void *p = realloc(iR->ptr,size);
#else
  void *p = logrHeap->stretch(iR->ptr,size);
#endif

  if( p!=NULL )
  {
   iR->size=size;
   iR->ptr=p;
  }

  gcUnlockInstRef( iR );
 }

 #ifdef GC_CHECK_HEAP_INTEGRITY
  logrHeap->check();
 #endif
}

//-----------------------------------------------------------------------------

void coreShrinkInstRef( InstRef *iR, int size ) /*fold00*/
// - if size==0 -> reference is deleted
// - in case of performance chunk in heap is not shrinked
{
 #ifndef GC_PURE_MACRO_REFERENCE
  DBGsection(DBG_REF,(referenceDebugManageCore(I_REALLOC,iR,size,NULL,0)));
 #endif

 if( !size )                   // if size is shrinked to 0 -> delete reference
 {
#ifdef PEDANT_REF_DEBUG
     free(iR->ptr);     
#else
     logrHeap->free(iR->ptr);     // delete chunk inside heap
#endif

  logrHeapRefs->putInstRef(iR);// return reference to system

  iR->state = REF_NOT_VALID;   // from now GC can work with this reference
 }
 else // !size
 {
  if( iR->size < size )
   WORD_OF_DEATH("attempt to make stretch using shrink.")
  else // OK make shrink
  {
   gcLockInstRef( iR );

#ifdef PEDANT_REF_DEBUG
   iR->ptr=realloc(iR->ptr,size); 
#else
   logrHeap->shrink(iR->ptr,size); // delete chunk inside heap
#endif

    iR->size=size;

   gcUnlockInstRef( iR );
  }
 }

 #ifdef GC_CHECK_HEAP_INTEGRITY
  logrHeap->check();
 #endif
}

//-----------------------------------------------------------------------------

void coreReallocInstRef( InstRef *iR, int size ) /*fold00*/
{
 #ifndef GC_PURE_MACRO_REFERENCE
  DBGsection(DBG_REF,(referenceDebugManageCore(I_REALLOC,iR,size,NULL,0)));
 #endif

 if(size==0)
 {
  coreDeleteInstRef(iR);
  return;
 }

 if(iR==NULL)
 {
  iR=coreNewInstRef(size);
  return;
 }

 if( iR->size < size ) // stretch
  coreStretchInstRef(iR,size);
 else                  // shrink
  coreShrinkInstRef(iR,size);

 #ifdef GC_CHECK_HEAP_INTEGRITY
  logrHeap->check();
 #endif
}

//-----------------------------------------------------------------------------

void corePrintInstRef( InstRef *iR ) /*fold00*/
{
 fprintf(stderr,"\n InstRef %p:"
                "\n  ptr       %p"
                "\n  size      %i"
                "\n  refCount  %i"
                "\n  lockCount %i"
                "\n  name      %s"
                "\n  color     %i ... red 5, orange 6, green 3, blue 7"
                "\n  heapRefs  %i"
                "\n  flags     %i"
                "\n  state     %i"
                "\n"
                "\n"
                ,
                iR,
                iR->ptr,
                iR->size,
                iR->refCount,
                iR->lockCount,
                iR->name,
                iR->color,
                iR->heapRefCount,
                iR->finalizerFlags,
                iR->state
          );
}
 /*FOLD00*/
//-----------------------------------------------------------------------------
//                            GARBAGE COLLECTOR
//-----------------------------------------------------------------------------

void gcRunAtJvmStartup( void ) /*fold00*/
// run GC daemon by parameters given on command line
{
  #ifdef GC_RELEASE

   if( cmdargs.verbosegc )
    logrHeapGC->verboseOn();
   else
    logrHeapGC->verboseOff();

   #ifdef GC_ENABLE_GC

    // GC is enabled
    if( !( cmdargs.noasyncgc
            ||
           cmdargs.noclassgc
            ||
           cmdargs.singlethread
         )
      )
    {
     logrHeapGC->daemonize();

     DBGprintf(DBG_REF,"+-------------------------------------------+");
     DBGprintf(DBG_REF,"| Reference -INIT-: GC daemon summonized... |");
     DBGprintf(DBG_REF,"+-------------------------------------------+");
    }

   #endif

  #else // GC_RELEASE

   // make GC verbose (printed on stdout)
   logrHeapGC->verboseOn();

   #ifdef GC_ENABLE_GC

    logrHeapGC->daemonize();

    DBGprintf(DBG_REF,"+-------------------------------------------+");
    DBGprintf(DBG_REF,"| Reference -INIT-: GC daemon summonized... |");
    DBGprintf(DBG_REF,"+-------------------------------------------+");

   #endif // GC_ENABLE_GC

  #endif // GC_RELEASE
}

//-----------------------------------------------------------------------------

void gcShutdown( void ) /*fold00*/
// - called for GC deinitialization
{
 DBGsection(DBG_REF,(logrHeapGC->printInfo()));

 // stop garbage collector
 logrHeapGC->terminate();

 #ifdef GC_SYNC_GC
  // run one garbage collection and block until it terminates
  logrHeapGC->runSyncCollection();
 #endif
}

//-----------------------------------------------------------------------------

void gcDaemonize( void ) /*fold00*/
// runs asynchronous GC
{
  logrHeapGC->daemonize();
}

//-----------------------------------------------------------------------------

void gcTerminate( void ) /*fold00*/
// destroys GC daemon
{
 logrHeapGC->terminate();
}

//-----------------------------------------------------------------------------

int  gcIsDaemon( void ) /*fold00*/
{
 return logrHeapGC->isDaemon();
}

//-----------------------------------------------------------------------------

int  gcHuntedInstances( void ) /*fold00*/
{
 return logrHeapGC->huntedInstances;
}

//-----------------------------------------------------------------------------

int  gcHuntedBytes( void ) /*fold00*/
{
 return logrHeapGC->huntedBytes;
}

//-----------------------------------------------------------------------------

void gcJavaLangRuntimeGc( void ) /*fold00*/
{
 logrHeapGC->javaLangRuntimeGcInternal();
}

//-----------------------------------------------------------------------------

void gcJavaLangRuntimeRunFinalization( void ) /*fold00*/
{
 logrHeapGC->javaLangRuntimeRunFinalizationInternal();
}

//-----------------------------------------------------------------------------

void gcSyncRun( void )
// - run one garbage collection and block until it terminates
{
 logrHeapGC->runSyncCollection();
}
 /*FOLD00*/

//- EOF -----------------------------------------------------------------------
