 /* ISC license. */

#include <errno.h>
#include <skalibs/bytestr.h>
#include <skalibs/tai.h>
#include <skalibs/siovec.h>
#include <skalibs/unixmessage.h>
#include <s6/s6-fdholder.h>

int s6_fdholder_store_async (s6_fdholder_t *a, int fd, char const *id, tain_t const *limit)
{
  unsigned int idlen = str_len(id) ;
  char pack[2 + TAIN_PACK] = "S" ;
  siovec_t v[2] = { { .s = pack, .len = 2 + TAIN_PACK }, { .s = (char *)id, .len = idlen + 1 } } ;
  unixmessage_v_t m = { .v = v, .vlen = 2, .fds = &fd, .nfds = 1 } ;
  if (idlen > S6_FDHOLDER_ID_SIZE) return (errno = ENAMETOOLONG, 0) ;
  tain_pack(pack + 1, limit) ;
  pack[1+TAIN_PACK] = (unsigned char)idlen ;
  return unixmessage_putv(&a->connection.out, &m) ;
}
