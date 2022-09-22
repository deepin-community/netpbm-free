SRCDIR=../../..
BUILDDIR = $(SRCDIR)

include $(BUILDDIR)/Makefile.config

INCLUDE = -I$(SRCDIR)/shhopt -I. -I.. -I../lib -I../input -I../output

OBJECTS =  approx.o bintree.o coder.o coeff.o \
           control.o decoder.o dfiasco.o domain-pool.o ip.o motion.o mwfa.o \
           options.o prediction.o subdivide.o tiling.o wfalib.o

all: libfiasco_codec.a

libfiasco_codec.a: $(OBJECTS)
	$(AR) -rc $@ $(OBJECTS)
	$(RANLIB) $@

.PHONY: clean
clean: clean.common

include $(SRCDIR)/Makefile.common
