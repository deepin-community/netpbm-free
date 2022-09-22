SRCDIR = ../../..
BUILDDIR = $(SRCDIR)

include $(BUILDDIR)/Makefile.config

OBJECTS =  basis.o matrices.o mc.o nd.o read.o tree.o weights.o
INCLUDE = -I.. -I../lib -I../codec

all: libfiasco_input.a

libfiasco_input.a: $(OBJECTS)
	$(AR) -rc $@ $(OBJECTS)
	$(RANLIB) $@

.PHONY: clean
clean: clean.common

include $(SRCDIR)/Makefile.common
