/* ISC license. */

#include <skalibs/strerr2.h>
#include <s6/s6-supervise.h>

#define USAGE "s6-svc [ -abqhkti12fFpcoduxO ] servicedir"

int main (int argc, char const *const *argv)
{
  PROG = "s6-svc" ;
  return s6_svc_main(argc, argv, "abqhkti12fFpcoduxO", USAGE, "supervise") ;
}
