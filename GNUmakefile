# Makefile for Netpbm
 
# Configuration should normally be done in the included file Makefile.config.

# Targets in this file:
#
#   nonmerge:     Build everything, in the source directory.
#   merge:        Build everything as merged executables, in the source dir
#   install:      install-run + install-dev
#   install-run:  install stuff needed to run Netpbm programs
#   install-dev:  Install stuff needed to develop/build programs that use
#                 Netpbm libraries:  headers and static libraries.
#   install.bin:  Install executables
#   install.lib:  Install Netpbm shared libraries
#   install.man:  Install man pages
#   install.staticlib:  Install Netpbm static libraries
#   install.sharedlibstub:  Install the ".so" file - the file used at link
#                 time to prepare a program for runtime linking of a library
#   install.hdr:  Install Netpbm library interface headers

# Neue Targets
# build
#  .bin
#  .lib
#   .static
#   .shared
# install
#  .bin
#  .lib
#   .static
#   .old-static (no auto)
#   .shared
#    .lib
#    .devel
#    .old-devel (no auto)
#   .old-shared (no auto)
#    .lib
#    .devel
#   .hdr
#  .man
#   .bin
#   .lib
#   .old-lib (no auto)
#   .general

#   the DEFAULT_TARGET variable set by Makefile.config.

# About the "merge" target: Normally the Makefiles build and install
# separate executables for each program.  However, on some systems
# (especially those without shared libraries) this can mean a lot of
# space.  In this case you might try building a "merge" instead.  The
# idea here is to link all the programs together into one huge
# executable, along with a tiny dispatch program that runs one of the
# programs based on the command name with which it was invoked.  You
# install the merged executable with a file system link for the name
# of each program it includes.  On a Sun3 under SunOS 3.5 the space
# for executables went from 2970K to 370K in an older Netpbm.
# On a Linux x86 system with Netpbm 8.4, it saved 615K.

# To build a "merge" system, just set DEFAULT_TARGET to "merge" instead
# of "nomerge" in Makefile.config.  In that case, you should probably also
# set NETPBMLIBTYPE to "unixstatic", since shared libraries don't do you 
# much good.

# The CURDIR variable presents a problem because it was introduced in
# GNU Make 3.77.  We need the CURDIR variable in order for our 'make
# -C xxx -f xxx' commands to work.  If we used the obvious alternative
# ".", that wouldn't work because it would refer to the directory
# named in -C, not the directory the make file you are reading is
# running in.  The -f option is necessary in order to have separate
# source and object directories in the future.

ifeq ($(CURDIR)x,x)
$(GOALS):
	@echo "YOU NEED AT LEAST VERSION 3.77 OF GNU MAKE TO BUILD NETPBM."
	@echo "Netpbm's makefiles need the CURDIR variable that was "
	@echo "introduced in 3.77.  Your version does not have CURDIR."
	@echo
	@echo "You can get a current GNU Make via http://www.gnu.org/software"
	@echo 
	@echo "If upgrading is impossible, try modifying GNUMakefile and "
	@echo "Makefile.common to replace \$(CURDIR) with \$(shell /bin/pwd) "
else


ifeq ($(SRCDIR)x,x)
  SRCDIR := $(CURDIR)
endif
BUILDDIR = .
# Some day, we'll figure out how to make BUILDDIR != SRCDIR

include Makefile.config

all: build

build: build.bin build.lib
build.lib: build.lib.static build.lib.shared
build.old-lib: build.lib.old-static build.lib.old-shared
install: install.bin install.lib install.man
install.lib: install.lib.static install.lib.shared install.lib.hdr
install.lib.old-shared: install.lib.old-shared.lib install.lib.old-shared.devel
install.lib.shared: install.lib.shared.lib install.lib.shared.devel
install.man: install.man.bin install.man.lib install.man.general
build.lib.default: build.lib.shared # should be an option

GOALS := \
 build.bin build.lib \
 build.lib.static build.lib.shared \
 install.bin install.lib install.man \
 install.lib.static install.lib.shared install.lib.hdr \
 install.lib.shared.lib install.lib.shared.devel \
 install.man.bin install.man.lib install.man.general \
 clean
 
SUBGOALS := \
 build.bin \
 install.bin \
 install.man.bin install.man.general install.man.old-lib \
 clean

.PHONY: depend
.depend depend:
	@buildtools/depend.pl > .depend

include .depend

lib/static lib/shared:
	mkdir -p $@

.PHONY: config
config: Makefile.config

Makefile.config: $(SRCDIR)/Makefile.config.in
	$(SRCDIR)/configure $<
	@echo
	@echo

.PHONY: install-run
ifeq ($(DEFAULT_TARGET),merge)
install-run: install.merge install.lib install.man
else
install-run: install.bin install.lib install.man
endif

MERGELIST = $(patsubst %,%.merge,$(SUBDIRS))

.PHONY: $(MERGELIST)
$(MERGELIST):
	$(MAKE) -C $(patsubst %.merge,%,$@) \
	  -f $(SRCDIR)/$(patsubst %.merge,%,$@)/Makefile   merge
# e.g.  make -C pbm -f /usr/src/netpbm/pbm/Makefile merge

.PHONY: merge
merge:  $(MERGELIST)

SUBDIRS = pbm pgm ppm pnm include deprecated

#$(GOALS): $(patsubst %,x$@-%, $(SUBDIRS))
$(SUBGOALS):%: local-% pbm-% pgm-% pnm-% ppm-% include-% deprecated-%
$(SUBGOALS):%: pbmtoppa-% pnmtopalm-% ppmtompeg-%
ifneq ($(BUILD_FIASCO), N)
$(SUBGOALS):%: fiasco-%
endif


$(patsubst %, %-build.bin, $(SUBDIRS)): buildtools/libopt build.lib.shared
$(patsubst %, %-install.bin, $(SUBDIRS)): buildtools/libopt

local-%:
	@true
pbm-%:
	$(MAKE) -C pbm $*
pgm-%:
	$(MAKE) -C pgm $*
pnm-%:
	$(MAKE) -C pnm $*
ppm-%:
	$(MAKE) -C ppm $*
include-%:
	$(MAKE) -C include $*
deprecated-%:
	$(MAKE) -C deprecated $*
pbmtoppa-%:
	$(MAKE) -C pbm/pbmtoppa $*
fiasco-%:
	$(MAKE) -C pnm/fiasco $*
pnmtopalm-%:
	$(MAKE) -C pnm/pnmtopalm $*
ppmtompeg-%:
	$(MAKE) -C ppm/ppmtompeg $*

buildtools/libopt:
	$(MAKE) -C buildtools libopt
build.lib.static: \
	lib/static/libnetpbm.a
build.lib.old-static: \
	lib/static/libpbm.a \
	lib/static/libpgm.a \
	lib/static/libpnm.a \
	lib/static/libppm.a
build.lib.shared: \
	lib/shared/libnetpbm.so
build.lib.old-shared: \
	lib/shared/libpbm.so \
	lib/shared/libpgm.so \
	lib/shared/libpnm.so \
	lib/shared/libppm.so
install.lib.hdr: local-install.lib.hdr include-install.lib.hdr \
	deprecated-install.lib.hdr ppm-install.lib.hdr
install.man.lib: install.man.old-lib

local-install.man.general:
	$(MANCP) $(SRCDIR)/netpbm.1 \
            $(INSTALLMANUALS1)/netpbm.$(SUFFIXMANUALS1) ;
	$(MANCP) $(SRCDIR)/pbm/pbmfilters.1 \
            $(INSTALLMANUALS1)/pbmfilters.$(SUFFIXMANUALS1) ;
local-install.man.lib:
	$(MANCP) $(SRCDIR)/libnetpbm.3 \
            $(INSTALLMANUALS3)/libnetpbm.$(SUFFIXMANUALS3) ;

# Note that you might install the development package and NOT the runtime
# package.  If you have a special system for building stuff, maybe for 
# multiple platforms, that's what you'd do.  Ergo, install.lib is here even
# though it is also part of the runtime install.

local-install.lib.hdr:
# See notes in Makefile.common about how $(INSTALL) varies from one 
# platform to another.
	$(INSTALL) -c -m $(INSTALL_PERM_HDR) \
	    $(SRCDIR)/shhopt/shhopt.h $(INSTALLHDRS)/netpbm-shhopt.h

local-clean:
	for i in urt shhopt buildtools; do \
	    $(MAKE) -C $$i -f $(SRCDIR)/$$i/Makefile clean ; \
	done
	-rm lib/*/*

.PHONY: distclean
distclean: clean
	rm -f Makefile.config

shhopt/netpbm-shhopt.h:
	ln -sf shhopt.h shhopt/netpbm-shhopt.h

# The following endif is for the else block that contains virtually the
# whole file, for the test of the existence of CURDIR.
endif
