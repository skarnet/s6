/* ISC license. */

#include <skalibs/posixplz.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include "ftrig1.h"

void ftrig1_free (ftrig1_t *p)
{
  if (p->name.s)
  {
    unlink_void(p->name.s) ;
    stralloc_free(&p->name) ;
  }
  if (p->fd >= 0)
  {
    fd_close(p->fd) ;
    p->fd = -1 ;
  }
  if (p->fdw >= 0)
  {
    fd_close(p->fdw) ;
    p->fdw = -1 ;
  }
}
