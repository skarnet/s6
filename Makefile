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

define library_definition
LIB$(1) := lib$(2).$(if $(DO_ALLSTATIC),a,so).xyzzy
ifdef DO_SHARED
SHARED_LIBS += lib$(2).so.xyzzy
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

-include config.mak
include package/targets.mak

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
	exec rm -rf /tmp/$$package-$$version

strip: $(ALL_LIBS) $(ALL_BINS)
ifneq ($(strip $(STATIC_LIBS)),)
	exec $(STRIP) -x -R .note -R .comment $(STATIC_LIBS)
endif
ifneq ($(strip $(ALL_BINS)$(SHARED_LIBS)),)
	exec $(STRIP) -R .note -R .comment $(ALL_BINS) $(SHARED_LIBS)
endif

install: install-dynlib install-libexec install-bin install-symlinks install-lib install-include install-pkgconfig
install-dynlib: $(SHARED_LIBS:lib%.so.xyzzy=$(DESTDIR)$(dynlibdir)/lib%.so)
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

$(DESTDIR)$(dynlibdir)/lib%.so $(DESTDIR)$(dynlibdir)/lib%.so.$(version_M): lib%.so.xyzzy
	$(INSTALL) -D -m 755 $< $@.$(version) && \
	$(INSTALL) -l $(@F).$(version) $@.$(version_M) && \
	exec $(INSTALL) -l $(@F).$(version_M) $@

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
