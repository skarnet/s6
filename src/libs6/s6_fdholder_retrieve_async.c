 /* ISC license. */

#include <errno.h>
#include <skalibs/bytestr.h>
#include <skalibs/tai.h>
#include <skalibs/siovec.h>
#include <skalibs/unixmessage.h>
#include <s6/s6-fdholder.h>

int s6_fdholder_retrieve_maybe_delete_async (s6_fdholder_t *a, char const *id, int dodelete)
{
  unsigned int idlen = str_len(id) ;
  char pack[3] = "R" ;
  siovec_t v[2] = { { .s = pack, .len = 3 }, { .s = (char *)id, .len = idlen + 1 } } ;
  unixmessage_v_t m = { .v = v, .vlen = 2, .fds = 0, .nfds = 0 } ;
  if (idlen > S6_FDHOLDER_ID_SIZE) return (errno = ENAMETOOLONG, 0) ;
  pack[1] = !!dodelete ;
  pack[2] = (unsigned char)idlen ;
  return unixmessage_putv(&a->connection.out, &m) ;
}
