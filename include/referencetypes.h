/*
 * referencetypes.h
 *
 * Author: Dvorka
 *
 * Description:
 *      Types of references into Logr heap.
 *
 * ToDo:
 */

#ifndef __REFERENCETYPES_H
 #define __REFERENCETYPES_H

 #include <stdio.h>
 #include <sys/cdefs.h>

 #include "bool.h"
 #include "nativethreads.h"



 __BEGIN_DECLS

 //-----------------------------------------------------------------------------

 // reference validation
 #define REF_NOT_VALID                 -1
 #define REF_VALID                      0

 // flags for reference finalization
 #define FINALIZER_VOID                 0x00000000
 #define FINALIZER_JAVA                 0x00000001
 #define FINALIZER_STRING_INTERN        0x00000002

 #define FINALIZER_VALID                0x00000004   // set when instance
                                                     // becomes valid
 #define FINALIZER_CANDIDATE            0x40000000
 #define FINALIZER_READY_TO_DIE         0x80000000



 #define FINALIZER_VALID_STR            "4"

 //-----------------------------------------------------------------------------

 /*
  * struct StatRef
  *
  * Reference to Fix data or Runtime data in LH
  *
  * !!! This structure MUST be same as begining of structure InstRef !!!
  *       (InstRef & StatRef structures differs in //. fields)
  */
 #define STATREF_VOID                            0
 #define STATREF_FIXED_DATA      	         1
 #define STATREF_RUNTIME_DATA    		 2
 #define STATREF_ARRAY_HASHTABLE    		 3
 #define STATREF_CLASSLOADER_HASHTABLE		 4
 #define STATREF_INTERFACE_METHOD_TABLE          5

 typedef struct _StatRef
 {
    void *ptr;           // position in memory
#ifdef PEDANT_REF_DEBUG
    void *swap;      // where this ref was moved to
#endif
    int  size;           // size of object in LH
    int  refCount;
    int  lockCount;
    int  state;          //  0 ... GC not active
                         //  1 ... GC active
                         // -1 ... reference not used (REF_NOT_VALID)

    char *name;          // deBug name

    int  fieldLock;      // lock for field access (long and double)

    int  finalizerFlags; // flags for reference finalization
    int  color;          // GC color

    int  type;           //. STATREF_*

 } StatRef;



 //-----------------------------------------------------------------------------
 /*
  * struct InstRef
  *
  * Reference to Instance of object in LH
  * Pointer to this structure is Java reference
  *
  * !!! Begining of this structure MUST be same as StatRef !!!
  *    (InstRef & StatRef structures differs in //. fields)
  */
 typedef struct _InstRef
 {
    void *ptr;          // position in memory
#ifdef PEDANT_REF_DEBUG
    void *swap;      // where this ref was moved to
#endif
    int  size;          // size of object in LH
    int  refCount;
    int  lockCount;
    int  state;         //  0 ... GC not active
                        //  1 ... GC active
                        // -1 ... reference not used (REF_NOT_VALID)

    char *name;         // deBug name

    int  fieldLock;     // lock for field access (long and double)

    int  finalizerFlags;// flags for reference finalization
    int  color;         // GC color

    /*
     * next fields are for monitors
     */
    pthread_t       monitorOwner; //. which thread has monitor
    int             monitorCount; //. how many times thread holds the monitor

    pthread_mutex_t syncMutex;    //. mutex
    pthread_cond_t  syncCond;     //. condition var. for implementing monitors



    int  heapRefCount;            //. number of references from heap
                                  //. (used by finalizer)

 } InstRef;



 //-----------------------------------------------------------------------------

 __END_DECLS

#endif

//- EOF -----------------------------------------------------------------------
