/* ISC license. */

#include <skalibs/strerr2.h>
#include <s6/s6-supervise.h>

#define USAGE "s6-svscanctl [ -phratszbnNiq0678 ] svscandir"

int main (int argc, char const *const *argv)
{
  PROG = "s6-svscanctl" ;
  return s6_svc_main(argc, argv, "phratszbnNiq0678", USAGE, ".s6-svscan") ;
}
