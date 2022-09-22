all:

include ../Makefile.config

install.hdr install.merge install.man install.lib install.staticlib dep clean:
build.bin install.man.bin install.man.general install.man.lib clean:
install.man.old-lib:

BINS=ppmtomap

install.lib.hdr:
	ln -sf pm_config.h $(INSTALLHDRS)/pbmplus.h

install.man.bin:
	for i in $(BINS); do \
      $(MANCP) $$i.1 $(INSTALLMANUALS1)/$$i.$(SUFFIXMANUALS1) ; \
	done
	ln -sf bmptopnm.1.gz $(INSTALLMANUALS1)/bmptoppm.1.gz
	ln -sf pamstretch.1.gz $(INSTALLMANUALS1)/pnminterp.1.gz
	ln -sf pamstretch-gen.1.gz $(INSTALLMANUALS1)/pnminterp-gen.1.gz
	# pnmnorm replace pgmnorm in March 2002.
	ln -sf pnmnorm.1.gz $(INSTALLMANUALS1)/pgmnorm.1.gz
	ln -sf pnmnorm.1.gz $(INSTALLMANUALS1)/ppmnorm.1.gz
	# pamoil replaced pgmoil in June 2001.
	ln -sf pamoil.1.gz $(INSTALLMANUALS1)/pgmoil.1.gz
	ln -sf pnmtoplainpnm.1.gz $(INSTALLMANUALS1)/pnmnoraw.1.gz
	ln -sf gemtopnm.1.gz $(INSTALLMANUALS1)/gemtopbm.1.gz
	ln -sf pnmtojpeg.1.gz $(INSTALLMANUALS1)/ppmtojpeg.1.gz

install.bin:
	for i in $(BINS); do \
	  $(INSTALL)  -c -m $(INSTALL_PERM_BIN) $$i $(INSTALLBINARIES)/$$i; \
	done
	# In March 2002, bmptopnm replaced bmptoppm
	ln -sf bmptopnm $(INSTALLBINARIES)/bmptoppm
	ln -sf pamstretch $(INSTALLBINARIES)/pnminterp
	ln -sf pamstretch-gen $(INSTALLBINARIES)/pnminterp-gen
	# pamoil replaced pgmoil in June 2001.
	ln -sf pamoil $(INSTALLBINARIES)/pgmoil
	ln -sf pnmtoplainpnm $(INSTALLBINARIES)/pnmnoraw
	ln -sf gemtopnm $(INSTALLBINARIES)/gemtopbm
	# In October 2001, pnmtojpeg replaced ppmtojpeg
	ln -sf pnmtojpeg $(INSTALLBINARIES)/ppmtojpeg
	# In March 2002, pnmnorm replaced ppmnorm and pgmnorm
	ln -sf pnmnorm $(INSTALLBINARIES)/ppmnorm
	ln -sf pnmnorm $(INSTALLBINARIES)/pgmnorm
