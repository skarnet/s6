/* ISC license. */

#include <sys/uio.h>
#include <string.h>
#include <errno.h>
#include <skalibs/unixmessage.h>
#include <s6/fdholder.h>

int s6_fdholder_delete_async (s6_fdholder_t *a, char const *id)
{
  size_t idlen = strlen(id) ;
  char pack[2] = "D" ;
  struct iovec v[2] = { { .iov_base = pack, .iov_len = 2 }, { .iov_base = (char *)id, .iov_len = idlen + 1 } } ;
  unixmessagev m = { .v = v, .vlen = 2, .fds = 0, .nfds = 0 } ;
  if (idlen > S6_FDHOLDER_ID_SIZE) return (errno = ENAMETOOLONG, 0) ;
  pack[1] = (unsigned char)idlen ;
  return unixmessage_putv(&a->connection.out, &m) ;
}
