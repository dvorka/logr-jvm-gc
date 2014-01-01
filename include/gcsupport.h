/*
 * gcsupport.h
 *
 * Author: Dvorka
 *
 * ToDo: 
 */

#ifndef __GCSUPPORT_H
 #define __GCSUPPORT_H



 typedef unsigned char byte;
 typedef unsigned long dword;



 //- Verbose killer -----------------------------------------------------------

 #define WORD_OF_DEATH( MASAGE )                                                \
         {                                                                    \
          fprintf(stderr,"\n Error: reported from %s line %d:\n  "MASAGE"\n\n",\
                  __FILE__,                                                   \
                  __LINE__                                                    \
                 );                                                           \
          exit(0);                                                            \
         }

 //- PThread Locks ------------------------------------------------------------

 #define LOCK_MUTEX(lock)			    \
 do                                                 \
 {						    \
  if(pthread_mutex_lock(lock))                      \
  {		                                    \
   WORD_OF_DEATH("unable to lock pthread lock.");   \
  }						    \
 }                                                  \
 while(0)

 #define UNLOCK_MUTEX(lock)			    \
 do                                                 \
 {						    \
  if(pthread_mutex_unlock(lock))                    \
  {		                                    \
   WORD_OF_DEATH("unable to unlock pthread lock."); \
  }						    \
 } while(0)

 //- Quick locks ---------------------------------------------------------------

 // Fast locks used for short-time-locks

 #define FLASH_LOCK_INIT(LOCK)                                                \
 do                                                                           \
 {						                              \
  LOCK = 0;					                              \
 }                                                                            \
 while(0)


 #define FLASH_LOCK(LOCK)                                                     \
 do                                                                           \
 {                                                                            \
    __asm__ __volatile__ (                                                    \
	"  movl %0, %%ebx               \n"                                   \
                                                                              \
	"  movl $1, %%eax               \n"                                   \
	"0:                             \n"                                   \
	"  xchgl (%%ebx), %%eax         \n"                                   \
	"  cmpl $1, %%eax               \n"                                   \
	"  jnz 1f                       \n"                                   \
                                                                              \
	"  pushl %%ebx                  \n"                                   \
	"   call yield                  \n"                                   \
	"  popl %%ebx                   \n"                                   \
                                                                              \
	"  jmp 0b                       \n"                                   \
	"1:                             \n"                                   \
	:                                                                     \
	: "m" (&(LOCK))                                                       \
	: "eax", "ebx", "memory"                                              \
    );                                                                        \
 }                                                                            \
 while(0)


 #define FLASH_UNLOCK(LOCK)			                              \
 do                                                                           \
 {						                              \
  LOCK = 0;					                              \
 }                                                                            \
 while(0)

#endif

//- EOF -----------------------------------------------------------------------
