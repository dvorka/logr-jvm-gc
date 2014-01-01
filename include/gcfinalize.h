/*
 * gcfinalize.h
 *
 * Author: Dvorka
 *
 * ToDo: 
 */

#ifndef __GCFINALIZE_H
 #define __GCFINALIZE_H

 #include "exception.h"
 #include "gcoptions.h"
 #include "referencetypes.h"



 void *finalizerBody( void *attr );



 // action definitions
  // unset recursively FINALIZE_CANDIDATE flag
  #define GC_TRACER_FINALIZER_CANDIDATE         0x00000001
  // nonrecursive heapRefCount count
  #define GC_TRACER_HEAP_REF_COUNT              0x00000002
  // call Java finalizers recursively
  #define GC_TRACER_FINALIZE_RECURSIVELY        0x00000003

 void universalInstanceTracer(
                               InstRef *iR,
                               int action,
                               RtExceptionInfo *excInfo=NULL
                             );

 void runSyncFinalize();


#endif

//- EOF -----------------------------------------------------------------------
