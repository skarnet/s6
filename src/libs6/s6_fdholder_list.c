/* ISC license. */

#include <errno.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/error.h>
#include <skalibs/tai.h>
#include <skalibs/unixmessage.h>
#include <s6/fdholder.h>

int s6_fdholder_list (s6_fdholder_t *a, stralloc *sa, tain const *deadline, tain *stamp)
{
  s6_fdholder_list_result_t res = { .sa = sa } ;
  unixmessage m ;
  if (!s6_fdholder_list_async(a)) return -1 ;
  if (!unixmessage_sender_timed_flush(&a->connection.out, deadline, stamp)) return -1 ;
  if (sanitize_read(unixmessage_timed_receive(&a->connection.in, &m, deadline, stamp)) < 0) return -1 ;
  if (!s6_fdholder_list_cb(&m, &res)) return -1 ;
  if (res.err) return (errno = res.err, -1) ;
  return (int)res.n ;
}
