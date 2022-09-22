SRCDIR = ..
include $(SRCDIR)/Makefile.config

# $Id: Makefile,v 1.3 2003/08/28 20:12:30 aba-guest Exp $
DIST		= shhopt
VERMAJ		= 1
VERMIN		= 1
VERPAT		= 6
VERSION		= $(VERMAJ).$(VERMIN).$(VERPAT)

# Define SHARED as 1 for Linux shared ELF library
#SHARED		= 1

ifeq ($(SHARED),1)
LIBTARGET	= lib$(DIST).so.$(VERSION)
LIBTARGETSO	= lib$(DIST).so
LIBTARGETSOMAJ	= $(LIBTARGETSO).$(VERMAJ)
CCSHRD		= -fPIC
else
LIBTARGET	= lib$(DIST).a
endif

LIBHEAD		= $(DIST).h
TARGETS		= $(LIBTARGET)

INSTBASEDIR	= /usr/local
INSTLIBDIR	= $(INSTBASEDIR)/lib
INSTINCDIR	= $(INSTBASEDIR)/include
INSTALL		= install -m 644
INSTALLPROG	= install -m 755
MKDIRP		= install -d -m 755

#CC		= gcc
OPTIM		= -O2

INCDIR		= -I.

#CCOPT		= -s -Wall $(OPTIM) $(INCDIR)
CCOPT		= $(CFLAGS) $(CFLAGS_SHLIB) $(INCDIR) $(CDEBUG)

# Object files to store in the library
LIBOBJS		= shhopt.o


all: $(TARGETS)

$(LIBTARGET): $(LIBOBJS)
ifeq ($(SHARED),1)
	$(CC) -shared -Wl,-soname,$(LIBTARGETSOMAJ) -o $(LIBTARGET) $(LIBOBJS)
else
	ar rc $(LIBTARGET) $(LIBOBJS)
	$(RANLIB) $(LIBTARGET) || true
endif

# Note that you may need GNU's -liberty if your libc lacks strtoul
example: $(LIBTARGET) example.o
	$(CC) -o example example.c \
		-L. -I. -L$(INSTLIBDIR) -I$(INSTINCDIR) -lshhopt

.c.o:
	$(CC) $(CCSHRD) -o $@ -c $(CCOPT) $<

depend dep:
	$(CC) $(INCDIR) -MM *.c >depend

install: $(LIBTARGET)
	$(MKDIRP) $(INSTLIBDIR) $(INSTINCDIR)
	$(INSTALL) $(LIBTARGET) $(INSTLIBDIR)
	$(INSTALL) $(LIBHEAD) $(INSTINCDIR)
ifeq ($(SHARED),1)
# The following is a little more complex than seems necessary because of
# the fact that $(SYMLINK) might just be a copy command.
	-(cd $(INSTLIBDIR) && rm -f $(LIBTARGETSOMAJ))
	$(SYMLINK) $(LIBTARGET) $(LIBTARGETSOMAJ)
	-(cd $(INSTLIBDIR) && rm -f $(LIBTARGETSO))
	$(SYMLINK) $(LIBTARGETSOMAJ) $(LIBTARGETSO)
	echo "If you use GNU/Linux, remember to run ldconfig"
endif

clean:
	rm -f *.o *.i *.s core *~ depend $(TARGETS)

chmod:
	chmod a+r *

# To let the author make a distribution. The rest of the Makefile
# should be used by the author only.
LSMFILE		= $(DIST)-$(VERSION).lsm
DISTDIR		= $(DIST)-$(VERSION)
DISTFILE	= $(DIST)-$(VERSION).tar.gz
DISTFILES	= README INSTALL CREDITS ChangeLog \
		  Makefile $(LSMFILE) \
		  $(LIBHEAD) \
		  shhopt.c \
		  example.c

$(LSMFILE): $(DIST).lsm.in
	VER=$(VERSION); \
	DATE=`date "+%d%b%y"|tr '[a-z]' '[A-Z]'`; \
	sed -e "s/VER/$$VER/g;s/DATE/$$DATE/g" $(DIST).lsm.in > $(LSMFILE)

veryclean: clean
	rm -f $(TARGETS) $(DISTFILE) $(LSMFILE) example

dist: $(LSMFILE) chmod
	mkdir $(DISTDIR)
	chmod a+rx $(DISTDIR)
	ln $(DISTFILES) $(DISTDIR)
	tar -cvzf $(DISTFILE) $(DISTDIR)
	chmod a+r $(DISTFILE)
	rm -rf $(DISTDIR)

ifeq (depend,$(wildcard depend))
include depend
endif
