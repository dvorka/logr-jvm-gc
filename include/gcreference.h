/*
 * gcreference.h
 *
 * Author: Martin Dvorak
 *
 * ToDo: 
 */

#ifndef __GCREFERENCE_H
 #define __GCREFERENCE_H

 #include "gclist.h"
 #include "gcoptions.h"



 //- struct StatRefVecsItem --------------------------------------------------



 class StatRefVecsItem : public ListItem /*fold00*/
 // StatRef vector: contains lng statRefs structures in array
 {
  public:

   StatRefVecsItem( int lng );
    #ifdef GC_ENABLE_SYSTEM_HEAP
     void *operator new(size_t s);
     void operator  delete(void *p);
    #endif
   ~StatRefVecsItem();

  public:

   StatRef *vector;
   int     lng;
 };
 /*FOLD00*/


 //- struct InstRefVecsItem --------------------------------------------------



 class InstRefVecsItem : public ListItem /*fold00*/
 // InstRef vector
 {
  public:

   InstRefVecsItem( int lng );
    #ifdef GC_ENABLE_SYSTEM_HEAP
     void *operator new(size_t s);
     void operator  delete(void *p);
    #endif
   ~InstRefVecsItem();

  public:

   InstRef *vector;
   int     lng;
 };
 /*FOLD00*/


 //- class ReferenceVectors --------------------------------------------------



 class ReferenceVectors /*FOLD00*/
 // This class manages work with references. Contains both references to
 // to StatRefs and InstRefs.
 {
  public:

    ReferenceVectors();
     #ifdef GC_ENABLE_SYSTEM_HEAP
      void   *operator new(size_t s);
      void   operator  delete(void *p);
     #endif
     StatRef *getStatRef();                     // get new reference
     InstRef *getInstRef();
     void    putStatRef( StatRef *sr );         // release reference
     void    putInstRef( InstRef *ir );
     void    iWalker();                         // prints InstRef references
     void    iWalker( int color );              // prints InstRef references
                                                // by color
     void    sWalker();                         // prints StatRef references
     void    sWalker( int color );              // prints StatRef references
                                                // by color
     void    walker();                          // prints InstRef&StatRef
    ~ReferenceVectors();

  public:

    #define GCREFVECS_START_SIZE 1000
    int  sVectorSize,                           // size of the oldest vectors
         iVectorSize;

    #define GCREFVECS_INCREMENT 100
    int  sVectorIncrement, // how long vector add when old overflows
         iVectorIncrement;

    List    *statRefVecs,  // lists of vectors
            *instRefVecs;
    StatRef *sFL;          // Free list contains linked list of unused refs,
    InstRef *iFL;          // item of list is statRef/instRef structure
                           // from vector. Items are linked through Ref.ptr

    int     *sShield,      // synchro lock for StatRef free list
            *iShield;      // synchro lock for InstRef free list
                           // ... because free list can be accessed only
                           // by one thread at the time

    void clean();          // Optimizes usage of vectors and defragments
                           // them. After optim. linked list grows -> so
                           // alloc of new reference is done from the begin
                           // of the  vector -> if you are lucky later
                           // next vector can ve completely freed
                           // so whole vector can be deleted

    int color;             // color which should be set in newly created
                           // reference (used  by garbage collector and
                           // finalizer):
                           //
                           //        RUNLEVEL       color
                           //       ----------------------
                           //           0           GREEN
                           //           1           GREEN
                           //           2           BLUE
                           //           3           RED
                           //           iv          RED
                           //
                           // color is set directly by member access
 };
 /*FOLD00*/

#endif
 /*FOLD00*/


//- EOF -----------------------------------------------------------------------
