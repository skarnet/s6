/* ISC license. */

#include <skalibs/djbunix.h>
#include <skalibs/unixmessage.h>
#include <s6/fdholder.h>

void s6_fdholder_end (s6_fdholder_t *a)
{
  fd_close(unixmessage_sender_fd(&a->connection.out)) ;
  s6_fdholder_free(a) ;
}
