#!/bin/sh -e

isunique () {
  x=$1
  set -- $2
  while test "$#" -gt 0 ; do
    if test "$x" = "$1" ; then
      return 1
    fi
    shift
  done
  return 0
}

uniqit () {
  res=
  while test "$#" -gt 0 ; do
    if isunique "$1" "$res" ; then
      res="${res}${res:+ }${1}"
    fi
    shift
  done
  printf %s\\n "$res"
}

filterout () {
  res=
  filter="$1"
  shift
  while test "$#" -gt 0 ; do
    if isunique "$1" "$filter" ; then
      res="${res}${res:+ }${1}"
    fi
    shift
  done
  printf %s\\n "$res"
}

print_requires () {
  line=
  oldifs="$IFS"
  while IFS=" 	" read condvar usedinlibs pkg ver libs ; do
    IFS="$oldifs"
    for h ; do
      i=lib${h##-l}
      for j in $libs ; do
        if test "$i" = "$j" ; then
          line="${line}${line:+, }${i} >= ${ver}"
        fi
      done
    done
  done < package/deps-build
  IFS="$oldifs"
  echo "Requires: $line"
}

. package/info

ilist=
dlist=
slist=

if test "${includedir}" != /usr/include ; then
  ilist="-I${includedir}"
fi
if test -n "${extra_includedirs}" ; then
  ilist="${ilist}${ilist:+ }${extra_includedirs}"
fi
ilist=`uniqit ${ilist}`

if test "${dynlibdir}" != /usr/lib && test "${dynlibdir}" != /lib ; then
  dlist="-L${dynlibdir}"
fi

if test "${libdir}" != /usr/lib && test "${libdir}" != /lib ; then
 slist="-L${libdir}"
fi
if test -n "${extra_libdirs}" ; then
  slist="${slist}${slist:+ }${extra_libdirs}"
fi
slist="$(filterout "${dlist}" $(uniqit ${slist}))"

echo "prefix=${prefix}"
echo "includedir=${includedir}"
echo "libdir=${libdir}"
echo "dynlibdir=${dynlibdir}"
echo
echo "Name: ${library}"
echo "Version: ${version}"
echo "Description: ${description:-The ${library} library.}"
echo "URL: ${url:-https://skarnet.org/software/${package}/}"
if test -n "${extra_libs}" ; then
  print_requires ${extra_libs}
fi
if test -n "$ilist" ; then
  echo "Cflags: ${ilist}"
fi
echo "Libs: ${dlist}${dlist:+ }-l${library}${ldlibs:+ }${ldlibs}"
if test -n "${extra_libs}" ; then
  echo "Libs.private: ${slist}${slist:+ }${extra_libs}"
fi
