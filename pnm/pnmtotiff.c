/*
** pnmtotiff.c - converts a portable anymap to a Tagged Image File
**
** Derived by Jef Poskanzer from ras2tif.c, which is:
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

#include <string.h>
#include "pnm.h"
#include <fcntl.h>
#ifdef VMS
#ifdef SYSV
#undef SYSV
#endif
#include <tiffioP.h>
#endif
#include <tiffio.h>

#include <string.h>

#include "ppmcmap.h"
#define MAXCOLORS 256

/* 2001.09.03: Old tiff.h doesn't contain these values: */
#ifndef COMPRESSION_ADOBE_DEFLATE
#define COMPRESSION_ADOBE_DEFLATE 8
#endif

struct sizeset {
    bool b1, b2, b4, b8;
};

struct cmdline_info {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
    char *input_filespec;  /* Filespecs of input files */
    int compression;   /* COMPRESSION Tiff tag value */
    int fillorder;     /* FILLORDER Tiff tag value */
    int g3options;     /* G3OPTIONS Tiff tag value or 0 for none */
    int predictor;     /* PREDICTOR Tiff tag value or -1 for none */
    int rowsperstrip;  /* -1 = unspecified */
    unsigned int minisblack;   /* logical: User wants MINISBLACK photometric */
    unsigned int miniswhite;   /* logical: User wants MINISWHITE photometric */
    unsigned int truecolor;    /* logical: User wants truecolor, not cmap */
    unsigned int color;        /* logical: assume not grayscale */
    float xresolution; /* XRESOLUTION Tiff tag value or -1 for none */
    float yresolution; /* YRESOLUTION Tiff tag value or -1 for none */
    struct sizeset indexsizeAllowed;
        /* Which bit widths are allowable in a raster of palette indices */
};



static void
parse_command_line(int argc, char ** argv,
                   struct cmdline_info *cmdline_p) {
/*----------------------------------------------------------------------------
   Note that the file spec array we return is stored in the storage that
   was passed to us as the argv array.
-----------------------------------------------------------------------------*/
    optEntry *option_def = malloc(100*sizeof(optEntry));

    optStruct3 opt;

    unsigned int none, packbits, lzw, g3, g4, msb2lsb, lsb2msb, opt_2d, fill;
    unsigned int flate, adobeflate;
    char * indexbits;

    unsigned int predictorSpec, rowsperstripSpec, xresolutionSpec,
                 yresolutionSpec, indexbitsSpec;

    unsigned int option_def_index;

    option_def_index = 0;   /* incremented by OPTENT3 */
    OPTENT3(0, "none",         OPT_FLAG,   NULL, &none,                    0);
    OPTENT3(0, "packbits",     OPT_FLAG,   NULL, &packbits,                0);
    OPTENT3(0, "lzw",          OPT_FLAG,   NULL, &lzw,                     0);
    OPTENT3(0, "g3",           OPT_FLAG,   NULL, &g3,                      0);
    OPTENT3(0, "g4",           OPT_FLAG,   NULL, &g4,                      0);
    OPTENT3(0, "flate",        OPT_FLAG,   NULL, &flate,                   0);
    OPTENT3(0, "adobeflate",   OPT_FLAG,   NULL, &adobeflate,              0);
    OPTENT3(0, "msb2lsb",      OPT_FLAG,   NULL, &msb2lsb,                 0);
    OPTENT3(0, "lsb2msb",      OPT_FLAG,   NULL, &lsb2msb,                 0);
    OPTENT3(0, "2d",           OPT_FLAG,   NULL, &opt_2d,                  0);
    OPTENT3(0, "fill",         OPT_FLAG,   NULL, &fill,                    0);
    OPTENT3(0, "minisblack",   OPT_FLAG,   NULL, &cmdline_p->minisblack,   0);
    OPTENT3(0, "mb",           OPT_FLAG,   NULL, &cmdline_p->minisblack,   0);
    OPTENT3(0, "miniswhite",   OPT_FLAG,   NULL, &cmdline_p->miniswhite,   0);
    OPTENT3(0, "mw",           OPT_FLAG,   NULL, &cmdline_p->miniswhite,   0);
    OPTENT3(0, "truecolor",    OPT_FLAG,   NULL, &cmdline_p->truecolor,    0);
    OPTENT3(0, "color",        OPT_FLAG,   NULL, &cmdline_p->color,        0);
    OPTENT3(0, "predictor",    OPT_UINT,   &cmdline_p->predictor,    
            &predictorSpec,    0);
    OPTENT3(0, "rowsperstrip", OPT_UINT,   &cmdline_p->rowsperstrip, 
            &rowsperstripSpec, 0);
    OPTENT3(0, "xresolution",  OPT_FLOAT,  &cmdline_p->xresolution,  
            &xresolutionSpec,  0);
    OPTENT3(0, "yresolution",  OPT_FLOAT,  &cmdline_p->yresolution,  
            &yresolutionSpec,  0);
    OPTENT3(0, "indexbits",    OPT_STRING, &indexbits, 
            &indexbitsSpec,    0);

    opt.opt_table = option_def;
    opt.short_allowed = FALSE;  /* We have no short (old-fashioned) options */
    opt.allowNegNum = FALSE;  /* We have no parms that are negative numbers */

    pm_optParseOptions3(&argc, argv, opt, sizeof(opt), 0);
        /* Uses and sets argc, argv, and some of *cmdline_p and others. */

    if (none + packbits + lzw + g3 + g4 + flate + adobeflate > 1)
        pm_error("You specified more than one compression option.  "
                 "Only one of -none, -packbits, -lze, -g3, and -g4 "
                 "is allowed.");
    
    if (none)
        cmdline_p->compression = COMPRESSION_NONE;
    else if (packbits)
        cmdline_p->compression = COMPRESSION_PACKBITS;
    else if (lzw)
        cmdline_p->compression = COMPRESSION_LZW;
    else if (g3)
        cmdline_p->compression = COMPRESSION_CCITTFAX3;
    else if (g4)
        cmdline_p->compression = COMPRESSION_CCITTFAX4;
    else if (adobeflate)
        cmdline_p->compression = COMPRESSION_ADOBE_DEFLATE;
    else if (flate)
        cmdline_p->compression = COMPRESSION_DEFLATE;
    else
        cmdline_p->compression = COMPRESSION_NONE;
    
    if (msb2lsb + lsb2msb > 1)
        pm_error("You specified both -msb2lsb and -lsb2msb.  "
                 "These are conflicting options.");

    if (msb2lsb)
        cmdline_p->fillorder = FILLORDER_MSB2LSB;
    else if (lsb2msb)
        cmdline_p->fillorder = FILLORDER_LSB2MSB;
    else 
        cmdline_p->fillorder = FILLORDER_MSB2LSB;
    

    if (cmdline_p->miniswhite && cmdline_p->minisblack)
        pm_error("You cannot specify both -miniswhite and -minisblack");

    cmdline_p->g3options = 0;  /* initial value */
    if (opt_2d)
        cmdline_p->g3options |= GROUP3OPT_2DENCODING;
    if (fill)
        cmdline_p->g3options |= GROUP3OPT_FILLBITS;

    if (predictorSpec) {
        if (cmdline_p->predictor != 1 && cmdline_p->predictor != 2)
            pm_error("-predictor may be only 1 or 2.  You specified %d.", 
                     cmdline_p->predictor);
    } else
        cmdline_p->predictor = -1;

    if (rowsperstripSpec) {
        if (cmdline_p->rowsperstrip < 1)
            pm_error("-rowsperstrip must be positive.  You specified %d.",
                     cmdline_p->rowsperstrip);
    } else
        cmdline_p->rowsperstrip = -1;

    if (xresolutionSpec) {
        if (cmdline_p->xresolution < 1)
            pm_error("-xresolution must be positive.  You specified %f.",
                     cmdline_p->xresolution);
    } else
        cmdline_p->xresolution = -1;

    if (yresolutionSpec) {
        if (cmdline_p->yresolution < 1)
            pm_error("-yresolution must be positive.  You specified %f.",
                     cmdline_p->yresolution);
    } else
        cmdline_p->yresolution = -1;
    
    if (indexbitsSpec) {
        if (strstr("1", indexbits))
            cmdline_p->indexsizeAllowed.b1 = TRUE;
        else
            cmdline_p->indexsizeAllowed.b1 = FALSE;
        if (strstr("2", indexbits))
            cmdline_p->indexsizeAllowed.b2 = TRUE;
        else
            cmdline_p->indexsizeAllowed.b2 = FALSE;
        if (strstr("4", indexbits))
            cmdline_p->indexsizeAllowed.b4 = TRUE;
        else
            cmdline_p->indexsizeAllowed.b4 = FALSE;
        if (strstr("8", indexbits))
            cmdline_p->indexsizeAllowed.b8 = TRUE;
        else
            cmdline_p->indexsizeAllowed.b8 = FALSE;
    } else {
        cmdline_p->indexsizeAllowed.b1 = FALSE;
        cmdline_p->indexsizeAllowed.b2 = FALSE;
        cmdline_p->indexsizeAllowed.b4 = FALSE;
        cmdline_p->indexsizeAllowed.b8 = TRUE;
    }

    if (argc-1 == 0) 
        cmdline_p->input_filespec = "-";
    else if (argc-1 != 1)
        pm_error("Program takes zero or one argument (filename).  You "
                 "specified %d", argc-1);
    else
        cmdline_p->input_filespec = argv[1];
}



/* Note: PUTSAMPLE doesn't work if bitspersample is 1-4. */

#define PUTSAMPLE { \
    if (maxval != tiff_maxval) \
        s = s * tiff_maxval / maxval; \
    if (bitspersample > 8) { \
        *((unsigned short *)tP) = s; \
        tP += sizeof(short); \
    } else { \
        *tP++ = s & 0xff; \
    } \
}



static void
fillRowOfSubBytePixels(const xel * const xelrow, unsigned char* const buf,
                       xelval const maxval, unsigned short const tiff_maxval,
                       unsigned short const photometric,
                       int const fillorder, int const bitspersample,
                       int const cols, colorhash_table const cht) {
/*----------------------------------------------------------------------------
  This subroutine deals with cases where multiple pixels are packed into
  a single byte of the Tiff output image, i.e. bits per pixel < 8.
-----------------------------------------------------------------------------*/
    int col;
    int bitshift;
    /* The number of bits we have to shift a pixel value
       left to line it up with where the current pixel goes
       in the current byte of the output buffer.
    */
    int const firstbitshift = 
        (fillorder == FILLORDER_MSB2LSB) ? 8 - bitspersample : 0;
    /* The value of 'bitshift' for the first pixel into a
       byte of the output buffer.  (MSB2LSB is normal).
                    */
    xelval s;
    /* The value of the current pixel */
    unsigned char* tP;
    /* The address of the byte in the output buffer in which
                       the current pixel goes.
                    */
    unsigned char byte;
    /* The under-construction value of the byte pointed to by
                       tP, above.
                    */
                
    bitshift = firstbitshift;
    byte = 0;
    for (col = 0, tP = buf; col < cols; ++col) {
        if (cht == NULL) {
	    /* It's grayscale */
	    s = PNM_GET1( xelrow[col] );
	    if ( maxval != tiff_maxval )
	        s = (long) s * tiff_maxval / maxval;
 
	    if (photometric == PHOTOMETRIC_MINISWHITE)
	        s = tiff_maxval - s;
	} else {
	    /* It's a colormapped raster */
	    int si;
	    si = ppm_lookupcolor(cht, &xelrow[col]);
	    if (si == -1)
	        pm_error("color not found?!?  "
			 "col=%d  r=%d g=%d b=%d",
			 col, PPM_GETR( xelrow[col] ), 
			 PPM_GETG(xelrow[col]),
			 PPM_GETB(xelrow[col]));
	    s = (unsigned char) si;
	}
        byte |= s << bitshift;
        if (fillorder == FILLORDER_MSB2LSB)
            bitshift -= bitspersample;  /* normal case */
        else
            bitshift += bitspersample;
        if (bitshift < 0 || bitshift > 7) {
            *tP++ = byte;
            bitshift = firstbitshift;
            byte = 0;
        }
    }
    if (bitshift != firstbitshift)
        *tP++ = byte;
}



static void
writeScanLines(FILE *          const ifp,
               TIFF *          const tif, 
               colorhash_table const cht,
               int             const format, 
               int             const cols, 
               int             const rows, 
               xelval          const maxval, 
               unsigned short  const tiff_maxval,
               unsigned short  const bitspersample, 
               unsigned short  const photometric,
               int             const bytesperrow, 
               bool            const grayscale,
               int             const fillorder) {
/*----------------------------------------------------------------------------
   Write out the raster for the image in file 'ifp', which is positioned
   to the header of the image.
-----------------------------------------------------------------------------*/
    xel * xelrow;  /* malloc'ed */
    unsigned char* buf;
    int row;

    /* The man page for TIFFWriteScanLine doesn't tell the format of
       it's 'buf' parameter, but here it is: Its format depends on the
       bits per pixel of the TIFF image.  If it's 16, 'buf' is an
       array of short (16 bit) integers, one per raster column.  If
       it's 8, 'buf' is an array of characters (8 bit integers), one
       per image column.  If it's less than 8, it's an array of characters,
       each of which represents 1-8 raster columns, packed
       into it in the order specified by the TIFF image's fill order,
       with don't-care bits on the right such that each byte contains only
       whole pixels.

       In all cases, the array elements are in order left to right going
       from low array indices to high array indices.
    */

    buf = (unsigned char*) malloc( bytesperrow );
    if ( buf == (unsigned char*) 0 )
    pm_error( "can't allocate memory for row buffer" );

    xelrow = pnm_allocrow(cols);
    
    for (row = 0; row < rows; ++row) {
        int col;

        pnm_readpnmrow(ifp, xelrow, cols, maxval, format);

        if ( PNM_FORMAT_TYPE(format) == PPM_TYPE && ! grayscale ) {
            if (cht == NULL) {
                /* It's an RGB raster */
                unsigned char* tP;
                for (col = 0, tP = buf; col < cols; ++col) {
                    register xelval s;

                    s = PPM_GETR( xelrow[col] );
                    PUTSAMPLE;  /* advances tP */
                    s = PPM_GETG( xelrow[col] );
                    PUTSAMPLE;  /* advances tP */
                    s = PPM_GETB( xelrow[col] );
                    PUTSAMPLE;  /* advances tP */
                }
            } else {
                /* It's a colormapped raster */
	        if (bitspersample == 8) {
		    for (col = 0; col < cols; ++col) {
			int s;

			s = ppm_lookupcolor(cht, &xelrow[col]);
			if (s == -1)
			    pm_error("color not found?!?  "
				     "row=%d col=%d  r=%d g=%d b=%d",
				     row, col, PPM_GETR( xelrow[col] ), 
				     PPM_GETG(xelrow[col]),
				     PPM_GETB(xelrow[col]));
			buf[col] = (unsigned char) s;
		    }
		} else {
		    fillRowOfSubBytePixels(xelrow, buf, 0, 0, 0, fillorder,
					   bitspersample, cols, cht);
                }
            }
        } else {
            /* It's grayscale */
            if (bitspersample == 8 || bitspersample == 16) {
                unsigned char* tP;
                tP = buf;
                for (col = 0; col < cols; ++col) {
                    xelval s = PNM_GET1(xelrow[col]);
                    PUTSAMPLE; /* advances tP */
                }
            } else 
                fillRowOfSubBytePixels(xelrow, buf, maxval, tiff_maxval,
                                       photometric,
                                       fillorder, bitspersample, cols, NULL);
        }
        if (TIFFWriteScanline( tif, buf, row, 0 ) < 0)
            pm_error( "failed a scanline write on row %d", row );
    }
    pnm_freerow(xelrow);
    free(buf);
}



static void
analyzeColors(FILE *              const ifp,
              struct cmdline_info const cmdline,
              int                 const cols,
              int                 const rows,
              xelval              const maxval,
              int                 const format,
              int                 const maxcolors, 
              colorhist_vector *  const chvP, 
              int *               const colorsP, 
              bool *              const grayscaleP) {
/*----------------------------------------------------------------------------
   Analyze the colors in the image that is in the file 'ifp', which is
   positioned to the raster. 

   If the colors, combined with command line options 'cmdline', indicate
   a colormapped TIFF should be generated, return as *chvP the address
   of a color map (in newly malloc'ed space).  If a colormapped TIFF is
   not indicated, return *chvP == NULL.

   Leave the file position undefined.
-----------------------------------------------------------------------------*/
    switch (PNM_FORMAT_TYPE(format)) {
    case PPM_TYPE:
        if (cmdline.color && cmdline.truecolor) {
            *chvP = NULL;
            *grayscaleP = FALSE;
        } else {
            colorhist_vector chv;
            bool grayscale;

            pm_message( "computing colormap..." );
            chv = ppm_computecolorhist2(ifp, cols, rows, maxval, format, 
                                        maxcolors, colorsP);
            if (chv == NULL) {
                grayscale = FALSE;
            } else {
                int i;
                pm_message("%d color%s found", 
                           *colorsP, *colorsP == 1 ? "" : "s");
                grayscale = TRUE;  /* initial assumption */
                for (i = 0; i < *colorsP && grayscale; ++i) {
                    xelval r, g, b;

                    r = PPM_GETR(chv[i].color);
                    g = PPM_GETG(chv[i].color);
                    b = PPM_GETB(chv[i].color);
                    if (r != g || g != b) 
                        grayscale = FALSE;
                }
            }
            *grayscaleP = grayscale;

            if (grayscale || cmdline.truecolor) {
                *chvP = NULL;
                if (chv)
                    ppm_freecolorhist(chv);
            } else {
                /* He wants a colormap.  But can we give it to him? */
                if (chv)
                    *chvP = chv;
                else {
                    pm_message("Too many colors - "
                               "proceeding to write a 24-bit RGB file.");
                    pm_message("If you want an 8-bit palette file, "
                               "try doing a 'ppmquant %d'.",
                               maxcolors);
                    *chvP = NULL;
                }
            }
        }
        break;

    default:
        *chvP = NULL;
        *grayscaleP = TRUE;
        break;
    }
}



static void
computeRasterParm(int              const cols,
                  int              const format, 
                  xelval           const maxval, 
                  colorhist_vector const chv, 
                  int              const colors, 
                  bool             const grayscale,
                  int              const compression,
                  bool             const minisblack,
                  bool             const miniswhite,
                  struct sizeset   const indexsizeAllowed,
                  int              const requested_rowsperstrip,
                  unsigned short * const samplesperpixelP,
                  unsigned short * const bitspersampleP,
                  unsigned short * const photometricP,
                  int            * const bytesperrowP,
                  unsigned short * const rowsperstripP) {
/*----------------------------------------------------------------------------
   Compute the parameters of the raster portion of the TIFF image.

   'minisblack' and 'miniswhite' mean the user requests the corresponding
   photometric.  Both FALSE means user has no explicit requirement.
-----------------------------------------------------------------------------*/
    unsigned short defaultPhotometric;
        /* The photometric we use if the user specified no preference */

    /* We determine the bits per sample by the maxval, except that if
       it would be more than 8, we just use 16.  I don't know if bits
       per sample between 8 and 16 are legal, but they aren't very
       nice in any case.  If users want them, we should provide an
       option.  It is not clear why we don't use bits per pixel < 8
       for RGB images.  Note that code to handle maxvals <= 255 was
       written long before maxval > 255 was possible and there are
       backward compatibility requirements.  
    */

    if (PNM_FORMAT_TYPE(format) == PBM_TYPE) {
        *samplesperpixelP = 1;
        *bitspersampleP = 1;
        if ((compression == COMPRESSION_CCITTFAX3) ||
            (compression == COMPRESSION_CCITTFAX4))
            defaultPhotometric = PHOTOMETRIC_MINISWHITE;
        else
            defaultPhotometric = PHOTOMETRIC_MINISBLACK;
    } else {
        if (chv) {
            *samplesperpixelP = 1;
            *bitspersampleP = 
                colors <=   2 && indexsizeAllowed.b1 ? 1 :
                colors <=   4 && indexsizeAllowed.b2 ? 2 :
                colors <=  16 && indexsizeAllowed.b4 ? 4 :
                colors <= 256 && indexsizeAllowed.b8 ? 8 :
                0;
            if (*bitspersampleP == 0)
                pm_error("Your -indexbits option is insufficient for the "
                         "%d colors in this image.", colors);

            defaultPhotometric = PHOTOMETRIC_PALETTE;
        } else {
            if (grayscale) {
                *samplesperpixelP = 1;
                *bitspersampleP = maxval > 255 ? 16 : pm_maxvaltobits(maxval);
                defaultPhotometric = PHOTOMETRIC_MINISBLACK;
            } else {
                *samplesperpixelP = 3;
                *bitspersampleP = maxval > 255 ? 16 : 8;
                defaultPhotometric = PHOTOMETRIC_RGB;
            }
        }
    }

    if (miniswhite)
        *photometricP = PHOTOMETRIC_MINISWHITE;
    else if (minisblack)
        *photometricP = PHOTOMETRIC_MINISBLACK;
    else
        *photometricP = defaultPhotometric;

    if (*bitspersampleP < 8) {
        int samplesperbyte;
        samplesperbyte = 8 / *bitspersampleP;
		overflow2(cols, *samplesperpixelP);
		overflow_add(cols * *samplesperpixelP, samplesperbyte);
        *bytesperrowP = 
            (cols * *samplesperpixelP + samplesperbyte-1) / samplesperbyte;
    } else {
		overflow3( *samplesperpixelP,  cols, *bitspersampleP);
        *bytesperrowP = (cols * *samplesperpixelP * *bitspersampleP) / 8;
	}

    if (requested_rowsperstrip == -1 )
        *rowsperstripP = (8 * 1024) / *bytesperrowP;
    else 
        *rowsperstripP = requested_rowsperstrip;

}



int
main(int argc, char *argv[]) {
    struct cmdline_info cmdline;
    char* inf;
    FILE* ifp;
    colorhist_vector chv;
    colorhash_table cht;
    unsigned short red[MAXCOLORS], grn[MAXCOLORS], blu[MAXCOLORS];
    int cols, rows, format, colors, i;
    xelval maxval;  /* maxval of PNM input */
    int grayscale;
    TIFF* tif;
    unsigned short photometric;
    unsigned short rowsperstrip;
    unsigned short samplesperpixel;
    unsigned short bitspersample;
    unsigned short tiff_maxval;
       /* This is the maxval of the samples in the tiff file.  It is 
          determined solely by the bits per sample ('bitspersample').
          */
    int bytesperrow;
    unsigned int rasterPos;

    pnm_init( &argc, argv );

    parse_command_line(argc, argv, &cmdline);
    
    ifp = pm_openr_seekable(cmdline.input_filespec);

    if (strcmp(cmdline.input_filespec, "-") == 0)
        inf = "Standard Input";
    else 
        inf = cmdline.input_filespec;

    pnm_readpnminit(ifp, &cols, &rows, &maxval, &format);

    rasterPos = pm_tell(ifp);

    analyzeColors(ifp, cmdline, cols, rows, maxval, format, MAXCOLORS, 
                  &chv, &colors, &grayscale);

    pm_seek(ifp, rasterPos);  /* Go back to beginning of raster */

    /* Open output file. */
    fcntl( 1, F_SETFL, O_NONBLOCK ) ; 
      /* acooke dec99 - otherwise blocks on read inside 
         next line (Linux i386) 
         */
    tif = TIFFFdOpen( 1, "Standard Output", "w" );
    if ( tif == NULL )
        pm_error( "error opening standard output as TIFF file" );

    /* Figure out TIFF parameters. */

    computeRasterParm(cols, format, maxval, chv, colors, grayscale, 
                      cmdline.compression,
                      cmdline.minisblack, cmdline.miniswhite,
                      cmdline.indexsizeAllowed,
                      cmdline.rowsperstrip,
                      &samplesperpixel, &bitspersample, &photometric,
                      &bytesperrow, &rowsperstrip);

    tiff_maxval = pm_bitstomaxval( bitspersample );

    /* Set TIFF parameters. */
    TIFFSetField( tif, TIFFTAG_IMAGEWIDTH, cols );
    TIFFSetField( tif, TIFFTAG_IMAGELENGTH, rows );
    TIFFSetField( tif, TIFFTAG_BITSPERSAMPLE, bitspersample );
    TIFFSetField( tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT );
    TIFFSetField( tif, TIFFTAG_COMPRESSION, cmdline.compression );
    if (cmdline.compression == COMPRESSION_CCITTFAX3 && cmdline.g3options != 0)
        TIFFSetField( tif, TIFFTAG_GROUP3OPTIONS, cmdline.g3options );
    if ( cmdline.compression == COMPRESSION_LZW && cmdline.predictor != -1 )
        TIFFSetField( tif, TIFFTAG_PREDICTOR, cmdline.predictor );
    TIFFSetField( tif, TIFFTAG_PHOTOMETRIC, photometric );
    TIFFSetField( tif, TIFFTAG_FILLORDER, cmdline.fillorder );
    TIFFSetField( tif, TIFFTAG_DOCUMENTNAME, inf );
    TIFFSetField( tif, TIFFTAG_IMAGEDESCRIPTION, "converted PNM file" );
    TIFFSetField( tif, TIFFTAG_SAMPLESPERPIXEL, samplesperpixel );
    TIFFSetField( tif, TIFFTAG_ROWSPERSTRIP, rowsperstrip );
    if ( cmdline.xresolution != -1 )
        TIFFSetField( tif, TIFFTAG_XRESOLUTION, cmdline.xresolution );
    if ( cmdline.yresolution != -1 )
        TIFFSetField( tif, TIFFTAG_YRESOLUTION, cmdline.yresolution );
    TIFFSetField( tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);
    /* TIFFSetField( tif, TIFFTAG_STRIPBYTECOUNTS, rows / rowsperstrip ); */
    TIFFSetField( tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG );

    if (!chv)
        cht = NULL;
    else {
        /* Make TIFF colormap. */
        for (i = 0; i < colors; ++i) {
            red[i] = (long) PPM_GETR( chv[i].color ) * 65535L / maxval;
            grn[i] = (long) PPM_GETG( chv[i].color ) * 65535L / maxval;
            blu[i] = (long) PPM_GETB( chv[i].color ) * 65535L / maxval;
        }
        /* Clear unused colormap entries. */
        for (i = colors; i < (1 << bitspersample); ++i) {
            red[i] = grn[i] = blu[i] = 0;
        }
        TIFFSetField( tif, TIFFTAG_COLORMAP, red, grn, blu );

        /* Convert color vector to color hash table, for fast lookup. */
        cht = ppm_colorhisttocolorhash( chv, colors );
        ppm_freecolorhist( chv );
    }

    writeScanLines(ifp, tif, cht, format, cols, rows, maxval, 
                   tiff_maxval, bitspersample, photometric, bytesperrow, 
                   grayscale, cmdline.fillorder);

    TIFFFlushData(tif);
    TIFFClose(tif);

    pm_close(ifp);

    exit(0);
}

