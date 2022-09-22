SRCDIR = ../../..
BUILDDIR = $(SRCDIR)

include $(BUILDDIR)/Makefile.config

OBJECTS =  matrices.o mc.o nd.o tree.o weights.o write.o
INCLUDE = -I.. -I../lib -I../codec

all: libfiasco_output.a

libfiasco_output.a: $(OBJECTS)
	$(AR) -rc $@ $(OBJECTS)
	$(RANLIB) $@

all: $(OBJECTS)

.PHONY: clean
clean: clean.common

include $(SRCDIR)/Makefile.common
