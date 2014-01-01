/*
 * gcheap.h
 *
 * Author: Dvorka
 *
 * ToDo: 
 */

#ifndef __GCHEAP_H
 #define __GCHEAP_H

 #include "bool.h"
 #include "gclist.h"
 #include "gcoptions.h"
 #include "reference.h"



 class GcHeap;



 //----------------------------------------------------------------------------

 // for dump functions
 #define GCHEAP_DUMP_2_FILE             0x0001
 #define GCHEAP_DUMP_2_STDERR           0x0002

 //- struct freeListItem ------------------------------------------------------

 class FreeListItem /*fold00*/
 // Free list of chunks from heap.
 {
  public:
   byte fake[13];        // fake chunk header
   // header of freeListItem looks like header of chunk inside heap
   // -> manipulations with free list are faster
   //
   // fake:
   // 0   1   byte tag;
   // 1   4   int  size;
   // 5   4   int  last;                        
   // 9   4   int  next;
   //
   // fake chunk header must be represented by an array because of aligning
   // fields in structures to DW (byte becomes int -> offset of the next
   // field is +4 and is not +1 as should be)

   int shield; // lock which protects linked list against concurrent access

  public:
   // operators cann't be rewritten because I am calling new to array of
   // freeListItems -> constructor is NOT called
   //   void *operator new(size_t s);
   //   void operator  delete(void *p);
   bool get( int &size, byte *&ptr );              // get chunk
   bool searchFor( int &size, byte *&ptr );        // search for chunk
   void listing();
 };
 /*FOLD00*/

 //- class EAsItem ------------------------------------------------------------

 class EAsItem : public ListItem /*FOLD00*/
 // one efemere area
 {
  public:
   byte *EA;          // pointer to EA
   int  size;         // size of EA

  public:
   EAsItem( int size, byte *EA );
    static bool create( int size, GcHeap *heap );       // for systemHeap
    #ifdef GC_ENABLE_SYSTEM_HEAP
     void *operator new(size_t s);
     void operator  delete(void *p);
    #endif
    void dump( int where=GCHEAP_DUMP_2_STDERR, int handle=0 );
    void walker();
    bool check();
    bool isPtr(byte *ptr);
    bool fillFree(byte pattern);
    bool checkFillFree(byte pattern);
   ~EAsItem();
 };
 /*FOLD00*/

 //- class gcHeap -------------------------------------------------------------

 /* gcHeap defines */ /*FOLD00*/

  // offsets in chunk
   // the first tag
   #define GCHEAP_TAG  0
   // the first size == sizeof(char)
   #define GCHEAP_SIZE 1
   // sizeof(char)+sizeof(int) === 1+4
   #define GCHEAP_LAST 5
   // sizeof(char)+sizeof(int)+sizeof(unsigned char *) === 1+4+4
   #define GCHEAP_NEXT 9

  // offset of data in chunk
   // sizeof(char)+sizeof(int) === 1+4 ... where begins data in full chunk
   #define GCHEAP_DATA 5

  // min chunk size allocated on heap (== GCHEAP_FREE_SERVINFO)
   #define GCHEAP_SLACK_LIMIT   18
   #define GCHEAP_SLACK_LIMITS  "18"
   // flags for chunks in heap
   #define GCHEAP_CHUNK_FULL    0xCC
   #define GCHEAP_CHUNK_FREE    0xFF
   // size of service info in chunk
   #define GCHEAP_FREE_SERVINFO 18
   #define GCHEAP_FULL_SERVINFO 6
   // size of service info data prefix in chunk
   #define GCHEAP_FREE_PREFIX   13
   #define GCHEAP_FULL_PREFIX   5
   // size of service info data postfix in chunk
   #define GCHEAP_FREE_POSTFIX  5
   #define GCHEAP_FULL_POSTFIX  1

  // command line arguments
   // size of heap when JVM starts, 1MB by default, 1kB minimum
   #define GCHEAP_MS_MIN       1024
   #define GCHEAP_MS_DEFAULT   1048576
   #define GCHEAP_MS_MAX       100000000

   #define GCHEAP_MS_MINS      "1024"
   #define GCHEAP_MS_DEFAULTS  "1048576"
   #define GCHEAP_MS_MAXS      "100000000"

   // maximum size of JVM heap, 16MB by default, 1kB minimum
   #define GCHEAP_MX_MIN       1024
   #define GCHEAP_MX_DEFAULT   16777216
   #define GCHEAP_MX_MAX       100000000

   #define GCHEAP_MX_MINS      "1024"
   #define GCHEAP_MX_DEFAULTS  "16777216"
   #define GCHEAP_MX_MAXS      "100000000"
 /*FOLD00*/


 class GcHeap /*FOLD00*/
 {
   public:

    friend class EAsItem;

    GcHeap( int ms, int mx );

     #ifdef GC_ENABLE_SYSTEM_HEAP
      void *operator new(size_t s);
      void operator  delete(void *p);
     #endif

     void *alloc( int size );
      void shrink( void *ptr, int newSize );
      void *stretch( void *ptr, int newSize );
     void free( void *ptr );

     void destroy();            // called if instance of heap is *SYSTEM HEAP*

     bool isPtrRaw(void *ptr);  // takes ptr to service info, checks if
                                // pointer points to heap
     bool isPtr(void *ptr);     // takes ptr to data, checks if pointer 
                                // points to heap

     bool isFreeChunkRaw(void *ptr); // takes ptr to service info
     bool isFreeChunk(void *ptr);    // takes ptr to data
     bool isFullChunkRaw(void *ptr); // takes ptr to service info
     bool isFullChunk(void *ptr);    // takes ptr to data

     void dumpChunk(void *p);                   // dumps chunk
     void dump(int where=GCHEAP_DUMP_2_STDERR); // prints to stdout heap dump
                                
     bool check();              // checks heap
     bool checkChunk(void *p);  // checks chunk

     void info();               // print statistic info about heap
     void walker();             // prints info about each chunk in heap
     void listingFL();          // listing content of free list
     int  coreLeft();           // how many bytes is free in heap
     int  totalMem();           // -mx === how many bytes can be allocated

     bool fillFree(byte pattern);      // checks & sets free chunk content to
                                       // specified value pattern
     bool checkFillFree(byte pattern); // checks if each free chunk is filled
                                       // with pattern
     bool checkFillFreeChunk(void *voidPtr,byte pattern); // ... for chunk

    ~GcHeap();

   public:

    List *EAs;                 // lists of efemer areas === heap itself

    int EAsCount;              // number of efemere areas

    // with service info
    int heapSizeSI,            // current size of heap, efemer1+efemer2+...
        allocatedBytesSI;      // nr of bytes allocated in heap SI included

    // without service info
    int // heapSizeLimit,      // heap size limit === -mx JVM cmd line param
        allocatedBytes;        // nr of bytes allocated in heap
        // freeBytes           // max - allocated



    int EAIgnoredBits;         // e.g.: heapSize==30 -> the biggest allocatable
                               // area is in interval <16,31>
                               // 31 ...  11111 -> 5b bits used
                               // -> 32b - 5b = 27 upper bits can be ignored
                               // ( recounted whenever new EA created )
    int lastUsedInterval;      // == EAIgnoredBits+1
    int maxAllocatableChunk;   // maximal size chunk allocatable by bits
                               // -> hash function must be able to offer
                               // correct interval, service info included
    int heapStretchShield;     // heap stretch synchro


    // --- free list fields&funs ---

    void createFreeChunk( int size, byte *ptr ); // deallocation
                                                 // (create new chunk and put
                                                 // it into FL)

    // number of free lists
    #define GCHEAP_NROFFREELISTS 32

    FreeListItem *freeList;



    // --- flags, which are given on JVM command line ---
    int ms;            // size of heap when JVM starts, 1MB by default
                       // 1kB minimum

    int mx;            // maximal size of heap, 16MB by default
                       // 1kB minimum
 };
 /*FOLD00*/


#endif

//- EOF -----------------------------------------------------------------------
