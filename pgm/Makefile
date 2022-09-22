SRCDIR = ..
BUILDDIR = ..
include $(BUILDDIR)/Makefile.config

INCLUDE = -I$(SRCDIR)/shhopt

PORTBINARIES =	asciitopgm bioradtopgm fstopgm hipstopgm \
		lispmtopgm pbmtopgm pgmbentley pgmenhance pgmhist \
		pgmnoise pgmramp pgmslice pgmtofs pgmtolispm \
		pgmtopbm psidtopgm sbigtopgm st4topgm
MATHBINARIES =	pgmcrater pgmedge pgmtexture rawtopgm pgmkernel
BINARIES =	$(PORTBINARIES) $(MATHBINARIES)

OBJECTS = $(patsubst %, %.o, $(BINARIES))
MERGEBINARIES = $(BINARIES)
MERGE_OBJECTS = $(patsubst %,%.o2, $(MERGEBINARIES))

LIBOBJECTS = libpgm1.o libpgm2.o

MANUALS1 =	$(BINARIES)
MANUALS3 =	libpgm
MANUALS5 =	pgm

MERGENAME = pgmmerge

.PHONY: all
all:	$(PGMLIB) $(BINARIES) $(EXTRA_STATICLIB)
.PHONY: merge
merge: $(MERGENAME)

# LIBRARIES

.PHONY: install.merge
install.merge: install.merge.common

include $(SRCDIR)/Makefile.common

.PHONY: clean
clean: clean.common

FORCE:
