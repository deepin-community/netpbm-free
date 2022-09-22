SRCDIR = ..
BUILDDIR = ..
include $(BUILDDIR)/Makefile.config

INCLUDE= -I$(SRCDIR)/shhopt

ifneq ($(LINUXSVGAHDR_DIR),NONE)
  INCLUDE += -I$(LINUXSVGAHDR_DIR)
endif

ifeq ($(LINUXSVGALIB_DIR),NONE)
  LINUXSVGALD =
else
  LINUXSVGALD = -L$(LINUXSVGALIB_DIR) -lvga -lm
endif

PORTBINARIES =	411toppm eyuvtoppm gouldtoppm ilbmtoppm imgtoppm \
		leaftoppm mtvtoppm pcxtoppm pgmtoppm pi1toppm \
		pjtoppm neotoppm \
		ppm3d ppmbrighten ppmchange ppmcolormask ppmcolors \
		ppmdim ppmdist ppmdither \
		ppmflash ppmhist ppmmake ppmmix \
		ppmquant ppmrelief ppmshift ppmspread ppmtoacad \
		ppmtobmp ppmtoeyuv ppmtoleaf \
		ppmtoicr ppmtoilbm ppmtolj ppmtomitsu \
		ppmtopcx ppmtopgm ppmtopi1 ppmtopict ppmtopj \
		ppmtopuzz ppmtorgb3 ppmtosixel \
		ppmtotga ppmtouil ppmtowinicon ppmtoxpm ppmtoyuv \
		ppmtoyuvsplit ppmtv ppmtoneo \
		qrttoppm rawtoppm rgb3toppm sldtoppm spctoppm \
		sputoppm tgatoppm winicontoppm ximtoppm xpmtoppm xvminitoppm \
		yuvtoppm yuvsplittoppm ppmtogif

#		picttoppm 
# We don't build vidtoppm by default, because it requires special libraries
# and there is no known requirement for vidtoppm.


MERGEBINARIES = $(PORTBINARIES) $(MATHBINARIES)
NOMERGEBINARIES =
ifneq ($(LINUXSVGAHDR_DIR),NONE)
  ifneq ($(LINUXSVGALIB_DIR),NONE)
    NOMERGEBINARIES += ppmsvgalib
  endif
endif

MATHBINARIES = ppmcie ppmforge ppmlabel ppmntsc ppmpat ppmqvga
BINARIES = $(MERGEBINARIES) $(NOMERGEBINARIES) 
SCRIPTS = ppmfade ppmshadow ppmquantall ppmrainbow # ppmtomap

OBJECTS = $(patsubst %, %.o, $(BINARIES))

MERGE_OBJECTS = $(patsubst %,%.o2, $(MERGEBINARIES))

LIBOBJECTS = libppm1.o libppm2.o libppmcmap.o libppm4.o libppm5.o \
	libppmfloyd.o

MANUALS1 = $(BINARIES) $(SCRIPTS)
MANUALS3 = libppm
MANUALS5 = ppm

MERGENAME= ppmmerge

.PHONY: all
all: $(LIBPPM) $(BINARIES) $(EXTRA_STATICLIB)
	$(MAKE) -C ppmtompeg all
.PHONY: merge
merge:      $(MERGENAME) $(NOMERGEBINARIES)
	$(MAKE) -C ppmtompeg all

ppmsvgalib: %: %.o $(NETPBMLIB)
	$(LD) $(LDFLAGS) -o $@ $< $(NETPBMLIB) $(LINUXSVGALD) \
	   $(CDEBUG) 

install.lib.hdr:
	$(INSTALL) -m $(INSTALL_PERM_HDR) ppmdraw.h $(INSTALLHDRS)/ppmdraw.h
			

include $(SRCDIR)/Makefile.common

.PHONY: clean
clean: clean.common
	$(MAKE) -C ppmtompeg clean

FORCE:
