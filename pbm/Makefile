SRCDIR = ..
SRCSUBDIR = $(SRCDIR)/pbm
BUILDDIR = ..

include $(BUILDDIR)/Makefile.config

INCLUDE = -I$(SRCDIR)/shhopt

PORTBINARIES =	atktopbm brushtopbm cmuwmtopbm g3topbm \
		icontopbm macptopbm mdatopbm mgrtopbm \
		pbmclean pbmlife pbmmake pbmmask pbmpscale \
		pbmreduce pbmtext pbmtextps \
		pbmto10x pbmtoascii pbmtoatk \
		pbmtobbnbg pbmtocmuwm \
		pbmtoepsi pbmtoepson \
		pbmtog3 pbmtogem pbmtogo pbmtoicon pbmtolj \
		pbmtomacp pbmtomda pbmtomgr pbmtonokia \
		pbmtopi3 pbmtoplot pbmtopsg3 pbmtoptx pbmtowbmp \
		pbmtox10bm pbmtoxbm pbmtoybm pbmtozinc \
		pbmupc pi3topbm \
		wbmptopbm xbmtopbm ybmtopbm	

ifneq ($(LEX)x,x)
  PORTBINARIES += thinkjettopbm
endif

#pbmpage uses sqrt(), which is sometimes in libc, not libm.  Is it ever
#in libm?
MATHBINARIES =	pbmpage
BINARIES =	$(PORTBINARIES) $(MATHBINARIES)

OBJECTS = $(patsubst %, %.o, $(BINARIES))

MERGEBINARIES = $(BINARIES)
MERGE_OBJECTS = $(patsubst %,%.o2, $(MERGEBINARIES))

# Library objects to be built and linked by Makefile.common:
LIBOBJECTS = libpm.o libpbm1.o libpbm2.o libpbm3.o libpbm4.o libpbm5.o bitio.o
ifneq (${VMS}x,x)
LIBOBJECTS += libpbmvms.o
endif
# Library objects to be linked but not built by Makefile.common:
LIBOBJECTS_X = $(BUILDDIR)/shhopt/shhopt.o

MANUALS1 =	$(BINARIES)
MANUALS3 =	libpbm
MANUALS5 =	pbm

MERGENAME = pbmmerge

all:	build.bin

.PHONY: merge
merge: $(MERGENAME)
	$(MAKE) -C pbmtoppa all

include $(SRCDIR)/Makefile.common

# Note that we create a new compile.h only for the first make after a
# make clean.  This is good enough.  We used to do stamp-date for
# every build of "all" from the Netpbm top directory, but nowhere
# else, so it was really sloppy.

$(BUILDDIR)/shhopt/shhopt.o:
	cd ../shhopt; $(MAKE) shhopt.o
$(BUILDDIR)/shhopt/libshhopt.a:
	cd ../shhopt; $(MAKE) libshhopt.a

.PHONY: clean
clean: clean.common
	$(MAKE) -C pbmtoppa clean

FORCE:
