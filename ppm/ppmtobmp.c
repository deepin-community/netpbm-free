/*\
 * $Id: ppmtobmp.c,v 1.4 2004/01/17 09:10:18 aba-guest Exp $
 *
 * ppmtobmp.c - Converts from a PPM file to a Microsoft Windows or OS/2
 * .BMP file.
 *
 * The current implementation is probably not complete, but it works for
 * me.  I welcome feedback.
 *
 * Copyright (C) 1992 by David W. Sanderson.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  This software is provided "as is"
 * without express or implied warranty.
 *
 * $Log: ppmtobmp.c,v $
 * Revision 1.4  2004/01/17 09:10:18  aba-guest
 * tab -> space
 *
 * Revision 1.3  2004/01/17 08:20:56  aba-guest
 * fixed bbp-values
 *
 * Revision 1.2  2003/08/14 19:38:51  aba-guest
 * First part to 9.25 update; could now be really broken
 *
 * Revision 1.1.1.1  2003/08/12 18:23:03  aba-guest
 * Latest debian release
 *
 * Revision 1.9  1992/11/24  19:39:33  dws
 * Added copyright.
 *
 * Revision 1.8  1992/11/17  02:16:52  dws
 * Moved length functions to bmp.h.
 *
 * Revision 1.7  1992/11/11  23:18:16  dws
 * Modified to adjust the bits per pixel to 1, 4, or 8.
 *
 * Revision 1.6  1992/11/11  22:43:39  dws
 * Commented out a superfluous message.
 *
 * Revision 1.5  1992/11/11  05:58:06  dws
 * First version that works.
 *
 * Revision 1.4  1992/11/11  03:40:32  dws
 * Moved calculation of bits per pixel to BMPEncode.
 *
 * Revision 1.3  1992/11/11  03:02:34  dws
 * Added BMPEncode function.
 *
 * Revision 1.2  1992/11/08  01:44:35  dws
 * Added option processing and reading of PPM file.
 *
 * Revision 1.1  1992/11/08  00:46:07  dws
 * Initial revision
\*/

#define _BSD_SOURCE 1    /* Make sure strdup() is in string.h */
#include        <string.h>
#include        "bmp.h"
#include        "ppm.h"
#include        "ppmcmap.h"
#include        "bitio.h"

#define MAXCOLORS 256

enum colortype {TRUECOLOR, PALLETTE};
/*
 * Utilities
 */

static char     er_write[] = "stdout: write error";

static struct cmdline_info {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
    char *input_filename;
    int class;  /* C_WIN or C_OS2 */
    int bits_per_pixel;
} cmdline;


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
        /* Instructions to OptParseOptions2 on how to parse our options.
         */
    optStruct2 opt;

    int windows, os2, bpp;

    unsigned int option_def_index;

    opt.opt_table = option_def;
    opt.short_allowed = FALSE;  /* We have no short (old-fashioned) options */
    opt.allowNegNum = FALSE;  /* We have no parms that are negative numbers */

    option_def_index = 0;   /* incremented by OPTENTRY */
    OPTENTRY('w', "windows",   OPT_FLAG,      &windows,   0);
    OPTENTRY('o', "os2",       OPT_FLAG,      &os2,       0);
    OPTENTRY(0,   "bpp",       OPT_UINT,      &bpp,       0);

    /* Set the defaults */
    windows = os2 = FALSE;
    bpp = -1;

    pm_optParseOptions2(&argc, argv, opt, 0);
        /* Uses and sets argc, and argv */

    if (argc - 1 == 0)
        cmdline_p->input_filename = strdup("-");  /* he wants stdin */
    else if (argc - 1 == 1)
        cmdline_p->input_filename = strdup(argv[1]);
    else 
        pm_error("Too many arguments.  The only argument accepted\n"
                 "is the input file specificaton");

    if (windows && os2) 
        pm_error("Can't specify both --windows and --os2 options.");
    else if (windows) 
        cmdline_p->class = C_WIN;
    else if (os2)
        cmdline_p->class = C_OS2;
    else 
        cmdline_p->class = C_WIN;


    if (bpp != -1 && bpp != 1 && bpp != 4 && bpp != 8 && bpp != 24)
        pm_error("Invalid --bpp value specified.  The only values valid\n"
                 "in the BMP format are 1, 4, 8, and 24 bits per pixel");
    cmdline_p->bits_per_pixel = bpp;

}



static void
PutByte(FILE *fp, char v)
{
        if (putc(v, fp) == EOF)
        {
                pm_error(er_write);
        }
}

static void
PutShort(FILE *fp, short v)
{
        if (pm_writelittleshort(fp, v) == -1)
        {
                pm_error(er_write);
        }
}

static void
PutLong(fp, v)
        FILE           *fp;
        long            v;
{
        if (pm_writelittlelong(fp, v) == -1)
        {
                pm_error(er_write);
        }
}

/*
 * BMP writing
 */

/*
 * returns the number of bytes written, or -1 on error.
 */
static int
BMPwritefileheader(fp, class, bitcount, x, y)
        FILE           *fp;
        int             class;
        unsigned long   bitcount;
        unsigned long   x;
        unsigned long   y;
{
        PutByte(fp, 'B');
        PutByte(fp, 'M');

        /* cbSize */
        PutLong(fp, BMPlenfile(class, bitcount, -1, x, y));

        /* xHotSpot */
        PutShort(fp, 0);

        /* yHotSpot */
        PutShort(fp, 0);

        /* offBits */
        PutLong(fp, BMPoffbits(class, bitcount, -1));

        return 14;
}

/*
 * returns the number of bytes written, or -1 on error.
 */
static int
BMPwriteinfoheader(fp, class, bitcount, x, y)
        FILE           *fp;
        int             class;
        unsigned long   bitcount;
        unsigned long   x;
        unsigned long   y;
{
        long    cbFix;

        /* cbFix */
        switch (class)
        {
        case C_WIN:
                cbFix = 40;
                PutLong(fp, cbFix);

                /* cx */
                PutLong(fp, x);
                /* cy */
                PutLong(fp, y);
                /* cPlanes */
                PutShort(fp, 1);
                /* cBitCount */
                PutShort(fp, bitcount);

                /*
                 * We've written 16 bytes so far, need to write 24 more
                 * for the required total of 40.
                 */

                PutLong(fp, 0);
                PutLong(fp, 0);
                PutLong(fp, 0);
                PutLong(fp, 0);
                PutLong(fp, 0);
                PutLong(fp, 0);


                break;
        case C_OS2:
                cbFix = 12;
                PutLong(fp, cbFix);

                /* cx */
                PutShort(fp, x);
                /* cy */
                PutShort(fp, y);
                /* cPlanes */
                PutShort(fp, 1);
                /* cBitCount */
                PutShort(fp, bitcount);

                break;
        default:
                pm_error(er_internal, "BMPwriteinfoheader");
        }

        return cbFix;
}

/*
 * returns the number of bytes written, or -1 on error.
 */
static int
BMPwritergb(FILE *fp, int class, pixval R, pixval G, pixval B)
{
        switch (class)
        {
        case C_WIN:
                PutByte(fp, B);
                PutByte(fp, G);
                PutByte(fp, R);
                PutByte(fp, 0);
                return 4;
        case C_OS2:
                PutByte(fp, B);
                PutByte(fp, G);
                PutByte(fp, R);
                return 3;
        default:
                pm_error(er_internal, "BMPwritergb");
        }
        return -1;
}

/*
 * returns the number of bytes written, or -1 on error.
 */
static int
BMPwritecolormap(FILE *fp, const int class, const int bpp,
                 const int colors, 
                 const unsigned char R[], const unsigned char G[], 
                 const unsigned char B[])
{
        int             nbyte = 0;
        int             i;
        long            ncolors;

        for (i = 0; i < colors; i++)
        {
                nbyte += BMPwritergb(fp,class,R[i],G[i],B[i]);
        }

        ncolors = (1 << bpp);

        for (; i < ncolors; i++)
        {
                nbyte += BMPwritergb(fp,class,0,0,0);
        }

        return nbyte;
}

/*
 * returns the number of bytes written, or -1 on error.
 */
static int
BMPwriterow_pallette(FILE *fp, const pixel * const row, 
                     const unsigned long cx, 
                     const unsigned short bpp, const colorhash_table cht)
{
        BITSTREAM       b;
        unsigned        nbyte = 0;
        int             rc;
        unsigned        x;

        if ((b = pm_bitinit(fp, "w")) == (BITSTREAM) 0)
        {
                return -1;
        }

        for (x = 0; x < cx; x++)
        {
            if ((rc = pm_bitwrite(b, bpp, 
                                  ppm_lookupcolor(cht, &row[x]))) == -1)
            {
                return -1;
            }
            nbyte += rc;
        }

        if ((rc = pm_bitfini(b)) == -1)
        {
                return -1;
        }
        nbyte += rc;

        /*
         * Make sure we write a multiple of 4 bytes.
         */
        while (nbyte % 4)
        {
                PutByte(fp, 0);
                nbyte++;
        }

        return nbyte;
}

/*
 * returns the number of bytes written, or -1 on error.
 */
static int
BMPwriterow_truecolor(FILE *fp, const pixel *row, const unsigned long cols) {
/*----------------------------------------------------------------------------
  Write a row of a truecolor BMP image to the file 'fp'.  The row is 
  'row', which is 'cols' columns long.

  Return the number of bytes written.

  On error, issue error message and exit program.
-----------------------------------------------------------------------------*/
    /* This only works for 24 bits per pixel.  To implement this for the
       general case (which is only hypothetical -- this program doesn't
       write any truecolor images except 24 bit and apparently no one
       else does either), you would move this function into 
       BMPwriterow_pallette, which writes arbitrary bit strings.  But
       that would be a lot slower and less robust.
       */

    int nbyte;  /* Number of bytes we have written to file so far */
    int col;  
        
    nbyte = 0;  /* initial value */
    for (col = 0; col < cols; col++) {
        PutByte(fp, PPM_GETB(row[col]));
        PutByte(fp, PPM_GETG(row[col]));
        PutByte(fp, PPM_GETR(row[col]));
        nbyte += 3;
    }

    /*
     * Make sure we write a multiple of 4 bytes.
     */
    while (nbyte % 4) {
        PutByte(fp, 0);
        nbyte++;
    }
    
    return nbyte;
}

/*
 * returns the number of bytes written, or -1 on error.
 */
static int
BMPwritebits(FILE *fp, const unsigned long cx, const unsigned long cy, 
             const enum colortype colortype,
             const unsigned short cBitCount, 
             const pixel ** const pixels, const colorhash_table cht)
{
        int             nbyte = 0;
        long            y;

        if(cBitCount > 24)
        {
                pm_error("cannot handle cBitCount: %d"
                         ,cBitCount);
        }

        /*
         * The picture is stored bottom line first, top line last
         */

        for (y = cy - 1; y >= 0; y--)
        {
                int rc;
                if (colortype == PALLETTE)
                    rc = BMPwriterow_pallette(fp, pixels[y], cx, 
                                              cBitCount, cht);
                else 
                    rc = BMPwriterow_truecolor(fp, pixels[y], cx);

                if(rc == -1)
                {
                        pm_error("couldn't write row %ld", y);
                }
                if(rc%4)
                {
                        pm_error("row had bad number of bytes: %d"
                                 ,rc);
                }
                nbyte += rc;
        }

        return nbyte;
}

/*
 * Write a BMP file of the given class.
 *
 * Note that we must have 'colors' in order to know exactly how many
 * colors are in the R, G, B, arrays.  Entries beyond those in the
 * arrays are undefined.
 */
static void
BMPEncode(FILE *fp, const int class, const enum colortype colortype,
          const int bpp,
          const int x, const int y, 
          const pixel ** const pixels, const int colors, 
          const colorhash_table cht, 
          const unsigned char R[], const unsigned char G[], 
          const unsigned char B[])
{
        unsigned long   nbyte = 0;

        if (colortype == PALLETTE)
            pm_message("Writing %d bits per pixel with a color pallette", bpp);
        else
            pm_message("Writing %d bits per pixel truecolor (no pallette)", 
                       bpp);

        nbyte += BMPwritefileheader(fp, class, bpp, x, y);
        nbyte += BMPwriteinfoheader(fp, class, bpp, x, y);
        if (colortype == PALLETTE)
            nbyte += BMPwritecolormap(fp, class, bpp, colors, R, G, B);

        if(nbyte !=     ( BMPlenfileheader(class)
                        + BMPleninfoheader(class)
                        + BMPlencolormap(class, bpp, -1)))
        {
                pm_error(er_internal, "BMPEncode 1");
        }

        nbyte += BMPwritebits(fp, x, y, colortype, bpp, pixels, cht);
        if(nbyte != BMPlenfile(class, bpp, -1, x, y))
        {
                pm_error(er_internal, "BMPEncode 2");
        }
}


#define adjust_minimum_bpp(bpp) \
        ((bpp <= 1) ? 1 : \
        (bpp <= 4) ? 4 : \
        (bpp <= 8) ? 8 : \
         24)


static void
analyze_colors(const pixel ** const pixels, 
               const int cols, const int rows, const pixval maxval, 
               int * const colors_p, 
               int * const minimum_bpp_p,
               colorhash_table *cht_p, 
               unsigned char Red[], 
               unsigned char Green[], 
               unsigned char Blue[]) {
/*----------------------------------------------------------------------------
  Look at the colors in the image 'pixels' and compute values to use in
  representing those colors in a BMP image.  

  First of all, count the distinct colors.  Return as *minimum_bpp_p
  the minimum number of bits per pixel it will take to represent all
  the colors in BMP format.

  If there are few enough colors to represent with a pallette, also
  return the number of colors as *colors_p and a suitable pallette
  (colormap) and a hash table in which to look up indexes into that
  pallette as Red[], Green[], Blue[], and *cht_p.  Use only the first
  *colors_p entries of the Red[], etc. array.

  If there are too many colors for a pallette, return *cht_p == NULL.

  Issue informational messages.

-----------------------------------------------------------------------------*/

    /* Figure out the colormap. */
    colorhist_vector chv;
    int i;

    pm_message("analyzing colors...");
    chv = ppm_computecolorhist((pixel**)pixels, cols, rows, MAXCOLORS, 
                               colors_p);
    if (chv == NULL) {
        pm_message("More than %d colors found", MAXCOLORS);
        *minimum_bpp_p = 24;
        *cht_p = NULL;
    } else {
        *minimum_bpp_p = adjust_minimum_bpp(pm_maxvaltobits(*colors_p-1));
        pm_message("%d colors found", *colors_p);
        /*
         * Now scale the maxval to 255 as required by BMP format.
         */
        for (i = 0; i < *colors_p; ++i)
        {
            Red[i] = (pixval) PPM_GETR(chv[i].color) * 255 / maxval;
            Green[i] = (pixval) PPM_GETG(chv[i].color) * 255 / maxval;
            Blue[i] = (pixval) PPM_GETB(chv[i].color) * 255 / maxval;
        }
    
        /* And make a hash table for fast lookup. */
        *cht_p = ppm_colorhisttocolorhash(chv, *colors_p);
        ppm_freecolorhist(chv);
    }
}



static void
choose_colortype_bpp(const struct cmdline_info cmdline,
                     const int colors, const int minimum_bpp,
                     enum colortype * const colortype_p, 
                     int * const bits_per_pixel_p) {

    if (cmdline.bits_per_pixel == -1) {
        /* User has no preference as to bits per pixel. */
        *bits_per_pixel_p = minimum_bpp;
    } else {
        if (cmdline.bits_per_pixel < minimum_bpp)
            pm_error("There are too many colors in the image to "
                     "represent in the\n"
                     "number of bits per pixel you requested: %d.\n"
                     "You may use Ppmquant to reduce the number of "
                     "colors in the image.",
                     cmdline.bits_per_pixel);
        else
            *bits_per_pixel_p = cmdline.bits_per_pixel;
    }
    if (*bits_per_pixel_p > 8) 
        *colortype_p = TRUECOLOR;
    else
        *colortype_p = PALLETTE;
}



int
main(int argc, char **argv)
{
        FILE *ifp = stdin;

        int rows;
        int cols;
        int colors;
        pixval maxval;
        int minimum_bpp;
        int bits_per_pixel;
        enum colortype colortype;

        pixel** pixels;
        colorhash_table cht;
        /* 'Red', 'Green', and 'Blue' are the color map for the BMP file,
           with indices the same as in 'cht', above.
        */
        unsigned char Red[MAXCOLORS], Green[MAXCOLORS], Blue[MAXCOLORS];

        ppm_init(&argc, argv);

        parse_command_line(argc, argv, &cmdline);

        ifp = pm_openr(cmdline.input_filename);

        pixels = ppm_readppm(ifp, &cols, &rows, &maxval);

        pm_close(ifp);


        analyze_colors((const pixel**)pixels, cols, rows, maxval, 
                       &colors, &minimum_bpp, &cht, Red, Green, Blue);

        choose_colortype_bpp(cmdline, colors, minimum_bpp, &colortype, 
                             &bits_per_pixel);

        BMPEncode(stdout, cmdline.class, colortype, bits_per_pixel,
                  cols, rows, (const pixel**)pixels, colors, cht,
                  Red, Green, Blue);

        if (cht) 
            ppm_freecolorhash(cht);

        pm_close(stdout);

        exit(0);
}



