SRCDIR = ../..
BUILDDIR = $(SRCDIR)
SRCSUBDIR = $(SRCDIR)/pnm/pnmtopalm

include $(BUILDDIR)/Makefile.config

INCLUDE = -I$(SRCDIR)/shhopt

BINARIES = palmtopnm pnmtopalm
OBJECTS = $(patsubst %, %.o, $(BINARIES)) palmcolormap.o

MANUALS1 = $(BINARIES)

all: $(BINARIES)

$(BINARIES): %: %.o palmcolormap.o $(NETPBMLIB)
	$(LD) $(LDFLAGS) -o $@ $< palmcolormap.o $(NETPBMLIB) \
	  $(CDEBUG)

gen_palm_colormap : $(SRCSUBDIR)/gen_palm_colormap.c palmcolormap.o
	$(CC) $(INCLUDE) $(CFLAGS) $(LDFLAGS) -o $@ $< palmcolormap.o \
	  $(NETPBMLIB) $(CDEBUG)


.PHONY: install.bin-maps
install.bin: install.bin-maps
install.bin-maps:
# The following probably ought to be a separate install.data target, but
# today this is the only place in Netpbm that installs data files, and we're
# too lazy to add install.data to the whole package.  2001.01.07 BJH
	for x in palmcolor8.map palmgray1.map palmgray2.map palmgray4.map ;\
	do \
	   $(INSTALL) -c -m $(INSTALL_PERM_DATA) $(SRCSUBDIR)/$$x \
               $(INSTALLDATA)/$$x; \
	done

include $(SRCDIR)/Makefile.common

.PHONY: clean
clean: clean.common
	rm -f gen_palm_colormap
FORCE:

