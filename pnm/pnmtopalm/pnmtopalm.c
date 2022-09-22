/* pnmtopalm.c - read a portable pixmap and write a Palm Tbmp file
 *
 * Program originally ppmtoTbmp.c by Ian Goldberg <iang@cs.berkeley.edu>
 *
 * Mods for multiple bits per pixel by George Caswell 
 * <tetsujin@sourceforge.net>
 *  and Bill Janssen <bill@janssen.org>
 *
 * Based on ppmtopuzz.c by Jef Poskanzer, from the netpbm-1mar1994 package.
 */

#include <string.h>
#include <stdio.h>
#include "pnm.h"
#include "palm.h"

struct cmdline_info {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
    char *inputFilespec;  /* Filespecs of input files */
    char *transparent;    /* -transparent value.  Null if unspec */
    unsigned int depth;     /* -depth value.  0 if unspec */
    unsigned int maxdepth;     /* -maxdepth value.  0 if unspec */
    unsigned int rle_compression;
    unsigned int scanline_compression;
    unsigned int verbose;
    unsigned int colormap;
    unsigned int offset;
};



static void
parseCommandLine(int argc, char ** argv,
                 struct cmdline_info *cmdlineP) {
/*----------------------------------------------------------------------------
   Note that the file spec array we return is stored in the storage that
   was passed to us as the argv array.
-----------------------------------------------------------------------------*/
    optStruct3 opt;  /* set by OPTENT3 */
    optEntry *option_def = malloc(100*sizeof(optStruct));
    unsigned int option_def_index;

    unsigned int transSpec, depthSpec, maxdepthSpec;

    option_def_index = 0;   /* incremented by OPTENTRY */
    OPTENT3(0, "transparent",      OPT_STRING, 
            &cmdlineP->transparent, &transSpec, 0);
    OPTENT3(0, "depth",            OPT_UINT, 
            &cmdlineP->depth,       &depthSpec, 0);
    OPTENT3(0, "maxdepth",         OPT_UINT, 
            &cmdlineP->maxdepth,    &maxdepthSpec, 0);
    OPTENT3(0, "rle_compression",  OPT_FLAG, 
            NULL,                   &cmdlineP->rle_compression, 0);
    OPTENT3(0, "scanline_compression", OPT_FLAG, 
            NULL,                   &cmdlineP->scanline_compression, 0);
    OPTENT3(0, "verbose",          OPT_FLAG, 
            NULL,                   &cmdlineP->verbose, 0);
    OPTENT3(0, "colormap",         OPT_FLAG, 
            NULL,                   &cmdlineP->colormap, 0);
    OPTENT3(0, "offset",           OPT_FLAG, 
            NULL,                   &cmdlineP->offset, 0);

    opt.opt_table = option_def;
    opt.short_allowed = FALSE; /* We have some short (old-fashioned) options */
    opt.allowNegNum = FALSE;  /* We have no parms that are negative numbers */

    pm_optParseOptions3(&argc, argv, opt, sizeof(opt), 0);
        /* Uses and sets argc, argv, and some of *cmdline_p and others. */

    if (depthSpec) {
        if (cmdlineP->depth != 1 && cmdlineP->depth != 2 
            && cmdlineP->depth != 4 && cmdlineP->depth != 8
            && cmdlineP->depth != 16)
            pm_error("invalid value for -depth: %u.  Valid values are "
                     "1, 2, 4, 8, and 16", cmdlineP->depth);
    } else
        cmdlineP->depth = 0;
    if (maxdepthSpec) {
        if (cmdlineP->maxdepth != 1 && cmdlineP->maxdepth != 2 
            && cmdlineP->maxdepth != 4 && cmdlineP->maxdepth != 8
            && cmdlineP->maxdepth != 16)
            pm_error("invalid value for -maxdepth: %u.  Valid values are "
                     "1, 2, 4, 8, and 16", cmdlineP->maxdepth);
    } else
        cmdlineP->maxdepth = 0;

    if (depthSpec && maxdepthSpec && 
        cmdlineP->depth > cmdlineP->maxdepth)
        pm_error("-depth value (%u) is greater than -maxdepth (%u) value.",
                 cmdlineP->depth, cmdlineP->maxdepth);

    if (!transSpec)
        cmdlineP->transparent = NULL;

    if (argc-1 > 1)
        pm_error("This program takes at most 1 argument: the file name.  "
                 "You specified %d", argc-1);
    else if (argc-1 > 0) 
        cmdlineP->inputFilespec = argv[1];
    else
        cmdlineP->inputFilespec = "-";
}



static void
determinePalmFormat(int const cols, int const rows, 
                    pixval const maxval, int const format, 
                    pixel ** const pixels,
                    unsigned int const specified_bpp,
                    unsigned int const max_bpp, bool const custom_colormap,
                    bool const verbose,
                    int * const bppP, int * const maxmaxvalP, 
                    bool * const directColorP, Colormap * const colormapP) {

    if (PNM_FORMAT_TYPE(format) == PBM_TYPE) {
        if (custom_colormap)
            pm_error("You specified -colormap with a black and white input "
                     "image.  -colormap is valid only with color.");
        if (specified_bpp)
            *bppP = specified_bpp;
        else
            *bppP = 1;    /* no point in wasting bits */
        *maxmaxvalP = 1;
        *directColorP = FALSE;
        *colormapP = NULL;
        if (verbose)
            pm_message("output is black and white");
    } else if (PNM_FORMAT_TYPE(format) == PGM_TYPE) {
        /* we can usually handle this one, but may not have enough
           pixels.  So check... */
        if (custom_colormap)
            pm_error("You specified -colormap with a black and white input"
                     "image.  -colormap is valid only with color.");
        if (specified_bpp)
            *bppP = specified_bpp;
        else if (max_bpp && (maxval >= (1 << max_bpp)))
            *bppP = max_bpp;
        else if (maxval > 16)
            *bppP = 4;
        else {
            /* scale to minimum number of bpp needed */
            for (*bppP = 1;  (1 << *bppP) < maxval;  *bppP *= 2)
                ;
        }
        if (*bppP > 4)
            *bppP = 4;
        if (verbose)
            pm_message("output is grayscale %d bits-per-pixel", *bppP);
        *maxmaxvalP = PGM_OVERALLMAXVAL;
        *directColorP = FALSE;
        *colormapP = NULL;
    } else if (PNM_FORMAT_TYPE(format) == PPM_TYPE) {

        /* We assume that we only get a PPM if the image cannot be
           represented as PBM or PGM.  There are two options here: either
           8-bit with a colormap, either the standard one or a custom one,
           or 16-bit direct color.  In the 8-bit case, if "custom_colormap"
           is specified (not recommended by Palm) we will put in our own
           colormap; otherwise we will assume that the colors have been
           mapped to the default Palm colormap by appropriate use of
           ppmquant.  We try for 8-bit color first, since it works on
           more PalmOS devices. */

        if ((specified_bpp == 16) || 
            (specified_bpp == 0 && max_bpp == 16)) {
            /* we do the 16-bit direct color */
            *directColorP = TRUE;
            *colormapP = NULL;
            *bppP = 16;
        } else if (!custom_colormap) {
            /* standard indexed 8-bit color */
            *colormapP = palmcolor_build_default_8bit_colormap();
            *bppP = 8;
            if (((specified_bpp != 0) && (specified_bpp != 8)) ||
                ((max_bpp != 0) && (max_bpp < 8)))
                pm_error("Must use depth of 8 for color pixmap without "
                         "custom color table.");
            *directColorP = FALSE;
            if (verbose)
                pm_message("Output is color with default colormap at 8 bpp");
        } else {
            /* indexed 8-bit color with a custom colormap */
            *colormapP = 
                palmcolor_build_custom_8bit_colormap(rows, cols, pixels);
            for (*bppP = 1; (1 << *bppP) < (*colormapP)->ncolors; *bppP *= 2);
            if (specified_bpp != 0) {
                if (specified_bpp >= *bppP)
                    *bppP = specified_bpp;
                else
                    pm_error("Too many colors for specified depth.  "
                             "Use ppmquant to reduce.");
            } else if ((max_bpp != 0) && (max_bpp < *bppP)) {
                pm_error("Too many colors for specified max depth.  "
                         "Use ppmquant to reduce.");
            }
            *directColorP = FALSE;
            if (verbose)
                pm_message("Output is color with custom colormap "
                           "with %d colors at %d bpp", 
                           (*colormapP)->ncolors, *bppP);
        }
        *maxmaxvalP = PPM_OVERALLMAXVAL;
    } else {
        pm_error("unknown format 0x%x on input file\n", (unsigned) format);
    }
}


static const char * 
formatName(int const format) {
    
    const char * retval;

    switch(PNM_FORMAT_TYPE(format)) {
    case PBM_TYPE: retval = "black and white"; break;
    case PGM_TYPE: retval = "grayscale";       break;
    case PPM_TYPE: retval = "color";           break;
    default:       retval = "???";             break;
    }
    return retval;
}

        

static void
findTransparentColor(char *         const colorSpec, 
                     pixval         const newMaxVal,
                     bool           const directColor, 
                     pixval         const maxval, 
                     Colormap       const colormap,
                     pixel *        const transcolorP, 
                     unsigned int * const transindexP) {

    *transcolorP = ppm_parsecolor(colorSpec, maxval);
    if (!directColor) {
        Color_s const temp_color = 
            ((((PPM_GETR(*transcolorP)*newMaxVal) / maxval) << 16) 
             | (((PPM_GETG(*transcolorP)*newMaxVal) / maxval) << 8)
             | ((PPM_GETB(*transcolorP)*newMaxVal) / maxval));
        Color const found = 
            (bsearch(&temp_color,
                     colormap->color_entries, colormap->ncolors,
                     sizeof(Color_s), palmcolor_compare_colors));
        if (!found) {
            pm_error("Specified transparent color %s not found "
                     "in colormap.", colorSpec);
        } else
            *transindexP = (*found >> 24) & 0xFF;
    }
}



int 
main( int argc, char **argv ) {
    struct cmdline_info cmdline; 
    FILE* ifp;
    pixel** pixels;
    pixel transcolor;
    unsigned int transindex;
    register unsigned char *outptr;
    unsigned char *rowdata = 0, *lastrow = 0;
    int rows, cols, row, rowbytes;
    xelval maxval;
    unsigned char outbyte, outbit;
    unsigned char outbytes[8];
    int format = 0;
    int bpp;
    int version, maxmaxval, directColor = 0;
    unsigned int newMaxVal, color, last_color, repeatcount, limit;
    Colormap colormap = 0;
    unsigned short flags;

    /* Parse default params */
    ppm_init( &argc, argv );

    parseCommandLine(argc, argv, &cmdline);

    ifp = pm_openr(cmdline.inputFilespec);

    pixels = pnm_readpnm (ifp, &cols, &rows, &maxval, &format);
    pm_close(ifp);

    if (cmdline.verbose)
        pm_message("Input is %dx%d %s, maxval %d", 
                   cols, rows, formatName(format), maxval);
    
    determinePalmFormat(cols, rows, maxval, format, pixels, 
                        cmdline.depth, cmdline.maxdepth, cmdline.colormap, 
                        cmdline.verbose, 
                        &bpp, &maxmaxval, &directColor, &colormap);

    newMaxVal = (1 << bpp) - 1;

    if (cmdline.transparent) 
        findTransparentColor(cmdline.transparent, newMaxVal, directColor,
                             maxval, colormap, &transcolor, &transindex);
    else 
        transindex = 0;

    flags = 0;
    if (cmdline.transparent)
        flags |= PALM_HAS_TRANSPARENCY_FLAG;
    if (cmdline.colormap)
        flags |= PALM_HAS_COLORMAP_FLAG;
    if (cmdline.rle_compression || cmdline.scanline_compression)
        flags |= PALM_IS_COMPRESSED_FLAG;
    if (directColor)
        flags |= PALM_DIRECT_COLOR;

    /* Write Tbmp header. */
    (void) pm_writebigshort( stdout, cols );    /* width */
    (void) pm_writebigshort( stdout, rows );    /* height */
	/* 
     * We want to say:
	 * rowbytes = ((cols + (16 / bpp -1)) / (16 / bpp)) * 2;
	 * 
	 * With fractions this is equally to 
	 * rowbytes = ((cols - 1) * bpp/16 + 1)*2
	 *          =  (cols - 1) * bpp/8  + 2.
	 * When using integers, bpp/8 is at maximum less than 1 off the real value.
	 * This gives that (speaking integerly now) (cols - 1) * (bpp/8+1) + 2
	 * is not smaller, to which cols * (bpp/8+1) + 2 is an upper bound.
	 *
	 * So we do the overflow checking on this, and then execute the normal code
	 */
	 overflow2(cols,(bpp/8+1));
	 overflow_add(cols*(bpp/8+1),2);
	
	 rowbytes = ((cols + (16 / bpp -1)) / (16 / bpp)) * 2;
        /* bytes per row - always a word boundary */
    pm_writebigshort(stdout, rowbytes);
    (void) pm_writebigshort( stdout, flags );
    fputc(bpp, stdout);

    /* we need to mark this as version 1 if we use more than 1 bpp,
       version 2 if we use compression or transparency */

    version = ((cmdline.transparent)
               || cmdline.rle_compression
               || cmdline.scanline_compression) ? 
        2 : (((bpp > 1)||(cmdline.colormap)) ? 1 : 0);
    fputc(version, stdout);

    if (cmdline.offset) {
        /* Offset is measured in double-words. Those are 4-byte, right? */
        /* Account for bitmap data size, plus header size, and round up */
        pm_writebigshort(stdout, (rowbytes * rows + 16 + 3)/4);
    } else {
        pm_writebigshort(stdout, 0);
    }

    fputc(transindex, stdout);  /* transparent index */
    if (cmdline.rle_compression)
        fputc(PALM_COMPRESSION_RLE, stdout);
    else if (cmdline.scanline_compression)
        fputc(PALM_COMPRESSION_SCANLINE, stdout);
    else
        fputc(PALM_COMPRESSION_NONE, stdout);

    (void) pm_writebigshort( stdout, 0 );   /* reserved by Palm */

    /* if there's a colormap, write it out */
    if (cmdline.colormap) {
        if (!colormap)
            pm_error("Internal error: user specified -colormap, but we did "
                     "not generate a colormap.");
        qsort (colormap->color_entries, colormap->ncolors,
               sizeof(Color_s), palmcolor_compare_indices);
        pm_writebigshort( stdout, colormap->ncolors );
        for (row = 0;  row < colormap->ncolors;  row++)
            pm_writebiglong (stdout, colormap->color_entries[row]);
        qsort (colormap->color_entries, colormap->ncolors,
               sizeof(Color_s), palmcolor_compare_colors);
    }

    if (directColor) {
        if (bpp == 16) {
            fputc(5, stdout);   /* # of bits of red */
            fputc(6, stdout);   /* # of bits of green */    
            fputc(5, stdout);   /* # of bits of blue */
            fputc(0, stdout);   /* reserved by Palm */
        } else {
            pm_error("Don't know how to create %d bit DirectColor bitmaps.", 
                     bpp);
        }
        if (cmdline.transparent) {
            fputc(0, stdout);
            fputc((PPM_GETR(transcolor) * 255) / maxval, stdout);
            fputc((PPM_GETG(transcolor) * 255) / maxval, stdout);
            fputc((PPM_GETB(transcolor) * 255) / maxval, stdout);
        } else {
            pm_writebiglong(stdout, 0);     /* no transparent color */
        }
    }

    last_color = 0xFFFFFFFF;
    rowdata = malloc(rowbytes);
    if (cmdline.scanline_compression)
        lastrow = malloc(rowbytes);
    /* And write out the data. */
    for ( row = 0; row < rows; ++row ) {
        memset(rowdata, 0, rowbytes);
        outbyte = 0x00;

        if (directColor) {
            int col;
            outptr = rowdata;
            for (col = 0; col < cols; ++col) {
                if (bpp == 16) {
                    color = ((((PPM_GETR(pixels[row][col])*31)/maxval) << 11) |
                             (((PPM_GETG(pixels[row][col])*63)/maxval) << 5) |
                             ((PPM_GETB(pixels[row][col])*31)/maxval));
                    *outptr++ = (color >> 8) & 0xFF;
                    *outptr++ = color & 0xFF;
                }
            }
        } else {
            int col;
            /* outbit is the lowest bit number we want to access for
               this pixel 
            */
            outptr = rowdata;
            for ( outbit = 8 - bpp, col = 0; col < cols; ++col ) {

                if (colormap == 0) {
                    /* we assume grayscale, and use simple scaling */
                    color = (PNM_GET1(pixels[row][col]) * newMaxVal)/maxval;
                    if (color > newMaxVal)
                        pm_error("oops.  Bug in color re-calculation code.  "
                                 "color of %u.", color);
                    color = (newMaxVal - color); 
                        /* note grayscale maps are inverted */
                } else {
                    Color_s const temp_color =
                        ((((PPM_GETR(pixels[row][col])*newMaxVal)/maxval)<<16) 
                         | (((PPM_GETG(pixels[row][col])*newMaxVal)/maxval)<<8)
                         | (((PPM_GETB(pixels[row][col])*newMaxVal)/maxval)));
                    Color const found = (bsearch (&temp_color,
                                                  colormap->color_entries, 
                                                  colormap->ncolors,
                                                  sizeof(Color_s), 
                                                  palmcolor_compare_colors));
                    if (!found) {
                        pm_error("Color %d:%d:%d not found in colormap.  "
                                 "Try using ppmquant to reduce the "
                                 "number of colors.",
                                 PPM_GETR(pixels[row][col]), 
                                 PPM_GETG(pixels[row][col]), 
                                 PPM_GETB(pixels[row][col]));
                    }
                    color = (*found >> 24) & 0xFF;
                }

                if (color > newMaxVal)
                    pm_error("oops.  Bug in color re-calculation code.  "
                             "color of %u.", color);
                outbyte |= (color << outbit);
                if (outbit == 0) {
                    *outptr++ = outbyte;
                    outbyte = 0x00;
                    outbit = 8 - bpp;
                } else {
                    outbit -= bpp;
                }
            }
            if ((cols % (8 / bpp)) != 0) {
                /* Handle case of partial byte */
                *outptr++ = outbyte;
            }
        }
        /* now output the row of pixels, compressed appropriately */

        if (cmdline.scanline_compression) {
            int col;
            for (col = 0;  col < rowbytes;  col += 8) {
                limit = ((rowbytes - col) < 8) ? (rowbytes - col) : 8;
                for (outbit = 0, outbyte = 0, outptr = outbytes;  
                     outbit < limit;  
                     outbit++) {
                    if ((row == 0) 
                        || (lastrow[col + outbit] != rowdata[col + outbit])) {
                        outbyte |= (1 << (7 - outbit));
                        *outptr++ = rowdata[col + outbit];
                    }
                }
                putc(outbyte, stdout);
                fwrite(outbytes, (outptr - outbytes), 1, stdout);
            }
            memcpy (lastrow, rowdata, rowbytes);
      
        } else if (cmdline.rle_compression) {
            int col;
            /* we output a count of the number of bytes a value is
               repeated, followed by that byte value 
            */
            col = 0;
            while (col < rowbytes) {
                outptr = rowdata + col;
                for (repeatcount = 1;  
                     repeatcount < (rowbytes - col) && repeatcount < 256;  
                     ++repeatcount)
                    if (*outptr != *(outptr + repeatcount))
                        break;
                putc(repeatcount, stdout);
                putc(*outptr, stdout);
                col += repeatcount;
            }

        } else /* no compression */ {
            fwrite(rowdata, rowbytes, 1, stdout);
        }
    }
    /* If we've set the offset, pad to the next doubleword */
    if (cmdline.offset) {
        int cc;

        for(cc = (rowbytes * rows + 16)%4; cc > 0; cc--) {
            fputc(0, stdout);
        }
    }

    exit( 0 );
}
