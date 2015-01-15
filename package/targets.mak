BIN_TARGETS := \
s6-ftrigrd \
s6-ftrig-listen1 \
s6-ftrig-listen \
s6-ftrig-notify \
s6-ftrig-wait \
s6lockd \
s6-cleanfifodir \
s6-mkfifodir \
s6-notifywhenup \
s6-svscan \
s6-supervise \
s6-svc \
s6-svscanctl \
s6-svok \
s6-svstat \
s6-svwait \
s6-envdir \
s6-envuidgid \
s6-fghack \
s6-log \
s6-setlock \
s6-setsid \
s6-softlimit \
s6-tai64n \
s6-tai64nlocal \
s6-accessrules-cdb-from-fs \
s6-accessrules-fs-from-cdb \
s6-connlimit \
s6-ioconnect \
s6-ipcclient \
s6-ipcserver-access \
s6-ipcserver-socketbinder \
s6-ipcserver \
s6-ipcserverd \
s6-sudo \
s6-sudoc \
s6-sudod \
ucspilogd

SBIN_TARGETS := \
s6-applyuidgid \
s6-setuidgid

LIBEXEC_TARGETS := s6lockd-helper

ifdef DO_ALLSTATIC
LIBS6 := libs6.a
else
LIBS6 := libs6.so
endif

ifdef DO_SHARED
SHARED_LIBS := libs6.so
endif

ifdef DO_STATIC
STATIC_LIBS := libs6.a
endif
