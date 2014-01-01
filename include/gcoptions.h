/*
 * gcoptions.h
 *
 * Author: Dvorka
 *
 * Description:
 *      This file contains directives for heap and GC configuration
 */

#ifndef __GCOPTIONS_H
 #define __GCOPTIONS_H



 #include "threads.h"   // contains SINGLE_THREAD directive



 #ifndef DEBUGVERSION

    #define GC_RELEASE
                        // If defined then release version of sources
                        // is created -> deBug and testing code is disabled
 #endif



 #define GC_NO_GC
                        // Forbids both sync and async garbage collection


 #ifdef GC_RELEASE

        #define GC_INTERN_RELEASE
                        // Enables object release from internal JVM structures
                        // during finalization

        #define GC_FINALIZER_CALL
                        // If disabled then Java finalizers are not called

        #ifndef GC_NO_GC

                #define GC_ENABLE_GC
                        // If defined then garbage collector *daemon* is
                        // enabled
        #endif

 #else

        #ifndef GC_NO_GC

           #define GC_ENABLE_GC
                        // If defined then garbage collector *daemon* is
                        // enabled

           #ifndef GC_ENABLE_GC

             #define GC_SYNC_GC
                        // If defined then GC can be run using GC->syncRun()
                        // (no thread created) and function is blocked until
                        // one collection finishes
           #endif

        #endif

        //#define GC_INTERN_RELEASE
                        // Enables object release from internal JVM structures
                        // during finalization

        //#define GC_FINALIZER_CALL
                        // If disabled then Java finalizers are not called
 #endif



 #define GC_FREE_SPACE_DEFRAG
                        // If defined then heap free space defragmentation
                        // is enabled.



 #define GC_ENABLE_SYSTEM_HEAP
                        // If defined then JVM uses own heap: systemHeap,
                        // else code for it is compiled out.



// #define GC_CHECK_HEAP_INTEGRITY
                        // This directive is usable when hunting bugs.
                        // If defined then before each heap 
                        // function such us gcHeap::alloc() is checked
                        // heap integrity.



 #define GC_PURE_MACRO_REFERENCE
                        // If defined then each function declared in
                        // reference.h is implemented as macro. Else it
                        // is fair function.
                        // It is usable for debug (it always speeds up
                        // execution when debuglog is off).



 #define GC_CHECK_INSTANCE_VALIDATION
                        // If defined then instance validation flag is checked
                        // during garbage collection


 //#define GC_FAKE_COLLECTION
                        // Make fake collection: do colorization, but
                        // do *NOT* free and finalize anything

    #ifdef GC_FAKE_COLLECTION

        #undef GC_INTERN_RELEASE
                        // Enables object release from internal JVM structures
                        // during finalization

        #undef GC_FINALIZER_CALL
                        // If disabled then Java finalizers are not called
    #endif



#endif

//- EOF -----------------------------------------------------------------------
