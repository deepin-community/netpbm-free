/* libppm2.c - ppm utility library part 2
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

#include "ppm.h"
#include "libppm.h"

static void putus ARGS((unsigned short n, FILE* file));
static void ppm_writeppmrowplain ARGS((FILE* file, pixel* pixelrow, int cols, pixval maxval));
static void ppm_writeppmrowraw ARGS((FILE* file, pixel* pixelrow, int cols, pixval maxval));



void
ppm_writeppminit(FILE* file, int cols, int rows, pixval maxval, 
                 int forceplain) {

    if (maxval > PPM_OVERALLMAXVAL && !forceplain) 
        pm_error("too-large maxval passed to ppm_writeppminit(): %d.\n"
                 "Maximum allowed by the PPM format is %d.",
                 maxval, PPM_OVERALLMAXVAL);

	fprintf(file, "%c%c\n%d %d\n%d\n", 
            PPM_MAGIC1, 
            forceplain || maxval >= 1<<16 ? PPM_MAGIC2 : RPPM_MAGIC2, 
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

/* fastread/write is in solving bug #187395 */
static void
fastwrite_rawppm_256max(FILE* file, pixel* pixelrow, int cols) {
#define FASTWRITE_BATCHSIZE 2048
	unsigned char buf[3*FASTWRITE_BATCHSIZE];
	int col;
 
	for(col=0;col<cols;) {
		int writepixels = FASTWRITE_BATCHSIZE;
		size_t amt;
		int i;
		unsigned char *bufptr;
 
		if (writepixels > (cols - col))
			writepixels = cols - col;
 
		for(i=0,bufptr = buf;i<writepixels;++i,bufptr += 3,++pixelrow) {
			bufptr[0] = PPM_GETR(*pixelrow);
			bufptr[1] = PPM_GETG(*pixelrow);
			bufptr[2] = PPM_GETB(*pixelrow);
		}
		amt = fwrite(buf,3,writepixels,file);
		if (amt != writepixels)
			pm_error( "EOF / write error writing a row of pixels" );
		
		col += writepixels;
	}
}

static void
ppm_writeppmrowraw(FILE *file, pixel *pixelrow, int cols, pixval maxval ) {
    register int col;
    register pixval val;

	if (maxval < 256) {
		fastwrite_rawppm_256max(file,pixelrow,cols);
		return;
	}

    for ( col = 0; col < cols; ++col )
	{
	val = PPM_GETR( pixelrow[col] );
#ifdef DEBUG
	if ( val > maxval )
	    pm_error( "r value out of bounds (%u > %u)", val, maxval );
#endif /*DEBUG*/
	pgm_writerawsample( file, val, maxval );
	val = PPM_GETG( pixelrow[col] );
#ifdef DEBUG
	if ( val > maxval )
	    pm_error( "g value out of bounds (%u > %u)", val, maxval );
#endif /*DEBUG*/
	pgm_writerawsample( file, val, maxval );
	val = PPM_GETB( pixelrow[col] );
#ifdef DEBUG
	if ( val > maxval )
	    pm_error( "b value out of bounds (%u > %u)", val, maxval );
#endif /*DEBUG*/
	pgm_writerawsample( file, val, maxval );
        }
    }

static void
ppm_writeppmrowplain(FILE *file, pixel *pixelrow, int cols, pixval maxval ) {
    register int col, charcount;
    register pixel* pP;
    register pixval val;

    charcount = 0;
    for ( col = 0, pP = pixelrow; col < cols; ++col, ++pP )
	{
	if ( charcount >= 65 )
	    {
	    (void) putc( '\n', file );
	    charcount = 0;
	    }
	else if ( charcount > 0 )
	    {
	    (void) putc( ' ', file );
	    (void) putc( ' ', file );
	    charcount += 2;
	    }
	val = PPM_GETR( *pP );
#ifdef DEBUG
	if ( val > maxval )
	    pm_error( "r value out of bounds (%u > %u)", val, maxval );
#endif /*DEBUG*/
	putus( val, file );
	(void) putc( ' ', file );
	val = PPM_GETG( *pP );
#ifdef DEBUG
	if ( val > maxval )
	    pm_error( "g value out of bounds (%u > %u)", val, maxval );
#endif /*DEBUG*/
	putus( val, file );
	(void) putc( ' ', file );
	val = PPM_GETB( *pP );
#ifdef DEBUG
	if ( val > maxval )
	    pm_error( "b value out of bounds (%u > %u)", val, maxval );
#endif /*DEBUG*/
	putus( val, file );
	charcount += 11;
	}
    if ( charcount > 0 )
	(void) putc( '\n', file );
    }

#if __STDC__
void
ppm_writeppmrow( FILE* file, pixel* pixelrow, int cols, pixval maxval, int forceplain )
#else /*__STDC__*/
void
ppm_writeppmrow( file, pixelrow, cols, maxval, forceplain )
    FILE* file;
    pixel* pixelrow;
    int cols;
    pixval maxval;
    int forceplain;
#endif /*__STDC__*/
    {
    if (forceplain || maxval >= 1<<16) 
        ppm_writeppmrowplain( file, pixelrow, cols, maxval );
    else 
        ppm_writeppmrowraw( file, pixelrow, cols, maxval );
    }

#if __STDC__
void
ppm_writeppm( FILE* file, pixel** pixels, int cols, int rows, pixval maxval, int forceplain )
#else /*__STDC__*/
void
ppm_writeppm( file, pixels, cols, rows, maxval, forceplain )
    FILE* file;
    pixel** pixels;
    int cols, rows;
    pixval maxval;
    int forceplain;
#endif /*__STDC__*/
    {
    int row;

    ppm_writeppminit( file, cols, rows, maxval, forceplain );

    for ( row = 0; row < rows; ++row )
	ppm_writeppmrow( file, pixels[row], cols, maxval, forceplain );
    }
