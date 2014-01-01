/*
 * referencedefines.cc
 *
 */
#include <stdio.h>

#include "referencetypes.h"

#define GET_OFFSET(var,item) (((char *)&((var).item))-((char *)&(var)))

int main (void) {
    InstRef instRef;
    StatRef statRef;

    printf ("/* Machine generated file - do not edit. */\n");
    printf ("#ifndef __REFERENCEDEFINES_H\n");
    printf ("#define __REFERENCEDEFINES_H\n");
    printf ("\n");

    printf ("#define INSTREF_REFCOUNT_STR \"%i\"\n",GET_OFFSET (instRef,refCount));
    printf ("#define INSTREF_LOCKCOUNT_STR \"%i\"\n",GET_OFFSET (instRef,lockCount));
    printf ("#define INSTREF_COLOR_STR \"%i\"\n",GET_OFFSET (instRef,color));
    printf ("#define INSTREF_STATE_STR \"%i\"\n",GET_OFFSET (instRef,state));
    printf ("#define INSTREF_FLAGS_STR \"%i\"\n",GET_OFFSET (instRef,finalizerFlags));
    printf ("\n");

    printf ("#define STATREF_REFCOUNT_STR \"%i\"\n",GET_OFFSET (statRef,refCount));
    printf ("#define STATREF_LOCKCOUNT_STR \"%i\"\n",GET_OFFSET (statRef,lockCount));
    printf ("#define STATREF_COLOR_STR \"%i\"\n",GET_OFFSET (statRef,color));
    printf ("#define STATREF_STATE_STR \"%i\"\n",GET_OFFSET (statRef,state));
    printf ("#define STATREF_FLAGS_STR \"%i\"\n",GET_OFFSET (statRef,finalizerFlags));
    printf ("\n");
    printf ("#endif\n");
    return 0;
}
