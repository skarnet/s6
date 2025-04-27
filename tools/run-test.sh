#!/bin/sh -e

prog="$1"

if test -x "./src/tests/${prog}.wrapper" ; then
  cmd="./src/tests/${prog}.wrapper $prog"
else
  cmd="./$prog"
fi

if test -r "./src/tests/${prog}.expected" ; then
  cp -f "./src/tests/${prog}.expected" "./${prog}.expected"
elif test -x "./src/tests/${prog}.baseline" ; then
  "./src/tests/${prog}.baseline" > "./${prog}.expected"
else
  echo "run-test.sh: fatal: missing baseline for $prog" 1>&2 ; exit 100
fi

$cmd | diff "./${prog}.expected" -

rm -f "./${prog}.expected"
echo "run-test.sh: info: $prog: pass" 1>&2
