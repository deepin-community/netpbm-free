SRCDIR = ../..
BUILDDIR = $(SRCDIR)
include $(BUILDDIR)/Makefile.config

INCLUDE = -I$(SRCDIR)/shhopt -I$(SRCDIR)/include/

all: pbmtoppa

merge: pbmtoppa

pbmtoppa: pbmtoppa.o ppa.o pbm.o cutswath.o $(NETPBMLIB)
	$(LD) $(LDFLAGS) -o pbmtoppa pbmtoppa.o ppa.o pbm.o cutswath.o \
	  -L$(SRCDIR)/lib/shared/ $(NETPBMLIB)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

BINARIES=pbmtoppa
MANUALS1=pbmtoppa

include $(SRCDIR)/Makefile.common

clean: clean.common

FORCE:
