SRCDIR=../..
BUILDDIR = $(SRCDIR)

include $(BUILDDIR)/Makefile.config

INCLUDE = -I$(SRCDIR)/shhopt -I. -Icodec -Iinput -Ioutput -Ilib

BINARIES = pnmtofiasco fiascotopnm

MANUALS1 = $(BINARIES)

all: $(BINARIES)

pnmtofiasco_OBJECTS = binerror.o cwfa.o getopt.o getopt1.o params.o
fiascotopnm_OBJECTS =  binerror.o dwfa.o getopt.o getopt1.o params.o

OBJECTS = binerror.o cwfa.o getopt.o getopt1.o params.o dwfa.o

FIASCOLIBS = codec/libfiasco_codec.a input/libfiasco_input.a \
	    output/libfiasco_output.a lib/libfiasco_lib.a 

pnmtofiasco: $(pnmtofiasco_OBJECTS) $(FIASCOLIBS) $(LIBOPT) $(NETPBMLIB)
	$(LD) $(LDFLAGS) -o $@ $(pnmtofiasco_OBJECTS) \
	`$(LIBOPT) $(FIASCOLIBS)` \
	 -L$(SRCDIR)/lib/share/ $(NETPBMLIB) -lm $(CDEBUG)

fiascotopnm: $(fiascotopnm_OBJECTS) $(FIASCOLIBS) $(LIBOPT)
	$(LD) $(LDFLAGS) -o $@ $(fiascotopnm_OBJECTS) \
	`$(LIBOPT) $(FIASCOLIBS)` \
	 -L$(SRCDIR)/lib/share/ $(NETPBMLIB) -lm $(CDEBUG)

$(pnmtofiasco_OBJECTS) $(fiascotopnm_OBJECTS): \
	fiasco.h config.h

codec/libfiasco_codec.a: FORCE
	$(MAKE) -C codec libfiasco_codec.a

input/libfiasco_input.a: FORCE
	$(MAKE) -C input libfiasco_input.a

output/libfiasco_output.a: FORCE
	$(MAKE) -C output libfiasco_output.a

lib/libfiasco_lib.a: FORCE
	$(MAKE) -C lib libfiasco_lib.a


include $(SRCDIR)/Makefile.common

.PHONY: clean
clean: clean.common
	$(MAKE) -C codec clean
	$(MAKE) -C input clean
	$(MAKE) -C output clean
	$(MAKE) -C lib clean

FORCE:

