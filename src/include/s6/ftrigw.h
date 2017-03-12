/* ISC license. */

#ifndef FTRIGW_H
#define FTRIGW_H

#include <sys/stat.h>
#include <string.h>

extern int ftrigw_fifodir_make (char const *, gid_t, int) ;
extern int ftrigw_notify (char const *, char) ;
extern int ftrigw_notifyb (char const *, char const *, size_t) ;
extern int ftrigw_notifyb_nosig (char const *, char const *, size_t) ;
#define ftrigw_notifys(f, s) ftrigw_notifyb(f, (s), strlen(s))
extern int ftrigw_clean (char const *) ;

#endif
