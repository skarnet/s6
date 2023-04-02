#!/bin/sh

usage() {
  echo "usage: $0 [ -D ] [ -l ] [ -m mode ] [ -O owner:group ] src dst" 1>&2
  exit 1
}

mkdirp=false
symlink=false
mode=0755
og=

while getopts Dlm:O: name ; do
  case "$name" in
    D) mkdirp=true ;;
    l) symlink=true ;;
    m) mode=$OPTARG ;;
    O) og=$OPTARG ;;
    ?) usage ;;
  esac
done
shift $(($OPTIND - 1))

test "$#" -eq 2 || usage
src=$1
dst=$2
tmp="$dst.tmp.$$"

case "$dst" in
  */) echo "$0: $dst ends in /" 1>&2 ; exit 1 ;;
esac

set -C
set -e

if $mkdirp ; then
  umask 022
  case "$2" in
    */*) mkdir -p "${dst%/*}" ;;
  esac
fi

trap 'rm -f "$tmp"' EXIT INT QUIT TERM HUP

umask 077

if $symlink ; then
  ln -s "$src" "$tmp"
else
  cat < "$1" > "$tmp"
  if test -n "$og" ; then
    chown -- "$og" "$tmp"
  fi
  chmod -- "$mode" "$tmp"
fi

mv -f "$tmp" "$dst"
if test -d "$dst" ; then
  rm -f "$dst/$(basename $tmp)"
  if $symlink ; then
    mkdir "$tmp"
    ln -s "$src" "$tmp/$(basename $dst)"
    mv -f "$tmp/$(basename $dst)" "${dst%/*}"
    rmdir "$tmp"
  else
    echo "$0: $dst is a directory" 1>&2
    exit 1
  fi
fi
