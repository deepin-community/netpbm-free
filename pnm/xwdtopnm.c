/* xwdtopnm.c - read an X11 or X10 window dump file and write a portable anymap
**
** Copyright (C) 1989, 1991 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

/* IMPLEMENTATION NOTES:

   The file X11/XWDFile.h from the X Window System is an authority for the
   format of an XWD file.  Netpbm uses its own declaration, though.
*/


#define _BSD_SOURCE 1
    /* Make sure strdup() is in string.h */

#include <stdio.h>
#include <string.h>
#include "pnm.h"
#include "x10wd.h"
#include "x11wd.h"
#include "shhopt.h"

static struct cmdline_info {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
    char *input_filename;
    int debug;
} cmdline;


static void
parse_command_line(int argc, char ** argv,
                   struct cmdline_info *cmdline_p);


struct row_control {
    /* This structure contains the state of the getpix() reader as it
       reads across a row in the input image.
       */
    union {
        /* The current item (the item most recently read from the
           image file.  Meaningless if 'bits_left' == 0.
           
           Which of the members applies depends on the bits per item of
           the input image.  
        */
        int32n l;    /* 32 bits per item */
        short s;   /* 16 bits per item */
        char c;    /*  8 bits per item */
    } item;
    int bits_used;
      /* This is the number of bits in the current item that have already
         been returned as part of a pixel for a previous call or saved
         as carryover_bits for use in a future call.
         */
    int bits_left;
      /* This is the number of bits in the current item that have not yet
         been returned as part of a pixel or saved as carryover bits.
         */
    uint32n carryover_bits;
      /* This is the odd bits left over from one item that need to be
         combined with more bits from the next item to form a pixel
         value (the pixel is split between two items).  An item is a
         unit of data read from the image file.  

         It is a 32 bit bitstring with the carryover bits right-justified
         in it.  
      */
    int bits_carried_over;
      /* This is the number of carryover bits in 'carryover_bits' */
};

static short bs_short ARGS(( short s ));
static int32n bs_int32 ARGS(( int32n l ));

static int byte_swap;


static int
zero_bits(const uint32n mask) {
/*----------------------------------------------------------------------------
   Return the number of consective zero bits at the least significant end
   of the binary representation of 'mask'.  E.g. if mask == 0x00fff800,
   we would return 11.
-----------------------------------------------------------------------------*/
    int i;
    uint32n shifted_mask;

    for (i=0, shifted_mask = mask; 
         i < sizeof(mask)*8 && (shifted_mask & 0x00000001) == 0;
         i++, shifted_mask >>= 1 );
    return(i);
}



static int
one_bits(const uint32n input) {
/*----------------------------------------------------------------------------
   Return the number of one bits in the binary representation of 'input'.
-----------------------------------------------------------------------------*/
    int one_bits;
    uint32n mask;

    one_bits = 0;   /* initial value */
    for (mask = 0x00000001; mask != 0x00000000; mask <<= 1)
        if (input & mask) one_bits++;

    return(one_bits);
}



static void
getinit( FILE *file, int * const colsP, int * const rowsP, 
         int * const padrightP, xelval * const maxvalP, 
         enum visualclass * const visualclassP, int * const formatP, 
         xel ** const colorsP, int * const bits_per_pixelP, 
         int * const bits_per_itemP, uint32n * const red_maskP, 
         uint32n * const green_maskP, uint32n * const blue_maskP,
         enum byteorder * const byte_orderP,
         enum byteorder * const bit_orderP)
    {
    int grayscale;
    /* Assume X11 headers are larger than X10 ones. */
    unsigned char header[sizeof(X11WDFileHeader)];
    X10WDFileHeader* h10P;
    X11WDFileHeader* h11P;

    byte_swap = 0;       /* Initial assumption */
	*maxvalP = 65535;   /* Initial assumption */
    h10P = (X10WDFileHeader*) header;
    h11P = (X11WDFileHeader*) header;
    if ( sizeof(*h10P) > sizeof(*h11P) )
	pm_error(
"ARGH!  On this machine, X10 headers are larger than X11 headers!\n    You will have to re-write xwdtopnm." );

    /* Read an X10 header. */
    if ( fread( &header[0], sizeof(*h10P), 1, file ) != 1 )
	pm_error( "couldn't read XWD file header" );

    if ( h10P->file_version == X10WD_FILE_VERSION ||
	 bs_int32( h10P->file_version ) == X10WD_FILE_VERSION )
	{
	int i;
	X10Color* x10colors;

	if ( h10P->file_version != X10WD_FILE_VERSION )
	    {
	    byte_swap = 1;
	    h10P->header_size = bs_int32( h10P->header_size );
	    h10P->file_version = bs_int32( h10P->file_version );
	    h10P->display_type = bs_int32( h10P->display_type );
	    h10P->display_planes = bs_int32( h10P->display_planes );
	    h10P->pixmap_format = bs_int32( h10P->pixmap_format );
	    h10P->pixmap_width = bs_int32( h10P->pixmap_width );
	    h10P->pixmap_height = bs_int32( h10P->pixmap_height );
	    h10P->window_width = bs_short( h10P->window_width );
	    h10P->window_height = bs_short( h10P->window_height );
	    h10P->window_x = bs_short( h10P->window_x );
	    h10P->window_y = bs_short( h10P->window_y );
	    h10P->window_bdrwidth = bs_short( h10P->window_bdrwidth );
	    h10P->window_ncolors = bs_short( h10P->window_ncolors );
	    }
	for ( i = 0; i < h10P->header_size - sizeof(*h10P); ++i )
	    if ( getc( file ) == EOF )
		pm_error( "couldn't read rest of X10 XWD file header" );

	/* Check whether we can handle this dump. */
	if ( h10P->window_ncolors > 256 )
	    pm_error( "can't handle X10 window_ncolors > %d", 256 );
	if ( h10P->pixmap_format != ZFormat && h10P->display_planes != 1 )
	    pm_error(
		"can't handle X10 pixmap_format %d with planes != 1",
		h10P->pixmap_format );

	grayscale = 1;
	if ( h10P->window_ncolors != 0 )
	    {
	    /* Read X10 colormap. */
	    x10colors = (X10Color*) malloc2(
		h10P->window_ncolors, sizeof(X10Color) );
	    if ( x10colors == 0 )
		pm_error( "out of memory" );
	    for ( i = 0; i < h10P->window_ncolors; ++i )
		{
		if ( fread( &x10colors[i], sizeof(X10Color), 1, file ) != 1 )
		    pm_error( "couldn't read X10 XWD colormap" );
		if ( byte_swap )
		    {
		    x10colors[i].red = bs_short( x10colors[i].red );
		    x10colors[i].green = bs_short( x10colors[i].green );
		    x10colors[i].blue = bs_short( x10colors[i].blue );
		    }
		if ( x10colors[i].red != x10colors[i].green ||
		     x10colors[i].green != x10colors[i].blue )
		    grayscale = 0;
		}
	    }

	if ( h10P->display_planes == 1 )
	    {
	    *formatP = PBM_TYPE;
	    *visualclassP = StaticGray;
	    *maxvalP = 1;
	    *colorsP = pnm_allocrow( 2 );
	    PNM_ASSIGN1( (*colorsP)[0], 0 );
	    PNM_ASSIGN1( (*colorsP)[1], *maxvalP );
	    overflow_add(h10P->pixmap_width, 15);
	    if(h10P->pixmap_width < 0)
		pm_error("assert: negative width");
	    *padrightP =
		( ( h10P->pixmap_width + 15 ) / 16 ) * 16 - h10P->pixmap_width;
	    *bits_per_itemP = 16;
	    *bits_per_pixelP = 1;
	    }
	else if ( h10P->window_ncolors == 0 )
	    { /* Must be grayscale. */
	    *formatP = PGM_TYPE;
	    *visualclassP = StaticGray;
	    *maxvalP = ( 1 << h10P->display_planes ) - 1;
	    overflow_add(*maxvalP, 1);
	    *colorsP = pnm_allocrow( *maxvalP + 1 );
	    for ( i = 0; i <= *maxvalP; ++i )
		PNM_ASSIGN1( (*colorsP)[i], i );
	    overflow_add(h10P->pixmap_width, 15);
	    if(h10P->pixmap_width < 0)
		pm_error("assert: negative width");
	    *padrightP =
		( ( h10P->pixmap_width + 15 ) / 16 ) * 16 - h10P->pixmap_width;
	    *bits_per_itemP = 16;
	    *bits_per_pixelP = 1;
	    }
	else
	    {
	    *colorsP = pnm_allocrow( h10P->window_ncolors );
	    *visualclassP = PseudoColor;
            if ( grayscale )
                {
                *formatP = PGM_TYPE;
		for ( i = 0; i < h10P->window_ncolors; ++i )
		    PNM_ASSIGN1( (*colorsP)[i], x10colors[i].red );
                }
            else
                {
                *formatP = PPM_TYPE;
		for ( i = 0; i < h10P->window_ncolors; ++i )
		    PPM_ASSIGN(
			(*colorsP)[i], x10colors[i].red, x10colors[i].green,
			x10colors[i].blue);
                }

	    *padrightP = h10P->pixmap_width & 1;
	    *bits_per_itemP = 8;
	    *bits_per_pixelP = 8;
	    }
	*colsP = h10P->pixmap_width;
	*rowsP = h10P->pixmap_height;
	*byte_orderP = MSBFirst;
	*bit_orderP = LSBFirst;
	}
    else if ( h11P->file_version == X11WD_FILE_VERSION ||
	     bs_int32( h11P->file_version ) == X11WD_FILE_VERSION )
	{
	int i;
	X11XColor* x11colors;

	if ( fread( &header[sizeof(*h10P)], sizeof(*h11P) - sizeof(*h10P), 1, file ) != 1 )
	    pm_error( "couldn't read X11 XWD file header" );
	if ( h11P->file_version != X11WD_FILE_VERSION )
	    {
	    byte_swap = 1;
	    h11P->header_size = bs_int32( h11P->header_size );
	    h11P->file_version = bs_int32( h11P->file_version );
	    h11P->pixmap_format = bs_int32( h11P->pixmap_format );
	    h11P->pixmap_depth = bs_int32( h11P->pixmap_depth );
	    h11P->pixmap_width = bs_int32( h11P->pixmap_width );
	    h11P->pixmap_height = bs_int32( h11P->pixmap_height );
	    h11P->xoffset = bs_int32( h11P->xoffset );
	    h11P->byte_order = bs_int32( h11P->byte_order );
	    h11P->bitmap_unit = bs_int32( h11P->bitmap_unit );
	    h11P->bitmap_bit_order = bs_int32( h11P->bitmap_bit_order );
	    h11P->bitmap_pad = bs_int32( h11P->bitmap_pad );
	    h11P->bits_per_pixel = bs_int32( h11P->bits_per_pixel );
	    h11P->bytes_per_line = bs_int32( h11P->bytes_per_line );
	    h11P->visual_class = bs_int32( h11P->visual_class );
	    h11P->red_mask = bs_int32( h11P->red_mask );
	    h11P->green_mask = bs_int32( h11P->green_mask );
	    h11P->blue_mask = bs_int32( h11P->blue_mask );
	    h11P->bits_per_rgb = bs_int32( h11P->bits_per_rgb );
	    h11P->colormap_entries = bs_int32( h11P->colormap_entries );
	    h11P->ncolors = bs_int32( h11P->ncolors );
	    h11P->window_width = bs_int32( h11P->window_width );
	    h11P->window_height = bs_int32( h11P->window_height );
	    h11P->window_x = bs_int32( h11P->window_x );
	    h11P->window_y = bs_int32( h11P->window_y );
	    h11P->window_bdrwidth = bs_int32( h11P->window_bdrwidth );
	    }
	for ( i = 0; i < h11P->header_size - sizeof(*h11P); ++i )
	    if ( getc( file ) == EOF )
		pm_error( "couldn't read rest of X11 XWD file header" );

    if (cmdline.debug) pm_message("ncolors=%d", h11P->ncolors);
	/* Check whether we can handle this dump. */
	if ( h11P->pixmap_depth > 24 )
	    pm_error( "can't handle X11 pixmap_depth > 24" );
	if ( h11P->bits_per_rgb > 24 )
	    pm_error( "can't handle X11 bits_per_rgb > 24" );
	if ( h11P->pixmap_format != ZPixmap && h11P->pixmap_depth != 1 )
	    pm_error(
		"can't handle X11 pixmap_format %d with depth != 1",
		h11P->pixmap_format );
	if ( h11P->bitmap_unit != 8 && h11P->bitmap_unit != 16 &&
	     h11P->bitmap_unit != 32 )
	    pm_error(
		"X11 bitmap_unit (%d) is non-standard - can't handle",
		h11P->bitmap_unit );

	grayscale = 1;
	if ( h11P->ncolors > 0 )
	    {
	    /* Read X11 colormap. */
	    x11colors = (X11XColor*) malloc2(
		h11P->ncolors, sizeof(X11XColor) );
	    if ( x11colors == 0 )
		pm_error( "out of memory" );
	    if ( fread( x11colors, sizeof(X11XColor), h11P->ncolors, file ) !=
			h11P->ncolors )
		pm_error( "couldn't read X11 XWD colormap" );
	    for ( i = 0; i < h11P->ncolors; ++i )
		{
		if ( byte_swap )
		    {
		    x11colors[i].red = bs_short( x11colors[i].red );
		    x11colors[i].green = bs_short( x11colors[i].green );
		    x11colors[i].blue = bs_short( x11colors[i].blue );
		    }
		if ( x11colors[i].red != x11colors[i].green ||
		     x11colors[i].green != x11colors[i].blue )
		    grayscale = 0;

		}
        if (cmdline.debug) 
            for ( i = 0; i < h11P->ncolors && i < 8; ++i )
                pm_message("Color %d r/g/b = %d/%d/%d", i, 
                           x11colors[i].red, x11colors[i].green, 
                           x11colors[i].blue);
        }
    *visualclassP = (enum visualclass) h11P->visual_class;
    if ( *visualclassP == DirectColor ) {
        *formatP = PPM_TYPE;
        *maxvalP = 65535;
        /*
          DirectColor is like PseudoColor except that there are essentially
          3 colormaps (shade maps) -- one for each primary color.  Each pixel
          is composed of 3 separate indices
          */

	    *colorsP = pnm_allocrow( h11P->ncolors );
        for ( i = 0; i < h11P->ncolors; ++i )
            PPM_ASSIGN(
                (*colorsP)[i], x11colors[i].red, x11colors[i].green,
                x11colors[i].blue);
	} else if ( *visualclassP == TrueColor ) {
        *formatP = PPM_TYPE;

        *maxvalP = pm_lcm(pm_bitstomaxval(one_bits(h11P->red_mask)),
                          pm_bitstomaxval(one_bits(h11P->green_mask)),
                          pm_bitstomaxval(one_bits(h11P->blue_mask)),
                          PPM_OVERALLMAXVAL
                         );

    }
	else if ( *visualclassP == StaticGray && h11P->bits_per_pixel == 1 )
	    {
	    *formatP = PBM_TYPE;
	    *maxvalP = 1;
	    *colorsP = pnm_allocrow( 2 );
	    PNM_ASSIGN1( (*colorsP)[0], *maxvalP );
	    PNM_ASSIGN1( (*colorsP)[1], 0 );
	    }
	else if ( *visualclassP == StaticGray )
	    {
	    *formatP = PGM_TYPE;
	    *maxvalP = ( 1 << h11P->bits_per_pixel ) - 1;
	    overflow_add(*maxvalP, 1);
	    *colorsP = pnm_allocrow( *maxvalP + 1 );
	    for ( i = 0; i <= *maxvalP; ++i )
		PNM_ASSIGN1( (*colorsP)[i], i );
	    }
	else {
	    *colorsP = pnm_allocrow( h11P->ncolors );
        if ( grayscale ) {
            *formatP = PGM_TYPE;
            for ( i = 0; i < h11P->ncolors; ++i )
                PNM_ASSIGN1( (*colorsP)[i], x11colors[i].red );
        } else {
            *formatP = PPM_TYPE;
            for ( i = 0; i < h11P->ncolors; ++i )
                PPM_ASSIGN(
                    (*colorsP)[i], x11colors[i].red, x11colors[i].green,
                    x11colors[i].blue);
        }
    }

	*colsP = h11P->pixmap_width;
	*rowsP = h11P->pixmap_height;
	overflow2(h11P->bytes_per_line, 8);
	*padrightP =
	    h11P->bytes_per_line * 8 / h11P->bits_per_pixel -
	    h11P->pixmap_width;
    /* According to X11/XWDFile.h, the item size is 'bitmap_pad' for some
       images and 'bitmap_unit' for others.  This is strange, so there may
       be some subtlety of their definitions that we're missing.

       See comments in getpix() about what an item is.
    */
    if (h11P->pixmap_format == ZPixmap || h11P->pixmap_depth > 1)
        *bits_per_itemP = h11P->bitmap_pad;
    else
        *bits_per_itemP = h11P->bitmap_unit;
	*bits_per_pixelP = h11P->bits_per_pixel;
	*byte_orderP = (enum byteorder) h11P->byte_order;
	*bit_orderP = (enum byteorder) h11P->bitmap_bit_order;
    *red_maskP = h11P->red_mask;
    *green_maskP = h11P->green_mask;
    *blue_maskP = h11P->blue_mask;
	}
    else
	pm_error( "unknown XWD file version: %d", h11P->file_version );

    }



static uint32n
getpix(FILE *file, struct row_control * const row_controlP,
       const int bits_per_pixel, const int bits_per_item, 
       const enum byteorder byte_order, const enum byteorder bit_order) {
/*----------------------------------------------------------------------------
   Get a pixel from the input image.

   A pixel is a bit string.  It may be either an rgb triplet or an index
   into the colormap (or even an rgb triplet of indices into the colormap!).
   We don't care -- it's just a bit string.
   
   The basic unit of storage in the input file is an "item."  An item
   can be 1, 2, or 4 bytes, and 'bits_per_item' tells us which.  Each
   item can have its bytes stored in little-endian or big-endian
   format, and 'byte_order' tells us which.

   Each item can contain one or more pixels, and may contain
   fractional pixels.  'bits_per_pixel' tells us how many bits each
   pixel has, and 'bits_per_pixel' is always less than
   'bits_per_item', but not necessarily a factor of it.  Within an item,
   after taking care of the endianness of its storage format, the pixels
   may be arranged from left to right or right to left.  'bit_order' tells
   us which.

   But it's not that simple.  Each individual row contains an integral
   number of items.  This means the last item in the row may contain
   padding bits.  The padding is on the right or left, according to
   'bit_order'.
   
   The most difficult part of getting the pixel is the ones that span
   items.  We detect when an item has part, but not all, of the next
   pixel in it and store the fragment as "carryover" bits for use in the
   next call to this function.

   All this state information (carryover bits, etc.) is kept in 
   *row_controlP.

-----------------------------------------------------------------------------*/
    uint32n pixel;
      /* This is the pixel value we ultimately return.  It is a 32 bit
         bitstring, with the pixel value right-justified in it.
         */

    int bits_to_take;
      /* This is the number of bits we need to take from the current
         item to combine with carryover bits to form a pixel.
         */

    long tmplong; /* For pm_read*long */

    /*
      if (pixel_count < 4)
      pm_message("getting pixel %d", pixel_count);
    */

    if ( row_controlP->bits_left <= 0) {
        /* We've used up the most recently read item.  Get the next one
           from the input file.
           */
        switch ( bits_per_item )
	    {
	    case 8:
            row_controlP->item.c = getc( file );
            break;

	    case 16:
            switch (byte_order) {
            case MSBFirst:
                if (pm_readbigshort(file, &row_controlP->item.s) == -1)
                    pm_error( "error reading image" );
                break;
            case LSBFirst:
                if (pm_readlittleshort(file, &row_controlP->item.s) == -1)
                    pm_error( "error reading image" );
                break;
            }
            break;

	    case 32:
	    switch (byte_order) {
	    case MSBFirst:
		if (pm_readbiglong(file, &tmplong) == -1)
		    pm_error( "error reading image" );
		break;
	    case LSBFirst:
		if (pm_readlittlelong(file, &tmplong) == -1) 
		    pm_error( "error reading image");
		break;
	    }
	    row_controlP->item.l = tmplong;
	    break;
	    default:
            pm_error( "can't happen" );
	    }
        /* 
        if (pixel_count < 4) 
            pm_message("item: %.8lx", row_controlP->item.l); 
        */

        row_controlP->bits_used = 0;  
          /* Fresh item from file; none of it used yet. */
        row_controlP->bits_left = bits_per_item;
	}
    
    bits_to_take = bits_per_pixel - row_controlP->bits_carried_over;

    { 
        uint32n bits_to_take_mask;
        static uint32n bits_taken;
          /* The bits we took from the current item:  'bits_to_take' bits
             right-justified in a 32 bit bitstring.
          */
        int bit_shift;
          /* How far to shift item to get the bits we're taking
             right-justified 
          */

        if (bit_order == MSBFirst)
            bit_shift = row_controlP->bits_left - bits_to_take;
        else
            bit_shift = row_controlP->bits_used;

        if ( bits_to_take == sizeof(bits_to_take_mask) * 8 )
            bits_to_take_mask = -1;
        else
            bits_to_take_mask = ( 1 << bits_to_take ) - 1;
        
        switch ( bits_per_item ) {
        case 8:
            bits_taken = (row_controlP->item.c >> bit_shift) 
                & bits_to_take_mask;
            break;
        case 16:
            bits_taken = (row_controlP->item.s >> bit_shift) 
                & bits_to_take_mask;
            break;
        case 32:
            bits_taken = (row_controlP->item.l >> bit_shift) 
                & bits_to_take_mask;
            break;
        }

        /* Now combine any carried over bits with the bits just taken */
        if (bit_order == MSBFirst)
            pixel = bits_taken | row_controlP->carryover_bits << bits_to_take;
        else
            pixel = bits_taken << row_controlP->bits_carried_over |
                row_controlP->carryover_bits;

        /*
        if (pixel_count < 4)
        pm_message("  bits_taken: %lx(%d), carryover_bits: %lx(%d), "
                     "pixel: %lx",
                     bits_taken, bits_to_take, row_controlP->carryover_bits, 
                     row_controlP->bits_carried_over, pixel);
        */
    }

    row_controlP->bits_used += bits_to_take;
    row_controlP->bits_left -= bits_to_take;

    if (row_controlP->bits_left > 0 && 
        row_controlP->bits_left < bits_per_pixel) {
        /* Part, but not all, of the next pixel is in this item. Get it
           into carryover_bits and then mark this item all used up.
           */
        uint32n bits_left_mask;
        int bit_shift;
           /* How far to shift item to get the bits that are left
              right-justified 
           */
        if ( row_controlP->bits_left == sizeof(bits_left_mask) * 8 )
            bits_left_mask = -1;  /* All one bits */
        else
            bits_left_mask = ( 1 << row_controlP->bits_left ) - 1;

        if (bit_order == MSBFirst)
            bit_shift = 0;
        else
            bit_shift = row_controlP->bits_used;

        switch ( bits_per_item ) {
        case 8:
            row_controlP->carryover_bits = 
                (row_controlP->item.c >> bit_shift) & bits_left_mask;
            break;
        case 16:
            row_controlP->carryover_bits = 
                (row_controlP->item.s >> bit_shift) & bits_left_mask;
            break;
        case 32:
            row_controlP->carryover_bits = 
                (row_controlP->item.l >> bit_shift) & bits_left_mask;
            break;
        }
        row_controlP->bits_carried_over = row_controlP->bits_left;
        row_controlP->bits_used = bits_per_item;
        row_controlP->bits_left = 0;
    } else {
        row_controlP->carryover_bits = 0;
        row_controlP->bits_carried_over = 0;
    }
    /*
    if (pixel_count < 4) {
        pm_message("  row_control.bits_carried_over = %d"
                   "  carryover_bits= %.8lx", 
                   row_controlP->bits_carried_over,
                   row_controlP->carryover_bits);
        pm_message("  row_control.bits_used = %d", 
                   row_controlP->bits_used);
        pm_message("  row_control.bits_left = %d",
                   row_controlP->bits_left);
                   }
 
    pixel_count++;
    */
    return pixel;
}



static void
print_debug_info(const int cols, const int rows, const int padright, 
                 const xelval maxval, const enum visualclass visualclass,
                 const int format, const int bits_per_pixel,
                 const int bits_per_item, 
                 const int red_mask, const int green_mask, const int blue_mask,
                 const enum byteorder byte_order, 
                 const enum byteorder bit_order) {

    const char *visualclass_name;
    const char *byte_order_name;
    const char *bit_order_name;
    switch (visualclass) {
    case StaticGray:  visualclass_name="StaticGray";  break;
    case GrayScale:   visualclass_name="Grayscale";   break;
    case StaticColor: visualclass_name="StaticColor"; break;
    case PseudoColor: visualclass_name="PseudoColor"; break;
    case TrueColor:   visualclass_name="TrueColor";   break;
    case DirectColor: visualclass_name="DirectColor"; break;
    default:          visualclass_name="(invalid)";    break;
    }
    switch (byte_order) {
    case MSBFirst: byte_order_name = "MSBFirst";  break;
    case LSBFirst: byte_order_name = "LSBFirst";  break;
    default:       byte_order_name = "(invalid)"; break;
    }
    switch (bit_order) {
    case MSBFirst: bit_order_name = "MSBFirst";  break;
    case LSBFirst: bit_order_name = "LSBFirst";  break;
    default:       bit_order_name = "(invalid)"; break;
    }
    pm_message("%d rows of %d columns with maxval %d",
               rows, cols, maxval);
    pm_message("padright=%d.  visualclass = %s.  format=%d (%c%c)",
               padright, visualclass_name, 
               format, format/256, format%256);
    pm_message("bits_per_pixel=%d; bits_per_item=%d",
               bits_per_pixel, bits_per_item);
    pm_message("byte_order=%s; bit_order=%s",
               byte_order_name, bit_order_name);
    pm_message("red_mask=0x%.8x; green_mask=0x%.8x; blue_mask=0x%.8x",
               red_mask, green_mask, blue_mask);
}



static void
convert_row(FILE* ifp, FILE* outfile,
            const int bits_per_pixel, const int bits_per_item,
            const enum byteorder byte_order, const enum byteorder bit_order,
            const int padright, const int cols, const xelval maxval,
            const int format, 
            uint32n red_mask, uint32n green_mask, 
            uint32n blue_mask, const xel* const colors, 
            const enum visualclass visualclass
    ) {

    int col;
    xel* xelrow;
    xel* xP;
    struct row_control row_control;
    /* Initialize the state of the getpix() row reader */
    row_control.bits_left = 0;
    row_control.carryover_bits = 0;
    row_control.bits_carried_over = 0;

    xelrow = pnm_allocrow( cols );

    switch ( visualclass ) {
    case StaticGray:
    case GrayScale:
    case StaticColor:
    case PseudoColor:
        for ( col = 0, xP = xelrow; col < cols; ++col, ++xP )
            *xP = colors[getpix( ifp, &row_control, 
                                 bits_per_pixel, bits_per_item, 
                                 byte_order, bit_order)];
        break;
    case DirectColor: {
        unsigned int col;

        for (col = 0; col < cols; ++col) {
            uint32n pixel;
              /* This is a triplet of indices into the color map, packed
                 into this bit string according to red_mask, etc.
                 */
            unsigned int red_index, green_index, blue_index;
              /* These are indices into the color map, unpacked from 'pixel'.
              */
            
            pixel = getpix( ifp, &row_control, 
                            bits_per_pixel, bits_per_item, 
                            byte_order, bit_order);

            red_index =   (pixel & red_mask)   >> zero_bits(red_mask);
            green_index = (pixel & green_mask) >> zero_bits(green_mask); 
            blue_index =  (pixel & blue_mask)  >> zero_bits(blue_mask);

            PPM_ASSIGN(xelrow[col],
                        PPM_GETR(colors[red_index]),
                        PPM_GETG(colors[green_index]),
                        PPM_GETB(colors[blue_index])
                );
        }
    }
    break;
    case TrueColor: {
        unsigned int col;
        unsigned int red_shift, green_shift, blue_shift;
        unsigned int red_maxval, green_maxval, blue_maxval;

        red_shift =   zero_bits(red_mask);
        green_shift = zero_bits(green_mask);
        blue_shift =  zero_bits(blue_mask);

        red_maxval =   red_mask >> red_shift;
        green_maxval = green_mask >> green_shift;
        blue_maxval =  blue_mask >> blue_shift;

        for ( col = 0, xP = xelrow; col < cols; ++col, ++xP ) {
            uint32n pixel;

            pixel = getpix( ifp, &row_control, 
                            bits_per_pixel, bits_per_item, 
                            byte_order, bit_order);

            /* The parsing of 'pixel' used to be done with hardcoded layout
               parameters.  See comments at end of this file.
            */
            PPM_ASSIGN( *xP,
                        ((pixel & red_mask)   >> red_shift  )
                        *maxval / red_maxval,
                        ((pixel & green_mask) >> green_shift)
                        *maxval / green_maxval,
                        ((pixel & blue_mask)  >> blue_shift ) 
                        *maxval / blue_maxval
                );

        }
    }
    break;
            
    default:
        pm_error( "unknown visual class" );
    }
    for ( col = 0; col < padright; ++col )
        (void) getpix( ifp, &row_control, bits_per_pixel, bits_per_item,
                       byte_order, bit_order);
    pnm_writepnmrow( outfile, xelrow, cols, maxval, format, 0 );
    pnm_freerow(xelrow);
}



int
main( int argc, char *argv[] ) {
    FILE* ifp;
    int rows, cols, format, padright, row;
    int bits_per_pixel;
    int bits_per_item;
    uint32n red_mask, green_mask, blue_mask;
    xelval maxval;
    enum visualclass visualclass;
    enum byteorder byte_order, bit_order;
    xel *colors;  /* the color map */

    pnm_init( &argc, argv );

    parse_command_line(argc, argv, &cmdline);

    if ( cmdline.input_filename != NULL ) 
        ifp = pm_openr( cmdline.input_filename );
    else
        ifp = stdin;

    getinit( ifp, &cols, &rows, &padright, &maxval, &visualclass, &format, 
             &colors, &bits_per_pixel, &bits_per_item, 
             &red_mask, &green_mask, &blue_mask, &byte_order, &bit_order );

    if (cmdline.debug) 
        print_debug_info(cols, rows, padright, maxval, visualclass,
                         format, bits_per_pixel, bits_per_item,
                         red_mask, green_mask, blue_mask, 
                         byte_order, bit_order);

    pnm_writepnminit( stdout, cols, rows, maxval, format, 0 );
    switch ( PNM_FORMAT_TYPE(format) )
	{
	case PBM_TYPE:
	pm_message( "writing PBM file" );
	break;

	case PGM_TYPE:
	pm_message( "writing PGM file" );
	break;

	case PPM_TYPE:
	pm_message( "writing PPM file" );
	break;

	default:
	pm_error( "shouldn't happen" );
	}

    for ( row = 0; row < rows; ++row ) {
        convert_row(ifp, stdout, bits_per_pixel, bits_per_item,
                    byte_order, bit_order, padright, cols, maxval, format,
                    red_mask, green_mask, blue_mask, colors, visualclass);
	}
    
    pm_close( ifp );
    pm_close( stdout );
    
    exit( 0 );
    }


/* Byte-swapping junk. */

union cheat {
    uint32n l;
    short s;
    unsigned char c[4];
    };

static short
bs_short( short s )
    {
    union cheat u;
    unsigned char t;

    u.s = s;
    t = u.c[0];
    u.c[0] = u.c[1];
    u.c[1] = t;
    return u.s;
    }

static int32n
bs_int32( int32n l )
    {
    union cheat u;
    unsigned char t;

    u.l = l;
    t = u.c[0];
    u.c[0] = u.c[3];
    u.c[3] = t;
    t = u.c[1];
    u.c[1] = u.c[2];
    u.c[2] = t;
    return u.l;
    }


static void
parse_command_line(int argc, char ** argv,
                   struct cmdline_info *cmdline_p) {
/*----------------------------------------------------------------------------
   Note that many of the strings that this function returns in the
   *cmdline_p structure are actually in the supplied argv array.  And
   sometimes, one of these strings is actually just a suffix of an entry
   in argv!
-----------------------------------------------------------------------------*/
    optStruct *option_def = malloc(100*sizeof(optStruct));
        /* Instructions to OptParseOptions on how to parse our options.
         */
    unsigned int option_def_index;

    option_def_index = 0;   /* incremented by OPTENTRY */
    OPTENTRY(0, "debug",        OPT_FLAG,   &cmdline_p->debug,          0);

    /* Set the defaults */
    cmdline_p->debug = 0;

    pm_optParseOptions(&argc, argv, option_def, 0);
        /* Uses and sets argc, argv, and all of *cmdline_p. */

    if (argc - 1 == 0)
        cmdline_p->input_filename = NULL;  /* he wants stdin */
    else if (argc - 1 == 1) {
        if (strcmp(argv[1], "-") == 0)
            cmdline_p->input_filename = NULL;  /* he wants stdin */
        else 
            cmdline_p->input_filename = strdup(argv[1]);
    } else 
        pm_error("Too many arguments.  The only argument accepted\n"
                 "is the input file specificaton");
}


/* 
   This used to be the way we parsed a direct/true color pixel.  I'm 
   keeping it here in case we find out some applications needs it this way.

   There doesn't seem to be any reason to do this hard-coded stuff when
   the header contains 32 bit masks that tell exactly how to extract the
   3 colors in all cases.

   We know for a fact that 16 bit TrueColor output from XFree86's xwd
   doesn't match these hard-coded shift amounts, so we have replaced
   this whole switch thing.  -Bryan 00.03.01

   switch ( bits_per_pixel )
		    {
                
		    case 16:
		    PPM_ASSIGN( *xP,
			( ( ul & red_mask )   >> 0    ),
			( ( ul & green_mask ) >> 5  ),
			( ( ul & blue_mask )  >> 11) );
		    break;

		    case 24:
		    case 32:
		    PPM_ASSIGN( *xP, ( ( ul & 0xff0000 ) >> 16 ),
			( ( ul & 0xff00 ) >> 8 ),
			( ul & 0xff ) );
		    break;

		    default:
		    pm_error( "True/Direct only supports 16, 24, and 32 bits" );
		    }
*/
