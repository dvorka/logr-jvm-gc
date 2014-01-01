/*
 * reference.h
 *
 * Author: Dvorka
 *
 * Description:
 *      References to logr heap, garbage collector interface, heap.
 *
 * ToDo:
 */

#ifndef __REFERENCE_H
 #define __REFERENCE_H

 #include "bool.h"
 #include "debuglog.h"
 #include "gcoptions.h"
 #include "gcsupport.h"
 #include "referencedefines.h"
 #include "nativethreads.h"

 // reference types are declared in separate file
 #include "referencetypes.h"



 //- DeBug defines ------------------------------------------------------------

  // debug reference
  #define DBG_REF           2000



  // debug gclist
  #define DBG_GCLIST        2005
  // debug gcheap
  #define DBG_GCHEAP        2006
  // debug gcollector
  #define DBG_GC            2007
  // debug instance tracing
  #define DBG_GCTRACE       2008
  // debug gcreference
  #define DBG_GCREF         2009



__BEGIN_DECLS

//-----------------------------------------------------------------------------
//                             PUBLIC GLOBALS
//-----------------------------------------------------------------------------
/*
 * collection flag
 */
extern int gcIsRunning;         // true if collection running: RUNLEVEL 2
extern int gcOrMask;            // OR mask used in putRefField/getRefField
                                // is GC_NOMARK or GC_MARK depending on
                                // garbage collector RUNLEVEL (see gc.cc)

//-----------------------------------------------------------------------------
//                                  NULL
//-----------------------------------------------------------------------------
/*
 * nullRefs
 */
extern StatRef *nullStatRef;
extern InstRef *nullInstRef;

//-----------------------------------------------------------------------------
//                                INIT&CLOSE
//-----------------------------------------------------------------------------
/*
 * reference init and cleanup
 */
int referenceInitialize(void);
int referenceCleanup(void);



//-----------------------------------------------------------------------------
//                                   HEAP
//-----------------------------------------------------------------------------
/*
 * Logr and system heap
 */

 // heap for Logr/Java classes
 #define GC_LOGR_HEAP                      0x0001
 // JVM inner heap
 #define GC_SYSTEM_HEAP                    0x0002

 void *logrAlloc( size_t s );              // alloc in inner JVM heap
 void logrFree( void *p );                 // free in inner JVM heap

 int  referenceHeapCoreLeft(int heapName); // how many bytes free in heap
 int  referenceHeapTotalMem(int heapName); // -mx how -"- can be allocated

 void referenceHeapDump(int heapName);     // dump heap content into EA*.dump 
 void referenceHeapInfo(int heapName);     // print info about Logr heap
 void referenceHeapWalker(int heapName);   // heap walker
 int  referenceHeapCheck(int heapName);    // heap check, TRUE if OK

 int  referenceHeapCheckChunk(int heapName,void *p); // chunk check TRUE if OK
 void referenceHeapDumpChunk(int heapName,void *p);  // dump into CHUNK.dump

 int  referenceHeapFillFree(int heapName,byte pattern);      // check and fill
 int  referenceHeapCheckFillFree(int heapName,byte pattern); // check fill
 int  referenceHeapCheckFillFreeChunk(int heapName,void *p,byte pattern);
                                                             // for chunk
//-----------------------------------------------------------------------------
//                              STAT REF
//-----------------------------------------------------------------------------

 // core functions
 StatRef *coreNewStatRef( int size );
 StatRef *coreNewNamedStatRef( int size, char *name );
 void     coreSetStatRefName( StatRef *sR, char *name );
 char    *coreGetStatRefName( StatRef *sR );
 void     coreDeleteStatRef( StatRef *sR );
 void     coreSetStatRefType(StatRef *sR,int type);
 void     coreStretchStatRef( StatRef *sR, int size );
 void     coreShrinkStatRef( StatRef *sR, int size );
 void     coreReallocStatRef( StatRef *sR, int size );
 void     corePrintStatRef( StatRef *sR );

// mapping to reference core functions due to version
#ifdef GC_PURE_MACRO_REFERENCE

/* StatRef *newStatRef(int size)                                */ /*FOLD00*/
/*
 * allocates in logr heap
 * Return: NULL (C)  ... if cannot allocate,
 *         otherwise ... locked reference with refCount 1
 */
#define newStatRef( SIZE )                                                    \
    ({                                                                            \
    \
    StatRef *sR = coreNewStatRef( SIZE );                                        \
    \
    DBGsection(DBG_REF,(referenceDebugManage(S_WAS_BORN,sR,__FILE__,__LINE__))); \
    \
    sR;                                                                          \
    \
    })

/* StatRef *newNamedStatRef(int size,char *name)                */ /*fold00*/
/*
 * allocates in logr heap
 * Return: NULL (C)  ... if cannot allocate,
 *         otherwise ... locked reference with refCount 1
 */
#define newNamedStatRef( SIZE, NAME )                                         \
({                                                                            \
                                                                              \
 StatRef *sR = coreNewNamedStatRef( SIZE, NAME );                             \
                                                                              \
 DBGsection(DBG_REF,(referenceDebugManage(S_WAS_BORN,sR,__FILE__,__LINE__))); \
                                                                              \
 sR;                                                                          \
                                                                              \
})



/* void deleteStatRef( StatRef *sR )                            */ /*fold00*/
#define deleteStatRef( SR )                                                   \
({                                                                            \
do                                                                            \
{                                                                             \
                                                                              \
 DBGsection(DBG_REF,(referenceDebugManage(S_DELETE,(SR),__FILE__,__LINE__))); \
                                                                              \
 coreDeleteStatRef((SR));                                                     \
                                                                              \
}                                                                             \
while(0);                                                                     \
})



/* void stretchStatRef( StatRef *sR, int size )                 */ /*fold00*/
#define stretchStatRef( SR, SIZE )                                            \
({                                                                            \
do                                                                            \
{                                                                             \
                                                                              \
 DBGsection(DBG_REF,(referenceDebugManageCore(S_REALLOC,(SR),SIZE,            \
                    __FILE__,__LINE__)));                                     \
                                                                              \
 coreStretchStatRef( (SR), SIZE );                                            \
                                                                              \
}                                                                             \
while(0);                                                                     \
})



/* void shrinkStatRef( StatRef *sR, int size )                  */ /*fold00*/
// - if size==0 -> reference is deleted
// - in case of performance chunk in heap is not shrinked
#define shrinkStatRef( SR, SIZE )                                             \
({                                                                            \
do                                                                            \
{                                                                             \
                                                                              \
 DBGsection(DBG_REF,(referenceDebugManageCore(S_REALLOC,(SR),SIZE,            \
                    __FILE__,__LINE__)));                                     \
                                                                              \
 coreShrinkStatRef( (SR), SIZE );                                             \
                                                                              \
}                                                                             \
while(0);                                                                     \
})



/* void reallocStatRef( StatRef *sR, int size )                 */ /*fold00*/
#define reallocStatRef( SR, SIZE )                                            \
({                                                                            \
do                                                                            \
{                                                                             \
 DBGsection(DBG_REF,(referenceDebugManageCore(S_REALLOC,(SR),SIZE,            \
                    __FILE__,__LINE__)));                                     \
                                                                              \
                                                                              \
 coreReallocStatRef( (SR), SIZE );                                            \
                                                                              \
}                                                                             \
while(0);                                                                     \
})



 /*FOLD00*/

#else

 #define newStatRef( SIZE )                                                   \
  coreNewStatRef( SIZE )
 #define newNamedStatRef( SIZE, NAME )                                        \
  coreNewNamedStatRef( SIZE, NAME )
 #define deleteStatRef( SR )                                                  \
  coreDeleteStatRef( SR )
 #define stretchStatRef( SR, SIZE )                                           \
  coreStretchStatRef( SR, SIZE )
 #define shrinkStatRef( SR, SIZE )                                            \
  coreShrinkStatRef( SR, SIZE )
 #define reallocStatRef( SR, SIZE )                                           \
  coreReallocStatRef( SR, SIZE )

#endif

/* void setStatRefName( StatRef *sR, char *name )               */ /*fold00*/
#define setStatRefName( SR, NAME )                                            \
({                                                                            \
do                                                                            \
{                                                                             \
 if((SR)->name!=NULL)                                                         \
  free((SR)->name);                                                           \
                                                                              \
 if(NAME==NULL)                                                               \
  (SR)->name=NULL;                                                            \
 else                                                                         \
  (SR)->name=strdup(NAME);                                                    \
}                                                                             \
while(0);                                                                     \
})



/* char *getStatRefName( StatRef *sR )                          */ /*fold00*/
#define getStatRefName( SR )                                                  \
({                                                                            \
 (SR)->name;                                                                  \
})



/* void setStatRefType(StatRef *sR, int type)                   */ /*fold00*/
// - sets type of StatRef -> see reference.h
#define setStatRefType( SR, TYPE )                                            \
({                                                                            \
do                                                                            \
{                                                                             \
 DBGsection(DBG_REF,(referenceDebugManageCore(S_SET_TYPE,(SR),TYPE,             \
                    __FILE__,__LINE__)));                                     \
                                                                              \
 (SR)->type=TYPE;                                                               \
}                                                                             \
while(0);                                                                     \
})



/* void addStatRef( StatRef *STAT_REF )                         */ /*fold00*/
#define addStatRef( STAT_REF )				                      \
({                                                                            \
 do                                                                           \
 {							                      \
  DBGsection(2000,(referenceDebugManage(S_ADD_REF,STAT_REF,                   \
                                        __FILE__,__LINE__)));                 \
  __asm__ __volatile__ (                                                      \
                        "incl %0"                                             \
			:                                                     \
			: "m" ((STAT_REF)->refCount)                          \
                        : "memory"                                            \
                       );                                                     \
 }                                                                            \
 while(0);                                                                    \
})



 /* void releaseStatRef( StatRef *STAT_REF )                     */ /*fold00*/
#ifdef PEDANT_REF_DEBUG
    #define releaseStatRef( STAT_REF )                                        \
    ({                                                                        \
    do                                                                        \
    {                                                                         \
    DBGsection(2000,(referenceDebugManage(S_RELEASE_REF,STAT_REF,             \
                                        __FILE__,__LINE__)));                 \
    if (!((STAT_REF)->refCount) && (STAT_REF) != nullStatRef)    {                                         \
    fprintf(stderr,"ref count %p(%s) underflow!!!\n",STAT_REF,getStatRefName(STAT_REF)); \
    assert(!"ref count underflow!!!");                                        \
    }                                                                         \
    __asm__ __volatile__ (                                                    \
    "decl %0"                                             \
    :                                                     \
    : "m" ((STAT_REF)->refCount)                          \
    : "memory"                                            \
    );                                                     \
    }                                                                            \
    while(0);                                                                     \
    })
#else
    #define releaseStatRef( STAT_REF )                                            \
    ({                                                                            \
    do                                                                           \
    {                                                                            \
    DBGsection(2000,(referenceDebugManage(S_RELEASE_REF,STAT_REF,               \
    __FILE__,__LINE__)));                 \
    __asm__ __volatile__ (                                                      \
    "decl %0"                                             \
    :                                                     \
    : "m" ((STAT_REF)->refCount)                          \
    : "memory"                                            \
    );                                                     \
    }                                                                            \
    while(0);                                                                     \
    })
#endif                                                    \


/* void lockStatRef( StatRef *STAT_REF )                        */ /*fold00*/
 // if successful then lockCount is increased by 1 and state stays 0
#ifdef PEDANT_REF_DEBUG
    #define lockStatRef( STAT_REF )                                               \
    ({                                                                            \
    do                                                                            \
    {                                                                             \
    DBGsection(2000,(referenceDebugManage(S_LOCK,STAT_REF,                        \
    __FILE__,__LINE__)));                                                         \
    __asm__ __volatile__ (                                                        \
    "  movl %0, %%eax                       \n"           \
    \
    "0:                                     \n"           \
    "  incl "STATREF_LOCKCOUNT_STR"(%%eax)  \n"           \
    "  orl $0, "STATREF_STATE_STR"(%%eax)   \n"           \
    "  jz 1f                                \n"           \
    \
    "  decl "STATREF_LOCKCOUNT_STR"(%%eax)  \n"           \
    \
    "  pusha                                \n"           \
    "   call yield                          \n"           \
    "  popa                                 \n"           \
    "  jmp 0b                               \n"           \
    \
    "1:                                     \n"           \
    :                                                     \
    : "m" (STAT_REF)                                      \
    : "eax", "memory"                                     \
    );                                                    \
    if (PEDANT_REF_DEBUG_SREF && ((STAT_REF)->lockCount)==1 && (STAT_REF) != nullStatRef)    {               \
        assert((STAT_REF)->swap);                                                 \
        (STAT_REF)->ptr=(STAT_REF)->swap;                                         \
        (STAT_REF)->swap=0;                                                       \
    }                                                                             \
    }                                                                            \
    while(0);                                                                    \
    })

#else
    #define lockStatRef( STAT_REF )                                               \
    ({                                                                            \
    do                                                                           \
    {                                                                            \
    DBGsection(2000,(referenceDebugManage(S_LOCK,STAT_REF,                      \
    __FILE__,__LINE__)));                 \
    __asm__ __volatile__ (                                                      \
    "  movl %0, %%eax                       \n"           \
    \
    "0:                                     \n"           \
    "  incl "STATREF_LOCKCOUNT_STR"(%%eax)  \n"           \
    "  orl $0, "STATREF_STATE_STR"(%%eax)   \n"           \
    "  jz 1f                                \n"           \
    \
    "  decl "STATREF_LOCKCOUNT_STR"(%%eax)  \n"           \
    \
    "  pusha                                \n"           \
    "   call yield                          \n"           \
    "  popa                                 \n"           \
    "  jmp 0b                               \n"           \
    \
    "1:                                     \n"           \
    :                                                     \
    : "m" (STAT_REF)                                      \
    : "eax", "memory"                                     \
    );                                                    \
    }                                                                            \
    while(0);                                                                    \
    })
#endif



 /* void unlockStatRef( StatRef *STAT_REF )                      */ /*fold00*/
#ifdef PEDANT_REF_DEBUG
#define unlockStatRef( STAT_REF )                                             \
    ({                                                                            \
    do                                                                           \
    {                                                                            \
    DBGsection(2000,(referenceDebugManage(S_UNLOCK,STAT_REF,                    \
    __FILE__,__LINE__)));                 \
    if (!((STAT_REF)->lockCount) && (STAT_REF) != nullStatRef)    {                                        \
    fprintf(stderr,"lock count %p(%s) underflow!!!\n",STAT_REF,getStatRefName(STAT_REF)); \
    assert(!"lock count underflow!!!");                                       \
    }                                                                         \
    if (PEDANT_REF_DEBUG_SREF && ((STAT_REF)->lockCount)==1 && (STAT_REF) != nullStatRef)    {           \
        void *p;                                                         \
        assert(p=malloc((size_t) (STAT_REF)->size));                     \
        memcpy(p,(STAT_REF)->ptr,(size_t) (STAT_REF)->size);             \
        free((STAT_REF)->ptr);                                                \
        (STAT_REF)->ptr=0;                                                    \
        (STAT_REF)->swap=p; /* assignment at last ... to be sure swap is NULL or correct */     \
    }                                                                         \
    __asm__ __volatile__ (                                                    \
    "decl %0"                                                                 \
    :                                                                         \
    : "m" ((STAT_REF)->lockCount)                                             \
    : "memory"                                                                \
    );                                                                        \
    }                                                                             \
    while(0);                                                                    \
    })
#else
#define unlockStatRef( STAT_REF )                                             \
    ({                                                                            \
    do                                                                           \
    {                                                                            \
    DBGsection(2000,(referenceDebugManage(S_UNLOCK,STAT_REF,                    \
    __FILE__,__LINE__)));                 \
    __asm__ __volatile__ (                                                    \
    "decl %0"                                             \
    :                                                     \
    : "m" ((STAT_REF)->lockCount)                         \
    : "memory"                                            \
    );                                                     \
    }                                                                             \
    while(0);                                                                    \
    })
#endif



/* void gcLockStatRef( StatRef *STAT_REF )                      */ /*fold00*/
// lockCount must be 0 -> state is set to 1
#ifdef PEDANT_REF_DEBUG
#define gcLockStatRef( STAT_REF )                                             \
({                                                                            \
 do                                                                           \
 {                                                                            \
    if (((STAT_REF)->state))    {                                        \
        fprintf(stderr,"GC already active (state ==  %d) on %p(%s)\n",(STAT_REF)->state,STAT_REF,getStatRefName(STAT_REF)); \
        assert(!"GC already active!!!");                                       \
    }                                                                         \
                                                                              \
  __asm__ __volatile__ (                                                      \
			"  movl %0, %%eax                               \n"   \
			"0:                                             \n"   \
			"  movl $1, "STATREF_STATE_STR"(%%eax)          \n"   \
			"  cmpl $0, "STATREF_LOCKCOUNT_STR"(%%eax)      \n"   \
			"  jz 1f                                        \n"   \
                                                                              \
			"  pusha                                        \n"   \
			"   call yield                                  \n"   \
			"  popa                                         \n"   \
			"  jmp 0b                                       \n"   \
                                                                              \
			"1:                                             \n"   \
			:                                                     \
			: "m" (STAT_REF)                                      \
                        : "eax",  "memory"                                    \
                       );                                                     \
    if (PEDANT_REF_DEBUG_SREF && (STAT_REF) != nullStatRef)    {               \
        assert((STAT_REF)->swap);                                                 \
        (STAT_REF)->ptr=(STAT_REF)->swap;                                         \
        (STAT_REF)->swap=0;                                                       \
    }                                                                             \
 }                                                                            \
 while(0);                                                                    \
})
#else
#define gcLockStatRef( STAT_REF )                                             \
    ({                                                                            \
    do                                                                           \
    {                                                                            \
    \
    __asm__ __volatile__ (                                                      \
    "  movl %0, %%eax                               \n"   \
    "0:                                             \n"   \
    "  movl $1, "STATREF_STATE_STR"(%%eax)          \n"   \
    "  cmpl $0, "STATREF_LOCKCOUNT_STR"(%%eax)      \n"   \
    "  jz 1f                                        \n"   \
    \
    "  pusha                                        \n"   \
    "   call yield                                  \n"   \
    "  popa                                         \n"   \
    "  jmp 0b                                       \n"   \
    \
    "1:                                             \n"   \
    :                                                     \
    : "m" (STAT_REF)                                      \
    : "eax",  "memory"                                    \
    );                                                     \
    }                                                                            \
    while(0);                                                                    \
    })
#endif


/* bool gcSoftLockStatRef( StatRef *STAT_REF )                  */ /*fold00*/
// lockCount must be 0 -> state is set to 1
#ifdef PEDANT_REF_DEBUG
#define gcSoftLockStatRef( STAT_REF )                                         \
({                                                                            \
 int gcSoftLockStatRefReturnValue=1;                                          \
                                                                              \
  DBGsection(2000,(referenceDebugManage(S_GCLOCK,STAT_REF,                    \
                                        __FILE__,__LINE__)));                 \
    if (((STAT_REF)->state))    {                                        \
        fprintf(stderr,"GC already active (state == %d) on %p(%s)\n",(STAT_REF)->state,STAT_REF,getStatRefName(STAT_REF)); \
        assert(!"GC already active!!!");                                       \
    }                                                                         \
  __asm__ __volatile__ (                                                      \
			"  movl $1, %0       \n"   /* state=1   */            \
			"  cmpl $0, %1       \n"   /* lockCount */            \
			"  jz 0f             \n"                              \
                                                                              \
			"  movl $0, %0       \n"   /* state=0   */            \
			"  movl $0, %2       \n"   /* fail      */            \
			"0:                  \n"   /* success   */            \
                                                                              \
			:                                                     \
			: "m" ((STAT_REF)->state),                            \
		          "m" ((STAT_REF)->lockCount),                        \
		          "m" gcSoftLockStatRefReturnValue                    \
                        : "memory"                                            \
                       );                                                     \
    if (gcSoftLockStatRefReturnValue)  {                                      \
        if (PEDANT_REF_DEBUG_SREF && (STAT_REF) != nullStatRef)    {               \
            assert((STAT_REF)->swap);                                                 \
            (STAT_REF)->ptr=(STAT_REF)->swap;                                         \
            (STAT_REF)->swap=0;                                                       \
        }                                                                             \
    }                                                                           \
 gcSoftLockStatRefReturnValue;                                                \
                                                                              \
})
#else
#define gcSoftLockStatRef( STAT_REF )                                         \
    ({                                                                            \
    int gcSoftLockStatRefReturnValue=1;                                          \
    \
    DBGsection(2000,(referenceDebugManage(S_GCLOCK,STAT_REF,                    \
    __FILE__,__LINE__)));                 \
    __asm__ __volatile__ (                                                      \
    "  movl $1, %0       \n"   /* state=1   */            \
    "  cmpl $0, %1       \n"   /* lockCount */            \
    "  jz 0f             \n"                              \
    \
    "  movl $0, %0       \n"   /* state=0   */            \
    "  movl $0, %2       \n"   /* fail      */            \
    "0:                  \n"   /* success   */            \
    \
    :                                                     \
    : "m" ((STAT_REF)->state),                            \
    "m" ((STAT_REF)->lockCount),                        \
    "m" gcSoftLockStatRefReturnValue                    \
    : "memory"                                            \
    );                                                     \
    gcSoftLockStatRefReturnValue;                                                \
    \
    })
#endif

/* void gcUnlockStatRef( StatRef *STAT_REF )                    */ /*fold00*/
// if successful then lockCount is set to 0 and state is set to 0
#ifdef PEDANT_REF_DEBUG
#define gcUnlockStatRef( STAT_REF )                                           \
({                                                                            \
 do                                                                           \
 {                                                                            \
  DBGsection(2000,(referenceDebugManage(S_GCUNLOCK,STAT_REF,                  \
                                        __FILE__,__LINE__)));                 \
    if ((STAT_REF)->state!=1)    {                                        \
        fprintf(stderr,"GC state %d != 1 on %p(%s)\n",(STAT_REF)->state,STAT_REF,getStatRefName(STAT_REF)); \
        assert(!"GC state illegal!!!");                                       \
    }                                                                         \
    if (PEDANT_REF_DEBUG_SREF && (STAT_REF) != nullStatRef)    {           \
        void *p;                                                         \
        assert(p=malloc((size_t) (STAT_REF)->size));                     \
        memcpy(p,(STAT_REF)->ptr,(size_t) (STAT_REF)->size);             \
        free((STAT_REF)->ptr);                                                \
        (STAT_REF)->ptr=0;                                                    \
        (STAT_REF)->swap=p; /* assignment at last ... to be sure swap is NULL or correct */     \
    }                                                                         \
  __asm__ __volatile__ (                                                      \
                        "decl %0"                                             \
			:                                                     \
			: "m" ((STAT_REF)->state)                             \
                        : "memory"                                            \
    );                                                                        \
 }                                                                            \
 while(0);                                                                    \
})
#else
#define gcUnlockStatRef( STAT_REF )                                           \
    ({                                                                            \
    do                                                                           \
    {                                                                            \
    DBGsection(2000,(referenceDebugManage(S_GCUNLOCK,STAT_REF,                  \
    __FILE__,__LINE__)));                 \
    __asm__ __volatile__ (                                                      \
    "decl %0"                                             \
    :                                                     \
    : "m" ((STAT_REF)->state)                             \
    : "memory"                                            \
    );                                                                        \
    }                                                                            \
    while(0);                                                                    \
    })
#endif

/* void printStatRef( StatRef *sR )                             */ /*fold00*/
#define printStatRef( SR )                                                    \
({                                                                            \
do                                                                            \
{                                                                             \
 fprintf(stderr,"\n StatRef %p:"                                              \
                "\n  ptr       %p"                                            \
                "\n  size      %i"                                            \
                "\n  refCount  %i"                                            \
                "\n  lockCount %i"                                            \
                "\n  name      %s"                                            \
                "\n  type      %i"                                            \
                "\n  color     %i ... red 5, orange 6, green 3, blue 7"       \
                "\n  state     %i"                                            \
                "\n  flags     %i"                                            \
                "\n"                                                          \
                "\n"                                                          \
                ,                                                             \
                (SR),                                                         \
                (SR)->ptr,                                                    \
                (SR)->size,                                                   \
                (SR)->refCount,                                               \
                (SR)->lockCount,                                              \
                (SR)->name,                                                   \
                (SR)->type,                                                   \
                (SR)->color,                                                  \
                (SR)->state,                                                  \
                (SR)->finalizerFlags                                          \
          );                                                                  \
}                                                                             \
while(0);                                                                     \
})
 /*FOLD00*/


//-----------------------------------------------------------------------------
//                               INSTREF
//-----------------------------------------------------------------------------

 // core functions
 InstRef *coreNewInstRef( int size );
 InstRef *coreNewNamedInstRef( int size, char *name );
 void    coreSetInstRefName( InstRef *iR, char *name );
 char    *coreGetInstRefName( InstRef *iR );
 void    coreReallocInstRef( InstRef *iR, int size );
 void    coreStretchInstRef( InstRef *iR, int size );
 void    coreShrinkInstRef( InstRef *iR, int size );
 void    coreDeleteInstRef( InstRef *iR );
 void    corePrintInstRef( InstRef *iR );

// mapping to reference core functions due to version
#ifdef GC_PURE_MACRO_REFERENCE

/* InstRef *newInstRef(int size)                                */ /*fold00*/
/*
 * allocates in logr heap
 * Return: NULL (C)  ... if cannot allocate,
 *         otherwise ... locked reference with refCount 1
 */
#define newInstRef(SIZE)                                                      \
({							                      \
                                                                              \
 InstRef *iR = coreNewInstRef( SIZE );                                        \
                                                                              \
 DBGsection(DBG_REF,(referenceDebugManage(I_WAS_BORN,iR,__FILE__,__LINE__))); \
                                                                              \
 iR;                                                                          \
                                                                              \
})



/* InstRef *newNamedInstRef(int size,char *name)                */ /*fold00*/
/*
 * allocates in logr heap
 * Return: NULL (C)  ... if cannot allocate,
 *         otherwise ... locked reference with refCount 1
 */
#define newNamedInstRef(SIZE,NAME)                                            \
({                                                                            \
                                                                              \
 InstRef *iR = coreNewNamedInstRef( SIZE, NAME );                             \
                                                                              \
 DBGsection(DBG_REF,(referenceDebugManage(I_WAS_BORN,iR,__FILE__,__LINE__))); \
                                                                              \
 iR;                                                                          \
                                                                              \
})



/* void deleteInstRef( InstRef *iR )                            */ /*fold00*/
#define deleteInstRef( IR )                                                   \
({                                                                            \
do                                                                            \
{                                                                             \
                                                                              \
 DBGsection(DBG_REF,(referenceDebugManage(I_DELETE,(IR),__FILE__,__LINE__))); \
                                                                              \
 coreDeleteInstRef((IR));                                                     \
                                                                              \
}                                                                             \
while(0);                                                                     \
})



/* void stretchInstRef( InstRef *iR, int size )                 */ /*fold00*/
#define stretchInstRef( IR, SIZE )                                            \
({                                                                            \
do                                                                            \
{                                                                             \
                                                                              \
 DBGsection(DBG_REF,(referenceDebugManageCore(I_REALLOC,(IR),SIZE,            \
                                              __FILE__,__LINE__)));           \
                                                                              \
 coreStretchInstRef( (IR), SIZE );                                            \                              \
                                                                              \
}                                                                             \
while(0);                                                                     \
})



/* void shrinkInstRef( InstRef *iR, int size )                  */ /*fold00*/
// - if size==0 -> reference is deleted
// - in case of performance chunk in heap is not shrinked
#define shrinkInstRef( IR, SIZE )                                             \
({                                                                            \
do                                                                            \
{                                                                             \
                                                                              \
 DBGsection(DBG_REF,(referenceDebugManageCore(I_REALLOC,(IR),SIZE,            \
                                              __FILE__,__LINE__)));           \
                                                                              \
 coreShrinkInstRef((IR),SIZE);                                                \
                                                                              \
}                                                                             \
while(0);                                                                     \
})



/* void reallocInstRef( InstRef *iR, int size )                 */ /*fold00*/
#define reallocInstRef( IR, SIZE )                                            \
({                                                                            \
do                                                                            \
{                                                                             \
 DBGsection(DBG_REF,(referenceDebugManageCore(I_REALLOC,(IR),SIZE,NULL,       \
                                              __FILE__,__LINE__)));           \
                                                                              \
 coreReallocInstRef( (IR), SIZE );                                            \
                                                                              \
}                                                                             \
while(0);                                                                     \
})



 /*FOLD00*/

#else

 #define newInstRef( SIZE )                                                   \
  coreNewInstRef( SIZE )
 #define newNamedInstRef( SIZE, NAME )                                        \
  coreNewNamedInstRef( SIZE, NAME )
 #define reallocInstRef( IR, SIZE )                                           \
  coreReallocInstRef( IR, SIZE )
 #define stretchInstRef( IR, SIZE )                                           \
  coreStretchInstRef( IR, SIZE )
 #define coreShrinkInstRef( IR, SIZE )                                        \
  coreShrinkInstRef( IR, SIZE )
 #define deleteInstRef( IR )                                                  \
  coreDeleteInstRef( IR )

#endif



/* void setInstRefName( InstRef *iR, char *name )               */ /*fold00*/
#define setInstRefName( IR, NAME )                                            \
({                                                                            \
do                                                                            \
{                                                                             \
 if((IR)->name!=NULL)                                                         \
  free((IR)->name);                                                           \
                                                                              \
 if(NAME==NULL)                                                               \
  (IR)->name=NULL;                                                            \
 else                                                                         \
  (IR)->name=strdup(NAME);                                                    \
}                                                                             \
while(0);                                                                     \
})



/* char *getInstRefName( InstRef *iR )                          */ /*fold00*/
#define getInstRefName( IR )                                                  \
({                                                                            \
 (IR)->name;                                                                  \
})



/* void addInstRef( InstRef *INST_REF )                         */ /*fold00*/
#define addInstRef( INST_REF )				                      \
({                                                                            \
 do                                                                           \
 {							                      \
  DBGsection(2000,(referenceDebugManage(I_ADD_REF,INST_REF,                   \
                                        __FILE__,__LINE__)));                 \
  __asm__ __volatile__ (                                                      \
                        " movl %0, %%eax                        \n"           \
                                                                              \
                        " movl %1, %%ecx                        \n" /* orMask -> ECX    GC */\
                        " orl %%ecx, "INSTREF_COLOR_STR"(%%eax) \n" /* color  |= ECX    GC */\
                                                                                             \
			" incl "INSTREF_REFCOUNT_STR"(%%eax)    \n" /* refCount++          */\
			:                                                     \
			: "m" (INST_REF),                                     \
			  "m" (gcOrMask)                            /* 0 */   \
                        : "eax", "ecx", "memory"                    /* 1 */   \
                       );                                                     \
 }                                                                            \
 while(0);                                                                    \
})



/* void releaseInstRef( InstRef *INST_REF )                     */ /*fold00*/
#ifdef PEDANT_REF_DEBUG
#define releaseInstRef( INST_REF )                                            \
    ({                                                                            \
    do                                                                           \
    {                                                                            \
    DBGsection(2000,(referenceDebugManage(I_RELEASE_REF,INST_REF,               \
    __FILE__,__LINE__)));                 \
    if (!((INST_REF)->refCount) && (INST_REF) != nullInstRef)    {                                         \
    fprintf(stderr,"ref count %p(%s) underflow!!!",INST_REF,getInstRefName(INST_REF)); \
    assert(!"ref count underflow!!!");                                    \
    }                                                                         \
    __asm__ __volatile__ (                                                      \
    "decl %0"                                             \
    :                                                     \
    : "m" ((INST_REF)->refCount)                          \
    : "memory"                                            \
    );                                                     \
    }                                                                            \
    while(0);                                                                     \
    })
#else
#define releaseInstRef( INST_REF )                                            \
    ({                                                                            \
    do                                                                           \
    {                                                                            \
    DBGsection(2000,(referenceDebugManage(I_RELEASE_REF,INST_REF,               \
    __FILE__,__LINE__)));                 \
    __asm__ __volatile__ (                                                      \
    "decl %0"                                             \
    :                                                     \
    : "m" ((INST_REF)->refCount)                          \
    : "memory"                                            \
    );                                                     \
    }                                                                            \
    while(0);                                                                     \
    })
#endif



/* void lockInstRef( InstRef *INST_REF )                        */ /*fold00*/
// if successful then lockCount is increased by 1 and state stays 0
#ifdef PEDANT_REF_DEBUG
#define lockInstRef( INST_REF )                                               \
    ({                                                                            \
    do                                                                            \
    {                                                                            \
    DBGsection(2000,(referenceDebugManage(I_LOCK,INST_REF,__FILE__,__LINE__))); \
    \
    __asm__ __volatile__ (                                                      \
    "  movl %0, %%eax                       \n"           \
    \
    "0:                                     \n"           \
    "  incl "INSTREF_LOCKCOUNT_STR"(%%eax)  \n"           \
    "  orl $0, "INSTREF_STATE_STR"(%%eax)   \n"           \
    "  jz 1f                                \n"           \
    \
    "  decl "INSTREF_LOCKCOUNT_STR"(%%eax)  \n"           \
    \
    "  pusha                                \n"           \
    "   call yield                          \n"           \
    "  popa                                 \n"           \
    "  jmp 0b                               \n"           \
    \
    "1:                                     \n"           \
    " incl "INSTREF_REFCOUNT_STR"(%%eax)    \n" /* Buggy refCount++ */\
    :                                                     \
    : "m" (INST_REF)                                      \
    : "eax", "memory"                                     \
    );                                                     \
    if (PEDANT_REF_DEBUG_IREF && (INST_REF)->lockCount==1 && (INST_REF) != nullInstRef)    {                                        \
        assert((INST_REF)->swap);                                             \
        (INST_REF)->ptr=(INST_REF)->swap;                                             \
        (INST_REF)->swap=0;                                             \
    }                                                                         \
    }                                                                            \
    while(0);                                                                     \
    })
#else
#define lockInstRef( INST_REF )                                               \
    ({                                                                            \
    do                                                                            \
    {                                                                            \
    DBGsection(2000,(referenceDebugManage(I_LOCK,INST_REF,__FILE__,__LINE__))); \
    \
    __asm__ __volatile__ (                                                      \
    "  movl %0, %%eax                       \n"           \
    \
    "0:                                     \n"           \
    "  incl "INSTREF_LOCKCOUNT_STR"(%%eax)  \n"           \
    "  orl $0, "INSTREF_STATE_STR"(%%eax)   \n"           \
    "  jz 1f                                \n"           \
    \
    "  decl "INSTREF_LOCKCOUNT_STR"(%%eax)  \n"           \
    \
    "  pusha                                \n"           \
    "   call yield                          \n"           \
    "  popa                                 \n"           \
    "  jmp 0b                               \n"           \
    \
    "1:                                     \n"           \
    " incl "INSTREF_REFCOUNT_STR"(%%eax)    \n" /* Buggy refCount++ */\
    :                                                     \
    : "m" (INST_REF)                                      \
    : "eax", "memory"                                     \
    );                                                     \
    }                                                                            \
    while(0);                                                                     \
    })
#endif

/* void unlockInstRef( InstRef *INST_REF )                      */ /*fold00*/
#ifdef PEDANT_REF_DEBUG
#define unlockInstRef( INST_REF )                                             \
    ({                                                                            \
    do                                                                           \
    {                                                                            \
    DBGsection(2000,(referenceDebugManage(I_UNLOCK,INST_REF,                    \
    __FILE__,__LINE__)));                 \
    if (!((INST_REF)->lockCount) && (INST_REF) != nullInstRef)    {                                        \
        fprintf(stderr,"lock count %p(%s) underflow!!!",INST_REF,getInstRefName(INST_REF)); \
        assert(!"lock count underflow!!!");                                   \
    }                                                                         \
    if (PEDANT_REF_DEBUG_IREF && ((INST_REF)->lockCount)==1 && (INST_REF) != nullInstRef)    {                                        \
        void *p;                                                                               \
        assert(p=malloc((size_t) (INST_REF)->size));                            \
        memcpy(p,(INST_REF)->ptr,(size_t) (INST_REF)->size);            \
        free((INST_REF)->ptr);                                                \
        (INST_REF)->ptr=0;                                                    \
        (INST_REF)->swap=p;                                            \
        /* assign at last ... to be sure swap is NULL or holds correct data */  \
    }                                                                         \
    __asm__ __volatile__ (                                                      \
    "decl %0 \n"                                          \
    "decl %1 \n" /* Buggy refCount++ */                   \
    :                                                     \
    : "m" ((INST_REF)->lockCount),                        \
    "m" ((INST_REF)->refCount)                          \
    : "memory"                                            \
    );                                                     \
    }                                                                            \
    while(0);                                                                    \
    })
#else
#define unlockInstRef( INST_REF )                                             \
    ({                                                                            \
    do                                                                           \
    {                                                                            \
    DBGsection(2000,(referenceDebugManage(I_UNLOCK,INST_REF,                    \
    __FILE__,__LINE__)));                 \
    __asm__ __volatile__ (                                                      \
    "decl %0 \n"                                          \
    "decl %1 \n" /* Buggy refCount++ */                   \
    :                                                     \
    : "m" ((INST_REF)->lockCount),                        \
    "m" ((INST_REF)->refCount)                          \
    : "memory"                                            \
    );                                                     \
    }                                                                            \
    while(0);                                                                    \
    })
#endif


/* void gcLockInstRef( InstRef *INST_REF )                      */ /*fold00*/
// lockCount must be 0 -> state is set to 1
#ifdef PEDANT_REF_DEBUG
#define gcLockInstRef( INST_REF )                                             \
({                                                                            \
 do                                                                           \
 {                                                                            \
  DBGsection(2000,(referenceDebugManage(I_GCLOCK,INST_REF,                    \
                                        __FILE__,__LINE__)));                 \
    if (((INST_REF)->state))    {                                        \
        fprintf(stderr,"GC already active (state == %d) on %p(%s)\n",(INST_REF)->state,INST_REF,getInstRefName(INST_REF)); \
        assert(!"GC already active!!!");                                       \
    }                                                                         \
  __asm__ __volatile__ (                                                      \
			"  movl %0, %%eax                               \n"   \
			"0:                                             \n"   \
			"  movl $1, "INSTREF_STATE_STR"(%%eax)          \n"   \
			"  cmpl $0, "INSTREF_LOCKCOUNT_STR"(%%eax)      \n"   \
			"  jz 1f                                        \n"   \
                                                                              \
			"  pusha                                        \n"   \
			"   call yield                                  \n"   \
			"  popa                                         \n"   \
			"  jmp 0b                                       \n"   \
                                                                              \
			"1:                                             \n"   \
			:                                                     \
			: "m" (INST_REF)                                      \
                        : "eax",  "memory"                                    \
                       );                                                     \
    if (PEDANT_REF_DEBUG_IREF && (INST_REF) != nullInstRef)    {               \
        assert((INST_REF)->swap);                                                 \
        (INST_REF)->ptr=(INST_REF)->swap;                                         \
        (INST_REF)->swap=0;                                                       \
    }                                                                             \
 }                                                                            \
 while(0);                                                                    \
})
#else
#define gcLockInstRef( INST_REF )                                             \
    ({                                                                            \
    do                                                                           \
    {                                                                            \
    DBGsection(2000,(referenceDebugManage(I_GCLOCK,INST_REF,                    \
    __FILE__,__LINE__)));                 \
    __asm__ __volatile__ (                                                      \
    "  movl %0, %%eax                               \n"   \
    "0:                                             \n"   \
    "  movl $1, "INSTREF_STATE_STR"(%%eax)          \n"   \
    "  cmpl $0, "INSTREF_LOCKCOUNT_STR"(%%eax)      \n"   \
    "  jz 1f                                        \n"   \
    \
    "  pusha                                        \n"   \
    "   call yield                                  \n"   \
    "  popa                                         \n"   \
    "  jmp 0b                                       \n"   \
    \
    "1:                                             \n"   \
    :                                                     \
    : "m" (INST_REF)                                      \
    : "eax",  "memory"                                    \
    );                                                     \
    }                                                                            \
    while(0);                                                                    \
    })
#endif


/* bool gcSoftLockInstRef( InstRef *INST_REF )                  */ /*fold00*/
// lockCount must be 0 -> state is set to 1
#ifdef PEDANT_REF_DEBUG
#define gcSoftLockInstRef( INST_REF )                                         \
({                                                                            \
 int gcSoftLockInstRefReturnValue=1;                                          \
                                                                              \
  DBGsection(2000,(referenceDebugManage(I_GCLOCK,INST_REF,                    \
                                        __FILE__,__LINE__)));                 \
    if (((INST_REF)->state))    {                                        \
        fprintf(stderr,"GC already active (state == %d) on %p(%s)\n",(INST_REF)->state,INST_REF,getInstRefName(INST_REF)); \
        assert(!"GC already active!!!");                                       \
    }                                                                         \
  __asm__ __volatile__ (                                                      \
			"  movl $1, %0       \n"   /* state=1   */            \
			"  cmpl $0, %1       \n"   /* lockCount */            \
			"  jz 0f             \n"                              \
                                                                              \
			"  movl $0, %0       \n"   /* state=0   */            \
			"  movl $0, %2       \n"   /* fail      */            \
			"0:                  \n"   /* success   */            \
                                                                              \
			:                                                     \
			: "m" ((INST_REF)->state),                            \
		          "m" ((INST_REF)->lockCount),                        \
		          "m" gcSoftLockInstRefReturnValue                    \
                        : "memory"                                            \
                       );                                                     \
    if (gcSoftLockInstRefReturnValue)  {                                      \
        if (PEDANT_REF_DEBUG_IREF && (INST_REF) != nullInstRef)    {               \
            assert((INST_REF)->swap);                                                 \
            (INST_REF)->ptr=(INST_REF)->swap;                                         \
            (INST_REF)->swap=0;                                                       \
        }                                                                             \
    }                                                                           \
                                                                              \
 gcSoftLockInstRefReturnValue;                                                \
                                                                              \
})
#else
#define gcSoftLockInstRef( INST_REF )                                         \
    ({                                                                            \
    int gcSoftLockInstRefReturnValue=1;                                          \
    \
    DBGsection(2000,(referenceDebugManage(I_GCLOCK,INST_REF,                    \
    __FILE__,__LINE__)));                 \
    __asm__ __volatile__ (                                                      \
    "  movl $1, %0       \n"   /* state=1   */            \
    "  cmpl $0, %1       \n"   /* lockCount */            \
    "  jz 0f             \n"                              \
    \
    "  movl $0, %0       \n"   /* state=0   */            \
    "  movl $0, %2       \n"   /* fail      */            \
    "0:                  \n"   /* success   */            \
    \
    :                                                     \
    : "m" ((INST_REF)->state),                            \
    "m" ((INST_REF)->lockCount),                        \
    "m" gcSoftLockInstRefReturnValue                    \
    : "memory"                                            \
    );                                                     \
    \
    gcSoftLockInstRefReturnValue;                                                \
    \
    })
#endif


/* void gcUnlockInstRef( InstRef *INST_REF )                    */ /*fold00*/
// if successful then lockCount is set to 0 and state is set to 0
#ifdef PEDANT_REF_DEBUG
#define gcUnlockInstRef( INST_REF )                                           \
({                                                                            \
 do                                                                           \
 {                                                                            \
  DBGsection(2000,(referenceDebugManage(I_GCUNLOCK,INST_REF,                  \
                                        __FILE__,__LINE__)));                 \
    if ((INST_REF)->state!=1)    {                                        \
        fprintf(stderr,"GC state %d != 1 on %p(%s)\n",(INST_REF)->state,INST_REF,getInstRefName(INST_REF)); \
        assert(!"GC state illegal!!!");                                       \
    }                                                                         \
    if (PEDANT_REF_DEBUG_IREF && (INST_REF) != nullInstRef)    {           \
        void *p;                                                         \
        assert(p=malloc((size_t) (INST_REF)->size));                     \
        memcpy(p,(INST_REF)->ptr,(size_t) (INST_REF)->size);             \
        free((INST_REF)->ptr);                                                \
        (INST_REF)->ptr=0;                                                    \
        (INST_REF)->swap=p; /* assignment at last ... to be sure swap is NULL or correct */     \
    }                                                                         \
  __asm__ __volatile__ (                                                      \
                        "decl %0"                                             \
			:                                                     \
			: "m" ((INST_REF)->state)                             \
                        : "memory"                                            \
    );                                                                        \
 }                                                                            \
 while(0);                                                                    \
})
#else
#define gcUnlockInstRef( INST_REF )                                           \
    ({                                                                            \
    do                                                                           \
    {                                                                            \
    DBGsection(2000,(referenceDebugManage(I_GCUNLOCK,INST_REF,                  \
    __FILE__,__LINE__)));                 \
    __asm__ __volatile__ (                                                      \
    "decl %0"                                             \
    :                                                     \
    : "m" ((INST_REF)->state)                             \
    : "memory"                                            \
    );                                                                        \
    }                                                                            \
    while(0);                                                                    \
    })
#endif

/* InstRef *getRefField(InstRef **refPtr)                       */ /*fold00*/
#define getRefField( REF_PTR )                                                \
({                                                                            \
 InstRef *getRefFieldReturnValue;                                             \
 __asm__ __volatile__ (                                                       \
			"  movl %0, %%ebx                     \n"             \
			"  movl %1, %%edx                     \n"             \
			"  movl %2, %%ecx                     \n"             \
                                                                              \
			"  movl $-1, %%eax                    \n"             \
			"0:                                   \n"             \
			"  xchgl (%%ebx), %%eax               \n" /* eax <- refPtr          */\
			"  cmpl $-1, %%eax                    \n" /* was read?              */\
			"  jnz 1f                             \n"             \
                                                                              \
                        "  pusha                              \n"             \
			"   call yield                        \n"             \
                        "  popa                               \n"             \
                                                                              \
			"  jmp 0b                             \n"             \
                                                                              \
			"1:                                   \n"             \
			"  incl "INSTREF_REFCOUNT_STR"(%%eax) \n" /* refCount++             */\
        		"  movl %%eax, (%%ebx)                \n" /* allow access to others */\
                                                                                              \
                        "  movl (%%ecx),  %%ecx               \n" /* orMask -> ECX    GC    */\
                        "  orl %%ecx, "INSTREF_COLOR_STR"(%%eax) \n" /* color  |= ECX    GC */\
                                                                                              \
        		"  movl %%eax, (%%edx)                \n" /* save copy              */\
			:                                                     \
			: "m" (REF_PTR),                          /* 0 */     \
                          "g" (&getRefFieldReturnValue),          /* 1 */     \
                          "m" (&gcOrMask)                         /* 2 */     \
                        : "eax", "ebx", "ecx", "edx", "memory"                \
                       );                                                     \
                                                                              \
 DBGsection(2000,(referenceDebugManage(I_GETFIELD,*REF_PTR,                   \
                                        __FILE__,__LINE__)));                 \
 getRefFieldReturnValue;                                                      \
})



/* void putRefField(InstRef **refPtr, InstRef *ref)             */ /*fold00*/
#define putRefField( REF_PTR, REF )                                           \
({                                                                            \
do                                                                            \
{                                                                             \
 __asm__ __volatile__ (                                                       \
			"  movl %0, %%ebx                     \n"             \
			"  movl %1, %%edx                     \n"             \
			"  movl %2, %%ecx                     \n"             \
                                                                              \
			"  movl $-1, %%eax                    \n"             \
			"0:                                   \n"             \
			"  xchgl (%%ebx), %%eax               \n" /* refPtr -> eax */\
			"  cmpl $-1, %%eax                    \n" /* was read?     */\
                        "  jnz 1f                             \n"             \
                                                                              \
                        "  pusha                              \n"             \
                        "   call yield                        \n"             \
                        "  popa                               \n"             \
                                                                              \
			"  jmp 0b                             \n"             \
                                                                              \
			"1:                                   \n"             \
			"  movl (%%edx), %%eax                \n"             \
                                                                              \
                        "  movl (%%ecx),  %%ecx                  \n" /* orMask -> ECX    GC */\
                        "  orl %%ecx, "INSTREF_COLOR_STR"(%%eax) \n" /* color  |= ECX    GC */\
                                                                              \
			"  movl %%eax, (%%ebx)                \n"             \
			:                                                     \
			: "m" (REF_PTR),                             /* 0 */  \
                          "g" (&(REF)),                              /* 1 */  \
                          "m" (&gcOrMask)                            /* 2 */  \
                        : "eax", "ebx", "ecx", "edx", "memory"                \
                       );                                                     \
                                                                              \
 DBGsection(2000,(referenceDebugManage(I_PUTFIELD,*REF_PTR,                   \
                                        __FILE__,__LINE__)));                 \
}                                                                             \
while(0);                                                                     \
})



/* InstRef *readRefField(InstRef **refPtr)                      */ /*fold00*/
#define readRefField( REF_PTR )                                               \
({                                                                            \
 InstRef *readRefFieldReturnValue;                                            \
                                                                              \
 DBGprintf(2000,"Trying to read InstRef field %p %s %i",REF_PTR,              \
                                               __FILE__,__LINE__);            \
                                                                              \
 __asm__ __volatile__ (                                                       \
			"  movl %0, %%ebx                     \n"             \
			"  movl %1, %%ecx                     \n"             \
                                                                              \
			"  movl $-1, %%eax                    \n"             \
			"0:                                   \n"             \
			"  xchgl (%%ebx), %%eax               \n" /* refPtr -> eax */\
			"  cmpl $-1, %%eax                    \n" /* was read?     */\
                        "  jnz 1f                             \n"             \
                                                                              \
                        "  pusha                              \n"             \
                        "   call yield                        \n"             \
                        "  popa                               \n"             \
                                                                              \
			"  jmp 0b                             \n"             \
                                                                              \
			"1:                                   \n"             \
        		"  movl %%eax, (%%ebx)                \n" /* allow access to others */\
        		"  movl %%eax, (%%ecx)                \n" /* save copy              */\
			:                                                     \
			: "m" (REF_PTR),                                      \
                          "g" (&readRefFieldReturnValue)                      \
                        : "eax", "ebx", "ecx", "memory"                       \
                       );                                                     \
                                                                              \
 DBGsection(2000,(referenceDebugManage(I_GETFIELD,*REF_PTR,                   \
                                        __FILE__,__LINE__)));                 \
 readRefFieldReturnValue;                                                     \
})



/* void printInstRef( InstRef *iR )                             */ /*fold00*/
#define printInstRef( IR )                                                    \
({                                                                            \
do                                                                            \
{                                                                             \
 fprintf(stderr,"\n InstRef %p:"                                              \
                "\n  ptr       %p"                                            \
                "\n  size      %i"                                            \
                "\n  refCount  %i"                                            \
                "\n  lockCount %i"                                            \
                "\n  name      %s"                                            \
                "\n  color     %i ... red 5, orange 6, green 3, blue 7"       \
                "\n  heapRefs  %i"                                            \
                "\n  state     %i"                                            \
                "\n  flags     %i"                                            \
                "\n"                                                          \
                "\n"                                                          \
                ,                                                             \
                (IR),                                                         \
                (IR)->ptr,                                                    \
                (IR)->size,                                                   \
                (IR)->refCount,                                               \
                (IR)->lockCount,                                              \
                (IR)->name,                                                   \
                (IR)->color,                                                  \
                (IR)->heapRefCount,                                           \
                (IR)->state,                                                  \
                (IR)->finalizerFlags                                          \
          );                                                                  \
}                                                                             \
while(0);                                                                     \
})
 /*FOLD00*/


//-----------------------------------------------------------------------------
//                            GARBAGE COLLECTOR
//-----------------------------------------------------------------------------
/*
 * Garbage collector
 */



#define GC_RED    0x5           // bin: 101   dec: 5
#define GC_ORANGE 0x6           // bin: 110   dec: 6
#define GC_GREEN  0x3           // bin: 011   dec: 3

#define GC_MARK   0x4           // bin: 100
                                // is neutral together with RED and ORANGE
                                // but GREEN is turned into BLUE
#define GC_NOMARK 0x0           // bin: 000

#define GC_BLUE   0x7           // bin: 111   dec: 7



void gcRunAtJvmStartup( void ); // run GC daemon by parameters given on
                                // command line
void gcShutdown( void );        // stop GC daemon if running
                                
void gcDaemonize( void );       // summon GC daemon
void gcTerminate( void );       // kill GC daemon

int  gcHuntedInstances( void ); // number of hunted instances
int  gcHuntedBytes( void );     // number of bytes hunted by GC
int  gcIsDaemon( void );        // true if asynchronous GC is running

void gcJavaLangRuntimeGc( void );
void gcJavaLangRuntimeRunFinalization( void );

void gcSyncRun( void );         // synchro GC routine invocation
                                // (caller is blocked till return)


/* void checkInstanceValidation( InstRef *iR) */ /*fold00*/
/*  - block till reference instance is not valid for finalizer trace */
#define checkInstanceValidation( INST_REF )                                   \
({                                                                            \
 do                                                                           \
 {                                                                            \
                                                                              \
  DBGprintf(2000,"Reading InstRef to RD: %p %s %i flags %i",INST_REF,                  \
                                               __FILE__,__LINE__, INST_REF->finalizerFlags);            \
  DBGsection(2000,(printInstRef(INST_REF)));                                  \
                                                                              \
  __asm__ __volatile__ (                                                      \
                        " movl %0, %%ebx                                \n"   \
                                                                              \
                        "0:                                             \n"   \
                        " movl "INSTREF_FLAGS_STR"(%%ebx), %%eax        \n"   \
                        " andl $"FINALIZER_VALID_STR", %%eax            \n"   \
                        " jnz 1f                                        \n"   \
                                                                              \
                        " pusha                                         \n"   \
                        "  call yield                                   \n"   \
                        " popa                                          \n"   \
                        " jmp 0b                                        \n"   \
                                                                              \
                        "1:                                             \n"   \
			:                                                     \
			: "m" (INST_REF)                                      \
                        : "memory"                                            \
                       );                                                     \
 }                                                                            \
 while(0);                                                                    \
})
 /*FOLD00*/
//----------------------------------------------------------------------------
//                             DEBUG RECORDING
//----------------------------------------------------------------------------
/*
 * supported debug actions
 * (
 *   make OR of these codes
 *   e.g. STOP_RECORDING|PRINT_COMPREHENSION|DESTROY_RECORD
 * )
 */
#define  KLINGER               0x0000

#define  START_RECORDING       0x0001 // start recording require
#define  STOP_RECORDING        0x0002 // stop recording require

#define  DESTROY_RECORD        0x0004 // destroy record

#define  PRINT_COMPLETE_RECORD 0x0008 // complete record
#define  PRINT_COMPREHENSION   0x0010 // comprehensive info about ref with specified *name*
#define  PRINT_SINGLE_RECORD   0x0020 // everything about ref with specified *name*
#define  PRINT_CHANGE          0x0040 // changed fields in ref with specified *name* 
#define  PRINT_TRIM_CHANGE     0x0080 // changed fields in ref with specified *name*, (ND) and N()D not printed

#define  START_PRN             0x0100 // start print everything immediatelly
#define  STOP_PRN              0x0200 // stop print everything immediatelly


/*
 * functions for debugging - *FOR HUNTING BUGS USE ONLY THESE TWO FUNCTIONS*
 */

// use this function with named references
#define referenceNamedDebug( ACTION, NAME )                                   \
        ({ if (DBGallowed (2000)) ({referenceNamedDbg( ACTION, NAME );}); })

// this fun is used for noname
#define referenceDebug( ACTION )                                              \
        ({ if (DBGallowed (2000)) ({referenceNamedDbg( ACTION, NULL );}); })


//----------------------------------------------------------------------------
//                             FORBIDDEN ZONE
//----------------------------------------------------------------------------
/*
 * reference actions which are recorded 
 */
#define  I_WAS_BORN              1000
#define  I_REALLOC               1001
#define  I_DELETE                1002
#define  I_ADD_REF               1003
#define  I_RELEASE_REF           1004
#define  I_LOCK                  1005
#define  I_UNLOCK                1006
#define  I_GETFIELD              1007
#define  I_PUTFIELD              1008
#define  I_GCLOCK                1009
#define  I_GCUNLOCK              1010

#define  S_WAS_BORN              1021
#define  S_REALLOC               1022
#define  S_DELETE                1023
#define  S_SET_TYPE              1024
#define  S_ADD_REF               1025
#define  S_RELEASE_REF           1026
#define  S_LOCK                  1027
#define  S_UNLOCK                1028
#define  S_GCLOCK                1029
#define  S_GCUNLOCK              1030

/*
 *                        *DO NOT USE THESE FUNCTIONS*
 * recording (declared here because functions are used inside macros)
 *                        *DO NOT USE THESE FUNCTIONS*
 */
#define referenceDebugManage( ACTION, PTR, FILE, LINE )                       \
         referenceDebugManageCore( ACTION, PTR, 0, FILE, LINE )

void referenceDebugManageCore( int action, void *ptr, int oldSize,
                               char *file, int line
                             );

void referenceNamedDbg( int action, char *name ); 



__END_DECLS

#endif

//- EOF -----------------------------------------------------------------------
