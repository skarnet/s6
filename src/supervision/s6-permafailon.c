/* ISC license. */

#include <sys/stat.h>
#include <string.h>
#include <signal.h>

#include <skalibs/types.h>
#include <skalibs/strerr2.h>
#include <skalibs/bitarray.h>
#include <skalibs/sig.h>
#include <skalibs/tai.h>
#include <skalibs/exec.h>

#include <s6/s6-supervise.h>

#define USAGE "s6-permafailon seconds deathcount statuslist prog..."
#define dieusage() strerr_dieusage(100, USAGE)

static inline void list_scan (char const *s, unsigned char *codes, sigset_t *sigs)
{
  size_t pos = 0 ;
  memset(codes, 0, 32) ;
  sigemptyset(sigs) ;
  while (s[pos])
  {
    unsigned int u ;
    size_t len = uint_scan(s + pos, &u) ;
    if (len)
    {
      if (u > 255) strerr_dief1x(100, "invalid exit code") ;
      pos += len ;
      if (s[pos] == '-')
      {
        unsigned int v ;
        pos++ ;
        len = uint_scan(s + pos, &v) ;
        if (!len) strerr_dief1x(100, "invalid interval specification") ;
        if (v > 255) strerr_dief1x(100, "invalid exit code") ;
        if (v < u) strerr_dief1x(100, "invalid interval") ;
        pos += len ;
        bitarray_setn(codes, u, v - u + 1) ;
      }
      else bitarray_set(codes, u) ;
    }
    else
    {
      int sig ;
      size_t next = pos ;
      while (!strchr(",; \n\r\t", s[next])) next++ ;
      char tmp[next - pos + 1] ;
      memcpy(tmp, s + pos, next - pos) ;
      tmp[next - pos] = 0 ;
      len = sig0_scan(tmp, &sig) ;
      if (!len) strerr_dief1x(100, "invalid status list specification") ;
      pos += len ;
      if (sigaddset(sigs, sig) < 0) strerr_dief1x(100, "invalid signal") ;
    }
    while (memchr(",; \n\r\t", s[pos], 6)) pos++ ;
  }
}

int main (int argc, char const *const *argv)
{
  unsigned char codes[32] ;
  sigset_t sigs ;
  unsigned int total, seconds, n ;
  struct stat st ;
  PROG = "s6-permafailon" ;
  if (argc < 4) dieusage() ;

  if (!uint0_scan(argv[1], &seconds)) dieusage() ;
  if (!uint0_scan(argv[2], &n)) dieusage() ;
  if (!n) dieusage() ;
  if (n > S6_MAX_DEATH_TALLY) n = S6_MAX_DEATH_TALLY ;
  list_scan(argv[3], codes, &sigs) ;

  if (stat(S6_DTALLY_FILENAME, &st) < 0)
  {
    strerr_warnwu2sys("stat ", S6_DTALLY_FILENAME) ;
    goto cont ;
  }
  if (st.st_size % S6_DTALLY_PACK || st.st_size > S6_DTALLY_PACK * S6_MAX_DEATH_TALLY)
  {
    strerr_warnw2x("invalid ", S6_DTALLY_FILENAME) ;
    goto cont ;
  }
  total = st.st_size / S6_DTALLY_PACK ;
  {
    tain_t mintime ;
    unsigned int matches = 0 ;
    s6_dtally_t tab[total] ;
    ssize_t r = s6_dtally_read(".", tab, total) ;
    if (r <= 0)
    {
      if (r < 0) strerr_warnwu2sys("read ", S6_DTALLY_FILENAME) ;
      goto cont ;
    }
    if (r < n) goto cont ;
    tain_uint(&mintime, seconds) ;
    {
      tain_t now ;
      tain_wallclock_read(&now) ;
      tain_sub(&mintime, &now, &mintime) ;
    }

    for (unsigned int i = 0 ; i < r ; i++)
    {
      if (!tain_less(&tab[i].stamp, &mintime)
       && ((tab[i].sig && sigismember(&sigs, tab[i].sig)) || bitarray_peek(codes, tab[i].exitcode))
       && ++matches >= n)
      {
        char fmtevent[4] ;
        char fmtseconds[UINT_FMT] ;
        char fmtn[UINT_FMT] ;
        fmtevent[uint_fmt(fmtevent, tab[i].sig ? tab[i].sig : tab[i].exitcode)] = 0 ;
        fmtseconds[uint_fmt(fmtseconds, seconds)] = 0 ;
        fmtn[uint_fmt(fmtn, n)] = 0 ;
        strerr_warni8x("PERMANENT FAILURE triggered after ", fmtn, " events involving ", tab[i].sig ? "signal " : "exit code ", fmtevent, " in the last ", fmtseconds, " seconds") ;
        return 125 ;
      }
    }
  }

 cont:
  xexec0(argv+4) ;
}
