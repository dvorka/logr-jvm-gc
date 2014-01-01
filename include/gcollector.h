/*
 * gcollector.h
 *
 * Author: Dvorka
 *
 * ToDo: 
 */

#ifndef __GCOLLECTOR_H
 #define __GCOLLECTOR_H

 #include "bool.h"
 #include "gcoptions.h"



  //- class GarbageCollector --------------------------------------------------



  class GarbageCollector /*FOLD00*/
  {

   public:

    GarbageCollector();

     #ifdef GC_ENABLE_SYSTEM_HEAP
      void *operator new(size_t s);
      void operator  delete(void *p);
     #endif

     void daemonize();
     void terminate();

     void suspend();            
     void resume();

     void verboseOn();
     void verboseOff();

     void printInfo();               // print GC statistic

     void javaLangRuntimeGcInternal();
     void collectionFinished();
     void javaLangRuntimeRunFinalizationInternal();
     void finalizationFinished();

     bool isDaemon();
     void setPriority();
     void hunted();

     void runSyncCollection();       // makes one collection cycle
                                     // caller is blocked till function
                                     // returns

    ~GarbageCollector();

   public:

    bool            termSignal,
                    suspendSignal,

                    verbose;         // --verbosegc

    pthread_t       *gcDaemon;       // handle to GC daemon thread

    pthread_mutex_t *narcosis,       // mutex used for daemon suspending
                    *shield;         // mutex for critical sections:
                                     //      daemonize()
                                     //      terminate()
                                     //      suspend()
                                     //      resume()
    int             huntedInstances, // number of hunted Java instances
                    huntedBytes,     // number of hunted bytes
                    iterations;      // number of collections done by GC

    pthread_cond_t  *collectionDone; // condition variable which is used
                                     // for java.lang.gc() invoke.
                                     // Each thread which calls this method
                                     // is blocked until one collection
                                     // cycle finishes on this condition
                                     // variable (using wait).
                                     //         When collection is done
                                     // then condition broadcast method is
                                     // called -> threads are unblocked.
    pthread_mutex_t  *collectionDoneMutex;
                                     // mutex used by condition variable

    pthread_cond_t  *finalizationDone;
                                     // condition variable which is used
                                     // for java.lang.runFinalization().
                                     // Each thread which calls this method
                                     // is blocked until finalization phase
                                     // finishes on this condition
                                     // variable (using wait).
                                     //         When collection is done
                                     // then condition broadcast method is
                                     // called -> threads are unblocked.
    pthread_mutex_t  *finalizationDoneMutex;
                                     // mutex used by condition variable
  };                                 
 /*FOLD00*/


#endif

//- EOF -----------------------------------------------------------------------
