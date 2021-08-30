/* ISC license. */

#include <errno.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/tai.h>
#include <skalibs/unixmessage.h>
#include <s6/fdholder.h>

int s6_fdholder_retrieve_maybe_delete (s6_fdholder_t *a, char const *id, int dodelete, tain const *deadline, tain *stamp)
{
  unixmessage m ;
  s6_fdholder_retrieve_result_t res ;
  if (!s6_fdholder_retrieve_maybe_delete_async(a, id, dodelete)) return -1 ;
  if (!unixmessage_sender_timed_flush(&a->connection.out, deadline, stamp)) return -1 ;
  if (sanitize_read(unixmessage_timed_receive(&a->connection.in, &m, deadline, stamp)) < 0) return -1 ;
  if (!s6_fdholder_retrieve_cb(&m, &res)) return -1 ;
  if (res.err) return (errno = res.err, -1) ;
  return res.fd ;
}
