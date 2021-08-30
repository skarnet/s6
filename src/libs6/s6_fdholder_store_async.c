/* ISC license. */

#include <sys/uio.h>
#include <string.h>
#include <errno.h>
#include <skalibs/tai.h>
#include <skalibs/unixmessage.h>
#include <s6/fdholder.h>

int s6_fdholder_store_async (s6_fdholder_t *a, int fd, char const *id, tain const *limit)
{
  size_t idlen = strlen(id) ;
  char pack[2 + TAIN_PACK] = "S" ;
  struct iovec v[2] = { { .iov_base = pack, .iov_len = 2 + TAIN_PACK }, { .iov_base = (char *)id, .iov_len = idlen + 1 } } ;
  unixmessagev m = { .v = v, .vlen = 2, .fds = &fd, .nfds = 1 } ;
  if (idlen > S6_FDHOLDER_ID_SIZE) return (errno = ENAMETOOLONG, 0) ;
  tain_pack(pack + 1, limit) ;
  pack[1+TAIN_PACK] = (unsigned char)idlen ;
  return unixmessage_putv(&a->connection.out, &m) ;
}
