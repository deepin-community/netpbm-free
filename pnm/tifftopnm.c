/*
** tifftopnm.c - converts a Tagged Image File to a portable anymap
**
** Derived by Jef Poskanzer from tif2ras.c, which is:
**
** Copyright (c) 1990 by Sun Microsystems, Inc.
**
** Author: Patrick J. Naughton
** naughton@wind.sun.com
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation.
**
** This file is provided AS IS with no warranties of any kind.  The author
** shall have no liability with respect to the infringement of copyrights,
** trade secrets or any patents by this file or any part thereof.  In no
** event will the author be liable for any lost revenue or profits or
** other special, indirect and consequential damages.
*/

/* Implementation note:

  This probably ought to be reimplemented using TIFFRGBAImageGet() 
  from the Tiff library, because that routine automatically decodes
  anything and everything that might be in a Tiff image file now or
  in the future.

  Much of the code below is basically copied from the Tiff library
  because we use the more primitive TIFFReadScanLine() access method.

  -Bryan 00.03.05
*/

#define _BSD_SOURCE 1
    /* Make sure strdup() is in string.h */

#include <string.h>
#include "pnm.h"
#ifdef VMS
#ifdef SYSV
#undef SYSV
#endif
#include <tiffioP.h>
#endif
#include <tiffio.h>

/* The following are in current tiff.h, but so that we can compile against
   older tiff libraries, we define them here.
*/

#ifndef PHOTOMETRIC_LOGL
#define PHOTOMETRIC_LOGL 32844
#endif
#ifndef PHOTOMETRIC_LOGLUV
#define PHOTOMETRIC_LOGLUV 32845
#endif

#define MAXCOLORS 1024
#ifndef PHOTOMETRIC_DEPTH
#define PHOTOMETRIC_DEPTH 32768
#endif

struct cmdlineInfo {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
    char *inputFilename;
    unsigned int headerdump;
    char *alphaFilename;
    int alphaStdout;
    int respectfillorder;   /* -respectfillorder option */
} cmdline;



static void
parseCommandLine(int argc, char ** argv,
                 struct cmdlineInfo *cmdlineP) {
/*----------------------------------------------------------------------------
   Note that many of the strings that this function returns in the
   *cmdlineP structure are actually in the supplied argv array.  And
   sometimes, one of these strings is actually just a suffix of an entry
   in argv!
-----------------------------------------------------------------------------*/
    optStruct3 opt;
    optEntry *option_def = malloc(100*sizeof(optEntry));
    unsigned int option_def_index;
    unsigned int alpha_spec;

    opt.opt_table = option_def;
    opt.short_allowed = FALSE;
    opt.allowNegNum = FALSE;

    option_def_index = 0;   /* incremented by OPTENTRY */
    OPTENT3(0, "respectfillorder", 
            OPT_FLAG,   NULL, &cmdlineP->headerdump, 0);
    OPTENT3('h', "headerdump", 
            OPT_FLAG,   NULL, &cmdlineP->headerdump, 0);
    OPTENT3(0,   "alphaout",   
            OPT_STRING, &cmdlineP->alphaFilename, &alpha_spec, 0);

    pm_optParseOptions3(&argc, argv, opt, sizeof(opt), 0);

    if (argc - 1 == 0)
        cmdlineP->inputFilename = NULL;  /* he wants stdin */
    else if (argc - 1 == 1)
        cmdlineP->inputFilename = strdup(argv[1]);
    else 
        pm_error("Too many arguments.  The only argument accepted\n"
                 "is the input file specificaton");

    if (alpha_spec && strcmp(cmdlineP->alphaFilename, "-") == 0 )
        cmdlineP->alphaStdout = 1;
    else 
        cmdlineP->alphaStdout = 0;
}




static void read_directory(TIFF * const tif,
                           unsigned short * const bps_p,
                           unsigned short * const spp_p,
                           unsigned short * const photomet_p,
                           unsigned short * const planarconfig_p,
                           unsigned short * const fillorder_p,
                           unsigned int *   const cols_p,
                           unsigned int *   const rows_p,
                           bool             const headerdump) {
/*----------------------------------------------------------------------------
   Read various values of TIFF tags from the TIFF directory, and
   default them if not in there and make guesses where values are
   invalid.  Exit program with error message if required tags aren't
   there or values are inconsistent or beyond our capabilities.  if
   'headerdump' is true, issue informational messages about what we
   find.

   The TIFF library is capable of returning invalid values (if the
   input file contains invalid values).  We generally return those
   invalid values to our caller.
-----------------------------------------------------------------------------*/
    int rc;
    unsigned short tiff_bps;
    unsigned short tiff_spp;

    if (headerdump)
        TIFFPrintDirectory(tif, stderr, TIFFPRINT_NONE);

    rc = TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &tiff_bps);
    *bps_p = (rc == 0) ? 1 : tiff_bps;

    if (*bps_p < 1 || (*bps_p > 8 && *bps_p != 16))
        pm_error("This program can process Tiff images with only "
                 "1-8 or 16 bits per sample.  The input Tiff image "
                 "has %d bits per sample.", *bps_p);

    rc = TIFFGetFieldDefaulted(tif, TIFFTAG_FILLORDER, fillorder_p);
    rc = TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &tiff_spp);
    *spp_p = (rc == 0) ? 1: tiff_spp;

    rc = TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, photomet_p);
    if (rc == 0)
        pm_error("PHOTOMETRIC tag is not in Tiff file.  "
                 "TIFFGetField() of it failed.\n"
                 "This means the input is not valid Tiff.");

    if (*spp_p > 1) {
        rc = TIFFGetField(tif, TIFFTAG_PLANARCONFIG, planarconfig_p);
        if (rc == 0)
            pm_error("PLANARCONFIG tag is not in Tiff file, though it "
                     "has more than one sample per pixel.  "
                     "TIFFGetField() of it failed.  This means the input "
                     "is not valid Tiff.");
    } else {
        *planarconfig_p = PLANARCONFIG_CONTIG;
    }

    switch (*spp_p) {
    case 1:
    case 3:
    case 4:
        break;
    default:
        pm_error(
            "This program can only handle 1-channel gray scale "
            "or 1- or 3-channel color.  The input Tiff file has %d channels "
            "(samples per pixel)",
            *spp_p);
    }

    switch(*planarconfig_p) {
    case PLANARCONFIG_CONTIG:
        break;
    case PLANARCONFIG_SEPARATE:
        if (*photomet_p != PHOTOMETRIC_RGB && 
            *photomet_p != PHOTOMETRIC_SEPARATED)
            pm_error("This program can handle separate planes only "
                     "with RGB (PHOTOMETRIC tag = %d) or SEPARATED "
                     "(PHOTOMETRIC tag = %d) data.  The input Tiff file " 
                     "has PHOTOMETRIC tag = %d.",
                     PHOTOMETRIC_RGB, PHOTOMETRIC_SEPARATED, *photomet_p);
        break;
    default:
        pm_error("Unrecognized PLANARCONFIG tag value in Tiff input: %d.\n",
                 *planarconfig_p);
    }

    rc = TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, cols_p);
    if (rc == 0)
        pm_error("Input Tiff file is invalid.  It has no IMAGEWIDTH tag.");
    rc = TIFFGetField( tif, TIFFTAG_IMAGELENGTH, rows_p );
    if (rc == 0)
        pm_error("Input Tiff file is invalid.  It has no IMAGELENGTH tag.");

    if (headerdump) {
        pm_message( "%dx%dx%d image", *cols_p, *rows_p, *bps_p * *spp_p );
        pm_message( "%d bits/sample, %d samples/pixel", *bps_p, *spp_p );
    }
}



static void
readscanline(TIFF * const tif, unsigned short * const samplebuf,
             unsigned char scanbuf[], int const row, int const plane,
             unsigned int const cols, unsigned short const bps,
             unsigned short const spp,
             unsigned short const fillorder) {
/*----------------------------------------------------------------------------
   Read one scanline out of the Tiff input and store it into samplebuf[].
   Unlike the scanline returned by the Tiff library function, samplebuf[]
   is composed of one sample per array element, which makes it easier for
   our caller to process.

   scanbuf[] is a scratch array for our use, which is big enough to hold
   a Tiff scanline.
-----------------------------------------------------------------------------*/
    int rc;
    const unsigned int bpsmask = (1 << bps) - 1;
      /* A mask for taking the lowest 'bps' bits of a number */

    /* The TIFFReadScanline man page doesn't tell the format of its
       'buf' return value, but it is exactly the same format as the 'buf'
       input to TIFFWriteScanline.  The man page for that doesn't say 
       anything either, but the source code for Pnmtotiff contains a
       specification.
    */

    rc = TIFFReadScanline(tif, scanbuf, row, plane);
    if (rc < 0)
        pm_error( "Unable to read row %d, plane %d of input Tiff image.  "
                  "TIFFReadScanline() failed.",
                  row, plane);
    else if (bps == 8) {
        int sample;
        for (sample = 0; sample < cols*spp; sample++) 
            samplebuf[sample] = scanbuf[sample];
    } else if (bps < 8) {
        /* Note that in this format, samples do not span bytes.  Rather,
           each byte may have don't-care bits in the right-end positions.
           At least that's how I infer the format from reading pnmtotiff.c
           -Bryan 00.11.18
           */
        int sample;
        int bitsleft;
        unsigned char * inP;

        for (sample = 0, bitsleft=8, inP=scanbuf; 
             sample < cols*spp; 
             sample++) {
            if (bitsleft == 0) {
                ++inP; 
                bitsleft = 8;
            } 
            switch (fillorder) {
            case FILLORDER_LSB2MSB:
                samplebuf[sample] = (*inP >> (8-bitsleft)) & bpsmask;
                break;
            case FILLORDER_MSB2LSB:
            default:
                samplebuf[sample] = (*inP >> (bitsleft-bps)) & bpsmask; 
				break;
            }
            bitsleft -= bps; 
            if (bitsleft < bps)
                /* Don't count dregs at end of byte */
                bitsleft = 0;
       }
    } else if (bps == 16) {
        /* Before Netpbm 9.17, this program assumed that scanbuf[]
           contained an array of bytes as read from the Tiff file.  In
           fact, in this bps == 16 case, it's an array of "shorts",
           each stored in whatever format this platform uses (which is
           none of our concern).  The pre-9.17 code also presumed that
           the TIFF "FILLORDER" tag determined the order in which the
           bytes of each sample appear in a TIFF file, which is
           contrary to the TIFF spec.  
        */
        memcpy((char *)&samplebuf[0], &scanbuf[0], cols*spp*2);
    } else
        pm_error("Internal error: invalid bits per sample passed to "
                 "readscanline()");
}



static void
pick_cmyk_pixel(const unsigned short samplebuf[], const int sample_cursor,
                xelval * const r_p, xelval * const b_p, xelval * const g_p) {

    unsigned char c, y, m, k;

    c = samplebuf[sample_cursor+0];
    y = samplebuf[sample_cursor+1];
    m = samplebuf[sample_cursor+2];
    k = samplebuf[sample_cursor+3];

    /* I don't know why this works.  It seems to me that yellow pigment should
       translate into both red and green light, not just green as you see here.
       But this is the formula used by TIFFRGBAImageGet() in the Tiff library.
       */
    
    *r_p = ((255-k)*(255-c))/255;
    *g_p = ((255-k)*(255-y))/255;
    *b_p = ((255-k)*(255-m))/255;
}



static void
computeFillorder(unsigned short   const fillorderTag, 
                 unsigned short * const fillorderP, 
                 bool             const respectfillorder) {

    if (respectfillorder) {
        if (fillorderTag != FILLORDER_MSB2LSB && 
            fillorderTag != FILLORDER_LSB2MSB)
            pm_error("Invalid value in Tiff input for the FILLORDER tag: %u.  "
                     "Valid values are %u and %u.  Try omitting the "
                     "-respectfillorder option.", 
                     fillorderTag, FILLORDER_MSB2LSB, FILLORDER_LSB2MSB);
        else
            *fillorderP = fillorderTag;
    } else {
        *fillorderP = FILLORDER_MSB2LSB;
        if (fillorderTag != *fillorderP)
            pm_message("Warning: overriding FILLORDER tag in the tiff input "
                       "and assuming msb-to-lsb.  Consider the "
                       "-respectfillorder option.");
    }
}



static void
analyze_image_type(TIFF * const tif, unsigned short const bps, 
                   unsigned short const spp, unsigned short const photomet,
                   xelval * const maxvalP, int * const formatP, 
                   xel colormap[], bool const headerdump) {

    bool grayscale; 

    if ( bps == 1 && spp == 1 ) {
        if ( cmdline.headerdump )
            pm_message("monochrome" );
        grayscale = TRUE;
        *maxvalP = 1;
    } else {
        /* How come we don't deal with the photometric for the monochrome 
           case (make sure it's one we know)?  -Bryan 00.03.04
           */
        switch ( photomet ) {
        case PHOTOMETRIC_MINISBLACK:
            grayscale = TRUE;
            *maxvalP = (1 << bps) - 1;
            if (headerdump)
                pm_message( "%d graylevels (min=black)", *maxvalP + 1 );
            break;
            
        case PHOTOMETRIC_MINISWHITE:
            grayscale = TRUE;
            *maxvalP = (1 << bps) - 1;
            if (headerdump)
                pm_message( "%d graylevels (min=white)", *maxvalP + 1 );
            break;
            
        case PHOTOMETRIC_PALETTE: {
            int i;
            int numcolors;
            unsigned short* redcolormap;
            unsigned short* greencolormap;
            unsigned short* bluecolormap;

            if (headerdump)
                pm_message("colormapped");

            if (!TIFFGetField(tif, TIFFTAG_COLORMAP, 
                              &redcolormap, &greencolormap, &bluecolormap))
                pm_error("error getting colormaps");

            numcolors = 1 << bps;
            if (numcolors > MAXCOLORS)
                pm_error("too many colors");
            *maxvalP = PNM_MAXMAXVAL;
            grayscale = FALSE;
            for (i = 0; i < numcolors; ++i) {
                xelval r, g, b;
                r = (long) redcolormap[i] * PNM_MAXMAXVAL / 65535L;
                g = (long) greencolormap[i] * PNM_MAXMAXVAL / 65535L;
                b = (long) bluecolormap[i] * PNM_MAXMAXVAL / 65535L;
                PPM_ASSIGN(colormap[i], r, g, b);
            }
        }
        break;

        case PHOTOMETRIC_SEPARATED: {
            unsigned short inkset;

            if (headerdump)
                pm_message("color separation");
            if (TIFFGetField(tif, TIFFTAG_INKNAMES, &inkset) == 1
                && inkset != INKSET_CMYK)
            if (inkset != INKSET_CMYK) 
                pm_error("This color separation file uses an inkset (%d) "
                         "we can't handle.  We handle only CMYK.", inkset);
            if (spp != 4) 
                pm_error("This CMYK color separation file is %d samples per "
                         "pixel.  "
                         "We need 4 samples, though: C, M, Y, and K.  ",
                         spp);
            grayscale = FALSE;
            *maxvalP = (1 << bps) - 1;
        }
        break;
            
        case PHOTOMETRIC_RGB:
            if (headerdump)
                pm_message("RGB truecolor");
            grayscale = FALSE;
            *maxvalP = (1 << bps) - 1;
            break;

        case PHOTOMETRIC_MASK:
            pm_error("don't know how to handle PHOTOMETRIC_MASK");

        case PHOTOMETRIC_DEPTH:
            pm_error("don't know how to handle PHOTOMETRIC_DEPTH");

        case PHOTOMETRIC_YCBCR:
            pm_error("don't know how to handle PHOTOMETRIC_YCBCR");

        case PHOTOMETRIC_CIELAB:
            pm_error("don't know how to handle PHOTOMETRIC_CIELAB");

        case PHOTOMETRIC_LOGL:
            pm_error("don't know how to handle PHOTOMETRIC_LOGL");

        case PHOTOMETRIC_LOGLUV:
            pm_error("don't know how to handle PHOTOMETRIC_LOGLUV");
            
        default:
            pm_error("unknown photometric: %d", photomet);
        }
    }
    if (*maxvalP > PNM_OVERALLMAXVAL)
        pm_error("bits/sample (%d) in the input image is too large.",
                 bps);
    if (grayscale) {
        if (*maxvalP == 1) {
            *formatP = PBM_TYPE;
            pm_message("writing PBM file");
        } else {
            *formatP = PGM_TYPE;
            pm_message("writing PGM file");
        }
    } else {
        *formatP = PPM_TYPE;
        pm_message("writing PPM file");
    }
}



static void
convertRow(unsigned short samplebuf[], xel xelrow[], gray alpharow[], 
           int const cols, xelval const maxval, 
           unsigned short const photomet, unsigned short const spp,
           xel colormap[]) {
/*----------------------------------------------------------------------------
   Assuming samplebuf[] is a scan line as returned by the Tiff library,
   convert it to a libpnm row in xelrow[] and alpharow[].

-----------------------------------------------------------------------------*/
    switch (photomet) {
    case PHOTOMETRIC_MINISBLACK: {
        int col;
        for (col = 0; col < cols; ++col) {
            PNM_ASSIGN1(xelrow[col], samplebuf[col]);
            alpharow[col] = 0;
        }
    }
    break;
    
    case PHOTOMETRIC_MINISWHITE: {
        int col;
        for (col = 0; col < cols; ++col) {
            PNM_ASSIGN1(xelrow[col], maxval - samplebuf[col]);
            alpharow[col] = 0;
        }
    }
    break;

    case PHOTOMETRIC_PALETTE: {
        int col;
        for ( col = 0; col < cols; ++col ) {
            /* We know the following array index is in bounds because
               we filled samplebuf with samples of 'bps' bits each and
               we verified that the largest number that fits in 'bps'
               bits is less than MAXCOLORS, the dimension of the array.
            */
            xelrow[col] = colormap[samplebuf[col]];
            alpharow[col] = 0;
        }
    }
    break;

    case PHOTOMETRIC_SEPARATED: {
        int col, sample;
        for (col = 0, sample = 0; col < cols; ++col, sample+=spp) {
            xelval r, g, b;
            pick_cmyk_pixel(samplebuf, sample, &r, &b, &g);
            
            PPM_ASSIGN(xelrow[col], r, g, b);
            alpharow[col] = 0;
        }
    }
    break;

    case PHOTOMETRIC_RGB: {
        int col, sample;
        for (col = 0, sample = 0; col < cols; ++col, sample+=spp) {
            PPM_ASSIGN(xelrow[col], samplebuf[sample+0],
                       samplebuf[sample+1], samplebuf[sample+2]);
            if (spp >= 4)
                alpharow[col] = samplebuf[sample+3];
            else
                alpharow[col] = 0;
        }
        break;
    }       
    default:
        pm_error("internal error:  unknown photometric in the picking "
                 "routine: %d", photomet);
    }
}



static void
convertRaster(FILE * const imageoutFile, 
              FILE * const alphaFile,
              int const cols, 
              int const rows,
              xelval const maxval,
              int const format, 
              TIFF * const tif,
              unsigned short const photomet, 
              unsigned short const planarconfig,
              unsigned short const bps,
              unsigned short const spp,
              unsigned short const fillorder,
              xel colormap[]) {

    unsigned char * scanbuf;
        /* Buffer for a raster line in the format returned by TIFF library's
           TIFFReadScanline
        */
    unsigned short * samplebuf;
        /* Same info as 'scanbuf' above, but with each raster column (sample)
           represented as single array element, so it's easy to work with.
        */
    xel* xelrow;
        /* The ppm-format row of the image row we are presently converting */
    gray* alpharow;
        /* The pgm-format row representing the alpha values for the image 
           row we are presently converting.
           */

    int row;

    scanbuf = (unsigned char *) malloc(TIFFScanlineSize(tif));
    if (scanbuf == NULL)
        pm_error("can't allocate memory for scanline buffer");

    samplebuf = (unsigned short *) malloc3(cols , sizeof(unsigned short) , spp);
    if (samplebuf == NULL)
        pm_error ("can't allocate memory for row buffer");

    xelrow = pnm_allocrow(cols);
    alpharow = pgm_allocrow(cols);

    for ( row = 0; row < rows; ++row ) {
        /* Read one row of samples into samplebuf[] */

        if (planarconfig == PLANARCONFIG_CONTIG) {
            readscanline(tif, samplebuf, scanbuf, row, 0, cols, bps, spp, 
                         fillorder);


            convertRow(samplebuf, xelrow, alpharow, cols, maxval, 
                       photomet, spp, colormap);
        } else {
            /* The input is in separate planes, so we need to read one
               scanline for the reds, another for the greens, then another
               for the blues.
            */

            int col;

            if (photomet != PHOTOMETRIC_RGB)
                pm_error("This is a multiple-plane file, but is not an RGB "
                         "file.  This program does not "
                         "know how to handle that.");
            else {
                /* First, clear the buffer so we can add red, green,
                   and blue one at a time.  
                */
                for (col = 0; col < cols; ++col) 
                    PPM_ASSIGN(xelrow[col], 0, 0, 0);

                readscanline(tif, samplebuf, scanbuf, row, 0, cols, bps, spp, 
                             fillorder);
                
                /* Read the reds */
                for (col = 0; col < cols; ++col) 
                    PPM_PUTR(xelrow[col], samplebuf[col]);
                
                /* Next the greens */
                readscanline(tif, samplebuf, scanbuf, row, 1,
                             cols, bps, spp, fillorder);
                for (col = 0; col < cols; ++col) 
                    PPM_PUTG( xelrow[col], samplebuf[col] );
            
                /* And finally the blues */
                readscanline(tif, samplebuf, scanbuf, row, 2,
                             cols, bps, spp, fillorder);
                for (col = 0; col < cols; ++col) 
                    PPM_PUTB(xelrow[col], samplebuf[col]);

                /* Could there be an alpha plane?  (We assume no.  But if so,
                   here is where to read it) 
                */
                for (col = 0; col < cols; ++col) 
                    alpharow[col] = 0;
            }
        }

        if (imageoutFile != NULL) 
            pnm_writepnmrow( imageoutFile, 
                             xelrow, cols, (xelval) maxval, format, 0 );
        if (alphaFile != NULL) 
            pgm_writepgmrow( alphaFile, alpharow, cols, (gray) maxval, 0);
    }
    pgm_freerow(alpharow);
    pnm_freerow(xelrow);

    free(samplebuf);
    free(scanbuf);
}    



int
main(int argc, char * argv[]) {

    unsigned int cols, rows;
    int format;
    TIFF* tif;
    FILE *alphaFile, *imageoutFile;
    xelval maxval;
    unsigned short bps, spp;

    xel colormap[MAXCOLORS];
    unsigned short photomet, planarconfig, fillorderTag;
    unsigned short fillorder;

    pnm_init( &argc, argv );

    parseCommandLine(argc, argv, &cmdline);

    if ( cmdline.inputFilename != NULL ) {
        tif = TIFFOpen( cmdline.inputFilename, "r" );
        if ( tif == NULL )
            pm_error( "error opening TIFF file %s", cmdline.inputFilename );
    } else {
        tif = TIFFFdOpen( 0, "Standard Input", "r" );
        if ( tif == NULL )
            pm_error( "error opening standard input as TIFF file" );
    }

    if (cmdline.alphaStdout)
        alphaFile = stdout;
    else if (cmdline.alphaFilename == NULL) 
        alphaFile = NULL;
    else {
        alphaFile = pm_openw(cmdline.alphaFilename);
    }

    if (cmdline.alphaStdout) 
        imageoutFile = NULL;
    else
        imageoutFile = stdout;

    read_directory(tif, &bps, &spp, &photomet, &planarconfig, &fillorderTag,
                   &cols, &rows, 
                   cmdline.headerdump);


    computeFillorder(fillorderTag, &fillorder, cmdline.respectfillorder);

    analyze_image_type(tif, bps, spp, photomet, 
                       &maxval, &format, colormap, cmdline.headerdump);

    if (imageoutFile != NULL) 
        pnm_writepnminit( imageoutFile, 
                          cols, rows, (xelval) maxval, format, 0 );
    if (alphaFile != NULL) 
        pgm_writepgminit( alphaFile, cols, rows, (gray) maxval, 0 );

    convertRaster(imageoutFile, alphaFile, cols, rows, maxval, format, 
                  tif, photomet, planarconfig, bps, spp, fillorder,
                  colormap);

    if (imageoutFile != NULL) 
        pm_close( imageoutFile );
    if (alphaFile != NULL)
        pm_close( alphaFile );

    /* If the program failed, it previously aborted with nonzero completion
       code, via various function calls.
    */
    return 0;
}
