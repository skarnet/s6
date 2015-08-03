#
# This Makefile requires GNU make.
#
# Do not make changes here.
# Use the included .mak files.
#

it: all

make_need := 4.0
ifeq "" "$(strip $(filter $(make_need), $(firstword $(sort $(make_need) $(MAKE_VERSION)))))"
fail := $(error Your make ($(MAKE_VERSION)) is too old. You need $(make_need) or newer)
endif

CC = $(error Please use ./configure first)

STATIC_LIBS :=
SHARED_LIBS :=
INTERNAL_LIBS :=
EXTRA_TARGETS :=

-include config.mak
include package/targets.mak
include package/deps.mak

version_m := $(basename $(version))
version_M := $(basename $(version_m))
version_l := $(basename $(version_M))
CPPFLAGS_ALL := -iquote src/include-local -Isrc/include $(CPPFLAGS)
CFLAGS_ALL := $(CFLAGS) -pipe -Wall
CFLAGS_SHARED := -fPIC
LDFLAGS_ALL := $(LDFLAGS)
LDFLAGS_SHARED := -shared
LDLIBS_ALL := $(LDLIBS)
REALCC = $(CROSS_COMPILE)$(CC)
AR := $(CROSS_COMPILE)ar
RANLIB := $(CROSS_COMPILE)ranlib
STRIP := $(CROSS_COMPILE)strip
INSTALL := ./tools/install.sh

ALL_BINS := $(LIBEXEC_TARGETS) $(BIN_TARGETS) $(SBIN_TARGETS)
ALL_LIBS := $(SHARED_LIBS) $(STATIC_LIBS) $(INTERNAL_LIBS)
ALL_INCLUDES := $(wildcard src/include/$(package)/*.h)

all: $(ALL_LIBS) $(ALL_BINS) $(ALL_INCLUDES)

clean:
	@exec rm -f $(ALL_LIBS) $(ALL_BINS) $(wildcard src/*/*.o src/*/*.lo) $(EXTRA_TARGETS)

distclean: clean
	@exec rm -f config.mak src/include/${package}/config.h

tgz: distclean
	@. package/info && \
	rm -rf /tmp/$$package-$$version && \
	cp -a . /tmp/$$package-$$version && \
	cd /tmp && \
	tar -zpcv --owner=0 --group=0 --numeric-owner --exclude=.git* -f /tmp/$$package-$$version.tar.gz $$package-$$version && \
	exec rm -rf /tmp/$$package-$$version

strip: $(ALL_LIBS) $(ALL_BINS)
ifneq ($(strip $(ALL_LIBS)),)
	exec ${STRIP} -x -R .note -R .comment -R .note.GNU-stack $(ALL_LIBS)
endif
ifneq ($(strip $(ALL_BINS)),)
	exec ${STRIP} -R .note -R .comment -R .note.GNU-stack $(ALL_BINS)
endif

install: install-dynlib install-libexec install-bin install-sbin install-lib install-include
install-dynlib: $(SHARED_LIBS:lib%.so=$(DESTDIR)$(dynlibdir)/lib%.so)
install-libexec: $(LIBEXEC_TARGETS:%=$(DESTDIR)$(libexecdir)/%)
install-bin: $(BIN_TARGETS:%=$(DESTDIR)$(bindir)/%)
install-sbin: $(SBIN_TARGETS:%=$(DESTDIR)$(sbindir)/%)
install-lib: $(STATIC_LIBS:lib%.a=$(DESTDIR)$(libdir)/lib%.a)
install-include: $(ALL_INCLUDES:src/include/$(package)/%.h=$(DESTDIR)$(includedir)/$(package)/%.h)

ifneq ($(exthome),)

update:
	exec $(INSTALL) -l $(notdir $(home)) $(DESTDIR)$(exthome)

global-links: $(DESTDIR)$(exthome) $(SHARED_LIBS:lib%.so=$(DESTDIR)$(sproot)/library.so/lib%.so) $(BIN_TARGETS:%=$(DESTDIR)$(sproot)/command/%) $(SBIN_TARGETS:%=$(DESTDIR)$(sproot)/command/%)

$(DESTDIR)$(sproot)/command/%: $(DESTDIR)$(home)/command/%
	exec $(INSTALL) -D -l ..$(subst $(sproot),,$(exthome))/command/$(<F) $@

$(DESTDIR)$(sproot)/library.so/lib%.so: $(DESTDIR)$(dynlibdir)/lib%.so
	exec $(INSTALL) -D -l ..$(subst $(sproot),,$(exthome))/library.so/$(<F) $@

.PHONY: update global-links

endif

$(DESTDIR)$(dynlibdir)/lib%.so: lib%.so
	$(INSTALL) -D -m 755 $< $@.$(version) && \
	$(INSTALL) -l $<.$(version) $@.$(version_m) && \
	$(INSTALL) -l $<.$(version_m) $@.$(version_M) && \
	$(INSTALL) -l $<.$(version_M) $@.$(version_l) && \
	exec $(INSTALL) -l $<.$(version_l) $@

$(DESTDIR)$(libexecdir)/% $(DESTDIR)$(bindir)/% $(DESTDIR)$(sbindir)/%: % package/modes
	exec $(INSTALL) -D -m 600 $< $@
	grep -- ^$(@F) < package/modes | { read name mode owner && \
	if [ x$$owner != x ] ; then chown -- $$owner $@ ; fi && \
	chmod $$mode $@ ; }

$(DESTDIR)$(libdir)/lib%.a: lib%.a
	exec $(INSTALL) -D -m 644 $< $@

$(DESTDIR)$(includedir)/$(package)/%.h: src/include/$(package)/%.h
	exec $(INSTALL) -D -m 644 $< $@

%.o: %.c
	exec $(REALCC) $(CPPFLAGS_ALL) $(CFLAGS_ALL) -c -o $@ $<

%.lo: %.c
	exec $(REALCC) $(CPPFLAGS_ALL) $(CFLAGS_ALL) $(CFLAGS_SHARED) -c -o $@ $<

$(ALL_BINS):
	exec $(REALCC) -o $@ $(CFLAGS_ALL) $(LDFLAGS_ALL) $(LDFLAGS_NOSHARED) $^ $(EXTRA_LIBS) $(LDLIBS_ALL)

lib%.a:
	exec $(AR) rc $@ $^
	exec $(RANLIB) $@

lib%.so:
	exec $(REALCC) -o $@ $(CFLAGS_ALL) $(CFLAGS_SHARED) $(LDFLAGS_ALL) $(LDFLAGS_SHARED) -Wl,-soname,$@.$(version_l) $^

.PHONY: it all clean distclean tgz strip install install-dynlib install-bin install-sbin install-lib install-include

.DELETE_ON_ERROR:
