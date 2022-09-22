/* libpbm3.c - pbm utility library part 3
**
** Copyright (C) 1988 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

#include "pbm.h"
#include "libpbm.h"

static void pbm_writepbmrowplain ARGS((FILE* file, bit* bitrow, int cols));
static void pbm_writepbmrowraw ARGS((FILE* file, bit* bitrow, int cols));



void
pbm_writepbminit( file, cols, rows, forceplain )
    FILE* file;
    int cols, rows;
    int forceplain;
    {
    if ( ! forceplain ) {
	fprintf( file, "%c%c\n%d %d\n", PBM_MAGIC1, RPBM_MAGIC2, cols, rows );
#ifdef VMS
        set_outfile_binary();
#endif
        }
    else
	fprintf( file, "%c%c\n%d %d\n", PBM_MAGIC1, PBM_MAGIC2, cols, rows );
    }



static void
pbm_writepbmrowraw( file, bitrow, cols )
    FILE* file;
    bit* bitrow;
    int cols;
    {
    register int col, bitshift;
    register unsigned char item;
    register bit* bP;

    bitshift = 7;
    item = 0;
    for ( col = 0, bP = bitrow; col < cols; ++col, ++bP )
	{
	if ( *bP )
	    item += 1 << bitshift;
	--bitshift;
	if ( bitshift == -1 )
	    {
	    (void) putc( item, file );
	    bitshift = 7;
	    item = 0;
	    }
	}
    if ( bitshift != 7 )
	(void) putc( item, file );
    }



static void
pbm_writepbmrowplain( file, bitrow, cols )
    FILE* file;
    bit* bitrow;
    int cols;
    {
    register int col, charcount;
    register bit* bP;

    charcount = 0;
    for ( col = 0, bP = bitrow; col < cols; ++col, ++bP )
	{
	if ( charcount >= 70 )
	    {
	    (void) putc( '\n', file );
	    charcount = 0;
	    }
	(void) putc( *bP ? '1' : '0', file );
	++charcount;
	}
    (void) putc( '\n', file );
    }



void
pbm_writepbmrow( file, bitrow, cols, forceplain )
    FILE* file;
    bit* bitrow;
    int cols;
    int forceplain;
    {
    if ( ! forceplain )
	pbm_writepbmrowraw( file, bitrow, cols );
    else
	pbm_writepbmrowplain( file, bitrow, cols );
    }



void
pbm_writepbmrow_packed(FILE * const file, 
                       const unsigned char * const packed_bits,
                       const int cols, const int forceplain) {

    if (!forceplain) {
        int bytes_written;
        bytes_written = fwrite(packed_bits, 1, pbm_packed_bytes(cols), file);
        if (bytes_written < pbm_packed_bytes(cols)) 
            pm_error("I/O error writing packed row to raw PBM file.");
    } else {
        bit *bitrow;
        int col;

        bitrow = pbm_allocrow(cols);

        for (col = 0; col < cols; ++col) 
            bitrow[col] = 
                packed_bits[col/8] & (0x80 >> (col%8)) ? PBM_BLACK : PBM_WHITE;
        pbm_writepbmrowplain(file, bitrow, cols);
        pbm_freerow(bitrow);
    }
}



void
pbm_writepbm( file, bits, cols, rows, forceplain )
    FILE* file;
    bit** bits;
    int cols, rows;
    int forceplain;
    {
    int row;

    pbm_writepbminit( file, cols, rows, forceplain );

    for ( row = 0; row < rows; ++row )
	pbm_writepbmrow( file, bits[row], cols, forceplain );
    }
