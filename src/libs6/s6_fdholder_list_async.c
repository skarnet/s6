/* ISC license. */

#include <skalibs/unixmessage.h>
#include <s6/fdholder.h>

int s6_fdholder_list_async (s6_fdholder_t *a)
{
  unixmessage m = { .s = "L", .len = 1, .fds = 0, .nfds = 0 } ;
  return unixmessage_put(&a->connection.out, &m) ;
}
