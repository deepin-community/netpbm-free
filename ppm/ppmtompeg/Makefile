SRCDIR = ../..
BUILDDIR = $(SRCDIR)
include $(BUILDDIR)/Makefile.config

ifeq ($(JPEGLIB_DIR),NONE)
  JPEGLD =
  # 'nojpeg' is a module that implements all the jpeg access routines as
  # error messages that tell you we don't have jpeg capability
  JPEG_MODULE = nojpeg
else
  JPEGLD = -L$(JPEGLIB_DIR) -ljpeg
  ifeq ($(NEED_RUNTIME_PATH),Y)
    JPEGLD += -R$(JPEGLIB_DIR)
  endif
  # 'jpeg' is a module that accesses J-movies via the JPEG library.
  JPEG_MODULE = jpeg
endif

# shhopt is in the following for the benefit of make dep
INCLUDE = -I. -Iheaders -I$(SRCDIR)/shhopt
ifneq ($(JPEGHDR_DIR),NONE)
  INCLUDE += -I$(JPEGHDR_DIR)
endif

# use -DLONG_32 iff
#	1) long's are 32 bits and
#	2) int's are not
#
# if you are using a non-ANSI compiler, then use:
#	-DNON_ANSI_COMPILER
#
# one other option:
#	-DHEINOUS_DEBUG_MODE
#

MP_BASE_OBJS = mfwddct.o postdct.o huff.o bitio.o mheaders.o
MP_ENCODE_OBJS = iframe.o pframe.o bframe.o psearch.o bsearch.o block.o 
MP_OTHER_OBJS = mpeg.o subsample.o param.o rgbtoycc.o \
	readframe.o combine.o jrevdct.o frame.o fsize.o frametype.o \
	specifics.o rate.o opts.o
ifeq ($(NETWORKLD),NONE)
  MP_PARALLEL_OBJS = noparallel.o
  LIB_NETWORKLD =
else
  MP_PARALLEL_OBJS = parallel.o
  LIB_NETWORKLD = $(NETWORKLD)
endif
MP_ALL_OBJS = $(MP_BASE_OBJS) $(MP_OTHER_OBJS) $(MP_ENCODE_OBJS) \
	      $(MP_PARALLEL_OBJS) $(JPEG_MODULE).o main.o
MP_INCLUDE = mproto.h mtypes.h huff.h bitio.h
MP_MISC = Makefile huff.table parse_huff.pl

.PHONY: all merge
all merge: ppmtompeg

ppmtompeg: $(MP_ALL_OBJS) $(NETPBMLIB)
	$(LD) -o $@ $(LDFLAGS) $(INCLUDE) \
          $(MP_ALL_OBJS) $(NETPBMLIB) $(JPEGLD) $(LIB_NETWORKLD) \
	  -lm \
	  $(CDEBUG)

profile: $(MP_ALL_OBJS) $(NETPBMLIB)
	$(LD) -o $@ -Bstatic -pg $(LDFLAGS) $(INCLUDE) \
          $(MP_ALL_OBJS) $(NETPBMLIB) $(JPEGLD) $(LIB_NETWORKLD) \
	-lm \
	  $(CDEBUG)


$(MP_ALL_OBJS) headers.o:%.o:%.c # $(HEADERLINKS)
	$(CC) -c $< -o $@ $(CFLAGS) $(INCLUDE) $(CDEBUG)


BINARIES = ppmtompeg
NOMERGEBINARIES = ppmtompeg
SCRIPTS = 
MANUALS1 = ppmtompeg

include $(SRCDIR)/Makefile.common

#########
# OTHER #
#########

#
# Perl is necessary if you want to modify the Huffman RLE encoding table.
#
PERL = perl

# The following stuff is for the Huffman encoding tables.  It's commented-out
# because you probably don't want to change this.  If you do, then uncommment
# it.
#
# huff.h: huff.c
#
# huff.c: parse_huff.pl huff.table
#	$(PERL) parse_huff.pl huff.table

MP_ALL_SRCS = $(patsubst %.o, %.c, $(MP_ALL_OBJS))

depend: huff.c
	makedepend -- $(CFLAGS) $(INCLUDE) -- $(MP_ALL_SRCS)

wc:;		wc -l *.[ch] headers/*.h *.pl *.table
ci:;		ci -l $(MP_ALL_SRCS) $(MP_INCLUDE) $(MP_MISC)
tags: $(MP_ALL_SRCS)
	ctags -t $(MP_ALL_SRCS)
	etags -f TAGS -t $(MP_ALL_SRCS) headers/*.h

new:    clean
	$(MAKE) depend
	$(MAKE) tags

clean: clean.common

#
# WARNING: this assumes you're using GNU indent...
#
indent:;	indent -T FILE -T int8 -T int16 -T int32 -T uint8 -T uint16 -T uint32  -T BitBucket -T MpegFrame -T Block -T FlatBlock $(MP_ALL_SRCS)

spotless: clean	
	rm -f huff.c huff.h *.pure.a

FORCE:

# 
# Copyright (c) 1995 The Regents of the University of California.
# All rights reserved.
# 
# Permission to use, copy, modify, and distribute this software and its
# documentation for any purpose, without fee, and without written agreement is
# hereby granted, provided that the above copyright notice and the following
# two paragraphs appear in all copies of this software.
# 
# IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY FOR
# DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
# OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF
# CALIFORNIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# 
# THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
# AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
# ON AN "AS IS" BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
# PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
# 


##############################################################################
# DO NOT DELETE THIS LINE -- make depend depends on it.
