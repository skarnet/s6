BIN_TARGETS := \
ucspilogd \
s6-ftrigrd \
s6-ftrig-listen1 \
s6-ftrig-listen \
s6-ftrig-notify \
s6-ftrig-wait \
s6lockd \
s6-cleanfifodir \
s6-mkfifodir \
s6-svscan \
s6-supervise \
s6-svc \
s6-svscanctl \
s6-svok \
s6-svstat \
s6-svdt \
s6-svdt-clear \
s6-permafailon \
s6-svwait \
s6-svlisten1 \
s6-svlisten \
s6-notifyoncheck \
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
s6-fdholder-daemon \
s6-fdholderd \
s6-fdholder-delete \
s6-fdholder-store \
s6-fdholder-retrieve \
s6-fdholder-list \
s6-fdholder-getdump \
s6-fdholder-setdump \
s6-fdholder-transferdump \
s6-applyuidgid \
s6-setuidgid

LIBEXEC_TARGETS := s6lockd-helper

LIB_DEFS := S6=s6
