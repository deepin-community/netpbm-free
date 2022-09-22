/* libpgm2.c - pgm utility library part 2
**
** Copyright (C) 1989 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

#include "pgm.h"
#include "libpgm.h"

static void putus ARGS((unsigned short n, FILE* file));
static void pgm_writepgmrowplain ARGS((FILE* file, gray* grayrow, int cols, gray maxval));
static void pgm_writepgmrowraw ARGS((FILE* file, gray* grayrow, int cols, gray maxval));

void
pgm_writepgminit(FILE* file, int cols, int rows, gray maxval, 
                 int forceplain) {

    if (maxval > PGM_OVERALLMAXVAL && !forceplain) 
        pm_error("too-large maxval passed to ppm_writeppminit(): %d.\n"
                 "Maximum allowed by the PGM format is %d.",
                 maxval, PGM_OVERALLMAXVAL);

    fprintf(file, "%c%c\n%d %d\n%d\n", 
            PGM_MAGIC1, 
            forceplain || maxval >= 1<<16 ? PGM_MAGIC2 : RPGM_MAGIC2, 
            cols, rows, maxval );
#ifdef VMS
    if (!forceplain)
        set_outfile_binary();
#endif
}



static void
putus(unsigned short n, FILE *file ) {
    if ( n >= 10 )
	putus( n / 10, file );
    (void) putc( n % 10 + '0', file );
    }


void
pgm_writerawsample(FILE *file, const gray val, const gray maxval) {

    if (maxval < 256) {
        /* Samples fit in one byte, so write just one byte */
        int rc;
        rc = putc(val, file);
        if (rc == EOF)
            pm_error("Error writing single byte sample to file");
    } else {
        /* Samples are too big for one byte, so write two */
        int n_written;
        unsigned char outval[2];
        /* We could save a few instructions if we exploited the internal
           format of a gray, i.e. its endianness.  Then we might be able
           to skip the shifting and anding.
           */
        outval[0] = val >> 8;
        outval[1] = val & 0xFF;
        n_written = fwrite(&outval, 2, 1, file);
        if (n_written == 0) 
            pm_error("Error writing double byte sample to file");
    }
}



static void
pgm_writepgmrowraw(FILE *file, gray *grayrow, int cols, gray maxval ) {
    int col;

    for (col = 0; col < cols; ++col) {
#ifdef DEBUG
	if (grayrow[col] > maxval)
	    pm_error( "value out of bounds (%u > %u)", grayrow[col], maxval);
#endif /*DEBUG*/
	pgm_writerawsample(file, grayrow[col], maxval);
	}
}



static void
pgm_writepgmrowplain(FILE *file, gray *grayrow, int cols, gray maxval ) {
    register int col, charcount;
    register gray* gP;

    charcount = 0;
    for ( col = 0, gP = grayrow; col < cols; ++col, ++gP )
	{
	if ( charcount >= 65 )
	    {
	    (void) putc( '\n', file );
	    charcount = 0;
	    }
	else if ( charcount > 0 )
	    {
	    (void) putc( ' ', file );
	    ++charcount;
	    }
#ifdef DEBUG
	if ( *gP > maxval )
	    pm_error( "value out of bounds (%u > %u)", *gP, maxval );
#endif /*DEBUG*/
	putus( (unsigned long) *gP, file );
	charcount += 3;
	}
    if ( charcount > 0 )
	(void) putc( '\n', file );
    }

#if __STDC__
void
pgm_writepgmrow( FILE* file, gray* grayrow, int cols, gray maxval, int forceplain )
#else /*__STDC__*/
void
pgm_writepgmrow( file, grayrow, cols, maxval, forceplain )
    FILE* file;
    gray* grayrow;
    int cols;
    gray maxval;
    int forceplain;
#endif /*__STDC__*/
    {
    if (forceplain || maxval >= 1<<16)
        pgm_writepgmrowplain( file, grayrow, cols, maxval );
    else
        pgm_writepgmrowraw( file, grayrow, cols, maxval );
    }

#if __STDC__
void
pgm_writepgm( FILE* file, gray** grays, int cols, int rows, gray maxval, int forceplain )
#else /*__STDC__*/
void
pgm_writepgm( file, grays, cols, rows, maxval, forceplain )
    FILE* file;
    gray** grays;
    int cols, rows;
    gray maxval;
    int forceplain;
#endif /*__STDC__*/
    {
    int row;

    pgm_writepgminit( file, cols, rows, maxval, forceplain );

    for ( row = 0; row < rows; ++row )
	 pgm_writepgmrow( file, grays[row], cols, maxval, forceplain );
    }
