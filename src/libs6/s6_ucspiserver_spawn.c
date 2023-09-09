/* ISC license. */

#include <skalibs/env.h>
#include <skalibs/cspawn.h>

#include <s6/ucspiserver.h>

pid_t s6_ucspiserver_spawn (int fd, char const *const *argv, char const *const *envp, size_t envlen, char const *modifs, size_t modiflen, size_t modifn)
{
  cspawn_fileaction fa[2] =
  {
    [0] = { .type = CSPAWN_FA_MOVE, .x = { .fd2 = { [0] = 0, [1] = fd } } },
    [1] = { .type = CSPAWN_FA_COPY, .x = { .fd2 = { [0] = 1, [1] = 0 } } }
  } ;
  char const *newenvp[envlen + modifn + 1] ;
  env_mergen(newenvp, envlen + modifn + 1, envp, envlen, modifs, modiflen, modifn) ;
  return cspawn(argv[0], argv, newenvp, CSPAWN_FLAGS_SELFPIPE_FINISH, fa, 2) ;
}
