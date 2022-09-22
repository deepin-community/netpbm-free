
all:

# HEADERS=netpbm-shhopt.h pam.h
HEADERS=pam.h \
	libpbm.h libpgm.h libppm.h \
	pam.h pbm.h pgm.h \
	pm.h pm_config.h \
	pnm.h ppm.h ppmcmap.h

include ../Makefile.config

install.lib.hdr:
	for i in $(HEADERS); do \
		$(INSTALL) -m $(INSTALL_PERM_HDR) $$i $(INSTALLHDRS)/$$i; done

install.hdr install.merge install.man install.lib install.staticlib dep clean:
build.bin install.man.bin install.man.general install.man.lib clean:
install.bin:
install.man.old-lib:
