BIN_TARGETS := \
ucspilogd \
s6-ftrigrd \
s6-ftrig-listen1 \
s6-ftrig-listen \
s6-ftrig-notify \
s6-ftrig-wait \
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
s6-svperms \
s6-svlink \
s6-svunlink \
s6-notifyoncheck \
s6-envdir \
s6-envuidgid \
s6-fghack \
s6-log \
s6-setlock \
s6-setsid \
s6-softlimit \
s6-socklog \
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
s6-setuidgid \
s6-instance-create \
s6-instance-delete \
s6-instance-control \
s6-instance-status \
s6-instance-list \
s6-background-watch \

LIB_DEFS := S6=s6
S6_DESCRIPTION :=

$(shell grep -qFx "prctl: no" $(sysdeps)/sysdeps && grep -qFx "procctl: no" $(sysdeps)/sysdeps && grep -qFx "kevent: yes" $(sysdeps)/sysdeps)
ifeq ($(.SHELLSTATUS),0)
KEVENTPTHREAD_LIB := $(PTHREAD_LIB)
else
KEVENTPTHREAD_LIB :=
endif

ifneq ($(EXECLINE_LIB),)
LIB_DEFS += S6AUTO=s6auto
S6AUTO_DESCRIPTION := The s6auto library (C helpers to create service directories)
BIN_TARGETS += s6-usertree-maker s6-instance-maker
endif

WRAP_ANY :=

ifdef WRAP_DAEMONTOOLS

WRAP_ANY := 1

ifdef WRAP_SYMLINKS

DAEMONTOOLS_TARGETS := \
envdir \
envuidgid \
fghack \
multilog \
pgrphack \
readproctitle \
setlock \
setuidgid \
softlimit \
supervise \
svc \
svok \
svscan \
svscanboot \
svstat \
tai64n \
tai64nlocal \

else

DAEMONTOOLS_TARGETS :=

endif

install-bin: $(DAEMONTOOLS_TARGETS:%=$(DESTDIR)$(bindir)/%)

ifneq ($(exthome),)
global-links: $(DAEMONTOOLS_TARGETS:%=$(DESTDIR)$(sproot)/command/%)
endif

endif

ifdef WRAP_RUNIT

WRAP_ANY := 1

ifdef WRAP_SYMLINKS

RUNIT_TARGETS := \
runit \
runit-init \
runsv \
runsvchdir \
runsvdir \
svlogd \
utmpset
RUNIT_SPECIAL_TARGETS := chpst sv

else

RUNIT_TARGETS :=
RUNIT_SPECIAL_TARGETS :=

endif

BIN_TARGETS += s6-alias-sv s6-alias-chpst

install-bin: $(RUNIT_TARGETS:%=$(DESTDIR)$(bindir)/%) $(RUNIT_SPECIAL_TARGETS:%=$(DESTDIR)$(bindir)/%)

ifneq ($(exthome),)
global-links: $(RUNIT_TARGETS:%=$(DESTDIR)$(sproot)/command/%) $(RUNIT_SPECIAL_TARGETS:%=$(DESTDIR)$(sproot)/command/%)
endif

$(DESTDIR)$(bindir)/chpst: $(DESTDIR)$(bindir)/s6-alias-chpst
	exec $(INSTALL) -D -l s6-alias-chpst $@
$(DESTDIR)$(bindir)/sv: $(DESTDIR)$(bindir)/s6-alias-sv
	exec $(INSTALL) -D -l s6-alias-sv $@

endif

ifdef WRAP_ANY

BIN_TARGETS += s6-alias

$(DAEMONTOOLS_TARGETS:%=$(DESTDIR)$(bindir)/%) $(RUNIT_TARGETS:%=$(DESTDIR)$(bindir)/%): $(DESTDIR)$(bindir)/s6-alias
	exec $(INSTALL) -D -l s6-alias $@

endif
