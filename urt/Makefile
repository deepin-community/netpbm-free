SRCDIR = ..

include $(SRCDIR)/Makefile.config

OBJECTS = Runput.o cmd_name.o \
	rle_addhist.o rle_error.o rle_getcom.o rle_getrow.o rle_getskip.o \
	rle_global.o rle_hdr.o rle_open_f.o rle_putcom.o rle_putrow.o \
        rle_row_alc.o \
        scanargs.o vaxshort.o     

all: librle.a

librle.a: $(OBJECTS)
	rm -f $@
	ar rc $@ $^
	$(RANLIB) $@

# Rule for objects.
$(OBJECTS): %.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

.PHONY: clean
clean: 
	rm -f *.o *.s *.i librle.a

