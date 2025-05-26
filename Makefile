#
# This Makefile requires GNU make.
#
# Do not make changes here.
# Use the included .mak files.
#

it: all

make_need := 3.81
ifeq "" "$(strip $(filter $(make_need), $(firstword $(sort $(make_need) $(MAKE_VERSION)))))"
fail := $(error Your make ($(MAKE_VERSION)) is too old. You need $(make_need) or newer)
endif

uniq = $(if $1,$(firstword $1) $(call uniq,$(filter-out $(firstword $1),$1)))

CC = $(error Please use ./configure first)

STATIC_LIBS :=
SHARED_LIBS :=
INTERNAL_LIBS :=
EXTRA_TARGETS :=
PC_TARGETS :=
LIB_DEFS :=
BIN_SYMLINKS :=
TEST_BINS :=

-include config.mak
include package/targets.mak

define library_definition
LIB$(1) := lib$(2).$(if $(DO_ALLSTATIC),a,$(SHLIB_EXT)).xyzzy
ifdef DO_SHARED
SHARED_LIBS += lib$(2).$(SHLIB_EXT).xyzzy
endif
ifdef DO_STATIC
STATIC_LIBS += lib$(2).a.xyzzy
endif
ifdef DO_PKGCONFIG
PC_TARGETS += lib$(2).pc
endif

lib$(2).pc:
	exec env \
	  library="$(2)" \
	  includedir="$(includedir)" \
	  dynlibdir="$(dynlibdir)" \
	  libdir="$(libdir)" \
	  extra_includedirs="$(extra_includedirs)" \
	  extra_libdirs="$(extra_libdirs)" \
	  extra_libs="$$(strip $$(EXTRA_LIBS))" \
	  description="$$($(1)_DESCRIPTION)" \
	  url="$$($(1)_URL)" \
	  ldlibs="$(LDLIBS)" \
	  ./tools/gen-dotpc.sh > $$@.tmp
	exec mv -f $$@.tmp $$@

endef

define binary_installation_rule
$(DESTDIR)$(1)/$(2): ./$(2) package/modes
	exec $(INSTALL) -D -m 600 $$< $$@
	grep -- ^$$(@F) < package/modes | { read name mode owner && \
	if [ x$$$$owner != x ] ; then chown -- $$$$owner $$@ ; fi && \
	chmod $$$$mode $$@ ; }
endef

define symlink_installation_rule
$(DESTDIR)$(1)/$(2): $(DESTDIR)$(1)/$(SYMLINK_TARGET_$(2))
	exec $(INSTALL) -l $$(<F) $$@
endef

$(foreach var,$(LIB_DEFS),$(eval $(call library_definition,$(firstword $(subst =, ,$(var))),$(lastword $(subst =, ,$(var))))))

include package/deps.mak

version_m := $(basename $(version))
version_M := $(basename $(version_m))
version_l := $(basename $(version_M))
CPPFLAGS_ALL := $(CPPFLAGS_AUTO) $(CPPFLAGS)
CFLAGS_ALL := $(CFLAGS_AUTO) $(CFLAGS)
ifeq ($(strip $(STATIC_LIBS_ARE_PIC)),)
CFLAGS_SHARED := -fPIC
else
CFLAGS_SHARED :=
endif
LDFLAGS_ALL := $(LDFLAGS_AUTO) $(LDFLAGS)
AR := $(CROSS_COMPILE)ar
RANLIB := $(CROSS_COMPILE)ranlib
STRIP := $(CROSS_COMPILE)strip
INSTALL := ./tools/install.sh

$(foreach var,$(BIN_TARGETS),$(eval $(call binary_installation_rule,$(bindir),$(var))))
$(foreach var,$(LIBEXEC_TARGETS),$(eval $(call binary_installation_rule,$(libexecdir),$(var))))
$(foreach var,$(BIN_SYMLINKS),$(eval $(call symlink_installation_rule,$(bindir),$(var))))

ALL_BINS := $(LIBEXEC_TARGETS) $(BIN_TARGETS)
ALL_LIBS := $(SHARED_LIBS) $(STATIC_LIBS) $(INTERNAL_LIBS)
ALL_INCLUDES := $(wildcard src/include/$(package)/*.h)

all: $(ALL_LIBS) $(ALL_BINS) $(ALL_INCLUDES) $(EXTRA_INCLUDES) $(PC_TARGETS)

clean:
	@exec rm -f $(ALL_LIBS) $(ALL_BINS) $(TEST_BINS) $(wildcard src/*/*.o src/*/*.lo) $(PC_TARGETS) $(EXTRA_TARGETS)

distclean: clean
	@exec rm -f config.mak src/include/$(package)/config.h

tgz: distclean
	@. package/info && \
	rm -rf /tmp/$$package-$$version && \
	cp -a . /tmp/$$package-$$version && \
	cd /tmp && \
	tar -zpcv --owner=0 --group=0 --numeric-owner --exclude=.git* -f /tmp/$$package-$$version.tar.gz $$package-$$version && \
	sha256sum $$package-$$version.tar.gz > $$package-$$version.tar.gz.sha256 && \
	exec rm -rf /tmp/$$package-$$version

strip: $(ALL_LIBS) $(ALL_BINS)
ifneq ($(strip $(STATIC_LIBS)),)
	exec $(STRIP) -x -R .note -R .comment $(STATIC_LIBS)
endif
ifneq ($(strip $(ALL_BINS)$(SHARED_LIBS)),)
	exec $(STRIP) -R .note -R .comment $(ALL_BINS) $(SHARED_LIBS)
endif

install: install-dynlib install-libexec install-bin install-symlinks install-lib install-include install-pkgconfig
install-dynlib: $(SHARED_LIBS:lib%.$(SHLIB_EXT).xyzzy=$(DESTDIR)$(dynlibdir)/lib%.$(SHLIB_EXT))
install-libexec: $(LIBEXEC_TARGETS:%=$(DESTDIR)$(libexecdir)/%)
install-bin: $(BIN_TARGETS:%=$(DESTDIR)$(bindir)/%)
install-symlinks: $(BIN_SYMLINKS:%=$(DESTDIR)$(bindir)/%)
install-lib: $(STATIC_LIBS:lib%.a.xyzzy=$(DESTDIR)$(libdir)/lib%.a)
install-include: $(ALL_INCLUDES:src/include/$(package)/%.h=$(DESTDIR)$(includedir)/$(package)/%.h) $(EXTRA_INCLUDES:src/include/%.h=$(DESTDIR)$(includedir)/%.h)
install-pkgconfig: $(PC_TARGETS:%=$(DESTDIR)$(pkgconfdir)/%)

tests: $(TEST_BINS)

check: tests
	@for i in $(TEST_BINS) ; do ./tools/run-test.sh $$i || exit 1 ; done

ifneq ($(exthome),)

$(DESTDIR)$(exthome): $(DESTDIR)$(home)
	exec $(INSTALL) -l $(notdir $(home)) $(DESTDIR)$(exthome)

update: $(DESTDIR)$(exthome)

global-links: $(DESTDIR)$(exthome) $(SHARED_LIBS:lib%.so.xyzzy=$(DESTDIR)$(sproot)/library.so/lib%.so.$(version_M)) $(BIN_TARGETS:%=$(DESTDIR)$(sproot)/command/%) $(BIN_SYMLINKS:%=$(DESTDIR)$(sproot)/command/%)

$(DESTDIR)$(sproot)/command/%: $(DESTDIR)$(home)/command/%
	exec $(INSTALL) -D -l ..$(subst $(sproot),,$(exthome))/command/$(<F) $@

$(DESTDIR)$(sproot)/library.so/lib%.so.$(version_M): $(DESTDIR)$(dynlibdir)/lib%.so.$(version_M)
	exec $(INSTALL) -D -l ..$(subst $(sproot),,$(exthome))/library.so/$(<F) $@

.PHONY: update global-links

endif

$(DESTDIR)$(dynlibdir)/lib%.so $(DESTDIR)$(dynlibdir)/lib%.so.$(version_M) $(DESTDIR)$(dynlibdir)/lib%.so.$(version): lib%.so.xyzzy
	$(INSTALL) -D -m 755 $< $(@D)/lib$(*F).so.$(version) && \
	$(INSTALL) -l lib$(*F).so.$(version) $(@D)/lib$(*F).so.$(version_M) && \
	exec $(INSTALL) -l lib$(*F).so.$(version_M) $(@D)/lib$(*F).so

$(DESTDIR)$(libdir)/lib%.a: lib%.a.xyzzy
	exec $(INSTALL) -D -m 644 $< $@

$(DESTDIR)$(includedir)/$(package)/%.h: src/include/$(package)/%.h
	exec $(INSTALL) -D -m 644 $< $@

$(DESTDIR)$(includedir)/%.h: src/include/%.h
	exec $(INSTALL) -D -m 644 $< $@

$(DESTDIR)$(pkgconfdir)/lib%.pc: lib%.pc
	exec $(INSTALL) -D -m 644 $< $@

%.o: %.c
	exec $(CC) $(CPPFLAGS_ALL) $(CFLAGS_ALL) -c -o $@ $<

%.lo: %.c
	exec $(CC) $(CPPFLAGS_ALL) $(CFLAGS_ALL) $(CFLAGS_SHARED) -c -o $@ $<

$(ALL_BINS) $(TEST_BINS):
	exec $(CC) -o $@ $(CFLAGS_ALL) $(LDFLAGS_ALL) $(LDFLAGS_NOSHARED) $^ $(EXTRA_LIBS) $(LDLIBS)

lib%.a.xyzzy:
	exec $(AR) rc $@ $^
	exec $(RANLIB) $@

lib%.so.xyzzy:
	exec $(CC) -o $@ $(CFLAGS_ALL) $(CFLAGS_SHARED) $(LDFLAGS_ALL) $(LDFLAGS_SHARED) -Wl,-soname,$(patsubst lib%.so.xyzzy,lib%.so.$(version_M),$@) -Wl,-rpath=$(dynlibdir) $^ $(EXTRA_LIBS) $(LDLIBS)

.PHONY: it all clean distclean tests check tgz strip install install-dynlib install-bin install-lib install-include install-pkgconfig

.DELETE_ON_ERROR:

ifeq ($(SHLIB_EXT),dylib)

version_X := $(shell exec expr 256 '*' $(version_l) + $(subst .,,$(suffix $(version_M))))
version_XY := $(version_X)$(suffix $(version_m))
version_XYZ := $(version_XY)$(suffix $(version))

ifneq ($(exthome),)
global-links: $(SHARED_LIBS:lib%.dylib.xyzzy=$(DESTDIR)$(sproot)/library.so/lib%.$(version_X).dylib)

$(DESTDIR)$(sproot)/library.so/lib%.$(version_X).dylib: $(DESTDIR)$(sproot)/library.so/lib%.$(version_M).dylib
	exec $(INSTALL) -l $(<F) $@
endif

$(DESTDIR)$(dynlibdir)/lib%.dylib $(DESTDIR)$(dynlibdir)/lib%.$(version_X).dylib $(DESTDIR)$(dynlibdir)/lib%.$(version_M).dylib $(DESTDIR)$(dynlibdir)/lib%.$(version).dylib: lib%.dylib.xyzzy
	$(INSTALL) -D -m 755 $< $(@D)/lib$(*F).$(version).dylib && \
	$(INSTALL) -l lib$(*F).$(version).dylib $(@D)/lib$(*F).$(version_M).dylib && \
	$(INSTALL) -l lib$(*F).$(version_M).dylib $(@D)/lib$(*F).$(version_X).dylib && \
	exec $(INSTALL) -l lib$(*F).$(version_X).dylib $(@D)/lib$(*F).dylib

lib%.dylib.xyzzy: $(ALL_DOBJS)
	exec $(CC) -o $@ $(CFLAGS_ALL) $(CFLAGS_SHARED) $(LDFLAGS_ALL) $(LDFLAGS_SHARED) -Wl,-dylib_install_name,$(dynlibdir)/lib$(*F).$(version_M).dylib -Wl,-dylib_compatibility_version,$(version_XY) -Wl,-dylib_current_version,$(version_XYZ) -undefined dynamic_lookup $^ $(EXTRA_LIBS) $(SOCKET_LIB) $(SPAWN_LIB) $(SYSCLOCK_LIB) $(TAINNOW_LIB) $(TIMER_LIB) $(UTIL_LIB) $(LDLIBS)
endif

