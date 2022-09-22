/*****************************************************************************
                                    bmptopnm.c
******************************************************************************
 
 Bmptopnm - Converts from a Microsoft Windows or OS/2 .BMP file to a
 PBM, PGM, or PPM file.

 This program was formerly called Bmptoppm (and generated only PPM output).
 The name was changed in March 2002.

 Copyright (C) 1992 by David W. Sanderson.
 
 Permission to use, copy, modify, and distribute this software and its
 documentation for any purpose and without fee is hereby granted,
 provided that the above copyright notice appear in all copies and
 that both that copyright notice and this permission notice appear
 in supporting documentation.  This software is provided "as is"
 without express or implied warranty.

*****************************************************************************/

#include <string.h>
#include    "bmp.h"
#include    "pnm.h"
#include    "bitio.h"

/* MAXCOLORS is the maximum size of a color map in a BMP image */
#define MAXCOLORS       256

static xelval const bmpMaxval = 255;
    /* The maxval for intensity values in a BMP image -- either in a
       truecolor raster or in a colormap
    */

struct cmdline_info {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
    char *input_filespec;  /* Filespecs of input files */
    int verbose;    /* -verbose option */
};

static char    *ifname;



static void
parse_command_line(int argc, char ** argv,
                   struct cmdline_info *cmdline_p) {
/*----------------------------------------------------------------------------
   Note that the file spec array we return is stored in the storage that
   was passed to us as the argv array.
-----------------------------------------------------------------------------*/
    optStruct *option_def = malloc(100*sizeof(optStruct));
        /* Instructions to OptParseOptions2 on how to parse our options.
         */
    optStruct2 opt;

    unsigned int option_def_index;

    option_def_index = 0;   /* incremented by OPTENTRY */
    OPTENTRY(0,   "verbose",     OPT_FLAG,   &cmdline_p->verbose,         0);

    /* Set the defaults */
    cmdline_p->verbose = FALSE;

    opt.opt_table = option_def;
    opt.short_allowed = FALSE;  /* We have no short (old-fashioned) options */
    opt.allowNegNum = FALSE;  /* We have no parms that are negative numbers */

    pm_optParseOptions2(&argc, argv, opt, 0);
        /* Uses and sets argc, argv, and some of *cmdline_p and others. */

    if (argc-1 == 0) 
        cmdline_p->input_filespec = "-";
    else if (argc-1 != 1)
        pm_error("Program takes zero or one argument (filename).  You "
                 "specified %d", argc-1);
    else
        cmdline_p->input_filespec = argv[1];

}



static char     er_read[] = "%s: read error";

static int
GetByte(FILE * const fp) {

    int             v;

    if ((v = getc(fp)) == EOF)
        pm_error(er_read, ifname);

    return v;
}



static short
GetShort(FILE * const fp) {

    short           v;

    if (pm_readlittleshort(fp, &v) == -1)
        pm_error(er_read, ifname);

    return v;
}



static long
GetLong(FILE * const fp) {

    long v;

    if (pm_readlittlelong(fp, &v) == -1)
        pm_error(er_read, ifname);

    return v;
}



static void
readOffBytes(FILE * const fp, unsigned int const nbytes) {
/*----------------------------------------------------------------------------
   Read 'nbytes' from file 'fp'.  Abort program if read error.
-----------------------------------------------------------------------------*/
    int i;
    
    for(i = 0; i < nbytes; ++i) {
        int rc;
        rc = getc(fp);
        if (rc == EOF)
            pm_error(er_read, ifname);
    }
}



static void
BMPreadfileheader(FILE * const ifP, 
                  unsigned int * const bytesReadP, 
                  unsigned int * const offBitsP) {

    unsigned long   cbSize;
    unsigned short  xHotSpot;
    unsigned short  yHotSpot;
    unsigned long   offBits;

    if (GetByte(ifP) != 'B')
        pm_error("%s is not a BMP file", ifname);
    if (GetByte(ifP) != 'M')
        pm_error("%s is not a BMP file", ifname);

    cbSize = GetLong(ifP);
    xHotSpot = GetShort(ifP);
    yHotSpot = GetShort(ifP);
    offBits = GetLong(ifP);

    *offBitsP = offBits;

    *bytesReadP = 14;
}



static void
BMPreadinfoheader(FILE *          const ifP, 
                  unsigned int *  const bytesReadP,
                  int *           const colsP, 
                  int *           const rowsP, 
                  unsigned int *  const cBitCountP, 
                  enum bmpClass * const classP, 
                  int *           const cmapsizeP) {

    unsigned long   cbFix;
    unsigned short  cPlanes;

    unsigned long   cx;
    unsigned long   cy;
    unsigned short  cBitCount;
    unsigned long   compression;
    int             class;

    cbFix = GetLong(ifP);

    switch (cbFix) {
    case 12:
        class = C_OS2;

        cx = GetShort(ifP);
        cy = GetShort(ifP);
        cPlanes = GetShort(ifP);
        cBitCount = GetShort(ifP);
        /* I actually don't know if the OS/2 BMP format allows
           cBitCount > 8 or if it does, what it means, but ppmtobmp
           creates such BMPs, more or less as a byproduct of creating
           the same for Windows BMP, so we interpret cBitCount > 8 the
           same as for Windows.
        */
        if (cBitCount <= 8)
            *cmapsizeP = 1 << cBitCount;
        else if (cBitCount == 24)
            *cmapsizeP = 0;
        /* There is a 16 bit truecolor format, but we don't know how the
           bits are divided among red, green, and blue, so we can't handle it.
        */
        else
            pm_error("Unrecognized bits per pixel in BMP file header: %d",
                     cBitCount);

        break;
    case 40: {
        int colorsimportant;   /* ColorsImportant value from header */
        int colorsused;        /* ColorsUsed value from header */

        class = C_WIN;

        cx = GetLong(ifP);
        cy = GetLong(ifP);
        cPlanes = GetShort(ifP);
        cBitCount = GetShort(ifP);
 
        compression = GetLong(ifP);
        if (compression != 0) 
            pm_error("Input is compressed.  This program doesn't handle "
                     "compressed images.  Compression type code = %ld",
                     compression);

        /* And read the rest of the junk in the 40 byte header */
        GetLong(ifP);   /* ImageSize */
        GetLong(ifP);   /* XpixelsPerM */
        GetLong(ifP);   /* YpixelsPerM */
        colorsused = GetLong(ifP);   /* ColorsUsed */
        /* See comments in bmp.h for info about the definition of the following
           word and its relationship to the color map size (*pcmapsize).
           */
        colorsimportant = GetLong(ifP);  /* ColorsImportant */

        if (colorsused != 0)
            *cmapsizeP = colorsused;
        else {
            if (cBitCount <= 8)
                *cmapsizeP = 1 << cBitCount;
            else if (cBitCount == 24)
                *cmapsizeP = 0;
            /* There is a 16 bit truecolor format, but we don't know
               how the bits are divided among red, green, and blue, so
               we can't handle it.  */
            else
                pm_error("Unrecognized bits per pixel in BMP file header: %d",
                         cBitCount);
        }
    }
        break;
    default:
        pm_error("%s: unknown cbFix: %ld", ifname, cbFix);
        break;
    }

    if (cPlanes != 1) {
        pm_error("%s: don't know how to handle cPlanes = %d"
             ,ifname
             ,cPlanes);
    }

    switch (class) {
    case C_WIN:
        pm_message("Windows BMP, %ldx%ldx%d"
               ,cx
               ,cy
               ,cBitCount);
        break;
    case C_OS2:
        pm_message("OS/2 BMP, %ldx%ldx%d"
               ,cx
               ,cy
               ,cBitCount);
        break;
    }

#ifdef DEBUG
    pm_message("cbFix: %d", cbFix);
    pm_message("cx: %d", cx);
    pm_message("cy: %d", cy);
    pm_message("cPlanes: %d", cPlanes);
    pm_message("cBitCount: %d", cBitCount);
#endif

    *colsP = cx;
    *rowsP = cy;
    *cBitCountP = cBitCount;
    *classP = class;

    *bytesReadP = cbFix;
}



static void
BMPreadcolormap(FILE *         const ifP, 
                unsigned int   const cBitCount, 
                int            const class, 
                xel                  colormap[], 
                int            const cmapsize,
                unsigned int * const bytesReadP) {

    int i;
    
    *bytesReadP = 0;  /* initial value */

    for (i = 0; i < cmapsize; ++i) {
        /* There is a document that says the bytes are ordered R,G,B,Z,
           but in practice it appears to be the following instead:
        */
        unsigned int r, g, b;
        
        b = GetByte(ifP);
        g = GetByte(ifP);
        r = GetByte(ifP);

        PNM_ASSIGN(colormap[i], r, g, b);

        *bytesReadP += 3;

        if (class == C_WIN) {
            GetByte(ifP);
            *bytesReadP += 1;
        }
    }
}



static void
BMPreadrow(FILE *         const ifP, 
           xel                  xelrow[], 
           int            const cols, 
           unsigned int   const cBitCount, 
           xel                  colormap[],
           unsigned int * const bytesReadP, 
           bool *         const errorP
           ) {
/*----------------------------------------------------------------------------
   Read a raster row from the BMP file 'ifP' and return it as xelrow[].

   The BMP image has 'cBitCount' bits per pixel.

   If the image is colormapped, colormap[] is the colormap
   (R[i] is the red intensity value for the color with color index i).

   Return the number of bytes read from the file as *bytesReadP.

   Return *errorP == TRUE if unable to read row; in that case, other return
   values and state of file are undefined.
-----------------------------------------------------------------------------*/
    BITSTREAM       b;
    unsigned int bytesRead;

    b = pm_bitinit(ifP, "r");
    if (b == NULL)
        *errorP = TRUE;
    else {
        unsigned int    col;
        bytesRead = 0;
        *errorP = FALSE;  /* initial assumption */
        for (col = 0; col < cols && !*errorP ; ++col) {
            unsigned long v;
            int rc;

            /* There is a document that gives a much different format for
               24 bit BMPs.  But this seems to be the de facto standard.
            */
            rc = pm_bitread(b, cBitCount, &v);
            if (rc == -1)
                *errorP = TRUE;
            else {
                bytesRead += rc;

                if (cBitCount <= 8)
                    /* It's colormapped, so look up v in the colormap */
                    xelrow[col] = colormap[v];
                else if (cBitCount == 24)
                    /* It's truecolor, so just unpack v */
                    PNM_ASSIGN(xelrow[col], 
                               (v >>  0) & 0xff,
                               (v >>  8) & 0xff,
                               (v >> 16) & 0xff);
                
            }
        }
        if (!*errorP) {
            int rc;
            rc = pm_bitfini(b);
            if (rc != 0)
                *errorP = TRUE;
            else {
                /* Make sure we read a multiple of 4 bytes. */
                while (bytesRead % 4) {
                    GetByte(ifP);
                    ++bytesRead;
                }
            }
        }
    }
    *bytesReadP = bytesRead;
}



static void
BMPreadbits(FILE * const ifP, 
            int const cols, int const rows, 
            unsigned int const cBitCount, enum bmpClass const class, 
            xel colormap[], 
            xel *** const xelsP, unsigned int * const bytesReadP) {

    xel **xels;
    int row;

    xels = pnm_allocarray(cols, rows);

    /*
     * The picture is stored bottom line first, top line last
     */

    *bytesReadP = 0;
    for (row = rows - 1; row >= 0; --row) {
        bool error;
        unsigned int bytesRead;
        BMPreadrow(ifP, xels[row], cols, cBitCount, colormap,
                   &bytesRead, &error);
        if (error)
            pm_error("%s: couldn't read row %d", ifname, row);
        else {
            *bytesReadP += bytesRead;
            if (bytesRead % 4 != 0) 
                pm_error("%s: row had bad number of bytes: %u", 
                         ifname, bytesRead);
        }
    }
    *xelsP = xels;
}



static void
reportHeader(int const class, int const cols, int const rows,
             unsigned int const offBits, unsigned int const cBitCount,
             unsigned int const cmapsize) {
    
    pm_message("BMP image header says:");
    pm_message("  Class of BMP: %s", 
               class == C_WIN ? "Windows" : class == C_OS2 ? "OS/2" :
               "???");
    pm_message("  Width: %d pixels", cols);
    pm_message("  Height: %d pixels", rows);
    pm_message("  Byte offset of raster within file: %u", offBits);
    pm_message("  Bits per pixel in raster: %u", cBitCount);
    pm_message("  Colors in color map: %d", cmapsize);
}        



static void
analyzeColors(xel colormap[], unsigned int const cmapsize, xelval const maxval,
              bool * const grayPresentP, bool * const colorPresentP) {
    
    if (cmapsize == 0) {
        /* No colormap, and we're not about to search the entire raster,
           so we just assume it's full color 
        */
        *colorPresentP = TRUE;
        *grayPresentP = TRUE;
    } else {
        unsigned int i;
        *colorPresentP = FALSE;  /* initial assumption */
        *grayPresentP = FALSE;   /* initial assumption */
        for (i = 0; i < cmapsize; ++i) {
            xelval const r = PPM_GETR(colormap[i]);
            xelval const g = PPM_GETG(colormap[i]);
            xelval const b = PPM_GETB(colormap[i]);
            
            if (r != g || g != b) 
                *colorPresentP = TRUE;
            else {
                /* All components are equal */
                if (r != 0 && r != maxval)
                    *grayPresentP = TRUE;
            }
        }
    }
}



static void
readBmp(FILE * const ifP, xel *** const xelsP, 
        int * const colsP, int * const rowsP,
        bool * const grayPresentP, bool * const colorPresentP,
        bool const verbose) {

    unsigned int pos;

    /* The following are all information from the BMP headers */
    unsigned int offBits;
      /* Byte offset into file of raster */
    int cols, rows;
    unsigned int cBitCount;
    enum bmpClass class;
    int cmapsize;
      /* Number of colors in the colormap in the BMP image */
    xel colormap[MAXCOLORS];

    pos = 0;
    { 
        unsigned int bytesRead;
        BMPreadfileheader(ifP, &bytesRead, &offBits);
        pos += bytesRead;
    }
    {
        unsigned int bytesRead;
        BMPreadinfoheader(ifP, &bytesRead, 
                          &cols, &rows, &cBitCount, &class, &cmapsize);
        pos += bytesRead;
    }

    if (verbose) 
        reportHeader(class, cols, rows, offBits, cBitCount, cmapsize);

    if(offBits != BMPoffbits(class, cBitCount, cmapsize)) {
        pm_message("warning: offBits is %u, expected %u"
                   , offBits
                   , BMPoffbits(class, cBitCount, cmapsize));
    }
    {
        unsigned int bytesRead;
        BMPreadcolormap(ifP, cBitCount, class, colormap, cmapsize, &bytesRead);
        pos += bytesRead;
        if (bytesRead != BMPlencolormap(class, cBitCount, cmapsize)) {
            pm_message("warning: %u-byte RGB table, expected %u bytes"
                       , bytesRead
                   , BMPlencolormap(class, cBitCount, cmapsize));
        }
    }

    analyzeColors(colormap, cmapsize, bmpMaxval, grayPresentP, colorPresentP);

    readOffBytes(ifP, offBits-pos);

    pos = offBits;

    {
        unsigned int bytesRead;
        BMPreadbits(ifP, cols, rows, cBitCount, class, colormap, 
                    xelsP, &bytesRead);
        pos += bytesRead;
    }
    if(pos != BMPlenfile(class, cBitCount, cmapsize, cols, rows)) {
        pm_message("warning: read %u bytes, expected to read %u bytes"
                   , pos
                   , BMPlenfile(class, cBitCount, cmapsize, cols, rows));
    }
    *colsP = cols;
    *rowsP = rows;
}



int
main(int argc, char ** argv) {

    struct cmdline_info cmdline;
    FILE* ifP;
    int outputType;

    xel **xels;
    bool grayPresent, colorPresent;
        /* These tell whether the image contains shades of gray other than
           black and white and whether it has colors other than black, white,
           and gray.
        */
    int cols, rows;

    pnm_init(&argc, argv);

    parse_command_line(argc, argv, &cmdline);

    ifP = pm_openr(cmdline.input_filespec);
    if (strcmp(cmdline.input_filespec, "-"))
        ifname = "Standard Input";
    else 
        ifname = cmdline.input_filespec;

    readBmp(ifP, &xels, &cols, &rows, &grayPresent, &colorPresent, 
            cmdline.verbose);

    if (colorPresent) {
        outputType = PPM_TYPE;
        pm_message("WRITING PPM IMAGE");
    } else if (grayPresent) {
        outputType = PGM_TYPE;
        pm_message("WRITING PGM IMAGE");
    } else {
        outputType = PBM_TYPE;
        pm_message("WRITING PBM IMAGE");
    }
    pm_close(ifP);
    pnm_writepnm(stdout, xels, cols, rows, bmpMaxval, outputType, FALSE);
    pm_close(stdout);

    exit(0);
}

