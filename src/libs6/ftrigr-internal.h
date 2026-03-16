/* ISC license. */

#ifndef FTRIGR_INTERNAL_H
#define FTRIGR_INTERNAL_H

#include <skalibs/stralloc.h>

#define FTRIGR_BANNER1 "ftrigr v2.0 (b)\n"
#define FTRIGR_BANNER2 "ftrigr v2.0 (a)\n"

typedef struct ftrigr_data_s ftrigr_data, *ftrigr_data_ref ;
struct ftrigr_data_s
{
  int status ;
  stralloc sa ;
} ;
#define FTRIGR_DATA_ZERO { .id = 0, .status = 0, .sa = STRALLOC_ZERO }


#endif
