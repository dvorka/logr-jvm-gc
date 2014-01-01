/*
 * gc.h
 *
 * Author: Dvorka
 *
 * ToDo: 
 */

#ifndef __GC_H
 #define __GC_H

 #include "gcoptions.h"



 void *gcBody( void *attr );            // daemon thread body

 void instanceCollection();



#endif

//- EOF -----------------------------------------------------------------------
