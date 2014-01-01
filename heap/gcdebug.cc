/*
 * gcdebug.cc
 *
 * Author: Dvorka
 *
 * Description:
 * - this file contains reference debug facilities and is included 
 *   into reference.cc
 *
 * ToDo: 
 */

//-----------------------------------------------------------------------------
//                             DEBUG RECORDING
//-----------------------------------------------------------------------------

List *recordList;     // linked list for debug recording
bool recording=FALSE; // record flag
bool printing=FALSE;  // print flag

//-----------------------------------------------------------------------------
//                                 RECORD
//-----------------------------------------------------------------------------



struct RecordItem : public ListItem /*fold00*/
{
 int        action;
 void       *ptr;

 int        size,            // these fields are used for different infos
            type;

 int        refCount,
            lockCount;
 bool       done;            // used in print change, if true -> item was 
                             // worked out
 char       *name;

 RecordItem( int action_, void *ptr_);
  #ifdef GC_ENABLE_SYSTEM_HEAP
   void *operator new(size_t s);
   void operator  delete(void *p);
  #endif
 virtual ~RecordItem();
};

RecordItem::RecordItem( int action_, void *ptr_) /*fold00*/
{
 action = action_;
 ptr    = ptr_;
 name   = NULL;
}

#ifdef GC_ENABLE_SYSTEM_HEAP

void *RecordItem::operator new(size_t s) /*fold00*/
{
 if( systemHeapEnabled )
  return systemHeap->alloc(s);
 else
  return ::new byte[s];
}

void RecordItem::operator delete(void *p) /*fold00*/
{
 if( systemHeapEnabled )
  systemHeap->free(p);
 else
  ::delete p;
}

#endif

RecordItem::~RecordItem() /*fold00*/
{
 if( name ) free(name);
}
 /*FOLD00*/


//-----------------------------------------------------------------------------
//                                CORE FUNS
//-----------------------------------------------------------------------------

RecordItem *printChangeGetHead() /*fold00*/
// - returns the first item of the list which has done==FALSE
//   if not exists returns NULL
{
 ListItem *item=recordList->GetHead();

 while( item )
 {
  //DBGprintf(DBG_REF1,"--> GetHead %p",item);

  if( ((RecordItem *)item)->done==FALSE )
  {
   //DBGprintf(DBG_REF1,"--> %p",item);
   return (RecordItem *)item;
  }

  item=recordList->Next(item);
 } // while

 return NULL;
}

//-----------------------------------------------------------------------------

void printChangeUnDone() /*fold00*/
// - sets in each RecordItem done to FALSE
{

 ListItem *item=recordList->GetHead();

 while( item )
 {
  ((RecordItem *)item)->done=FALSE;

  item=recordList->Next(item);
 } // while

}

//-----------------------------------------------------------------------------

RecordItem *printChangeNext( ListItem *item ) /*fold00*/
// - returns the first item behind item which has done==FALSE
//   if not exists returns NULL
{
 // first try system next
 item=recordList->Next(item);

 while( item )
 {
  if( ((RecordItem *)item)->done==FALSE )
   return (RecordItem *)item;

  item=recordList->Next(item);
 } // while

 return NULL;
}

//-----------------------------------------------------------------------------

bool sameNames( char *name1, char *name2 ) /*fold00*/
{
 if( !name1 && !name2 ) return TRUE;
 if( !name1 || !name2 ) return FALSE;
 if( !strcmp(name1,name2) ) return TRUE;
 return FALSE;
}

//-----------------------------------------------------------------------------

void referenceNamedDbg( int action, char *name ) /*fold00*/
// - name is used only together with PRINT_COMPREHENSION
//   -> comprehensive informations about named reference are printed
// - PRINT_SINGLE
//   -> all informations about named reference are printed
{
 // START_RECORDING
 if( action & START_RECORDING ) /*fold01*/
 {
  DBGprintf(2000,"--> Begin of recording...");
  recording=TRUE;
 }

 // STOP_RECORDING
 if( (action & STOP_RECORDING)) /*fold01*/
 //if( (action & STOP_RECORDING) || (action & DESTROY_RECORD))
 {
  DBGprintf(2000,"--> End of recording...");
  recording=FALSE;
 }

 // START_PRN
 if( action & START_PRN ) /*fold01*/
 {
  DBGprintf(2000,"--> Begin of printing...");
  printing=TRUE;
 }

 // STOP_PRN
 if( action & STOP_PRN ) /*fold01*/
 {
  DBGprintf(2000,"--> End of printing...");
  printing=FALSE;
 }

 // PRINT_COMPLETE_RECORD
 if( (action & PRINT_COMPLETE_RECORD) || (action & PRINT_SINGLE_RECORD) ) /*fold01*/
  {
   DBGprintf(2000,"");

   if( action&PRINT_COMPLETE_RECORD )
    DBGprintf(2000,"Complete record:");
   else
    DBGprintf(2000,"Single record for %s:",name);

   ListItem *item=recordList->GetHead();

   while( item )
   {

    if( (action&PRINT_SINGLE_RECORD) && sameNames(((RecordItem *)item)->name,name)
        ||
        (action&PRINT_COMPLETE_RECORD)
      )
    {

     switch(((RecordItem *)item)->action)
     {
      // InstRef
      case I_WAS_BORN:
       DBGprintf(2000," %p -> newInstRef, size %i, name %s",
              ((RecordItem *)item)->ptr,
              ((RecordItem *)item)->size,
              ((RecordItem *)item)->name
             );
      break;
      case I_REALLOC:
       DBGprintf(2000," %p -> reallocInstRef, size %i -> %i, name %s",
              ((RecordItem *)item)->ptr,
              ((RecordItem *)item)->size,
              ((RecordItem *)item)->type,
              ((RecordItem *)item)->name
             );
      break;
      case I_DELETE:
       DBGprintf(2000," %p -> deleteInstRef, refCount %i, lockCount %i, name %s ",
              ((RecordItem *)item)->ptr,
              ((RecordItem *)item)->size,
              ((RecordItem *)item)->type,
              ((RecordItem *)item)->name
             );
      break;
      case I_LOCK:
       DBGprintf(2000," %p -> lockInstRef, lockCount %i -> %i, name %s ",
              ((RecordItem *)item)->ptr,
              ((RecordItem *)item)->size,
              ((RecordItem *)item)->size+1,
              ((RecordItem *)item)->name
            );
      break;
      case I_UNLOCK:
       DBGprintf(2000," %p -> unlockInstRef, lockCount %i -> %i, name %s ",
              ((RecordItem *)item)->ptr,
              ((RecordItem *)item)->size,
              ((RecordItem *)item)->size-1,
              ((RecordItem *)item)->name
            );
      break;
      case I_GETFIELD:
       DBGprintf(2000," %p -> getInstRefField, name %s ",
              ((RecordItem *)item)->ptr,
              ((RecordItem *)item)->name
            );
      break;
      case I_PUTFIELD:
       DBGprintf(2000," %p -> putInstRefField, name %s ",
              ((RecordItem *)item)->ptr,
              ((RecordItem *)item)->name
            );
      break;
      case I_GCLOCK:
       DBGprintf(2000," %p -> gcLockInstRef, state %i -> %i, name %s ",
              ((RecordItem *)item)->ptr,
              ((RecordItem *)item)->size,
              ((RecordItem *)item)->size+1,
              ((RecordItem *)item)->name
            );
      break;
      case I_GCUNLOCK:
       DBGprintf(2000," %p -> gcUnlockInstRef, state %i -> %i, name %s ",
              ((RecordItem *)item)->ptr,
              ((RecordItem *)item)->size,
              ((RecordItem *)item)->size-1,
              ((RecordItem *)item)->name
            );
      break;
      case I_ADD_REF:
       DBGprintf(2000," %p -> addInstRefCount, refCount %i -> %i, name %s ",
              ((RecordItem *)item)->ptr,
              ((RecordItem *)item)->size,
              ((RecordItem *)item)->size+1,
              ((RecordItem *)item)->name
            );
      break;
      case I_RELEASE_REF:
       DBGprintf(2000," %p -> releaseInstRefCount, refCount %i -> %i, name %s ",
              ((RecordItem *)item)->ptr,
              ((RecordItem *)item)->size,
              ((RecordItem *)item)->size-1,
              ((RecordItem *)item)->name
             );
      break;

      // StatRef
      case S_WAS_BORN:
       DBGprintf(2000," %p -> newStatRef, size %i, type %i, name %s ",
              ((RecordItem *)item)->ptr,
              ((RecordItem *)item)->size,
              ((RecordItem *)item)->type,
              ((RecordItem *)item)->name
            );
      break;
      case S_REALLOC:
       DBGprintf(2000," %p -> reallocStatRef, size %i -> %i, name %s ",
              ((RecordItem *)item)->ptr,
              ((RecordItem *)item)->size,
              ((RecordItem *)item)->type,
              ((RecordItem *)item)->name
             );
      break;
      case S_DELETE:
       DBGprintf(2000," %p -> deleteStatRef, refCount %i, lockCount %i, name %s ",
              ((RecordItem *)item)->ptr,
              ((RecordItem *)item)->size,
              ((RecordItem *)item)->type,
              ((RecordItem *)item)->name
             );
      break;
      case S_LOCK:
       DBGprintf(2000," %p -> lockStatRef, lockCount %i -> %i, name %s ",
              ((RecordItem *)item)->ptr,
              ((RecordItem *)item)->size,
              ((RecordItem *)item)->size+1,
              ((RecordItem *)item)->name
             );
      break;
      case S_UNLOCK:
       DBGprintf(2000," %p -> unlockStatRef, lockCount %i -> %i, name %s ",
              ((RecordItem *)item)->ptr,
              ((RecordItem *)item)->size,
              ((RecordItem *)item)->size-1,
              ((RecordItem *)item)->name
             );
      break;
      case S_GCLOCK:
       DBGprintf(2000," %p -> gcLockStatRef, state %i -> %i, name %s ",
              ((RecordItem *)item)->ptr,
              ((RecordItem *)item)->size,
              ((RecordItem *)item)->size+1,
              ((RecordItem *)item)->name
            );
      break;
      case S_GCUNLOCK:
       DBGprintf(2000," %p -> gcUnlockStatRef, state %i -> %i, name %s ",
              ((RecordItem *)item)->ptr,
              ((RecordItem *)item)->size,
              ((RecordItem *)item)->size-1,
              ((RecordItem *)item)->name
             );
      break;
      case S_ADD_REF:
       DBGprintf(2000," %p -> addStaRefCount, refCount %i -> %i, name %s",
              ((RecordItem *)item)->ptr,
              ((RecordItem *)item)->size,
              ((RecordItem *)item)->size+1,
              ((RecordItem *)item)->name
             );
      break;
      case S_RELEASE_REF:
       DBGprintf(2000," %p -> releaseStatRefCount, refCount %i -> %i, name %s",
              ((RecordItem *)item)->ptr,
              ((RecordItem *)item)->size,
              ((RecordItem *)item)->size-1,
              ((RecordItem *)item)->name
             );
      break;
      case S_SET_TYPE:
       DBGprintf(2000," %p -> setStatRefType, type: %i -> %i, name %s ",
              ((RecordItem *)item)->ptr,
              ((RecordItem *)item)->size,
              ((RecordItem *)item)->type,
              ((RecordItem *)item)->name
             );
      break;

      default:
       DBGprintf(2000,"\n Error: reported from %s line %d:\n UFO managing operation...", __FILE__, __LINE__ );
     } // case
    } // if
    item=recordList->Next(item);
   } // while
  } // if

 if( action & PRINT_COMPREHENSION ) /*fold01*/
  {
   int reallocations=0,
       setTypes     =0,
       locks        =0,
       unLocks      =0,
       gcLocks      =0,
       gcUnlocks    =0,
       getFields    =0,
       putFields    =0,
       addRefs      =0,
       releaseRefs  =0,

       sizeIs=0, typeIs=0, refCountIs=0;

   bool wasDeleted=FALSE;

   // get informations
   ListItem *item=recordList->GetHead();

   while( item )
   {

    if( sameNames(((RecordItem *)item)->name,name) )
    {

     switch(((RecordItem *)item)->action)
     {

      case I_WAS_BORN:
      case S_WAS_BORN:
       sizeIs=((RecordItem *)item)->size;
      break;
      case I_REALLOC:
      case S_REALLOC:
       sizeIs=((RecordItem *)item)->type;
       reallocations++;
      break;
      case I_DELETE:
      case S_DELETE:
       wasDeleted=TRUE;
      break;
      case I_LOCK:
      case S_LOCK:
       locks++;
      break;
      case I_UNLOCK:
      case S_UNLOCK:
       unLocks++;
      break;
      case I_GETFIELD:
       getFields++;
      break;
      case I_PUTFIELD:
       putFields++;
      break;
      case I_GCLOCK:
      case S_GCLOCK:
       gcLocks++;
      break;
      case I_GCUNLOCK:
      case S_GCUNLOCK:
       gcUnlocks++;
      break;
      case I_ADD_REF:
      case S_ADD_REF:
       refCountIs=((RecordItem *)item)->size;
       addRefs++;
      break;
      case S_RELEASE_REF:
      case I_RELEASE_REF:
       refCountIs=((RecordItem *)item)->size;
       releaseRefs++;
      break;
      case S_SET_TYPE:
       setTypes++;
       typeIs=((RecordItem *)item)->type;
      break;

      default: ;
     }
    }

    item=recordList->Next(item);
   }

   DBGprintf(2000,"\n"
          "\n Comprehensive info about %s:"
          "\n     *ACTION*      #"
          "\n ---------------------"
          "\n  - lock         : %d"
          "\n  - unlock       : %d"
          "\n  - add ref      : %d"
          "\n  - release ref  : %d"
          "\n  - refCount is  : %d"
          "\n  - reallocation : %d"
          "\n  - size is      : %d"
          "\n  - set type     : %d"
          "\n  - type is      : %d"
          "\n  - gclock       : %d"
          "\n  - gcunlock     : %d"
          "\n  - getfield     : %d"
          "\n  - putfield     : %d"
          "\n  - was deleted  : ",
          name,
          locks,
          unLocks,
          addRefs,
          releaseRefs,
          refCountIs,
          reallocations,
          sizeIs,
          setTypes,
          typeIs,
          gcLocks,
          gcUnlocks,
          getFields,
          putFields
         );
    if( wasDeleted ) DBGprintf(2000,"yes"); else DBGprintf(2000,"no");
  }

 if( (action & PRINT_CHANGE) || (action & PRINT_TRIM_CHANGE) ) /*fold01*/
 {
  ListItem *item;
  // ListItem *oldItem;
  char     buf[160], // buffer for printing
           huf[80];  // helping buffer
  void     *ptr;
  char     *name=NULL;

  bool     born         =FALSE,
           change       =FALSE,
           refChanged   =FALSE,
           lockChanged  =FALSE,
           lockStartInit=FALSE,
           refStartInit =FALSE;

  int      lockStart =0,
           lockChange=0,
           lockNew   =0,
           lockDelete=0,
           lockNow   =0,

           refStart  =0,
           refChange =0,
           refNew    =0,
           refDelete =0,
           refNow    =0,

           isInstRef =0;        // 0 - not set, 1 - InstRef, 2 - StatRef


  // clear done flags
  printChangeUnDone();

  DBGprintf(2000,"");

  if( action&PRINT_CHANGE )
   DBGprintf(2000,"Change record:");
  else
   DBGprintf(2000,"Trim change record:");

  DBGprintf(2000,"            <<-------- ref ----->> <<------- lock ------>>  ");
  DBGprintf(2000,"                    C      D      |         C      D        ");
  DBGprintf(2000,"               S    h      e      |    S    h      e        ");
  DBGprintf(2000,"               t    a      l      |    t    a      l       N");
  DBGprintf(2000,"    * p *      a    n N    e    N |    a    n N    e    N  a");
  DBGprintf(2000,"    * t *      r    g e    t    o |    r    g e    t    o  m");
  DBGprintf(2000,"    * r *      t    e w    e    w |    t    e w    e    w  e");
  DBGprintf(2000,"<---------------------------------+------------------------->");

  //OLD while( !recordList->Empty() )
  while( printChangeGetHead() )
  {
    // delete name of old reference
    if( name )
    {
     free(name);
     name=NULL;
    }

   // select new reference
   // OLD item=recordList->GetHead();
   item=printChangeGetHead();

   // if some item exists
   if(item)
   {
    ptr =((RecordItem *)item)->ptr;

    // init name
    if( ((RecordItem *)item)->name )
      name = (char *)strdup(((RecordItem *)item)->name);
     else
      name = NULL;

    // init variables
    refStart =refChange =refNew =refDelete =refNow =
    lockStart=lockChange=lockNew=lockDelete=lockNow=0;
    refChanged=lockChanged=change=born=lockStartInit=refStartInit=FALSE;
   }
   else
    break;



   // go through record and search for the records of this reference
   while( item )
   {
    if( ( ptr == ((RecordItem *)item)->ptr )
        &&
        ( sameNames(((RecordItem *)item)->name,name) )
      )
    {

     isInstRef=0; // not set

     switch(((RecordItem *)item)->action)
     {
      case I_WAS_BORN:
          isInstRef=1;          // it is InstRef
      case S_WAS_BORN:
          if( !isInstRef )      // if it is not set
           isInstRef=2; 

          born=TRUE;
          change=TRUE;
          refStart    =lockStart    =0;
          refStartInit=lockStartInit=TRUE;
          refNew =refNow =1;
          lockNew=lockNow=1;
          break;

      case I_DELETE:
          isInstRef=1;          // it is InstRef
      case S_DELETE:
          if( !isInstRef )      // if it is not set
           isInstRef=2; 

          refNow=((RecordItem *)item)->size;
          if(!refStartInit) { refStartInit=TRUE; refStart=refNow; }
          lockNow=((RecordItem *)item)->type;
          if(!lockStartInit) { lockStartInit=TRUE; lockStart=lockNow; }

          refDelete =0-refNow;
          lockDelete=0-lockNow;
          refNow=lockNow=0;

          // change=TRUE;

          if( (action&PRINT_CHANGE)
                ||
              (action&PRINT_TRIM_CHANGE && !born)
            )
          {
           char *hname;

           if(name) hname=name; else hname=".";

           // print
           buf[0]=huf[0]=0;
           // ptr
           sprintf(buf,"*%p",ptr);

           // REF
            // start
            #define RECORD_MKSTRING( WHAT )    \
            {                                  \
             sprintf(huf," %4i",WHAT);         \
             strcat(buf,huf);                  \
            }                                  \
            else                               \
             strcat(buf,"    -");
            if( !born ) RECORD_MKSTRING(refStart)
            // change
            if( refChanged ) RECORD_MKSTRING(refChange)
            // new
            if( refNew )
            {
             sprintf(huf," %1i",refNew);
             strcat(buf,huf);
            }
            else
             strcat(buf," -");
            // delete - I am inside
            sprintf(huf," %4i",refDelete);
            strcat(buf,huf);
            // now
            strcat(buf,"    -"); // because was deleted

           // show type InstRef/StatRef
           switch(isInstRef)
           {
            case 1:
                        strcat(buf," i");
                   break;
            case 2:
                        strcat(buf," s");
                   break;
            default:
                        strcat(buf," |");
           }

           // LOCK
            // start
            if( !born ) RECORD_MKSTRING(lockStart)
            // change
            if( lockChanged ) RECORD_MKSTRING(lockChange)
            // new
            if( lockNew )
            {
             sprintf(huf," %1i",lockNew);
             strcat(buf,huf);
            }
            else
             strcat(buf," -");
            // delete - I am inside
            sprintf(huf," %4i",lockDelete);
            strcat(buf,huf);
            // now
            strcat(buf,"    -"); // because was deleted

           // NAME
           strcat(buf,"  ");
           strcat(buf,hname);




           DBGprintf(2000,"%s",buf);
          }

          // init each variable for the future use
          refStart=refChange=refNew=refDelete=refNow=
          lockStart=lockChange=lockNew=lockDelete=lockNow=0;
          change=refChanged=lockChanged=born=lockStartInit=refStartInit=FALSE;

      break;

      case I_LOCK:
          isInstRef=1;          // it is InstRef
      case S_LOCK:
          if( !isInstRef )      // if it is not set
           isInstRef=2; 

       lockChange++;
       change=TRUE;
       lockChanged=TRUE;
        lockNow=((RecordItem *)item)->size+1;
        if(!lockStartInit) { lockStartInit=TRUE; lockStart=lockNow-1; }
         refNow=((RecordItem *)item)->refCount; 
         if(!refStartInit) { refStartInit=TRUE;  refStart=refNow; }
      break;
      case I_UNLOCK:
          isInstRef=1;          // it is InstRef
      case S_UNLOCK:
          if( !isInstRef )      // if it is not set
           isInstRef=2; 

       lockChange--;
       change=TRUE;
       lockChanged=TRUE;
        lockNow=((RecordItem *)item)->size-1;
        if(!lockStartInit) { lockStartInit=TRUE; lockStart=lockNow+1; }
         refNow=((RecordItem *)item)->refCount; 
         if(!refStartInit) { refStartInit=TRUE; refStart=refNow; }
      break;
      case I_ADD_REF:
          isInstRef=1;          // it is InstRef
      case S_ADD_REF:
          if( !isInstRef )      // if it is not set
           isInstRef=2; 

       refChange++;
       change=TRUE;
       refChanged=TRUE;
        refNow=((RecordItem *)item)->size+1;
        if(!refStartInit) { refStartInit=TRUE; refStart=refNow-1; }
         lockNow=((RecordItem *)item)->lockCount;
         if(!lockStartInit) { lockStartInit=TRUE; lockStart=lockNow; }
      break;
      case I_RELEASE_REF:
          isInstRef=1;          // it is InstRef
      case S_RELEASE_REF:
          if( !isInstRef )      // if it is not set
           isInstRef=2; 

       refChange--;
       change=TRUE;
       refChanged=TRUE;
        refNow=((RecordItem *)item)->size-1;
        if(!refStartInit) { refStartInit=TRUE; refStart=refNow+1; }
         lockNow=((RecordItem *)item)->lockCount;
         if(!lockStartInit) { lockStartInit=TRUE; lockStart=lockNow; }
      break;

      case S_SET_TYPE:
          isInstRef=2;          // it is StatRef
      break;

      default: ;
     } // switch

     ((RecordItem *)item)->done=TRUE;
     item=printChangeNext(item);

    } // if sameName
    else
     item=printChangeNext(item);
   } // while item



   if( (action&PRINT_CHANGE && change)
        ||
       (action&PRINT_TRIM_CHANGE && (refChange || lockChange))
     )
   {
    char *hname;

    if(name) hname=name; else hname=".";

           // print
           buf[0]=huf[0]=0;
           // ptr
           sprintf(buf," %p",ptr);

           // REF
            // start
            #define RECORD_MKSTRING( WHAT )    \
            {                                  \
             sprintf(huf," %4i",WHAT);         \
             strcat(buf,huf);                  \
            }                                  \
            else                               \
             strcat(buf,"    -");
            if( !born ) RECORD_MKSTRING(refStart)
            // change
            if( refChanged ) RECORD_MKSTRING(refChange)
            // new
            if( refNew )
            {
             sprintf(huf," %1i",refNew);
             strcat(buf,huf);
            }
            else
             strcat(buf," -");
            // delete
            if( refDelete ) RECORD_MKSTRING(refDelete)
            // now
            sprintf(huf," %4i",refNow);
            strcat(buf,huf);

           // show type InstRef/StatRef
           switch(isInstRef)
           {
            case 1:
                        strcat(buf," i");
                   break;
            case 2:
                        strcat(buf," s");
                   break;
            default:
                        strcat(buf," |");
           }

           // LOCK
            // start
            if( !born ) RECORD_MKSTRING(lockStart)
            // change
            if( lockChanged ) RECORD_MKSTRING(lockChange)
            // new
            if( lockNew )
            {
             sprintf(huf," %1i",lockNew);
             strcat(buf,huf);
            }
            else
             strcat(buf," -");
            // delete
            if( lockDelete ) RECORD_MKSTRING(lockDelete)
            // now
            sprintf(huf," %4i",lockNow);
            strcat(buf,huf);

           // NAME
           strcat(buf,"  ");
           strcat(buf,hname);




           DBGprintf(2000,"%s",buf);

   }
  } // while !empty

  if( name )
  {
   free(name);
   name=NULL;
  }

 }

 if( action & DESTROY_RECORD ) /*fold01*/
 {
  DBGprintf(2000,"--> Destroying record...");
  recordList->Destroy();
 }

}

//-----------------------------------------------------------------------------

void referenceDebugManageCore( int action, void *ptr, int oldSize, char *file, int line ) /*fold00*/
{
 RecordItem *item=NULL;

 if( recording || printing )
 {
  if( file!=NULL )
  {
   char *s=strrchr (file,'/');

   if(s!=NULL)
    file=s+1;   // move across /
  }

  if( recording )
   item=new RecordItem( action, (void *)ptr );

  switch( action )
  {
   // InstRef
   case I_WAS_BORN:
    if(printing)
    DBGprintf(2000," %p -> newInstRef, size %iB, name %s ... %s:%d",
              ptr,
              ((InstRef *)ptr)->size,
              ((InstRef *)ptr)->name
              ,file,line
              );
    if( recording )
    {
     item->size = ((InstRef *)ptr)->size;
     #define RECORD_SET_INAME            \
      if( ((InstRef *)ptr)->name ) \
       item->name = (char *)strdup(((InstRef *)ptr)->name); \
      else                               \
       item->name = NULL;
     RECORD_SET_INAME
    }
    break;



   case I_REALLOC:
    if(printing)
    DBGprintf(2000," %p -> reallocInstRef, size: %dB -> %dB, name %s ... %s:%d",
              ptr,
              ((InstRef *)ptr)->size, // old size
              oldSize,                // new size
              ((InstRef *)ptr)->name
              ,file,line
             );
    if( recording )
    {
     item->size = oldSize;
     item->type = ((InstRef *)ptr)->size;
     RECORD_SET_INAME
    }
    break;



   case I_LOCK:
    if(printing)
    DBGprintf(2000," %p -> lockInstRef, lockCount %i -> %i, name %s ... %s:%d",
              ptr,
              ((InstRef *)ptr)->lockCount,
              ((InstRef *)ptr)->lockCount+1,
              ((InstRef *)ptr)->name
              ,file,line
             );
    if( recording )
    {
     // state before action is saved
     #define RECORD_ISAVE_REFLOCK                   \
     item->refCount  = ((InstRef *)ptr)->refCount;  \
     item->lockCount = ((InstRef *)ptr)->lockCount;
     RECORD_ISAVE_REFLOCK
     item->size      = ((InstRef *)ptr)->lockCount; // before action
     RECORD_SET_INAME
    }
    break;



   case I_UNLOCK:
    if(printing)
    DBGprintf(2000," %p -> unlockInstRef, lockCount %i -> %i, name %s ... %s:%d",
              ptr,
              ((InstRef *)ptr)->lockCount,
              ((InstRef *)ptr)->lockCount-1,
              ((InstRef *)ptr)->name
              ,file,line
             );
    if( recording )
    {
     RECORD_ISAVE_REFLOCK
     item->size = ((InstRef *)ptr)->lockCount; // before action
     RECORD_SET_INAME
    }
    break;



   case I_DELETE:
    if(printing)
    DBGprintf(2000," %p -> deleteInstRef, refCount %i, lockCount %i, name %s ... %s:%d",
              ptr,
              ((InstRef *)ptr)->refCount,
              ((InstRef *)ptr)->lockCount,
              ((InstRef *)ptr)->name
              ,file,line
             );
    if( recording )
    {
     RECORD_ISAVE_REFLOCK
     item->size = ((InstRef *)ptr)->refCount;
     item->type = ((InstRef *)ptr)->lockCount;
     RECORD_SET_INAME
    }
    break;



   case I_GCLOCK:
    if(printing)
    DBGprintf(2000," %p -> gcLockInstRef, state %i -> %i, name %s ... %s:%d",
              ptr,
              ((InstRef *)ptr)->state,
              ((InstRef *)ptr)->state+1,
              ((InstRef *)ptr)->name
              ,file,line
             );
    if( recording )
    {
     item->size = ((InstRef *)ptr)->state; // before action
     RECORD_SET_INAME
    }
    break;



   case I_GCUNLOCK:
    if(printing)
    DBGprintf(2000," %p -> gcUnlockInstRef, state %i -> %i, name %s ... %s:%d",
              ptr,
              ((InstRef *)ptr)->state,
              ((InstRef *)ptr)->state-1,
              ((InstRef *)ptr)->name
              ,file,line
             );
    if( recording )
    {
     item->size = ((InstRef *)ptr)->state; // before action
     RECORD_SET_INAME
    }
    break;



   case I_GETFIELD:
   case I_PUTFIELD:
    if(printing)
    DBGprintf(2000," %p -> fieldInstAction, name %s ... %s:%d",
              ptr,
              ((InstRef *)ptr)->name
              ,file,line
             );
    if( recording )
    {
     RECORD_SET_INAME
    }
    break;



   case I_ADD_REF:
    if(printing)
    DBGprintf(2000," %p -> addInstRef, refCount %i -> %i, name %s ... %s:%d",
              ptr,
              ((InstRef *)ptr)->refCount,   // old refCount
              ((InstRef *)ptr)->refCount+1, // new refCount
              ((InstRef *)ptr)->name
              ,file,line
            );
    if( recording )
    {
     RECORD_ISAVE_REFLOCK
     item->size = ((InstRef *)ptr)->refCount;
     RECORD_SET_INAME
    }
    break;



   case I_RELEASE_REF:
    if(printing)
    DBGprintf(2000," %p -> releaseInstRef, refCount %i -> %i, name %s ... %s:%d",
              ptr,
              ((InstRef *)ptr)->refCount,   // old refCount
              ((InstRef *)ptr)->refCount-1, // new refCount
              ((InstRef *)ptr)->name
              ,file,line
             );
    if( recording )
    {
     RECORD_ISAVE_REFLOCK
     item->size = ((InstRef *)ptr)->refCount;
     RECORD_SET_INAME
    }
    break;



   // --- StatRef ---



   case S_WAS_BORN:
    if(printing)
    DBGprintf(2000," %p -> newStatRef, size %iB, name %s ... %s:%d",
              ptr,
              ((StatRef *)ptr)->size,
              ((StatRef *)ptr)->name
              ,file,line
              );
    if( recording )
    {
     item->size = ((StatRef *)ptr)->size;
     item->type = ((StatRef *)ptr)->type;
     #define RECORD_SET_SNAME            \
      if( ((StatRef *)ptr)->name )       \
       item->name = (char *)strdup(((StatRef *)ptr)->name); \
      else                               \
       item->name = NULL;
     RECORD_SET_SNAME
    }
    break;



   case S_REALLOC:
    if(printing)
    DBGprintf(2000," %p -> reallocStatRef, size: %dB -> %dB, name %s ... %s:%d",
              ptr,
              ((StatRef *)ptr)->size, // old size
              oldSize,                // new size
              ((StatRef *)ptr)->name
              ,file,line
             );
    if( recording )
    {
     item->size = oldSize;
     item->type = ((StatRef *)ptr)->size;
     RECORD_SET_SNAME
    }
    break;



   case S_LOCK:

    if(printing)
    DBGprintf(2000," %p -> lockStatRef, lockCount %i -> %i, name %s ... %s:%d",
              ptr,
              ((StatRef *)ptr)->lockCount,
              ((StatRef *)ptr)->lockCount+1,
              ((StatRef *)ptr)->name
              ,file,line
             );
    if( recording )
    {
     // state before action is saved
     #define RECORD_SSAVE_REFLOCK                   \
     item->refCount  = ((StatRef *)ptr)->refCount;  \
     item->lockCount = ((StatRef *)ptr)->lockCount;
     RECORD_SSAVE_REFLOCK
     item->size = ((StatRef *)ptr)->lockCount; // before action
     RECORD_SET_INAME
    }
    break;



   case S_UNLOCK:
    if(printing)
    DBGprintf(2000," %p -> unlockStatRef, lockCount %i -> %i, name %s ... %s:%d",
              ptr,
              ((StatRef *)ptr)->lockCount,
              ((StatRef *)ptr)->lockCount-1,
              ((StatRef *)ptr)->name
              ,file,line
             );
    if( recording )
    {
     RECORD_SSAVE_REFLOCK
     item->size = ((StatRef *)ptr)->lockCount; // before action
     RECORD_SET_INAME
    }
    break;



   case S_DELETE:
    if(printing)
    DBGprintf(2000," %p -> deleteStatRef, refCount %i, lockCount %i, name %s ... %s:%d",
              ptr,
              ((StatRef *)ptr)->refCount,
              ((StatRef *)ptr)->lockCount,
              ((StatRef *)ptr)->name
              ,file,line
             );
    if( recording )
    {
     item->size = ((StatRef *)ptr)->refCount; // before action
     item->type = ((StatRef *)ptr)->lockCount; // before action
     RECORD_SET_SNAME
    }
    break;



   case S_GCLOCK:
    if(printing)
    DBGprintf(2000," %p -> gcLockStatRef, state %i -> %i, name %s ... %s:%d",
              ptr,
              ((StatRef *)ptr)->state,
              ((StatRef *)ptr)->state+1,
              ((StatRef *)ptr)->name
              ,file,line
             );
    if( recording )
    {
     item->size = ((StatRef *)ptr)->state; // before action
     RECORD_SET_INAME
    }
    break;



   case S_GCUNLOCK:
    if(printing)
    DBGprintf(2000," %p -> gcUnlockStatRef, state %i -> %i, name %s ... %s:%d",
              ptr,
              ((StatRef *)ptr)->state,
              ((StatRef *)ptr)->state-1,
              ((StatRef *)ptr)->name
              ,file,line
             );
    if( recording )
    {
     item->size = ((StatRef *)ptr)->state; // before action
     RECORD_SET_INAME
    }
    break;



   case S_SET_TYPE:
    if(printing)
     DBGprintf(2000," %p -> setStatRefType, type: %d -> %d, name %s ... %s:%d",
              ptr,
              ((StatRef *)ptr)->type, // old type
              oldSize,              // new type
              ((StatRef *)ptr)->name
              ,file,line
             );
    if( recording )
    {
     item->size = oldSize;
     item->type = ((StatRef *)ptr)->type;
     RECORD_SET_SNAME
    }
    break;



   case S_ADD_REF:
    if(printing)
    DBGprintf(2000," %p -> addStatRef, refCount %i -> %i, name %s ... %s:%d",
              ptr,
              ((StatRef *)ptr)->refCount,   // old refCount
              ((StatRef *)ptr)->refCount+1, // new refCount
              ((StatRef *)ptr)->name
              ,file,line
            );
    if( recording )
    {
     RECORD_SSAVE_REFLOCK
     item->size = ((StatRef *)ptr)->refCount; // before action
     RECORD_SET_SNAME
    }
    break;



   case S_RELEASE_REF:
    if(printing)
    DBGprintf(2000," %p -> releaseStatRef, refCount %i -> %i, name %s ... %s:%d",
              ptr,
              ((StatRef *)ptr)->refCount,  // old refCount
              ((StatRef *)ptr)->refCount-1,// new refCount
              ((StatRef *)ptr)->name
              ,file,line
             );
    if( recording )
    {
     RECORD_SSAVE_REFLOCK
     item->size = ((StatRef *)ptr)->refCount; // before action
     RECORD_SET_SNAME
    }
    break;



   default:
    DBGprintf(2000,"\n Error: reported from %s line %d:\n  UFO managing operation %d...", __FILE__, __LINE__, action);
  } // case

  if(recording)
   recordList->Insert(item);

 } // if

}
 /*FOLD00*/


//- EOF -----------------------------------------------------------------------
