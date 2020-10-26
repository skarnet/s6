/* ISC license. */

#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#include <skalibs/strerr2.h>
#include <skalibs/djbunix.h>

#include <s6/s6-supervise.h>

#ifdef PATH_MAX
# define S6_PATH_MAX PATH_MAX
#else
# define S6_PATH_MAX 4096
#endif

int s6_supervise_lock_mode (char const *subdir, unsigned int subdirmode, unsigned int controlmode)
{
  size_t subdirlen = strlen(subdir) ;
  int fdctl, fdctlw, fdlock ;
  char control[subdirlen + 9] ;
  char lock[subdirlen + 6] ;
  memcpy(control, subdir, subdirlen) ;
  memcpy(control + subdirlen, "/control", 9) ;
  memcpy(lock, subdir, subdirlen) ;
  memcpy(lock + subdirlen, "/lock", 6) ;
  if (mkdir(subdir, (mode_t)subdirmode) == -1)
  {
    if (errno != EEXIST) strerr_diefu2sys(111, "mkdir ", subdir) ;
    else
    {
      char buf[S6_PATH_MAX] ;
      ssize_t r = readlink(subdir, buf, S6_PATH_MAX) ;
      if (r < 0)
      {
        errno = EEXIST ;
        strerr_diefu2sys(111, "mkdir ", subdir) ;
      }
      if (r == S6_PATH_MAX)
      {
        errno = ENAMETOOLONG ;
        strerr_diefu2sys(111, "readlink ", subdir) ;
      }
      buf[r] = 0 ;
      if (mkdir(buf, (mode_t)subdirmode) == -1)
        strerr_diefu2sys(111, "mkdir ", buf) ;
    }
  }
  if (mkfifo(control, controlmode) < 0)
  {
    struct stat st ;
    if (errno != EEXIST)
      strerr_diefu2sys(111, "mkfifo ", control) ;
    if (stat(control, &st) < 0)
      strerr_diefu2sys(111, "stat ", control) ;
    if (!S_ISFIFO(st.st_mode))
      strerr_diefu2x(100, control, " is not a FIFO") ;
  }
  fdlock = openc_create(lock) ;
  if (fdlock < 0)
    strerr_diefu2sys(111, "open_create ", lock) ;
  if (lock_ex(fdlock) < 0)
    strerr_diefu2sys(111, "lock ", lock) ;
  fdctlw = openc_write(control) ;
  if (fdctlw >= 0) strerr_dief1x(100, "directory already locked") ;
  if (errno != ENXIO)
    strerr_diefu2sys(111, "open_write ", control) ;
  fdctl = openc_read(control) ;
  if (fdctl < 0)
    strerr_diefu2sys(111, "open_read ", control) ;
  fdctlw = openc_write(control) ;
  if (fdctlw < 0)
    strerr_diefu2sys(111, "open_write ", control) ;
  fd_close(fdlock) ;

  return fdctl ;
  /* we leak fdctlw but it's coe. */
}
