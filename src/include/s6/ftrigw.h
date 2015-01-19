/* ISC license. */

#ifndef FTRIGW_H
#define FTRIGW_H

#include <skalibs/bytestr.h>

extern int ftrigw_fifodir_make (char const *, int, int) ;
extern int ftrigw_notify (char const *, char) ;
extern int ftrigw_notifyb (char const *, char const *, unsigned int) ;
extern int ftrigw_notifyb_nosig (char const *, char const *, unsigned int) ;
#define ftrigw_notifys(f, s) ftrigw_notifyb(f, (s), str_len(s))
extern int ftrigw_clean (char const *) ;

#endif
