/* leaftoppm.c - read an ileaf img file and write a portable pixmap
 *
 * Copyright (C) 1994 by Bill O'Donnell.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  This software is provided "as is" without express or
 * implied warranty.
 *
 * known problems: doesn't do compressed ileaf images.
 * 
 */

#include <stdio.h>
#include "ppm.h"

#define SWAP16(x) ((short)( (((short)(x)>>8)&0xff) | \
			    (((short)(x)<<8)&0xff00) ))
#define SWAP32(x) ( (((int)(x)>>24)&0x000000ff) | (((x)>>8)&0x0000ff00) |  \
                    (((unsigned)(x)<<8)&0x00ff0000) | \
		    (((unsigned)(x)<<24)&0xff000000) )

#ifdef ALPHA
typedef unsigned int USINT32;
#else
typedef unsigned long USINT32;
#endif

#define MAXCOLORS 256
#define LEAF_MAXVAL 255



static int byte_swap;

static USINT32 
leaf_readlong(fp)
FILE* fp;
{
    USINT32 buf;
    USINT32 result;
    
    if (fread(&buf, 1, 4, fp) != 4) {
	pm_error("Read failure. short file?");
	exit(-1);
    }
    if (byte_swap) {
	result = SWAP32 (buf);
    } else {
	result = buf;
    }
    return (result);
}

static short 
leaf_readshort(fp)
FILE* fp;
{
    short buf;
    short result;
    
    if (fread(&buf, 1, 2, fp) != 2) {
	pm_error("Read failure. short file?");
	exit(-1);
    }
    if (byte_swap) {
	result = SWAP16 (buf);
    } else {
	result = buf;
    }
    return (result);
}

static void
leaf_init( fp, cols, rows, depth, ncolors, colors)
FILE *fp;
int *cols;
int *rows;
int *depth;
int *ncolors;
pixel *colors;
{
    USINT32 bufint;
    unsigned char buf[256];
    unsigned char compressed;
    USINT32 format;
    int i;
    short version;
    
    fread(&bufint, 1, 4, fp);
    if (bufint == 0x894f5053)
	byte_swap = 0;
    else if (bufint == 0x53504f89)
	byte_swap = 1;
    else 
	pm_error( "Warning: bad magic number, trying anyway...");
    
    /* version =   2 bytes
       hres =      2 
       vres =      2
       unique id = 4
       offset x =  2
       offset y =  2
       TOTAL    =  14 bytes */
    
    version = leaf_readshort(fp);

    if (fread(buf, 1, 12, fp) != 12) {
	pm_error( "bad header, short file?");
	exit(-1);
    }
    
    *cols = leaf_readshort(fp);
    *rows = leaf_readshort(fp);
    *depth = leaf_readshort(fp);
    
    if ((compressed = fgetc(fp)) != 0) {
	pm_error( "Can't do compressed images.");
    }
    if ((*depth == 1) && (version < 4))
    {
	fgetc(fp); 
	*ncolors = 0;
	return;
    }
    if ((*depth == 8) && (version < 4))
    {
	fgetc(fp); 
	*ncolors = 0;
	return;
    }
    format = leaf_readlong(fp);
    if (format == 0x29000000) 
    {
	/* color image */
	*ncolors = leaf_readshort(fp);
	
	if (*ncolors > 0) {
	    /* 8-bit image */
	    if (*ncolors > 256) {
		pm_error("Can't have > 256 colors in colormap.");
		exit(-1);
	    }
	    /* read colormap */
	    for (i=0; i<256; i++)
		colors[i].r = (unsigned char)fgetc(fp);
	    for (i=0; i<256; i++)
		colors[i].g = (unsigned char)fgetc(fp);
	    for (i=0; i<256; i++)
		colors[i].b = (unsigned char)fgetc(fp);
	    
	} else {
	    /* 24-bit image */
	    *ncolors = 0;
	}
    } else if (*depth ==1) {
	/* mono image */
	leaf_readshort(fp); /* must toss cmap size. */
	*ncolors = 0;
    } else {
	/* gray image */
	leaf_readshort(fp); /* must toss cmap size. */
	*ncolors = 0;
    }
}



int
main( argc, argv )
int argc;
char *argv[];
{
    FILE *ifd;
    register pixel *pixrow, *pP;
    gray *grayrow, *gP;
    static pixel colors[MAXCOLORS];
    int rows, cols, row, col, depth, ncolors;
    USINT32 maxval = LEAF_MAXVAL;
    
    ppm_init(&argc, argv);
    
    if ( argc > 2 )
	pm_usage( "[ileaf img file]" );
    
    if ( argc == 2 )
	ifd = pm_openr( argv[1] );
    else
	ifd = stdin;
    
    leaf_init( ifd, &cols, &rows, &depth, &ncolors, colors );
    
    
    if ((depth == 8) && (ncolors == 0)) {
	/* gray image */
	pgm_writepgminit( stdout, cols, rows, (pixval) maxval, 0 );
	grayrow = (gray *)pm_allocrow( cols, sizeof(gray) );
	for ( row = 0; row < rows; row++ )
	{
	    for ( col = 0, gP = grayrow; col < cols; col++, gP++ )
		*gP = (gray)fgetc(ifd);
	    if (cols % 2)
		fgetc (ifd); /* padding */
	    pgm_writepgmrow( stdout, grayrow, cols, maxval, 0);
	}
    } else if (depth == 24) {
	ppm_writeppminit( stdout, cols, rows, (pixval) maxval, 0);
	pixrow = ppm_allocrow( cols );
	/* true color */
	for ( row = 0; row < rows; row++ )
	{
	    for ( col = 0, pP = pixrow; col < cols; col++, pP++ )
		pP->r = (unsigned char)fgetc(ifd) ;
	    if (cols % 2)
		fgetc (ifd); /* padding */
	    for ( col = 0, pP = pixrow; col < cols; col++, pP++ )
		pP->g = (unsigned char)fgetc(ifd) ;
	    if (cols % 2)
		fgetc (ifd); /* padding */
	    for ( col = 0, pP = pixrow; col < cols; col++, pP++ )
		pP->b = (unsigned char)fgetc(ifd) ;
	    if (cols % 2)
		fgetc (ifd); /* padding */
	    ppm_writeppmrow( stdout, pixrow, cols, (pixval) maxval, 0);
	}
    } else if (depth == 8) {
	/* 8-bit (color mapped) image */
	ppm_writeppminit( stdout, cols, rows, (pixval) maxval, 0);
	pixrow = ppm_allocrow( cols );
	
	for ( row = 0; row < rows; row++ )
	{
	    for ( col = 0, pP = pixrow; col < cols; col++, pP++ )
		*pP = colors[ (int)fgetc(ifd) ];
	    if (cols %2)
		fgetc (ifd); /* padding */
	    ppm_writeppmrow( stdout, pixrow, cols, (pixval) maxval, 0);
	}
    } else if (depth == 1) {
	/* mono image */
	bit *bitrow, *bP;
	
	pbm_writepbminit(stdout, cols, rows, 0);
	bitrow = pbm_allocrow(cols);
	
	for ( row = 0; row < rows; row++ )
	{
	    unsigned char bits = 0x0;
	    for ( col = 0, bP = bitrow; col < cols; col++, bP++ ) {
		int shift = col % 8;
		if (shift == 0) 
		    bits = (unsigned char) fgetc(ifd);
		*bP = (bits & (unsigned char)(0x01 << (7 - shift))) ? 
		    PBM_WHITE : PBM_BLACK;
	    }
	    if ((cols % 16) && (cols % 16) <= 8)
		fgetc(ifd);  /* 16 bit pad */
	    pbm_writepbmrow(stdout, bitrow, cols, 0);
	}
    }
    
    pm_close( ifd );
    
    exit( 0 );
}

