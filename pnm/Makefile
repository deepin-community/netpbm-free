SRCDIR = ..
BUILDDIR = ..
include $(BUILDDIR)/Makefile.config

URTDIR = $(BUILDDIR)/urt
LIBURT = librle.a

INCLUDE = -I$(SRCDIR)/shhopt
ifneq ($(TIFFHDR_DIR),NONE)
  INCLUDE += -I$(TIFFHDR_DIR)
endif
ifneq ($(PNGHDR_DIR),NONE)
  INCLUDE += -I$(PNGHDR_DIR)
  INCLUDE += -I$(ZHDR_DIR)
endif
ifneq ($(JPEGHDR_DIR),NONE)
  INCLUDE += -I$(JPEGHDR_DIR)
endif
ifneq ($(URTHDR_DIR),NONE)
  INCLUDE += -I$(URTHDR_DIR)
endif

ifeq ($(JPEGLIB_DIR),NONE)
  JPEGLD =
else
  JPEGLD = -L$(JPEGLIB_DIR) -ljpeg
  ifeq ($(NEED_RUNTIME_PATH),Y)
    JPEGLD += -R$(JPEGLIB_DIR)
  endif
endif

ifeq ($(PNGLIB_DIR),NONE)
  PNGLD =
else
  PNGLD = -L$(PNGLIB_DIR) -lpng
  ifeq ($(NEED_RUNTIME_PATH),Y)
    PNGLD += -R$(PNGLIB_DIR)
  endif
endif

ifeq ($(URTLIB_DIR),NONE)
  URTLD =
else
  URTLD = -L$(URTLIB_DIR) -lrle
  ifeq ($(NEED_RUNTIME_PATH),Y)
    URTLD += -R$(URTLIB_DIR)
  endif
endif

ifeq ($(TIFFLIB_DIR),NONE)
  TIFFLD =
else
  TIFFLD = -L$(TIFFLIB_DIR) -ltiff
  ifeq ($(NEED_RUNTIME_PATH),Y)
    TIFFLD += -R$(TIFFLIB_DIR)
  endif
endif

ifeq ($(ZLIB_DIR),NONE)
  ZLD =
else
  ZLD = -L$(ZLIB_DIR) -lz
  ifeq ($(NEED_RUNTIME_PATH),Y)
    ZLD += -R$(ZLIB_DIR)
  endif
endif



# We tend to separate out the build targets so that we don't have
# any more dependencies for a given target than it really needs.
# That way, if there is a problem with a dependency, we can still
# successfully build all the stuff that doesn't depend upon it.
# This package is so big, it's useful even when some parts won't 
# build.

PORTBINARIES =  bmptopnm fitstopnm gemtopnm giftopnm pamcut \
		pamdeinterlace pamdice pamfile pamoil pamstretch \
		pnmalias pnmarith pnmcat pnmcomp pnmconvol pnmcrop \
		pnmcut pnmdepth pnmenlarge pnmfile pnmflip pnmhisteq \
		pnmhistmap pnminvert pnmmontage pnmpad pnmpaste \
		pnmnorm \
		pnmremap pnmsmooth pnmsplit \
		pnmtile pnmtoddif pnmtofits \
		pnmtoplainpnm pnmtops pnmtorast \
		pnmtosgi pnmtosir pnmtoxwd pstopnm \
		pamstack \
		rasttopnm sgitopnm sirtopnm xwdtopnm zeisstopnm
MATHBINARIES =	pnmcolormap pnmgamma pnmnlfilt pnmpsnr pnmrotate \
	pnmscale pnmscalefixed pnmshear

# We don't include programs that have special library dependencies in the
# merge scheme, because we don't want those dependencies to prevent us
# from building all the other programs.

NOMERGEBINARIES = 
ifneq ($(PNGHDR_DIR),NONE)
  ifneq ($(PNGLIB_DIR),NONE)
    NOMERGEBINARIES += pnmtopng pngtopnm
  endif
endif
ifneq ($(JPEGHDR_DIR),NONE)
  ifneq ($(JPEGLIB_DIR),NONE)
    NOMERGEBINARIES += jpegtopnm pnmtojpeg
  endif
endif
ifneq ($(TIFFHDR_DIR),NONE)
  ifneq ($(TIFFLIB_DIR),NONE)
    NOMERGEBINARIES += tifftopnm pnmtotiff pnmtotiffcmyk
  endif
endif
ifneq ($(URTHDR_DIR),NONE)
  ifneq ($(URTLIB_DIR),NONE)
    NOMERGEBINARIES +=  rletopnm pnmtorle
  endif
endif

MERGEBINARIES = $(PORTBINARIES) $(MATHBINARIES)


BINARIES = $(MERGEBINARIES) $(NOMERGEBINARIES)
SCRIPTS = anytopnm pamstretch-gen pnmindex pnmmargin pnmquant 
# MERGE_ALIASES is additional names to be linked to the merge binary which are
# alternate names for a program whose source is called something else.
# Every name here must be referenced in pnmmerge.c.
MERGE_ALIASES = pnmnoraw gemtopbm

OBJECTS = $(patsubst %, %.o, $(BINARIES)) exif.o

MERGE_OBJECTS = $(patsubst %,%.o2, $(MERGEBINARIES))

MANUALS1 = $(BINARIES) $(SCRIPTS)
MANUALS3 = libpnm
MANUALS5 = pam pnm

MERGENAME = pnmmerge

# The Tiff library references math functions.
# dsc@clara.net reports his Tiff library depending on Zlib on 2001.12.26.
# it never did before.
tifftopnm pnmtotiff pnmtotiffcmyk: %: %.o $(NETPBMLIB)
	$(LD) $(LDFLAGS) -o $@ $@.o $(NETPBMLIB) \
	  $(TIFFLD) $(ZLD) $(JPEGLD) -lm $(TIFFLIB_LDFLAGS) $(CDEBUG)

pngtopnm pnmtopng: %: %.o $(NETPBMLIB)
	$(LD) $(LDFLAGS) -o $@ $@.o $(NETPBMLIB) \
	  $(PNGLD) $(ZLD) -lm $(CDEBUG)

jpegtopnm: %: %.o exif.o $(NETPBMLIB)
	$(LD) $(LDFLAGS) -o $@ $< exif.o $(NETPBMLIB) $(JPEGLD) \
	   -lm $(CDEBUG) 

pnmtojpeg: %: %.o $(NETPBMLIB)
	$(LD) $(LDFLAGS) -o $@ $@.o $(NETPBMLIB) \
	  -L$(JPEGLIB_DIR) -ljpeg $(CDEBUG)

ifeq ($(URTLIB_DIR),$(URTDIR))
# We're using the internal URT library, so make sure it's built
URTLIB_DEP = $(URTDIR)/$(LIBURT)
else
# It's user's own URT library, so if it doesn't exist now -- too bad.
URTLIB_DEP = 
endif

rletopnm pnmtorle: %: %.o $(NETPBMLIB) $(URTLIB_DEP)
	$(LD) $(LDFLAGS) -o $@ $@.o $(NETPBMLIB) \
	  $(URTLD) $(CDEBUG)

$(URTDIR)/$(LIBURT): FORCE
	$(MAKE) -C $(URTDIR) $(LIBURT)

include $(SRCDIR)/Makefile.common

.PHONY: clean
clean: clean.common
	$(MAKE) -C fiasco clean
	$(MAKE) -C pnmtopalm clean

FORCE:
