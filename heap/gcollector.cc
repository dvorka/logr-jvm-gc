/*
 * gcollector.cc
 *
 * Author: Dvorka
 *
 * ToDo: 
 * - z funkci terminate a suspend/resume udelat kriticke sekce
 */

#include "gcollector.h"

#include <pthread.h>
#include <stdlib.h>
#include <sys/cdefs.h>
#include <unistd.h>

#include "debuglog.h"
#include "gc.h"
#include "gcheap.h"
#include "gcreference.h"
#include "gcsupport.h"
#include "reference.h"



// JVM inner heap
#ifdef GC_ENABLE_SYSTEM_HEAP
 extern GcHeap           *systemHeap;
 extern bool             systemHeapEnabled;
#endif

 extern GcHeap           *logrHeap;



//- class GarbageCollector methods --------------------------------------------



GarbageCollector::GarbageCollector() /*fold00*/
{
 if( (narcosis=new pthread_mutex_t)==NULL )
  WORD_OF_DEATH("cann't allocate mutex");
 if( pthread_mutex_init( narcosis, NULL ) )
  WORD_OF_DEATH("cann't create mutex");

 if( (shield=new pthread_mutex_t)==NULL )
  WORD_OF_DEATH("cann't allocate mutex");
 if( pthread_mutex_init( shield, NULL ) )
  WORD_OF_DEATH("cann't create mutex");

 gcDaemon           = NULL;
 termSignal         = FALSE;
 suspendSignal      = FALSE;

 huntedInstances    = 0;
 huntedBytes        = 0;
 iterations         = 0;

 if( (collectionDoneMutex=new pthread_mutex_t)==NULL )
  WORD_OF_DEATH("cann't allocate mutex");
 if( pthread_mutex_init( collectionDoneMutex, NULL ) )
  WORD_OF_DEATH("cann't create mutex");
 // lock it to be usable
 pthread_mutex_lock(collectionDoneMutex);
 if( (collectionDone=new pthread_cond_t)==NULL )
  WORD_OF_DEATH("cann't allocate condition");
 if( pthread_cond_init( collectionDone, NULL ) )
  WORD_OF_DEATH("cann't create condition");

 if( (finalizationDoneMutex=new pthread_mutex_t)==NULL )
  WORD_OF_DEATH("cann't allocate mutex");
 if( pthread_mutex_init( finalizationDoneMutex, NULL ) )
  WORD_OF_DEATH("cann't create mutex");
 // lock it to be usable
 pthread_mutex_lock(finalizationDoneMutex);
 if( (finalizationDone=new pthread_cond_t)==NULL )
  WORD_OF_DEATH("cann't allocate condition");
 if( pthread_cond_init( finalizationDone, NULL ) )
  WORD_OF_DEATH("cann't create condition");
}

//----------------------------------------------------------------------------

#ifdef GC_ENABLE_SYSTEM_HEAP

void *GarbageCollector::operator new(size_t s) /*fold00*/
{
 if( systemHeapEnabled )
  return systemHeap->alloc(s);
 else
  return ::new byte[s];
}

//----------------------------------------------------------------------------

void GarbageCollector::operator delete(void *p) /*fold00*/
{
 if( systemHeapEnabled )
  systemHeap->free(p);
 else
  ::delete p;
}

#endif

//----------------------------------------------------------------------------

void GarbageCollector::daemonize() /*fold00*/
{
 LOCK_MUTEX(shield);

 if( gcDaemon )
 {
  DBGprintf(DBG_GC,"ERROR: garbage collector daemon already running...");
 }
 else
 {
  if( (gcDaemon=new pthread_t)==NULL )
   WORD_OF_DEATH("cann't allocate garbage collector thread...")

  termSignal   = FALSE;
  suspendSignal= FALSE;

  if( pthread_create( gcDaemon,
                      NULL,             // default attributes
                      gcBody,           // thread routine
                      this              // arguments === instance of GC
                    )
    )
   WORD_OF_DEATH("cann't run garbage collector thread...")

  DBGprintf(DBG_GC,"... garbage collector daemon is now running...");
 }

 UNLOCK_MUTEX(shield);
}

//----------------------------------------------------------------------------

void GarbageCollector::terminate() /*fold00*/
{
 LOCK_MUTEX(shield);

 if( gcDaemon )
 {
  if( suspendSignal )
  {
   // mutex up -> gc wakes up
   suspendSignal=FALSE;
   UNLOCK_MUTEX(narcosis);
  }

  // stop thread and delete it
  termSignal=TRUE;

  DBGprintf(2000,"Stand-by while garbage daemon terminates...");

  if( pthread_join( *gcDaemon, NULL ) )
   WORD_OF_DEATH("cann't join garbage daemon...")

  // unblock threads waiting for collection finish
  pthread_cond_broadcast(collectionDone);

  delete gcDaemon;

  gcDaemon=NULL;
 }


 UNLOCK_MUTEX(shield);
}

//----------------------------------------------------------------------------

void GarbageCollector::suspend() /*fold00*/
{
 LOCK_MUTEX(shield);

 if( !suspendSignal )
 {
  // if possible, mutex is locked, if it is already locked, this process
  // won't block 
  pthread_mutex_trylock(narcosis);

  suspendSignal=TRUE;
 }
 // else daemon already suspended

 UNLOCK_MUTEX(shield);
}

//----------------------------------------------------------------------------

void GarbageCollector::resume() /*fold00*/
{
 LOCK_MUTEX(shield);

  // mutex up -> gc wakes up
  UNLOCK_MUTEX(narcosis);

 UNLOCK_MUTEX(shield);
}

//----------------------------------------------------------------------------

void GarbageCollector::verboseOn() /*fold00*/
{
 verbose=TRUE;
}

//----------------------------------------------------------------------------

void GarbageCollector::verboseOff() /*fold00*/
{
 verbose=FALSE;
}

//----------------------------------------------------------------------------

void GarbageCollector::printInfo() /*fold00*/
// print GC statistic
{
 float free = ((float)(logrHeap->mx-logrHeap->allocatedBytes))
               /
              (((float)(logrHeap->mx))/100.0);

 fprintf(stdout,"\n < GC: recycled %i objects of %i bytes in %i collection(s)"
                "\n     --> heap %i/%i/(%i), free %i\% >",
                huntedInstances,
                huntedBytes,
                iterations,
                logrHeap->allocatedBytes,
                logrHeap->mx - logrHeap->allocatedBytes,
                logrHeap->heapSizeSI,
                (int)free
        );
}

//----------------------------------------------------------------------------

void GarbageCollector::javaLangRuntimeGcInternal() /*fold00*/
// Caller of this method awaits maximal effort of system toward recycling
// unused objects in memory.
//      i) If GC is running then:
// Each thread which calls this method is blocked until one collection
// cycle finishes on condition variable collectionDone using wait().
//         When collection is done then condition broadcast() is
// called by garbage collector daemon -> threads are unblocked.
//      i) If GC is NOT running then:
// GC thread is launched and thread waits for collection finish the same way
// as described above.
{

 if( !gcDaemon )
 {
  // GC daemon not exists -> launch it
  daemonize();

  DBGprintf(DBG_GC,"Asleep ... java.lang.Runtime.gc()");

  // wait on condition for collection cycle end
  pthread_cond_wait(collectionDone,collectionDoneMutex);
 }
 else
 {
  DBGprintf(DBG_GC,"Asleep ... java.lang.Runtime.gc()");

  // wait on condition for collection cycle end
  pthread_cond_wait(collectionDone,collectionDoneMutex);
 }
}

//----------------------------------------------------------------------------

void GarbageCollector::collectionFinished() /*fold00*/
// - this function is called by GC daemon after it does one collection cycle
{
 DBGprintf(DBG_GC,"Wake up ... java.lang.Runtime.gc()");
 pthread_cond_broadcast(collectionDone);
}

//----------------------------------------------------------------------------

void GarbageCollector::javaLangRuntimeRunFinalizationInternal() /*fold00*/
// Caller of this method awaits maximal effort of system toward 
// calling object finalizers.
//      i) If GC is running then:
// Each thread which calls this method is blocked until finalization
// phase of instance collection finishes on condition variable using wait().
//         When phase is done then condition broadcast() is
// called by garbage collector daemon -> threads are unblocked.
//      i) If GC is NOT running then:
// GC thread is launched and thread waits for finalization phase finish 
// the same way as described above.
{

 if( !gcDaemon )
 {
  // GC daemon not exists -> launch it
  daemonize();

  DBGprintf(DBG_GC,"Asleep ... java.lang.Runtime.runFinalization()");

  // wait on condition for finalization phase end
  pthread_cond_wait(finalizationDone,finalizationDoneMutex);
 }
 else
 {
  DBGprintf(DBG_GC,"Asleep ... java.lang.Runtime.runFinalization()");

  // wait on condition for finalization phase end
  pthread_cond_wait(finalizationDone,finalizationDoneMutex);
 }
}

//----------------------------------------------------------------------------

void GarbageCollector::finalizationFinished() /*fold00*/
// - this function is called by GC daemon after it makes RUNLEVEL iv
//   (finalization phase)
{
 DBGprintf(DBG_GC,"Wake up ... java.lang.Runtime.runFinalization()");
 pthread_cond_broadcast(finalizationDone);
}

//----------------------------------------------------------------------------

bool GarbageCollector::isDaemon() /*fold00*/
{
 if( gcDaemon )
  return TRUE;
 else
  return FALSE;
}

//----------------------------------------------------------------------------

void GarbageCollector::setPriority() /*fold00*/
{
}

//----------------------------------------------------------------------------

void GarbageCollector::hunted() /*fold00*/
{
}

//----------------------------------------------------------------------------

void GarbageCollector::runSyncCollection() /*fold00*/
// - makes one collection cycle
{
 DBGprintf(DBG_GC,"<- GC ------------------------------------------------------------->");
 DBGprintf(DBG_GC," Starting 1 *sync* GC collection");
 DBGprintf(DBG_GC,"<- GC ------------------------------------------------------------->");



 instanceCollection();         // colorize reachable references
                               // and finalize everything what's possible


 DBGprintf(DBG_GC,"<- GC ------------------------------------>");
 DBGprintf(DBG_GC," 1 *sync* GC collection finished...");
 DBGprintf(DBG_GC,"<- GC ------------------------------------>");
}

//----------------------------------------------------------------------------

GarbageCollector::~GarbageCollector() /*fold00*/
{
 terminate();

 pthread_mutex_destroy( narcosis );
 delete narcosis;
 pthread_cond_destroy( collectionDone );
 delete collectionDone;
}

//----------------------------------------------------------------------------

 /*FOLD00*/


//- EOF -----------------------------------------------------------------------
