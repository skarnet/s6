/* ISC license. */

#include <sys/uio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <skalibs/uint32.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/bytestr.h>
#include <skalibs/error.h>
#include <skalibs/tai.h>
#include <skalibs/unixmessage.h>

#include <s6/fdholder.h>

#include <skalibs/posixishard.h>

int s6_fdholder_setdump (s6_fdholder_t *a, s6_fdholder_fd_t const *list, unsigned int ntot, tain const *deadline, tain *stamp)
{
  uint32_t trips ;
  if (!ntot) return 1 ;
  unsigned int i = 0 ;
  for (; i < ntot ; i++)
  {
    size_t zpos = byte_chr(list[i].id, S6_FDHOLDER_ID_SIZE + 1, 0) ;
    if (!zpos || zpos >= S6_FDHOLDER_ID_SIZE + 1) return (errno = EINVAL, 0) ;
  }
  {
    char pack[5] = "!" ;
    unixmessage m = { .s = pack, .len = 5, .fds = 0, .nfds = 0 } ;
    uint32_pack_big(pack+1, ntot) ;
    if (!unixmessage_put(&a->connection.out, &m)) return 0 ;
    if (!unixmessage_sender_timed_flush(&a->connection.out, deadline, stamp)) return 0 ;
    if (sanitize_read(unixmessage_timed_receive(&a->connection.in, &m, deadline, stamp)) < 0) return 0 ;
    if (!m.len || m.nfds) { unixmessage_drop(&m) ; return (errno = EPROTO, 0) ; }
    if (m.s[0]) return (errno = (unsigned char)m.s[0], 0) ;
    if (m.len != 5) return (errno = EPROTO, 0) ;
    uint32_unpack_big(m.s + 1, &trips) ;
    if (trips != 1 + (ntot-1) / UNIXMESSAGE_MAXFDS) return (errno = EPROTO, 0) ;
  }
  for (i = 0 ; i < trips ; i++, ntot -= UNIXMESSAGE_MAXFDS)
  {
    {
      unsigned int n = ntot > UNIXMESSAGE_MAXFDS ? UNIXMESSAGE_MAXFDS : ntot ;
      unsigned int j = 0 ;
      struct iovec v[1 + (n<<1)] ;
      int fds[n] ;
      unixmessagev m = { .v = v, .vlen = 1 + (n<<1), .fds = fds, .nfds = n } ;
      char pack[n * (TAIN_PACK+1)] ;
      v[0].iov_base = "." ; v[0].iov_len = 1 ;
      for (; j < n ; j++, list++, ntot--)
      {
        size_t len = strlen(list->id) ;
        v[1 + (j<<1)].iov_base = pack + j * (TAIN_PACK+1) ;
        v[1 + (j<<1)].iov_len = TAIN_PACK + 1 ;
        tain_pack(pack + j * (TAIN_PACK+1), &list->limit) ;
        pack[j * (TAIN_PACK+1) + TAIN_PACK] = (unsigned char)len ;
        v[2 + (j<<1)].iov_base = (char *)list->id ;
        v[2 + (j<<1)].iov_len = len + 1 ;
        fds[j] = list->fd ;
      }
      if (!unixmessage_putv(&a->connection.out, &m)) return 0 ;
    }
    if (!unixmessage_sender_timed_flush(&a->connection.out, deadline, stamp)) return 0 ;
    {
      unixmessage m ;
      if (sanitize_read(unixmessage_timed_receive(&a->connection.in, &m, deadline, stamp)) < 0) return 0 ;
      if (m.len != 1 || m.nfds)
      {
        unixmessage_drop(&m) ;
        return (errno = EPROTO, 0) ;
      }
      if (!error_isagain((unsigned char)m.s[0]) && i < trips-1)
        return errno = m.s[0] ? (unsigned char)m.s[0] : EPROTO, 0 ;
      if (i == trips - 1 && m.s[0])
        return errno = error_isagain((unsigned char)m.s[0]) ? EPROTO : (unsigned char)m.s[0], 0 ;
    }
  }
  return 1 ;
}
