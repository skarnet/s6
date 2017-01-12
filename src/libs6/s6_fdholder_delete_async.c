 /* ISC license. */

#include <sys/types.h>
#include <errno.h>
#include <skalibs/bytestr.h>
#include <skalibs/siovec.h>
#include <skalibs/unixmessage.h>
#include <s6/s6-fdholder.h>

int s6_fdholder_delete_async (s6_fdholder_t *a, char const *id)
{
  size_t idlen = str_len(id) ;
  char pack[2] = "D" ;
  siovec_t v[2] = { { .s = pack, .len = 2 }, { .s = (char *)id, .len = idlen + 1 } } ;
  unixmessage_v_t m = { .v = v, .vlen = 2, .fds = 0, .nfds = 0 } ;
  if (idlen > S6_FDHOLDER_ID_SIZE) return (errno = ENAMETOOLONG, 0) ;
  pack[1] = (unsigned char)idlen ;
  return unixmessage_putv(&a->connection.out, &m) ;
}
