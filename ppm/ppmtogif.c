/* ppmtogif.c - read a portable pixmap and produce a GIF file
**
** Based on GIFENCOD by David Rowley <mgardi@watdscu.waterloo.edu>.A
** Lempel-Zim compression based on "compress".
**
** Modified by Marcel Wijkstra <wijkstra@fwi.uva.nl>
**
** The non-LZW GIF generation stuff was adapted from the Independent
** JPEG Group's djpeg on 2001.09.29.  The uncompressed output subroutines
** are derived directly from the corresponding subroutines in djpeg's
** wrgif.c source file.  It's copyright notice say:

 * Copyright (C) 1991-1997, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
   The reference README file is README.JPEG in the Netpbm package.
**
** Copyright (C) 1989 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
**
** The Graphics Interchange Format(c) is the Copyright property of
** CompuServe Incorporated.  GIF(sm) is a Service Mark property of
** CompuServe Incorporated.
*/

/* TODO: merge the LZW and uncompressed subroutines.  They are separate
   only because they had two different lineages and the code is too
   complicated for me quickly to rewrite it.
*/
#include "ppm.h"
#include "ppmcmap.h"

#define MAXCMAPSIZE 256

/*
 * a code_int must be able to hold 2**BITS values of type int, and also -1
 */
typedef int             code_int;

#ifdef SIGNED_COMPARE_SLOW
typedef unsigned long int count_int;
typedef unsigned short int count_short;
#else /*SIGNED_COMPARE_SLOW*/
typedef long int          count_int;
#endif /*SIGNED_COMPARE_SLOW*/

struct cmap {
    /* This is the information for the GIF colormap (aka palette). */

    int red[MAXCMAPSIZE], green[MAXCMAPSIZE], blue[MAXCMAPSIZE];
        /* These arrays arrays map a color index, as is found in
           the raster part of the GIF, to an intensity value for the indicated
           RGB component.
        */
    int perm[MAXCMAPSIZE], permi[MAXCMAPSIZE];
        /* perm[i] is the position in the sorted colormap of the color which
           is at position i in the unsorted colormap.  permi[] is the inverse
           function of perm[].
        */
    int cmapsize;
        /* Number of entries in the GIF colormap.  I.e. number of colors
           in the image, plus possibly one fake transparency color.
        */
    int transparent;
        /* color index number in GIF palette of the color that is to be
           transparent.  -1 if no color is transparent.
        */
    colorhash_table cht;
        /* A hash table that relates a PPM pixel value to to a pre-sort
           GIF colormap index.
        */
    pixval maxval;
        /* The maxval for the colors in 'cht'. */
};

struct gif_dest {
    /* This structure controls output of uncompressed GIF raster */

    /* State for packing variable-width codes into a bitstream */
    int n_bits;         /* current number of bits/code */
    int maxcode;        /* maximum code, given n_bits */
    int cur_accum;      /* holds bits not yet output */
    int cur_bits;       /* # of bits in cur_accum */

    /* State for GIF code assignment */
    int ClearCode;      /* clear code (doesn't change) */
    int EOFCode;        /* EOF code (ditto) */
    int code_counter;   /* counts output symbols */
};

static void BumpPixel ARGS(( void ));
static void Putword ARGS(( int w, FILE* fp ));
static void writeerr ARGS(( void ));

struct cmdline_info {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
    char *input_filespec;  /* Filespec of input file */
    char *alpha_filespec;  /* Filespec of alpha file; NULL if none */
    char *alphacolor;      /* -alphacolor option value or default */
    unsigned int interlace; /* -interlace option value */
    unsigned int sort;     /* -sort option value */
    char *mapfile;         /* -mapfile option value.  NULL if none. */
    char *transparent;     /* -transparent option value.  NULL if none. */
    char *comment;         /* -comment option value; NULL if none */
    unsigned int nolzw;    /* -nolzw option */
};


static void
parse_command_line(int argc, char ** argv,
                   struct cmdline_info *cmdline_p) {
/*----------------------------------------------------------------------------
   Note that the file spec array we return is stored in the storage that
   was passed to us as the argv array.
-----------------------------------------------------------------------------*/
    optStruct3 opt;  /* set by OPTENT3 */
    optEntry *option_def = malloc(100*sizeof(optEntry));
    unsigned int option_def_index;

    option_def_index = 0;   /* incremented by OPTENT3 */
    OPTENT3(0,   "interlace",   OPT_FLAG,   
            NULL,                       &cmdline_p->interlace, 0);
    OPTENT3(0,   "sort",        OPT_FLAG,   
            NULL,                       &cmdline_p->sort, 0);
    OPTENT3(0,   "nolzw",       OPT_FLAG,   
            NULL,                       &cmdline_p->nolzw, 0);
    OPTENT3(0,   "mapfile",     OPT_STRING, 
            &cmdline_p->mapfile,        NULL, 0);
    OPTENT3(0,   "transparent", OPT_STRING, 
            &cmdline_p->transparent,    NULL, 0);
    OPTENT3(0,   "comment",     OPT_STRING, 
            &cmdline_p->comment,        NULL, 0);
    OPTENT3(0,   "alpha",       OPT_STRING, 
            &cmdline_p->alpha_filespec, NULL, 0);
    OPTENT3(0,   "alphacolor",  OPT_STRING, 
            &cmdline_p->alphacolor,     NULL, 0);
    
    /* Set the defaults */
    cmdline_p->mapfile = NULL;
    cmdline_p->transparent = NULL;  /* no transparency */
    cmdline_p->comment = NULL;      /* no comment */
    cmdline_p->alpha_filespec = NULL;      /* no alpha file */
    cmdline_p->alphacolor = "rgb:0/0/0";      
        /* We could say "black" here, but then we depend on the color names
           database existing.
        */

    opt.opt_table = option_def;
    opt.short_allowed = FALSE;  /* We have no short (old-fashioned) options */
    opt.allowNegNum = FALSE;  /* We may have parms that are negative numbers */

    pm_optParseOptions3(&argc, argv, opt, sizeof(opt), 0);
        /* Uses and sets argc, argv, and some of *cmdline_p and others. */

    if (argc-1 == 0) 
        cmdline_p->input_filespec = "-";
    else if (argc-1 != 1)
        pm_error("Program takes zero or one argument (filename).  You "
                 "specified %d", argc-1);
    else
        cmdline_p->input_filespec = argv[1];

    if (cmdline_p->alpha_filespec && cmdline_p->transparent)
        pm_error("You cannot specify both -alpha and -transparent.");
}



static int __inline__ sqr(const int x) {
    return x*x;
}


static int __inline__
closestcolor(const pixel color, pixval const maxval, struct cmap * cmapP) {
/*----------------------------------------------------------------------------
   Return the pre-sort colormap index of the color in the colormap *cmapP
   that is closest to the color 'color', whose maxval is 'maxval'.

   Also add 'color' to the colormap hash, with the colormap index we
   are returning.  Caller must ensure that the color is not already in
   there.
-----------------------------------------------------------------------------*/
    int i,r,g,b;
    int imin,dmin;

    r=(int)PPM_GETR(color)*255/maxval;
    g=(int)PPM_GETG(color)*255/maxval;
    b=(int)PPM_GETB(color)*255/maxval;

    dmin = sqr(255)*3;
    imin = 0;
    for (i=0;i < cmapP->cmapsize; i++) {
        int d;
        d=sqr(r-cmapP->red[i])+sqr(g-cmapP->green[i])+sqr(b-cmapP->blue[i]);
        if (d<dmin) {
            dmin=d;
            imin=i; 
        } 
    }
    ppm_addtocolorhash(cmapP->cht,&color,cmapP->permi[imin]);
    return cmapP->permi[imin];
}



static __inline__ int
GetPixel(pixel ** const pixels, pixval const input_maxval,
         gray ** const alpha, gray const alpha_threshold, 
         struct cmap * const cmapP,
         int const x, int const y) {
/*----------------------------------------------------------------------------
   Return the colormap index of the pixel at location (x,y) in the PPM
   raster 'pixels', using colormap *cmapP.
-----------------------------------------------------------------------------*/
    int colorindex;

    if (alpha && alpha[y][x] < alpha_threshold)
        colorindex = cmapP->transparent;
    else {
        int presort_colorindex;

        presort_colorindex = ppm_lookupcolor(cmapP->cht, &pixels[y][x]);
        if (presort_colorindex == -1)
            presort_colorindex = 
                closestcolor(pixels[y][x], input_maxval, cmapP);
        colorindex = cmapP->perm[presort_colorindex];
    }
    return colorindex;
}


#define TRUE 1
#define FALSE 0

static int Width, Height;
static int curx, cury;
static long CountDown;
static int Pass = 0;
static int Interlace;

/*
 * Bump the 'curx' and 'cury' to point to the next pixel
 */
static void
BumpPixel()
{
        /*
         * Bump the current X position
         */
        ++curx;

        /*
         * If we are at the end of a scan line, set curx back to the beginning
         * If we are interlaced, bump the cury to the appropriate spot,
         * otherwise, just increment it.
         */
        if( curx == Width ) {
                curx = 0;

                if( !Interlace )
                        ++cury;
                else {
                     switch( Pass ) {

                       case 0:
                          cury += 8;
                          if( cury >= Height ) {
                                ++Pass;
                                cury = 4;
                          }
                          break;

                       case 1:
                          cury += 8;
                          if( cury >= Height ) {
                                ++Pass;
                                cury = 2;
                          }
                          break;

                       case 2:
                          cury += 4;
                          if( cury >= Height ) {
                             ++Pass;
                             cury = 1;
                          }
                          break;

                       case 3:
                          cury += 2;
                          break;
                        }
                }
        }
}



static __inline__ int
GIFNextPixel(pixel ** const pixels, pixval const input_maxval,
             gray ** const alpha, gray const alpha_threshold, 
             struct cmap * const cmapP) {
/*----------------------------------------------------------------------------
   Return the pre-sort color index (index into the unsorted GIF color map)
   of the next pixel to be processed from the input image.

   'alpha_threshold' is the gray level such that a pixel in the alpha
   map whose value is less that that represents a transparent pixel
   in the output.
-----------------------------------------------------------------------------*/
    int r;

    if( CountDown == 0 )
        return EOF;
    
    --CountDown;
    
    r = GetPixel(pixels, input_maxval, alpha, alpha_threshold, cmapP, 
                 curx, cury);
    
    BumpPixel();
    
    return r;
}



static void
write_transparent_color_index_extension(FILE *fp, const int Transparent) {
/*----------------------------------------------------------------------------
   Write out extension for transparent color index.
-----------------------------------------------------------------------------*/

    fputc( '!', fp );
    fputc( 0xf9, fp );
    fputc( 4, fp );
    fputc( 1, fp );
    fputc( 0, fp );
    fputc( 0, fp );
    fputc( Transparent, fp );
    fputc( 0, fp );
}



static void
write_comment_extension(FILE *fp, const char comment[]) {
/*----------------------------------------------------------------------------
   Write out extension for a comment
-----------------------------------------------------------------------------*/
    char *segment;
    
    fputc('!', fp);   /* Identifies an extension */
    fputc(0xfe, fp);  /* Identifies a comment */

    /* Write it out in segments no longer than 255 characters */
    for (segment = (char *) comment; 
         segment < comment+strlen(comment); 
         segment += 255) {

        const int length_this_segment = min(255, strlen(segment));

        fputc(length_this_segment, fp);

        fwrite(segment, 1, length_this_segment, fp);
    }

    fputc(0, fp);   /* No more comment blocks in this extension */
}



/***************************************************************************
 *
 *  GIFCOMPR.C       - GIF Image compression routines
 *
 *  Lempel-Ziv compression based on 'compress'.  GIF modifications by
 *  David Rowley (mgardi@watdcsu.waterloo.edu)
 *
 ***************************************************************************/

/*
 * General DEFINEs
 */

#define BITS    12

#define HSIZE  5003            /* 80% occupancy */

#ifdef NO_UCHAR
 typedef char   char_type;
#else /*NO_UCHAR*/
 typedef        unsigned char   char_type;
#endif /*NO_UCHAR*/

/*
 *
 * GIF Image compression - modified 'compress'
 *
 * Based on: compress.c - File compression ala IEEE Computer, June 1984.
 *
 * By Authors:  Spencer W. Thomas       (decvax!harpo!utah-cs!utah-gr!thomas)
 *              Jim McKie               (decvax!mcvax!jim)
 *              Steve Davies            (decvax!vax135!petsd!peora!srd)
 *              Ken Turkowski           (decvax!decwrl!turtlevax!ken)
 *              James A. Woods          (decvax!ihnp4!ames!jaw)
 *              Joe Orost               (decvax!vax135!petsd!joe)
 *
 */
#include <ctype.h>

#define ARGVAL() (*++(*argv) || (--argc && *++argv))

static int n_bits;                        /* number of bits/code */
static int maxbits = BITS;                /* user settable max # bits/code */
static code_int maxcode;                  /* maximum code, given n_bits */
static code_int maxmaxcode = (code_int)1 << BITS; /* should NEVER generate this code */
#ifdef COMPATIBLE               /* But wrong! */
# define MAXCODE(n_bits)        ((code_int) 1 << (n_bits) - 1)
#else /*COMPATIBLE*/
# define MAXCODE(n_bits)        (((code_int) 1 << (n_bits)) - 1)
#endif /*COMPATIBLE*/

static count_int htab [HSIZE];
static unsigned short codetab [HSIZE];
#define HashTabOf(i)       htab[i]
#define CodeTabOf(i)    codetab[i]

static code_int hsize = HSIZE;                 /* for dynamic table sizing */

/*
 * To save much memory, we overlay the table used by compress() with those
 * used by decompress().  The tab_prefix table is the same size and type
 * as the codetab.  The tab_suffix table needs 2**BITS characters.  We
 * get this from the beginning of htab.  The output stack uses the rest
 * of htab, and contains characters.  There is plenty of room for any
 * possible stack (stack used to be 8000 characters).
 */

#define tab_prefixof(i) CodeTabOf(i)
#define tab_suffixof(i)        ((char_type*)(htab))[i]
#define de_stack               ((char_type*)&tab_suffixof((code_int)1<<BITS))

static code_int free_ent = 0;                  /* first unused entry */

/*
 * block compression parameters -- after all codes are used up,
 * and compression rate changes, start over.
 */
static int clear_flg = 0;

static int offset;
static long int in_count = 1;            /* length of input */
static long int out_count = 0;           /* # of codes output (for debugging) */

/*
 * compress stdin to stdout
 *
 * Algorithm:  use open addressing double hashing (no chaining) on the
 * prefix code / next character combination.  We do a variant of Knuth's
 * algorithm D (vol. 3, sec. 6.4) along with G. Knott's relatively-prime
 * secondary probe.  Here, the modular division first probe is gives way
 * to a faster exclusive-or manipulation.  Also do block compression with
 * an adaptive reset, whereby the code table is cleared when the compression
 * ratio decreases, but after the table fills.  The variable-length output
 * codes are re-sized at this point, and a special CLEAR code is generated
 * for the decompressor.  Late addition:  construct the table according to
 * file size for noticeable speed improvement on small files.  Please direct
 * questions about this implementation to ames!jaw.
 */

static int g_init_bits;
static FILE* g_outfile;

static int ClearCode;
static int EOFCode;

/***************************************************************************
*                          BYTE OUTPUTTER                 
***************************************************************************/

static char accum[256];
    /* The current packet, under construction */
static int a_count;
    /* Number of characters so far in the current packet */

static void
char_init() {
    a_count = 0;
}



static void
flush_char() {
/*----------------------------------------------------------------------------
   Write the current packet to the output file, then reset the current 
   packet to empty.
-----------------------------------------------------------------------------*/
    if( a_count > 0 ) {
        fputc( a_count, g_outfile );
        fwrite( accum, 1, a_count, g_outfile );
        a_count = 0;
    }
}



static void
char_out( int const c ) {
/*----------------------------------------------------------------------------
   Add a character to the end of the current packet, and if it is 254
   character, flush the packet to the output file.
-----------------------------------------------------------------------------*/
    accum[ a_count++ ] = c;
    if( a_count >= 254 )
        flush_char();
}



static unsigned long const masks[] = { 0x0000, 0x0001, 0x0003, 0x0007, 0x000F,
                                       0x001F, 0x003F, 0x007F, 0x00FF,
                                       0x01FF, 0x03FF, 0x07FF, 0x0FFF,
                                       0x1FFF, 0x3FFF, 0x7FFF, 0xFFFF };

static void
output( code_int const code, FILE * const outfile ) {
/*----------------------------------------------------------------------------
   Output one GIF code to the file.

   The code is represented as cur_bits+n_bits/8 bytes in the file.

 * Inputs:
 *      code:   A n_bits-bit integer.  If == -1, then EOF.  This assumes
 *              that n_bits =< (long)wordsize - 1.
 * Assumptions:
 *      Chars are 8 bits long.
 * Algorithm:
 *      Maintain a BITS character long buffer (so that 8 codes will
 * fit in it exactly).  Use the VAX insv instruction to insert each
 * code in turn.  When the buffer fills up empty it and start over.

-----------------------------------------------------------------------------*/
    static unsigned long cur_accum = 0;
    static int cur_bits = 0;

    g_outfile = outfile;

    cur_accum &= masks[ cur_bits ];

    if( cur_bits > 0 )
        cur_accum |= ((long)code << cur_bits);
    else
        cur_accum = code;

    cur_bits += n_bits;

    while( cur_bits >= 8 ) {
        char_out( (unsigned int)(cur_accum & 0xff) );
        cur_accum >>= 8;
        cur_bits -= 8;
    }

    /*
     * If the next entry is going to be too big for the code size,
     * then increase it, if possible.
     */
   if ( free_ent > maxcode || clear_flg ) {

            if( clear_flg ) {

                maxcode = MAXCODE (n_bits = g_init_bits);
                clear_flg = 0;

            } else {

                ++n_bits;
                if ( n_bits == maxbits )
                    maxcode = maxmaxcode;
                else
                    maxcode = MAXCODE(n_bits);
            }
        }

    if( code == EOFCode ) {
        /*
         * At EOF, write the rest of the buffer.
         */
        while( cur_bits > 0 ) {
                char_out( (unsigned int)(cur_accum & 0xff) );
                cur_accum >>= 8;
                cur_bits -= 8;
        }

        flush_char();

        fflush( g_outfile );

        if( ferror( g_outfile ) )
                writeerr();
    }
}



static void
cl_hash(hsize)          /* reset code table */
register count_int hsize;
{

        register count_int *htab_p = htab+hsize;

        register long i;
        register long m1 = -1;

        i = hsize - 16;
        do {                            /* might use Sys V memset(3) here */
                *(htab_p-16) = m1;
                *(htab_p-15) = m1;
                *(htab_p-14) = m1;
                *(htab_p-13) = m1;
                *(htab_p-12) = m1;
                *(htab_p-11) = m1;
                *(htab_p-10) = m1;
                *(htab_p-9) = m1;
                *(htab_p-8) = m1;
                *(htab_p-7) = m1;
                *(htab_p-6) = m1;
                *(htab_p-5) = m1;
                *(htab_p-4) = m1;
                *(htab_p-3) = m1;
                *(htab_p-2) = m1;
                *(htab_p-1) = m1;
                htab_p -= 16;
        } while ((i -= 16) >= 0);

        for ( i += 16; i > 0; --i )
                *--htab_p = m1;
}


/*
 * Clear out the hash table
 */
static void
cl_block (FILE * const outfile)     /* table clear for block compress */
{

        cl_hash ( (count_int) hsize );
        free_ent = ClearCode + 2;
        clear_flg = 1;

        output( (code_int)ClearCode, outfile );
}



static void
write_raster_LZW(pixel ** const pixels, pixval const input_maxval,
                 gray ** const alpha, gray const alpha_maxval, 
                 struct cmap * const cmapP, 
                 int const init_bits, FILE * const outfile) {
/*----------------------------------------------------------------------------
   Write the raster to file 'outfile'.

   The raster to write is 'pixels', which has maxval 'input_maxval',
   modified by alpha mask 'alpha', which has maxval 'alpha_maxval'.

   Use the colormap 'cmapP' to generate the raster ('pixels' is 
   composed of RGB samples; the GIF raster is colormap indices).

   Write the raster using LZW compression.
-----------------------------------------------------------------------------*/

    code_int ent;
    code_int disp;
    code_int hsize_reg;
    int hshift;
    bool eof;
    gray const alpha_threshold = (alpha_maxval + 1) / 2;
        /* gray levels below this in the alpha mask indicate transparent
           pixels in the output image.
        */
    
    /*
     * Set up the globals:  g_init_bits - initial number of bits
     *                      g_outfile   - pointer to output file
     */
    g_init_bits = init_bits;

    /*
     * Set up the necessary values
     */
    offset = 0;
    out_count = 0;
    clear_flg = 0;
    in_count = 1;
    maxcode = MAXCODE(n_bits = g_init_bits);

    ClearCode = (1 << (init_bits - 1));
    EOFCode = ClearCode + 1;
    free_ent = ClearCode + 2;

    char_init();

    ent = GIFNextPixel(pixels, input_maxval, alpha, alpha_threshold, cmapP);

    {
        long fcode;
        hshift = 0;
        for ( fcode = (long) hsize;  fcode < 65536L; fcode *= 2L )
            ++hshift;
        hshift = 8 - hshift;                /* set hash code range bound */
    }
    hsize_reg = hsize;
    cl_hash( (count_int) hsize_reg);            /* clear hash table */

    output( (code_int)ClearCode, outfile );

    eof = FALSE;
    while (!eof) {
        int gifpixel;
            /* The value for the pixel in the GIF image.  I.e. the colormap
               index.  Or -1 to indicate "no more pixels."
            */
        gifpixel = GIFNextPixel(pixels, 
                                input_maxval, alpha, alpha_threshold, cmapP);
        if (gifpixel == EOF) eof = TRUE;
        if (!eof) {
            long const fcode = (long) (((long) gifpixel << maxbits) + ent);
            code_int i;
                /* xor hashing */

            ++in_count;

            i = (((code_int)gifpixel << hshift) ^ ent);    

            if ( HashTabOf (i) == fcode ) {
                ent = CodeTabOf (i);
                continue;
            } else if ( (long)HashTabOf (i) < 0 )      /* empty slot */
                goto nomatch;
            disp = hsize_reg - i;        /* secondary hash (after G. Knott) */
            if ( i == 0 )
                disp = 1;
        probe:
            if ( (i -= disp) < 0 )
                i += hsize_reg;

            if ( HashTabOf (i) == fcode ) {
                ent = CodeTabOf (i);
                continue;
            }
            if ( (long)HashTabOf (i) > 0 )
                goto probe;
        nomatch:
            output ( (code_int) ent, outfile );
            ++out_count;
            ent = gifpixel;
            if ( free_ent < maxmaxcode ) {  /* } */
                CodeTabOf (i) = free_ent++; /* code -> hashtable */
                HashTabOf (i) = fcode;
            } else
                cl_block(outfile);
        }
    }
    /*
     * Put out the final code.
     */
    output( (code_int)ent, outfile );
    ++out_count;
    output( (code_int) EOFCode, outfile );
}



/* Routine to convert variable-width codes into a byte stream */

static void
output_uncompressed(struct gif_dest * const dinfo, int const code,
                    FILE * const outfile) {

    g_outfile = outfile;

    /* Emit a code of n_bits bits */
    /* Uses cur_accum and cur_bits to reblock into 8-bit bytes */
    dinfo->cur_accum |= ((int) code) << dinfo->cur_bits;
    dinfo->cur_bits += dinfo->n_bits;

    while (dinfo->cur_bits >= 8) {
        char_out(dinfo->cur_accum & 0xFF);
        dinfo->cur_accum >>= 8;
        dinfo->cur_bits -= 8;
    }
}


static void
write_raster_uncompressed_init (struct gif_dest * const dinfo, 
                                int const i_bits, FILE * const outfile)
/* Initialize pseudo-compressor */
{
  /* init all the state variables */
  dinfo->n_bits = i_bits;
  dinfo->maxcode = MAXCODE(dinfo->n_bits);
  dinfo->ClearCode = (1 << (i_bits - 1));
  dinfo->EOFCode = dinfo->ClearCode + 1;
  dinfo->code_counter = dinfo->ClearCode + 2;
  /* init output buffering vars */
  dinfo->cur_accum = 0;
  dinfo->cur_bits = 0;
  /* GIF specifies an initial Clear code */
  output_uncompressed(dinfo, dinfo->ClearCode, outfile);
}


static void
write_raster_uncompressed_pixel (struct gif_dest * const dinfo, 
                                 int const c, FILE * const outfile)
/* Accept and "compress" one pixel value.
 * The given value must be less than n_bits wide.
 */
{
  /* Output the given pixel value as a symbol. */
  output_uncompressed(dinfo, c, outfile);
  /* Issue Clear codes often enough to keep the reader from ratcheting up
   * its symbol size.
   */
  if (dinfo->code_counter < dinfo->maxcode) {
    dinfo->code_counter++;
  } else {
    output_uncompressed(dinfo, dinfo->ClearCode, outfile);
    dinfo->code_counter = dinfo->ClearCode + 2;	/* reset the counter */
  }
}


static void
write_raster_uncompressed_term (struct gif_dest * const dinfo,
                                FILE * const outfile)
/* Clean up at end */
{
  /* Send an EOF code */
  output_uncompressed(dinfo, dinfo->EOFCode, outfile);
  /* Flush the bit-packing buffer */
  if (dinfo->cur_bits > 0) {
    char_out(dinfo->cur_accum & 0xFF);
  }
  /* Flush the packet buffer */
  flush_char();
}



static void
write_raster_uncompressed(pixel ** const pixels, pixval const input_maxval,
                          gray ** const alpha, gray const alpha_maxval, 
                          struct cmap * const cmapP, 
                          int const init_bits, FILE * const outfile) {
/*----------------------------------------------------------------------------
   Write the raster to file 'outfile'.
   
   Same as write_raster_LZW(), except written out one code per
   pixel (plus some clear codes), so no compression.  And no use
   of the LZW patent.
-----------------------------------------------------------------------------*/
    gray const alpha_threshold = (alpha_maxval + 1) / 2;
        /* gray levels below this in the alpha mask indicate transparent
           pixels in the output image.
        */
    bool eof;

    struct gif_dest gif_dest;

    write_raster_uncompressed_init(&gif_dest, init_bits, outfile);

    g_outfile = outfile;

    eof = FALSE;
    while (!eof) {
        int gifpixel;
            /* The value for the pixel in the GIF image.  I.e. the colormap
               index.  Or -1 to indicate "no more pixels."
            */
        gifpixel = GIFNextPixel(pixels, 
                                input_maxval, alpha, alpha_threshold, cmapP);
        if (gifpixel == EOF) eof = TRUE;
        if (!eof) {
            write_raster_uncompressed_pixel(&gif_dest, gifpixel, outfile);
        }
    }
    write_raster_uncompressed_term(&gif_dest, outfile);
}



static void
writeerr()
{
        pm_error( "error writing output file" );
}

/******************************************************************************
 *
 * GIF Specific routines
 *
 ******************************************************************************/

static void
writeGifHeader(FILE * const fp,
               int const Width, int const Height, 
               int const GInterlace, int const Background, 
               int const BitsPerPixel, struct cmap * const cmapP,
               const char comment[]) {

    int B;
    int const Resolution = BitsPerPixel;
    int const ColorMapSize = 1 << BitsPerPixel;

    /* Calculate number of bits we are expecting */
    CountDown = (long)Width * (long)Height;

    /* Indicate which pass we are on (if interlace) */
    Pass = 0;

    /* Set up the current x and y position */
    curx = cury = 0;

    /* Write the Magic header */
    if (cmapP->transparent != -1 || comment)
        fwrite("GIF89a", 1, 6, fp);
    else
        fwrite("GIF87a", 1, 6, fp);

    /* Write out the screen width and height */
    Putword( Width, fp );
    Putword( Height, fp );

    /* Indicate that there is a global colour map */
    B = 0x80;       /* Yes, there is a color map */

    /* OR in the resolution */
    B |= (Resolution - 1) << 5;

    /* OR in the Bits per Pixel */
    B |= (BitsPerPixel - 1);

    /* Write it out */
    fputc( B, fp );

    /* Write out the Background color */
    fputc( Background, fp );

    /* Byte of 0's (future expansion) */
    fputc( 0, fp );

    {
        /* Write out the Global Color Map */
        int i;
        for( i=0; i < ColorMapSize; ++i ) {
            fputc( cmapP->red[i], fp );
            fputc( cmapP->green[i], fp );
            fputc( cmapP->blue[i], fp );
        }
    }
        
    if ( cmapP->transparent >= 0 ) 
        write_transparent_color_index_extension(fp, cmapP->transparent);

    if ( comment )
        write_comment_extension(fp, comment);
}



static void
GIFEncode(FILE * const fp, 
          pixel ** const pixels, pixval const input_maxval,
          int const GWidth, int const GHeight, 
          gray ** const alpha, gray const alpha_maxval,
          int const GInterlace, int const Background, 
          int const BitsPerPixel, struct cmap * const cmapP,
          const char comment[], bool const nolzw) {

    int const LeftOfs = 0;
    int const TopOfs = 0;
    int InitCodeSize;
    
    writeGifHeader(fp, GWidth, GHeight, GInterlace, Background,
                   BitsPerPixel, cmapP, comment);

    /* Write an Image separator */
    fputc( ',', fp );

    /* Write the Image header */

    Putword( LeftOfs, fp );
    Putword( TopOfs, fp );
    Putword( GWidth, fp );
    Putword( GHeight, fp );

    /* Write out whether or not the image is interlaced */
    if( GInterlace )
        fputc( 0x40, fp );
    else
        fputc( 0x00, fp );

    /* The initial code size */
    if( BitsPerPixel <= 1 )
        InitCodeSize = 2;
    else
        InitCodeSize = BitsPerPixel;

    /* Write out the initial code size */
    fputc( InitCodeSize, fp );

    /* Set some global variables for BumpPixel() */
    Interlace = GInterlace;
    Width = GWidth;
    Height = GHeight;

    /* Write the actual raster */
    if (nolzw)
        write_raster_uncompressed(pixels, 
                                  input_maxval, alpha, alpha_maxval, cmapP, 
                                  InitCodeSize+1, fp);
    else
        write_raster_LZW(pixels, 
                         input_maxval, alpha, alpha_maxval, cmapP, 
                         InitCodeSize+1, fp);

    /* Write out a Zero-length packet (to end the series) */
    fputc( 0, fp );

    /* Write the GIF file terminator */
    fputc( ';', fp );

    /* And close the file */
    fclose( fp );
}

/*
 * Write out a word to the GIF file
 */
static void
Putword(int const w, FILE * const fp) {

    fputc( w & 0xff, fp );
    fputc( (w / 256) & 0xff, fp );
}


static int
compute_transparent(const char colorarg[], 
                    struct cmap * const cmapP) {
/*----------------------------------------------------------------------------
   Figure out the color index (index into the colormap) of the color
   that is to be transparent in the GIF.

   colorarg[] is the string that specifies the color the user wants to
   be transparent (e.g. "red", "#fefefe").  Its maxval is the maxval
   of the colormap.  'cmap' is the full colormap except that its
   'transparent' component isn't valid.

   colorarg[] is a standard Netpbm color specification, except that
   may have a "=" prefix, which means it specifies a particular exact
   color, as opposed to without the "=", which means "the color that
   is closest to this and actually in the image."

   Return -1 if colorarg[] specifies an exact color and that color is not
   in the image.  Also issue an informational message.
-----------------------------------------------------------------------------*/
    int retval;

    const char *colorspec;
    bool exact;
    int presort_colorindex;
    pixel transcolor;

    if (colorarg[0] == '=') {
        colorspec = &colorarg[1];
        exact = TRUE;
    } else {
        colorspec = colorarg;
        exact = FALSE;
    }
        
    transcolor = ppm_parsecolor((char*)colorspec, cmapP->maxval);
    presort_colorindex = ppm_lookupcolor(cmapP->cht, &transcolor);
    
    if (presort_colorindex != -1)
        retval = cmapP->perm[presort_colorindex];
    else if (!exact)
        retval = cmapP->perm[closestcolor(transcolor, cmapP->maxval, cmapP)];
    else {
        retval = -1;
        pm_message(
            "Warning: specified transparent color does not occur in image.");
    }
    return retval;
}



static void
sort_colormap(int const sort, struct cmap * const cmapP) {
/*----------------------------------------------------------------------------
   Sort (in place) the colormap *cmapP.

   Create the perm[] and permi[] mappings for the colormap.

   'sort' is logical:  true means to sort the colormap by red intensity,
   then by green intensity, then by blue intensity.  False means a null
   sort -- leave it in the same order in which we found it.
-----------------------------------------------------------------------------*/
    int * const Red = cmapP->red;
    int * const Blue = cmapP->blue;
    int * const Green = cmapP->green;
    int * const perm = cmapP->perm;
    int * const permi = cmapP->permi;
    unsigned int const cmapsize = cmapP->cmapsize;
    
    int i;

    for (i=0; i < cmapsize; i++)
        permi[i] = i;

    if (sort) {
        pm_message("sorting colormap");
        for (i=0; i < cmapsize; i++) {
            int j;
            for (j=i+1; j < cmapsize; j++)
                if (((Red[i]*MAXCMAPSIZE)+Green[i])*MAXCMAPSIZE+Blue[i] >
                    ((Red[j]*MAXCMAPSIZE)+Green[j])*MAXCMAPSIZE+Blue[j]) {
                    int tmp;
                    
                    tmp=permi[i]; permi[i]=permi[j]; permi[j]=tmp;
                    tmp=Red[i]; Red[i]=Red[j]; Red[j]=tmp;
                    tmp=Green[i]; Green[i]=Green[j]; Green[j]=tmp;
                    tmp=Blue[i]; Blue[i]=Blue[j]; Blue[j]=tmp; } }
    }

    for (i=0; i < cmapsize; i++)
        perm[permi[i]] = i;
}



static void
normalize_to_255(colorhist_vector const chv, struct cmap * const cmapP) {
/*----------------------------------------------------------------------------
   With a PPM color histogram vector 'chv' as input, produce a colormap
   of integers 0-255 as output in *cmapP.
-----------------------------------------------------------------------------*/
    int i;
    pixval const maxval = cmapP->maxval;

    if ( maxval != 255 )
        pm_message(
            "maxval is not 255 - automatically rescaling colors" );

    for ( i = 0; i < cmapP->cmapsize; ++i ) {
        if ( maxval == 255 ) {
            cmapP->red[i] = (int) PPM_GETR( chv[i].color );
            cmapP->green[i] = (int) PPM_GETG( chv[i].color );
            cmapP->blue[i] = (int) PPM_GETB( chv[i].color );
        } else {
            cmapP->red[i] = (int) PPM_GETR( chv[i].color ) * 255 / maxval;
            cmapP->green[i] = (int) PPM_GETG( chv[i].color ) * 255 / maxval;
            cmapP->blue[i] = (int) PPM_GETB( chv[i].color ) * 255 / maxval;
        }
    }
}



static void add_to_colormap(struct cmap * const cmapP, char * const
                            colorspec, int * new_indexP) {
/*----------------------------------------------------------------------------
  Add a new entry to the colormap.  Make the color that specified by
  'colorspec', and return the index of the new entry as *new_indexP.

  'colorspec' is a color specification given by the user, e.g.
  "red" or "rgb:ff/03.0d".  The maxval for this color specification is
  that for the colormap *cmapP.
-----------------------------------------------------------------------------*/
    pixel const transcolor = ppm_parsecolor((char*)colorspec, cmapP->maxval);
    
    *new_indexP = cmapP->cmapsize++; 

    cmapP->red[*new_indexP] = PPM_GETR(transcolor);
    cmapP->green[*new_indexP] = PPM_GETG(transcolor); 
    cmapP->blue[*new_indexP] = PPM_GETB(transcolor); 
}



static void
colormap_from_file(char filespec[], unsigned int const maxcolors,
                   colorhist_vector * const chvP, pixval * const maxvalP,
                   int * const colorsP) {
/*----------------------------------------------------------------------------
   Read a colormap from the PPM file filespec[].  Return the color histogram
   vector (which is practically a colormap) of the input image as *cvhP
   and the maxval for that histogram as *maxvalP.
-----------------------------------------------------------------------------*/
    FILE *mapfile;
    int cols, rows;
    pixel ** colormap_ppm;

    mapfile = pm_openr(filespec);
    colormap_ppm = ppm_readppm(mapfile, &cols, &rows, maxvalP);
    pm_close(mapfile);
    
    /* Figure out the colormap from the <mapfile>. */
    pm_message("computing other colormap...");
    *chvP = 
        ppm_computecolorhist(colormap_ppm, cols, rows, maxcolors, colorsP);
    
    ppm_freearray(colormap_ppm, rows); 
}


static void
get_alpha(char * const alpha_filespec, int const cols, int const rows,
          gray *** const alphaP, gray * const maxvalP) {

    if (alpha_filespec) {
        int alpha_cols, alpha_rows;
        *alphaP = pgm_readpgm(pm_openr(alpha_filespec),
                              &alpha_cols, &alpha_rows, maxvalP);
        if (alpha_cols != cols || alpha_rows != rows)
            pm_error("alpha mask is not the same dimensions as the "
                     "input file (alpha is %dW x %dH; image is %dW x %dH)",
                     alpha_cols, alpha_rows, cols, rows);
    } else 
        *alphaP = NULL;
}


static void
compute_ppm_colormap(pixel ** const pixels, int const cols, int const rows,
                     int const input_maxval, bool const have_alpha, 
                     char * const mapfile, colorhist_vector * const chvP,
                     colorhash_table * const chtP,
                     pixval * const colormap_maxvalP, 
                     int * const colorsP) {
/*----------------------------------------------------------------------------
   Compute a colormap, PPM style, for the image 'pixels', which is
   'cols' by 'rows' with maxval 'input_maxval'.  If 'mapfile' is
   non-null, Use the colors in that (PPM) file for the color map
   instead of the colors in 'pixels'.

   Return the colormap as *chvP and *chtP.  Return the maxval for that
   colormap as *colormap_maxvalP.

   While we're at it, count the colors and validate that there aren't
   too many.  Return the count as *colorsP.  In determining if there are
   too many, allow one slot for a fake transparency color if 'have_alpha'
   is true.  If there are too many, issue an error message and abort the
   program.
-----------------------------------------------------------------------------*/
    unsigned int maxcolors;
        /* The most colors we can tolerate in the image.  If we have
           our own made-up entry in the colormap for transparency, it
           isn't included in this count.
        */

    if (have_alpha)
        maxcolors = MAXCMAPSIZE - 1;
    else
        maxcolors = MAXCMAPSIZE;

    if (mapfile) {
        /* Read the colormap from a separate colormap file. */
        colormap_from_file(mapfile, maxcolors, chvP, colormap_maxvalP, 
                           colorsP);
    } else {
        /* Figure out the color map from the input file */
        pm_message("computing colormap...");
        *chvP = ppm_computecolorhist(pixels, cols, rows, maxcolors, colorsP); 
        *colormap_maxvalP = input_maxval;
    }

    if (*chvP == NULL)
        pm_error("too many colors - try doing a 'ppmquant %d'", maxcolors);
    pm_message("%d colors found", *colorsP);

    /* And make a hash table for fast lookup. */
    *chtP = ppm_colorhisttocolorhash(*chvP, *colorsP);
}



int
main(int argc, char *argv[]) {
    struct cmdline_info cmdline;
    FILE* ifp;
    int rows, cols;
    int BitsPerPixel;
    pixel ** pixels;   /* The input image, in PPM format */
    pixval input_maxval;  /* Maxval for 'pixels' */
    gray ** alpha;     /* The supplied alpha mask; NULL if none */
    gray alpha_maxval; /* Maxval for 'alpha' */

    struct cmap cmap;
        /* The colormap, with all its accessories */
    colorhist_vector chv;
    int fake_transparent;
        /* colormap index of the fake transparency color we're using to
           implement the alpha mask.  Undefined if we're not doing an alpha
           mask.
        */

    ppm_init( &argc, argv );

    parse_command_line(argc, argv, &cmdline);

    ifp = pm_openr(cmdline.input_filespec);

    pixels = ppm_readppm(ifp, &cols, &rows, &input_maxval);

    pm_close(ifp);

    get_alpha(cmdline.alpha_filespec, cols, rows, &alpha, &alpha_maxval);

    compute_ppm_colormap(pixels, cols, rows, input_maxval, (alpha != NULL), 
                         cmdline.mapfile, 
                         &chv, &cmap.cht, &cmap.maxval, &cmap.cmapsize);

    /* Now turn the ppm colormap into the appropriate GIF colormap. */

    normalize_to_255(chv, &cmap);

    ppm_freecolorhist(chv);

    if (alpha) {
        /* Add a fake entry to the end of the colormap for transparency.  
           Make its color black. 
        */
        add_to_colormap(&cmap, cmdline.alphacolor, &fake_transparent);
    }
    sort_colormap(cmdline.sort, &cmap);

    BitsPerPixel = pm_maxvaltobits(cmap.cmapsize-1);

    if (alpha) {
        cmap.transparent = cmap.perm[fake_transparent];
    } else {
        if (cmdline.transparent)
            cmap.transparent = 
                compute_transparent(cmdline.transparent, &cmap);
        else 
            cmap.transparent = -1;
    }
    /* All set, let's do it. */
    GIFEncode(stdout, pixels, input_maxval, cols, rows, 
              alpha, alpha_maxval, 
              cmdline.interlace, 0, BitsPerPixel, &cmap, cmdline.comment,
              cmdline.nolzw);

    ppm_freearray(pixels, rows);
    if (alpha)
        pgm_freearray(alpha, rows);

    exit(0);
}


