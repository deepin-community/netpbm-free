SRCDIR = ../../..
BUILDDIR = $(SRCDIR)

include $(BUILDDIR)/Makefile.config

OBJECTS = arith.o bit-io.o dither.o error.o image.o list.o misc.o rpf.o

INCLUDE = -I$(SRCDIR)/shhopt -I..

all: libfiasco_lib.a

libfiasco_lib.a: $(OBJECTS)
	$(AR) -rc $@ $(OBJECTS)
	$(RANLIB) $@

.PHONY: clean
clean: clean.common

include $(SRCDIR)/Makefile.common
