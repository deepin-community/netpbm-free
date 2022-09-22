SRCDIR = ..
SRCSUBDIR = $(SRCDIR)/buildtools
BUILDDIR = ..
include $(BUILDDIR)/Makefile.config

#libopt is a utility program used by the make files
libopt: libopt.o
	$(LD_FOR_BUILD) -o $@ $<
libopt.o: $(SRCSUBDIR)/libopt.c
	$(CC_FOR_BUILD) -c -DSHLIBPREFIXLIST="\"$(SHLIBPREFIXLIST)\"" -o $@ $<





.PHONY: clean
clean:
	rm -f libopt libopt.o
