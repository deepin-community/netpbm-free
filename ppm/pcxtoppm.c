/*
 * pcxtoppm.c - Converts from a PC Paintbrush PCX file to a PPM file.
 *
 * Copyright (c) 1990 by Michael Davidson
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 * Modifications by Ingo Wilken (Ingo.Wilken@informatik.uni-oldenburg.de)
 * 20/Apr/94:
 *  - checks if 16-color-palette is completely black -> use standard palette
 *  - "-stdpalette" option to enforce this
 *  - row-by-row operation (PPM output)
 *  11/Dec/94:
 *  - support for 24bit and 32bit (24bit + 8bit intensity) images
 *  - row-by-row operation (PCX input, for 16-color and truecolor images)
 *  - some code restructuring
 *  15/Feb/95:
 *  - bugfix for 16 color-images: few bytes allocated for rawrow in some cases
 *  - added sanity checks for cols<->BytesPerLine
 *  17/Jul/95:
 *  - moved check of 16-color-palette into pcx_16col_to_ppm(),
 *    now checks if it contains only a single color
 */
#include        "ppm.h"

#define PCX_MAGIC       0x0a            /* PCX magic number             */
#define PCX_HDR_SIZE    128             /* size of PCX header           */
#define PCX_256_COLORS  0x0c            /* magic number for 256 colors  */

#define PCX_MAXVAL      (pixval)255

/* prototypes */
static void pcx_16col_to_ppm ARGS((FILE *ifp, int cols, int rows,
                int BytesPerLine, int BitsPerPixel, int Planes, pixel *cmap));
static void pcx_256col_to_ppm ARGS((FILE *ifp, int cols, int rows, int BytesPerLine));
static void pcx_truecol_to_ppm ARGS((FILE *ifp, int cols, int rows, int BytesPerLine, int Planes));
static void GetPCXRow ARGS((FILE *ifp, unsigned char *pcxrow, int bytesperline));
static void pcx_planes_to_pixels ARGS(( unsigned char *pixels, unsigned char *bitplanes, int bytesperline, int planes, int bitsperpixel ));
static void pcx_unpack_pixels ARGS(( unsigned char *pixels, unsigned char *bitplanes, int bytesperline, int planes, int bitsperpixel ));
static int GetByte ARGS(( FILE *fp ));

/* standard palette */
static unsigned char const StdRed[]   = { 0, 255,   0,   0, 170, 170, 170, 170, 85,  85,  85,  85, 255, 255, 255, 255 };
static unsigned char const StdGreen[] = { 0, 255, 170, 170,   0,   0, 170, 170, 85,  85, 255, 255,  85,  85, 255, 255 };
static unsigned char const StdBlue[]  = { 0, 255,   0, 170,   0, 170,   0, 170, 85, 255,  85, 255,  85, 255,  85, 255 };

static pixel stdPalette[16];

static void
generateStdPalette(void) {

    int i;
    for (i = 0; i < 16; i++)
        PPM_ASSIGN(stdPalette[i], StdRed[i], StdGreen[i], StdBlue[i]);
}



struct cmdlineInfo {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
    char *inputFilespec;  /* '-' if stdin */
    unsigned int verbose;  
    unsigned int stdpalette;
};



static void
parseCommandLine ( int argc, char ** argv,
                   struct cmdlineInfo *cmdlineP ) {
/*----------------------------------------------------------------------------
   parse program command line described in Unix standard form by argc
   and argv.  Return the information in the options as *cmdlineP.  

   If command line is internally inconsistent (invalid options, etc.),
   issue error message to stderr and abort program.

   Note that the strings we return are stored in the storage that
   was passed to us as the argv array.  We also trash *argv.
-----------------------------------------------------------------------------*/
    optEntry *option_def = malloc( 100*sizeof( optEntry ) );
        /* Instructions to optParseOptions3 on how to parse our options.
         */
    optStruct3 opt;

    unsigned int option_def_index;

    option_def_index = 0;   /* incremented by OPTENT3 */
    OPTENT3(0, "stdpalette",     OPT_FLAG,   NULL,                  
            &cmdlineP->stdpalette,    0 );
    OPTENT3(0, "verbose",        OPT_FLAG,   NULL,                  
            &cmdlineP->verbose,       0 );

    opt.opt_table = option_def;
    opt.short_allowed = FALSE;  /* We have no short (old-fashioned) options */
    opt.allowNegNum = FALSE;  /* We have no parms that are negative numbers */

    pm_optParseOptions3( &argc, argv, opt, sizeof(opt), 0 );
        /* Uses and sets argc, argv, and some of *cmdline_p and others. */

    if (argc-1 < 1)
        cmdlineP->inputFilespec = "-";
    else if (argc-1 == 1)
        cmdlineP->inputFilespec = argv[1];
    else
        pm_error("Program takes at most one argument "
                 "(input file specification).  You specified %d",
                 argc-1);
}



struct pcxHeader {
    int Version;
    /* Xmin, Ymin, Xmax, and Ymax are positions in some field (in units of
       pixels) of the edges of the image.  They may be negative.  You can
       derive the image width and height from these.
    */
    short Xmin;            
    short Ymin;
    short Xmax;
    short Ymax;
    short Encoding;
    short Planes;
    short BitsPerPixel;
    short BytesPerLine;
        /* Number of decompressed bytes each plane of each row of the image 
           takes.  Due to padding (this is always an even number), there may
           be garbage on the right end that isn't part of the image.
        */
    short PaletteInfo;
    short HorizontalResolution;
    short VerticalResolution;
    pixel cmap16[16];
};



static void
readInt(FILE * const ifP, short * const retvalP) {
/*----------------------------------------------------------------------------
   Read a 2-byte little-endian word from the file.
-----------------------------------------------------------------------------*/
    int rc;

    rc = pm_readlittleshort(ifP, retvalP);

    if (rc != 0)
        pm_error("EOF/error reading integer from input file.");
}



static void
readPcxHeader(FILE *             const ifP, 
              struct pcxHeader * const pcxHeaderP) {
/*----------------------------------------------------------------------------
   Read the PCX header
-----------------------------------------------------------------------------*/
    if (GetByte(ifP) != PCX_MAGIC)
        pm_error("bad magic number - not a PCX file");

    pcxHeaderP->Version = GetByte(ifP);  /* get version # */

    pcxHeaderP->Encoding = GetByte(ifP);
    if (pcxHeaderP->Encoding != 1)    /* check for PCX run length encoding   */
        pm_error("unknown encoding scheme: %d", pcxHeaderP->Encoding);

    pcxHeaderP->BitsPerPixel= GetByte(ifP);
    readInt(ifP, &pcxHeaderP->Xmin);
    readInt(ifP, &pcxHeaderP->Ymin);
    readInt(ifP, &pcxHeaderP->Xmax);
    readInt(ifP, &pcxHeaderP->Ymax);

    if (pcxHeaderP->Xmax < pcxHeaderP->Xmin)
        pm_error("Invalid PCX input:  minimum X value (%d) is greater than "
                 "maximum X value (%d).", 
                 pcxHeaderP->Xmin, pcxHeaderP->Xmax);
    if (pcxHeaderP->Ymax < pcxHeaderP->Ymin)
        pm_error("Invalid PCX input:  minimum Y value (%d) is greater than "
                 "maximum Y value (%d).", 
                 pcxHeaderP->Ymin, pcxHeaderP->Ymax);

    readInt(ifP, &pcxHeaderP->HorizontalResolution);  
    readInt(ifP, &pcxHeaderP->VerticalResolution);  

    {
        int i;
        /*
         * get the 16-color color map
         */
        for (i = 0; i < 16; i++) {
            int r, g, b;
            r = GetByte(ifP);
            g = GetByte(ifP);
            b = GetByte(ifP);
            PPM_ASSIGN(pcxHeaderP->cmap16[i], r, g, b);
        }
    }

    GetByte(ifP);                /* skip reserved byte       */
    pcxHeaderP->Planes      = GetByte(ifP);     /* # of color planes        */
    readInt(ifP, &pcxHeaderP->BytesPerLine);
    readInt(ifP, &pcxHeaderP->PaletteInfo);

    /* Read past a bunch of reserved space in the header.  We have read
       70 bytes of the header so far.  We would just seek here, except that
       we want to work with unseekable (e.g. pipe input).
    */
    {
        unsigned int pos;

        for (pos = 70; pos < PCX_HDR_SIZE; ++pos)
            GetByte(ifP);
    }
}



static void 
reportPcxHeader(struct pcxHeader const pcxHeader) {

    pm_message("Version: %d", pcxHeader.Version);
    pm_message("BitsPerPixel: %d", pcxHeader.BitsPerPixel);
    pm_message("Xmin: %d   Ymin: %d   Xmax: %d   Ymax: %d", 
               pcxHeader.Xmin, pcxHeader.Ymin, pcxHeader.Xmax, pcxHeader.Ymax);
    pm_message("Planes: %d    BytesPerLine: %d    PaletteInfo: %d", 
               pcxHeader.Planes, pcxHeader.BytesPerLine, 
               pcxHeader.PaletteInfo);
}



int
main(int argc, char *argv[]) {

    FILE *ifP;
    struct cmdlineInfo cmdline;
    struct pcxHeader pcxHeader;
    int Width, Height;
    pixel *cmap16;

    ppm_init( &argc, argv );

    generateStdPalette();

    parseCommandLine(argc, argv, &cmdline);

    ifP = pm_openr(cmdline.inputFilespec);

    readPcxHeader(ifP, &pcxHeader);

    if (cmdline.verbose)
        reportPcxHeader(pcxHeader);

    Width  = (pcxHeader.Xmax - pcxHeader.Xmin) + 1;
    Height = (pcxHeader.Ymax - pcxHeader.Ymin) + 1;

    if (cmdline.stdpalette) 
        cmap16 = stdPalette;
    else
        cmap16 = pcxHeader.cmap16;

    ppm_writeppminit(stdout, Width, Height, PCX_MAXVAL, 0);
    switch (pcxHeader.BitsPerPixel) {
    case 1:
        if(pcxHeader.Planes >= 1 && pcxHeader.Planes <= 4)
            pcx_16col_to_ppm(ifP, Width, Height, pcxHeader.BytesPerLine, 
                             pcxHeader.BitsPerPixel, pcxHeader.Planes, cmap16);
        else
            goto fail;
        break;
    case 2:
    case 4:
        if (pcxHeader.Planes == 1)
            pcx_16col_to_ppm(ifP, Width, Height, pcxHeader.BytesPerLine, 
                             pcxHeader.BitsPerPixel, pcxHeader.Planes, cmap16);
        else
            goto fail;
        break;
    case 8:
        switch(pcxHeader.Planes) {
        case 1:
            pcx_256col_to_ppm(ifP, Width, Height, pcxHeader.BytesPerLine);
            break;
        case 3:
        case 4:
            pcx_truecol_to_ppm(ifP, Width, Height, 
                               pcxHeader.BytesPerLine, pcxHeader.Planes);
            break;
        default:
            goto fail;
        }
        break;
    default:
    fail:
        pm_error("can't handle %d bits per pixel image with %d planes",
                 pcxHeader.BitsPerPixel, pcxHeader.Planes);
    }
    pm_close(ifP);
    exit(0);
}


static void
pcx_16col_to_ppm(ifp, cols, rows, BytesPerLine, BitsPerPixel, Planes, cmap)
    FILE *ifp;
    int cols, rows;
    int BytesPerLine, BitsPerPixel, Planes;
    pixel *cmap;    /* colormap from header */
{
    int row, col, rawcols, colors;
    unsigned char *pcxrow, *rawrow;
    pixel *ppmrow;
    short palette_ok = 0;

#ifdef DEBUG
    pm_message("16 color -> ppm");
#endif

    /* check if palette is ok  */
    colors = (1 << BitsPerPixel) * (1 << Planes);
    for( col = 0; col < colors-1; col++ ) {
        if( !PPM_EQUAL(cmap[col], cmap[col+1]) ) {
            palette_ok = 1;
            break;
        }
    }
    if( !palette_ok ) {
        pm_message("warning - useless header palette, "
                   "using builtin standard palette");
        for( col = 0; col < colors; col++ )
            PPM_ASSIGN(cmap[col], StdRed[col], StdGreen[col], StdBlue[col]);
    }

    /*  BytesPerLine should be >= BitsPerPixel * cols / 8  */
    overflow2(BytesPerLine, 8);
    rawcols = BytesPerLine * 8 / BitsPerPixel;
    if( cols > rawcols ) {
        pm_message("warning - BytesPerLine = %d, "
				"truncating image to %d pixels",
                    BytesPerLine, rawcols);
        cols = rawcols;
    }
    overflow2(Planes, BytesPerLine);
    pcxrow = (unsigned char *)pm_allocrow(Planes * BytesPerLine, sizeof(unsigned char));
    rawrow = (unsigned char *)pm_allocrow(rawcols, sizeof(unsigned char));
    ppmrow = ppm_allocrow(cols);

    for( row = 0; row < rows; row++ ) {
        GetPCXRow(ifp, pcxrow, Planes * BytesPerLine);

        if (Planes == 1)
            pcx_unpack_pixels(rawrow, pcxrow, BytesPerLine, 
                              Planes, BitsPerPixel);
        else
            pcx_planes_to_pixels(rawrow, pcxrow, BytesPerLine, 
                                 Planes, BitsPerPixel);

        for( col = 0; col < cols; col++ )
            ppmrow[col] = cmap[rawrow[col]];
        ppm_writeppmrow(stdout, ppmrow, cols, PCX_MAXVAL, 0);
    }

#ifdef DEBUG
    pm_message("done!");
#endif
    ppm_freerow(ppmrow);
    pm_freerow((char*)rawrow);
    pm_freerow((void*)pcxrow);
}



static void
pcx_256col_to_ppm(ifp, cols, rows, BytesPerLine)
    FILE *ifp;
    int cols, rows;
    int BytesPerLine;
{
    pixel colormap[256];
    pixel *ppmrow;
    unsigned char **image;
    unsigned char colormapSignature;
    int row, col;

#ifdef DEBUG
    pm_message("256 color -> ppm, reading index array...");
#endif

    if( cols > BytesPerLine ) {
        pm_message("warning - BytesPerLine = %d, "
                   "truncating image to %d pixels",
                   BytesPerLine,  BytesPerLine);
        cols = BytesPerLine;
    }

    image = (unsigned char **)pm_allocarray(BytesPerLine, rows, 
                                            sizeof(unsigned char));
    for( row = 0; row < rows; row++ )
        GetPCXRow(ifp, image[row], BytesPerLine);

#ifdef DEBUG
    pm_message("ok, now reading colormap...");
#endif

    /*
     * 256 color images have their color map at the end of the file
     * preceeded by a magic byte
     */
    colormapSignature = GetByte(ifp);
    if (colormapSignature != PCX_256_COLORS)
        pm_error("bad color map signature.  In a 1-plane PCX image "
                 "such as this, we expect a magic number of %u in the byte "
                 "following the raster, to introduce the color map.  "
                 "Instead, this image has %u.", 
                 PCX_256_COLORS, colormapSignature);
    else {
        unsigned int colorIndex;

        for(colorIndex = 0; colorIndex < 256; ++colorIndex) {
            int r, g, b;
            r = GetByte(ifp);
            g = GetByte(ifp);
            b = GetByte(ifp);
            PPM_ASSIGN(colormap[colorIndex], r, g, b);
        }
    }
#ifdef DEBUG
    pm_message("ok, converting...");
#endif

    ppmrow = ppm_allocrow(cols);
    for( row = 0; row < rows; row++ ) {
        for( col = 0; col < cols; col++ )
            ppmrow[col] = colormap[image[row][col]];
        ppm_writeppmrow(stdout, ppmrow, cols, PCX_MAXVAL, 0);
    }
#ifdef DEBUG
    pm_message("done!");
#endif

    ppm_freerow(ppmrow);
    pm_freearray((void *)image, rows);
}



static void
pcx_truecol_to_ppm(ifp, cols, rows, BytesPerLine, Planes)
    FILE *ifp;
    int cols, rows;
    int BytesPerLine, Planes;
{
    unsigned char *redrow, *greenrow, *bluerow, *intensityrow;
    pixel *ppmrow;
    int row, col;
    int r, g, b, i;

#ifdef DEBUG
    pm_message("%dbit truecolor -> ppm", Planes * 8);
#endif

    if( cols > BytesPerLine ) {
        pm_message("warning - BytesPerLine = %d, "
                   "truncating image to %d pixels",
                   BytesPerLine,  BytesPerLine);
        cols = BytesPerLine;
    }

    redrow   = (unsigned char *)
        pm_allocrow(BytesPerLine, sizeof(unsigned char*));
    greenrow = (unsigned char *)
        pm_allocrow(BytesPerLine, sizeof(unsigned char*));
    bluerow  = (unsigned char *)
        pm_allocrow(BytesPerLine, sizeof(unsigned char*));
    if( Planes == 4 )
        intensityrow = (unsigned char *)
            pm_allocrow(BytesPerLine, sizeof(unsigned char*));
    else
        intensityrow = (unsigned char *)NULL;

    ppmrow = ppm_allocrow(cols);
    for( row = 0; row < rows; row++ ) {
        GetPCXRow(ifp, redrow, BytesPerLine);
        GetPCXRow(ifp, greenrow, BytesPerLine);
        GetPCXRow(ifp, bluerow, BytesPerLine);
        if( intensityrow )
            GetPCXRow(ifp, intensityrow, BytesPerLine);
        for( col = 0; col < cols; col++ ) {
            r = redrow[col];
            g = greenrow[col];
            b = bluerow[col];
            if( intensityrow ) {
                i = intensityrow[col];
                r = r * i / 256;
                g = g * i / 256;
                b = b * i / 256;
            }
            PPM_ASSIGN(ppmrow[col], r, g, b);
        }
        ppm_writeppmrow(stdout, ppmrow, cols, PCX_MAXVAL, 0);
    }
#ifdef DEBUG
    pm_message("done!");
#endif

    ppm_freerow(ppmrow);
    if( intensityrow )
        pm_freerow((void*)intensityrow);
    pm_freerow((void*)bluerow);
    pm_freerow((void*)greenrow);
    pm_freerow((void*)redrow);
}


/*
 * convert multi-plane format into 1 pixel per byte
 */
static void
pcx_planes_to_pixels(pixels, bitplanes, bytesperline, planes, bitsperpixel)
    unsigned char   *pixels;
    unsigned char   *bitplanes;
    int             bytesperline;
    int             planes;
    int             bitsperpixel;
{
    int  i, j;
    int  npixels;
    unsigned char    *p;

    if (planes > 4)
        pm_error("can't handle more than 4 planes" );
    if (bitsperpixel != 1)
        pm_error("can't handle more than 1 bit per pixel" );

    /*
     * clear the pixel buffer
     */

    overflow2(bytesperline, 8);
    npixels = (bytesperline * 8) / bitsperpixel;
    p    = pixels;
    while (--npixels >= 0)
        *p++ = 0;

    /*
     * do the format conversion
     */
    for (i = 0; i < planes; i++)
    {
        int pixbit, bits, mask;

        p    = pixels;
        pixbit    = (1 << i);
        for (j = 0; j < bytesperline; j++)
        {
            bits = *bitplanes++;
            for (mask = 0x80; mask != 0; mask >>= 1, p++)
                if (bits & mask)
                    *p |= pixbit;
        }
    }
}

/*
 * convert packed pixel format into 1 pixel per byte
 */
static void
pcx_unpack_pixels(pixels, bitplanes, bytesperline, planes, bitsperpixel)
    unsigned char   *pixels;
    unsigned char   *bitplanes;
    int             bytesperline;
    int             planes;
    int             bitsperpixel;
{
    register int        bits;

    if (planes != 1)
        pm_error("can't handle packed pixels with more than 1 plane" );
#if 0
    if (bitsperpixel == 8)
    {
        while (--bytesperline >= 0)
            *pixels++ = *bitplanes++;
    }
    else
#endif
        if (bitsperpixel == 4)
        {
            while (--bytesperline >= 0)
            {
                bits        = *bitplanes++;
                *pixels++   = (bits >> 4) & 0x0f;
                *pixels++   = (bits     ) & 0x0f;
            }
        }
        else if (bitsperpixel == 2)
        {
            while (--bytesperline >= 0)
            {
                bits        = *bitplanes++;
                *pixels++   = (bits >> 6) & 0x03;
                *pixels++   = (bits >> 4) & 0x03;
                *pixels++   = (bits >> 2) & 0x03;
                *pixels++   = (bits     ) & 0x03;
            }
        }
        else if (bitsperpixel == 1)
        {
            while (--bytesperline >= 0)
            {
                bits        = *bitplanes++;
                *pixels++   = ((bits & 0x80) != 0);
                *pixels++   = ((bits & 0x40) != 0);
                *pixels++   = ((bits & 0x20) != 0);
                *pixels++   = ((bits & 0x10) != 0);
                *pixels++   = ((bits & 0x08) != 0);
                *pixels++   = ((bits & 0x04) != 0);
                *pixels++   = ((bits & 0x02) != 0);
                *pixels++   = ((bits & 0x01) != 0);
            }
        }
        else
            pm_error("pcx_unpack_pixels - can't handle %d bits per pixel", 
                     bitsperpixel);
}


static int rowsRead=0;

static int
GetByte(FILE * const fp) {

    int    c;

    if ((c = fgetc(fp)) == EOF)
        pm_error("unexpected end of file" );

    return c;
}



/*
 *  read a single row encoded row, handles encoding across rows
 */
static void
GetPCXRow(FILE * const ifP, unsigned char * const pcxrow, 
          int const bytesperline) {
/*----------------------------------------------------------------------------
   Read one row from the PCX raster.
   
   The PCX raster is run length encoded as follows:  If the upper two
   bits of a byte are 11, the lower 6 bits are a repetition count for the
   raster byte that follows.  If the upper two bits are not 11, the byte 
   _is_ a raster byte, with repetition count 1.

   A run can't span rows, but it can span planes within a row.  That's
   why 'repetitionsLeft' and 'c' are static variables in this
   subroutine.
-----------------------------------------------------------------------------*/
    static int repetitionsLeft = 0;
    static int c;
    int bytesGenerated;

    bytesGenerated = 0;
    while( bytesGenerated < bytesperline ) {
        if(repetitionsLeft > 0) {
            pcxrow[bytesGenerated++] = c;
            --repetitionsLeft;
        } else {
 if (rowsRead==241)
     pm_message("Reading Byte %ld", ftell(ifP));
            c = GetByte(ifP);
            if ((c & 0xc0) != 0xc0)
                /* This is a 1-shot byte, not a repetition count */
                pcxrow[bytesGenerated++] = c;
            else {
                /* This is a repetition count for the following byte */
                repetitionsLeft = c & 0x3f;
if (rowsRead == 241)
pm_message("doing run of %d of Byte %ld in Row 241", repetitionsLeft, ftell(ifP));
                c = GetByte(ifP);
            }
        }
    }
rowsRead++;
}

