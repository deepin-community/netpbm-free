/* ppmquant.c - quantize the colors in a pixmap down to a specified number
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

#include <limits.h>

#include "ppm.h"
#include "ppmcmap.h"

#define MAXCOLORS 32767

/* #define LARGE_NORM */
#define LARGE_LUM

/* #define REP_CENTER_BOX */
/* #define REP_AVERAGE_COLORS */
#define REP_AVERAGE_PIXELS

#define FS_SCALE 1024

typedef struct box* box_vector;
struct box {
    int ind;
    int colors;
    int sum;
};

struct fserr {
    long* thisrerr;
    long* nextrerr;
    long* thisgerr;
    long* nextgerr;
    long* thisberr;
    long* nextberr;
    bool fsForward;
};


struct cmdlineInfo {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
    char *inputFilespec;  /* Filespec of input file */
    char *mapFilespec;    /* Filespec of colormap file; NULL if none */
    unsigned int newcolors;   
        /* Number of colors argument; meaningless if mapFilespec non-null */
    unsigned int floyd;   /* Boolean: -floyd/-fs option */
};


/* These belong to convertRow() */
static colorhash_table cht;
static bool usehash;


static int __inline__ sqr(const int x) {
    return x*x;
}


static void
parseCommandLine(int argc, char ** argv,
                 struct cmdlineInfo *cmdlineP) {
/*----------------------------------------------------------------------------
   Note that the file spec array we return is stored in the storage that
   was passed to us as the argv array.
-----------------------------------------------------------------------------*/
    optStruct3 opt;  /* set by OPTENT3 */
    optEntry *option_def = malloc(100*sizeof(optEntry));
    unsigned int option_def_index;

    unsigned int mapfileSpec;
    unsigned int nofloyd;

    option_def_index = 0;   /* incremented by OPTENT3 */
    OPTENT3(0,   "floyd",       OPT_FLAG,   
            NULL,                       &cmdlineP->floyd, 0);
    OPTENT3(0,   "fs",          OPT_FLAG,   
            NULL,                       &cmdlineP->floyd, 0);
    OPTENT3(0,   "nofloyd",     OPT_FLAG,   
            NULL,                       &nofloyd, 0);
    OPTENT3(0,   "nofs",        OPT_FLAG,   
            NULL,                       &nofloyd, 0);
    OPTENT3(0,   "mapfile",     OPT_STRING, 
            &cmdlineP->mapFilespec,     &mapfileSpec, 0);
    

    opt.opt_table = option_def;
    opt.short_allowed = FALSE;  /* We have no short (old-fashioned) options */
    opt.allowNegNum = FALSE;  /* We have no parms that are negative numbers */

    pm_optParseOptions3(&argc, argv, opt, sizeof(opt), 0);
        /* Uses and sets argc, argv, and some of *cmdlineP and others. */

    if (cmdlineP->floyd && nofloyd)
        pm_error("You cannot specify both -floyd and -nofloyd options.");

    if (!mapfileSpec)
        cmdlineP->mapFilespec = NULL;

    if (mapfileSpec) {
        if (argc-1 == 0)
            cmdlineP->inputFilespec = "-";
        else if (argc-1 != 1) 
            pm_error("When you specify the -mapfile option, the only "
                     "argument allowed is the input file specification.  "
                     "You specified %d arguments.", argc-1);
        else
            cmdlineP->inputFilespec = argv[1];
    } else {
        if (argc-1 > 2)
            pm_error("Program takes at most two arguments: number of colors "
                     "and input file specification.  "
                     "You specified %d arguments.", argc-1);
        else {
            if (argc-1 < 2)
                cmdlineP->inputFilespec = "-";
            else
                cmdlineP->inputFilespec = argv[2];

            if (argc-1 < 1)
                pm_error("You must specify the number of colors in the "
                         "output as an argument, or specify the -mapfile "
                         "option.");
            else {
                char * tail;
                long int const newcolors = strtol(argv[1], &tail, 10);
                if (*tail != '\0')
                    pm_error("The number of colors argument '%s' is not "
                             "a number.", argv[1]);
                else if (newcolors < 1)
                    pm_error("The number of colors must be positive");
                else if (newcolors == 1)
                    pm_error("The number of colors must be greater than 1.");
                else
                    cmdlineP->newcolors = newcolors;
            }
        }
    }
}



static int
redcompare( const void *ch1, const void *ch2 )
    {
    return (int) PPM_GETR( ((colorhist_vector)ch1)->color ) - 
      (int) PPM_GETR( ((colorhist_vector)ch2)->color );
    }

static int
greencompare( const void *ch1, const void *ch2 ) {
    return (int) PPM_GETG(((colorhist_vector) ch1)->color ) - 
      (int) PPM_GETG( ((colorhist_vector)ch2)->color );
}

static int
bluecompare(const void *ch1, const void *ch2 ) {
    return (int) PPM_GETB(((colorhist_vector)ch1)->color ) - 
      (int) PPM_GETB(((colorhist_vector)ch2)->color );
}

static int
sumcompare(const void *b1, const void *b2 ) {
    return(((box_vector)b2)->sum - ((box_vector)b1)->sum);
    }




/*
** Here is the fun part, the median-cut colormap generator.  This is based
** on Paul Heckbert's paper "Color Image Quantization for Frame Buffer
** Display", SIGGRAPH '82 Proceedings, page 297.
*/

static colorhist_vector
mediancut(colorhist_vector chv, int colors, int sum, pixval maxval, 
          int newcolors) {

    colorhist_vector colormap;
    box_vector bv;
    register int bi, i;
    int boxes;

    bv = (box_vector) malloc2( sizeof(struct box),  newcolors );
    colormap =
        (colorhist_vector) malloc2( sizeof(struct colorhist_item), newcolors );
    if ( bv == (box_vector) 0 || colormap == (colorhist_vector) 0 )
	pm_error( "out of memory" );
    for ( i = 0; i < newcolors; ++i )
	PPM_ASSIGN( colormap[i].color, 0, 0, 0 );

    /*
    ** Set up the initial box.
    */
    bv[0].ind = 0;
    bv[0].colors = colors;
    bv[0].sum = sum;
    boxes = 1;

    /*
    ** Main loop: split boxes until we have enough.
    */
    while ( boxes < newcolors )
    {
    register int indx, clrs;
    int sm;
    register int minr, maxr, ming, maxg, minb, maxb, v;
    int halfsum, lowersum;

    /*
    ** Find the first splittable box.
    */
    for ( bi = 0; bi < boxes; ++bi )
        if ( bv[bi].colors >= 2 )
        break;
    if ( bi == boxes )
        break;  /* ran out of colors! */
    indx = bv[bi].ind;
    clrs = bv[bi].colors;
    sm = bv[bi].sum;

    /*
    ** Go through the box finding the minimum and maximum of each
    ** component - the boundaries of the box.
    */
    minr = maxr = PPM_GETR( chv[indx].color );
    ming = maxg = PPM_GETG( chv[indx].color );
    minb = maxb = PPM_GETB( chv[indx].color );
    for ( i = 1; i < clrs; ++i )
        {
        v = PPM_GETR( chv[indx + i].color );
        if ( v < minr ) minr = v;
        if ( v > maxr ) maxr = v;
        v = PPM_GETG( chv[indx + i].color );
        if ( v < ming ) ming = v;
        if ( v > maxg ) maxg = v;
        v = PPM_GETB( chv[indx + i].color );
        if ( v < minb ) minb = v;
        if ( v > maxb ) maxb = v;
        }

    if (boxes == 1)
    /*
    ** Find the largest dimension, and sort by that component.  I have
    ** included two methods for determining the "largest" dimension;
    ** first by simply comparing the range in RGB space, and second
    ** by transforming into luminosities before the comparison.  You
    ** can switch which method is used by switching the commenting on
    ** the LARGE_ defines at the beginning of this source file.
    */
#ifdef LARGE_NORM
    if ( maxr - minr >= maxg - ming && maxr - minr >= maxb - minb )
        qsort(
        (char*) &(chv[indx]), clrs, sizeof(struct colorhist_item),
        redcompare );
    else if ( maxg - ming >= maxb - minb )
        qsort(
        (char*) &(chv[indx]), clrs, sizeof(struct colorhist_item),
        greencompare );
    else
        qsort(
        (char*) &(chv[indx]), clrs, sizeof(struct colorhist_item),
        bluecompare );
#endif /*LARGE_NORM*/
#ifdef LARGE_LUM
    {
    pixel p;
    float rl, gl, bl;

    PPM_ASSIGN(p, maxr - minr, 0, 0);
    rl = PPM_LUMIN(p);
    PPM_ASSIGN(p, 0, maxg - ming, 0);
    gl = PPM_LUMIN(p);
    PPM_ASSIGN(p, 0, 0, maxb - minb);
    bl = PPM_LUMIN(p);

    if ( rl >= gl && rl >= bl )
        qsort(
        (char*) &(chv[indx]), clrs, sizeof(struct colorhist_item),
        &redcompare );
    else if ( gl >= bl )
        qsort(
        (char*) &(chv[indx]), clrs, sizeof(struct colorhist_item),
        &greencompare );
    else
        qsort(
        (char*) &(chv[indx]), clrs, sizeof(struct colorhist_item),
        &bluecompare );
    }
#endif /*LARGE_LUM*/
    
    /*
    ** Now find the median based on the counts, so that about half the
    ** pixels (not colors, pixels) are in each subdivision.
    */
    lowersum = chv[indx].value;
    halfsum = sm / 2;
    for ( i = 1; i < clrs - 1; ++i )
        {
        if ( lowersum >= halfsum )
        break;
        lowersum += chv[indx + i].value;
        }

    /*
    ** Split the box, and sort to bring the biggest boxes to the top.
    */
    bv[bi].colors = i;
    bv[bi].sum = lowersum;
    bv[boxes].ind = indx + i;
    bv[boxes].colors = clrs - i;
    bv[boxes].sum = sm - lowersum;
    ++boxes;
    qsort( (char*) bv, boxes, sizeof(struct box), sumcompare );
    }

    /*
    ** Ok, we've got enough boxes.  Now choose a representative color for
    ** each box.  There are a number of possible ways to make this choice.
    ** One would be to choose the center of the box; this ignores any structure
    ** within the boxes.  Another method would be to average all the colors in
    ** the box - this is the method specified in Heckbert's paper.  A third
    ** method is to average all the pixels in the box.  You can switch which
    ** method is used by switching the commenting on the REP_ defines at
    ** the beginning of this source file.
    */
    for ( bi = 0; bi < boxes; ++bi )
    {
#ifdef REP_CENTER_BOX
    register int indx = bv[bi].ind;
    register int clrs = bv[bi].colors;
    register int minr, maxr, ming, maxg, minb, maxb, v;

    minr = maxr = PPM_GETR( chv[indx].color );
    ming = maxg = PPM_GETG( chv[indx].color );
    minb = maxb = PPM_GETB( chv[indx].color );
    for ( i = 1; i < clrs; ++i )
        {
        v = PPM_GETR( chv[indx + i].color );
        minr = min( minr, v );
        maxr = max( maxr, v );
        v = PPM_GETG( chv[indx + i].color );
        ming = min( ming, v );
        maxg = max( maxg, v );
        v = PPM_GETB( chv[indx + i].color );
        minb = min( minb, v );
        maxb = max( maxb, v );
        }
    PPM_ASSIGN(
        colormap[bi].color, ( minr + maxr ) / 2, ( ming + maxg ) / 2,
        ( minb + maxb ) / 2 );
#endif /*REP_CENTER_BOX*/
#ifdef REP_AVERAGE_COLORS
    register int indx = bv[bi].ind;
    register int clrs = bv[bi].colors;
    register long r = 0, g = 0, b = 0;

    for ( i = 0; i < clrs; ++i )
        {
        r += PPM_GETR( chv[indx + i].color );
        g += PPM_GETG( chv[indx + i].color );
        b += PPM_GETB( chv[indx + i].color );
        }
    r = r / clrs;
    g = g / clrs;
    b = b / clrs;
    PPM_ASSIGN( colormap[bi].color, r, g, b );
#endif /*REP_AVERAGE_COLORS*/
#ifdef REP_AVERAGE_PIXELS
    register int indx = bv[bi].ind;
    register int clrs = bv[bi].colors;
    register long r = 0, g = 0, b = 0, sum = 0;

    for ( i = 0; i < clrs; ++i )
        {
        r += PPM_GETR( chv[indx + i].color ) * chv[indx + i].value;
        g += PPM_GETG( chv[indx + i].color ) * chv[indx + i].value;
        b += PPM_GETB( chv[indx + i].color ) * chv[indx + i].value;
        sum += chv[indx + i].value;
        }
    r = r / sum;
    if ( r > maxval ) r = maxval;   /* avoid math errors */
    g = g / sum;
    if ( g > maxval ) g = maxval;
    b = b / sum;
    if ( b > maxval ) b = maxval;
    PPM_ASSIGN( colormap[bi].color, r, g, b );
#endif /*REP_AVERAGE_PIXELS*/
    }

    /*
    ** All done.
    */
    return colormap;
    }




static void
computeHistogram(pixel ** const pixels, int const cols, int const rows, 
                 pixval const inputMaxval, 
                 colorhist_vector * const chvP, int * const colorsP, 
                 pixval * const newmaxvalP) {
/*----------------------------------------------------------------------------
  Attempt to make a histogram of the colors, unclustered.  If at first
  we don't succeed, lower maxval to increase color coherence and try
  again.  This will eventually terminate, with maxval at worst 15,
  since 32^3 is approximately MAXCOLORS.
-----------------------------------------------------------------------------*/
    colorhist_vector chv;
    int colors;
    pixval maxval;

    chv = NULL;  /* initial value */
    maxval = inputMaxval;  /* initial value */

    while (chv == NULL)
    {
        pm_message( "making histogram..." );
        chv = ppm_computecolorhist(pixels, cols, rows, MAXCOLORS, &colors);
        if (chv == NULL) {
            pixval const newmaxval = maxval / 2;
            int row;

            pm_message("too many colors!");
            pm_message("scaling colors from maxval=%d to maxval=%d "
                       "to improve clustering...",
                       maxval, newmaxval );
            for (row = 0; row < rows; ++row) {
                int col;
                for (col = 0; col < cols; ++col)
                    PPM_DEPTH(pixels[row][col], pixels[row][col], 
                              maxval, newmaxval);
            }
            maxval = newmaxval;
        }
    }
    pm_message( "%d colors found", colors );
    *chvP = chv;
    *colorsP = colors;
    *newmaxvalP = maxval;
}




static void
computeColorMapFromInput(pixel ** const pixels, int const cols, int const rows,
                         pixval const maxval, int const newcolors,
                         colorhist_vector * const colormapP,
                         pixval * const newmaxvalP) {

    colorhist_vector chv;
    int colors;
    pixval newmaxval;

    computeHistogram(pixels, cols, rows, maxval, 
                     &chv, &colors, &newmaxval);
        /* May modify pixels[][] to reduce its maxval */

    pm_message("choosing %d colors...", newcolors);
    *colormapP = mediancut(chv, colors, rows * cols, newmaxval, newcolors);
    *newmaxvalP = newmaxval;
    ppm_freecolorhist(chv);
}



static void
computeColorMapFromMap(pixel ** const pixels, int const cols, int const rows,
                       pixval const maxval, 
                       pixel ** const mappixels, 
                       int const mapcols, int const maprows, 
                       pixval const mapmaxval,
                       colorhist_vector * const colormapP,
                       int * const newcolorsP) {
    
    int colors; 

    if (mapmaxval != maxval) {
        int row;
        pm_message("rescaling input image to match colormap maxval");
        for (row = 0; row < rows; ++row) {
            int col;
            for (col = 0; col < cols; ++col)
                PPM_DEPTH(pixels[row][col], pixels[row][col], 
                          maxval, mapmaxval);
        }
    }
    *colormapP = ppm_computecolorhist(mappixels, mapcols, maprows, 
                                      MAXCOLORS, &colors);
    if (*colormapP == NULL)
        pm_error("too many colors in colormap!");
    pm_message("%d colors found in colormap", colors);
    *newcolorsP = colors;
}



static void
initFserr(struct fserr * const fserrP, int const cols) {
/*----------------------------------------------------------------------------
   Initialize the Floyd-Steinberg error vectors
-----------------------------------------------------------------------------*/
    int col;

    fserrP->thisrerr = (long*) pm_allocrow( cols + 2, sizeof(long) );
    fserrP->nextrerr = (long*) pm_allocrow( cols + 2, sizeof(long) );
    fserrP->thisgerr = (long*) pm_allocrow( cols + 2, sizeof(long) );
    fserrP->nextgerr = (long*) pm_allocrow( cols + 2, sizeof(long) );
    fserrP->thisberr = (long*) pm_allocrow( cols + 2, sizeof(long) );
    fserrP->nextberr = (long*) pm_allocrow( cols + 2, sizeof(long) );

    srand( (int) ( time( 0 ) ^ getpid( ) ) );

    for (col = 0; col < cols + 2; ++col) {
        fserrP->thisrerr[col] = rand( ) % ( FS_SCALE * 2 ) - FS_SCALE;
        fserrP->thisgerr[col] = rand( ) % ( FS_SCALE * 2 ) - FS_SCALE;
        fserrP->thisberr[col] = rand( ) % ( FS_SCALE * 2 ) - FS_SCALE;
        /* (random errors in [-1 .. 1]) */
    }
    fserrP->fsForward = TRUE;
}



static void
floydInitRow(struct fserr * const fserrP, int const cols) {
    int col;
    
    for ( col = 0; col < cols + 2; ++col ) {
        fserrP->nextrerr[col] = 0;
        fserrP->nextgerr[col] = 0;
        fserrP->nextberr[col] = 0;
    }
}



static void
floydAdjustColor(pixel * const pixelP, pixval const maxval, 
                 struct fserr * const fserrP, int const col) {
/*----------------------------------------------------------------------------
  Use Floyd-Steinberg errors to adjust actual color.
-----------------------------------------------------------------------------*/
    long int sr, sg, sb;

    sr = PPM_GETR(*pixelP) + fserrP->thisrerr[col + 1] / FS_SCALE;
    sg = PPM_GETG(*pixelP) + fserrP->thisgerr[col + 1] / FS_SCALE;
    sb = PPM_GETB(*pixelP) + fserrP->thisberr[col + 1] / FS_SCALE;

    if (sr < 0) 
        sr = 0;
    else if (sr > maxval) 
        sr = maxval;
    if (sg < 0) 
        sg = 0;
    else if (sg > maxval) 
        sg = maxval;
    if (sb < 0) 
        sb = 0;
    else if (sb > maxval) 
        sb = maxval;

    PPM_ASSIGN(*pixelP, sr, sg, sb);
}



static void
floydPropagateErr(struct fserr * const fserrP, int const col, 
                  pixel const oldpixel, pixel const newpixel) {
/*----------------------------------------------------------------------------
  Propagate Floyd-Steinberg error terms.
-----------------------------------------------------------------------------*/

    if (fserrP->fsForward) {
        {
            long const err = 
                (PPM_GETR(oldpixel) - PPM_GETR(newpixel)) * FS_SCALE;
            fserrP->thisrerr[col + 2] += ( err * 7 ) / 16;
            fserrP->nextrerr[col    ] += ( err * 3 ) / 16;
            fserrP->nextrerr[col + 1] += ( err * 5 ) / 16;
            fserrP->nextrerr[col + 2] += ( err     ) / 16;
        }
        {
            long const err = 
                (PPM_GETG(oldpixel) - PPM_GETG(newpixel)) * FS_SCALE;
            fserrP->thisgerr[col + 2] += ( err * 7 ) / 16;
            fserrP->nextgerr[col    ] += ( err * 3 ) / 16;
            fserrP->nextgerr[col + 1] += ( err * 5 ) / 16;
            fserrP->nextgerr[col + 2] += ( err     ) / 16;
        }
        {
            long const err = 
                (PPM_GETB(oldpixel) - PPM_GETB(newpixel)) * FS_SCALE;
            fserrP->thisberr[col + 2] += ( err * 7 ) / 16;
            fserrP->nextberr[col    ] += ( err * 3 ) / 16;
            fserrP->nextberr[col + 1] += ( err * 5 ) / 16;
            fserrP->nextberr[col + 2] += ( err     ) / 16;
        }
    } else {
        {
            long const err = 
                (PPM_GETR(oldpixel) - PPM_GETR(newpixel)) * FS_SCALE;
            fserrP->thisrerr[col    ] += ( err * 7 ) / 16;
            fserrP->nextrerr[col + 2] += ( err * 3 ) / 16;
            fserrP->nextrerr[col + 1] += ( err * 5 ) / 16;
            fserrP->nextrerr[col    ] += ( err     ) / 16;
        }
        {
            long const err = 
                (PPM_GETG(oldpixel) - PPM_GETG(newpixel)) * FS_SCALE;
            fserrP->thisgerr[col    ] += ( err * 7 ) / 16;
            fserrP->nextgerr[col + 2] += ( err * 3 ) / 16;
            fserrP->nextgerr[col + 1] += ( err * 5 ) / 16;
            fserrP->nextgerr[col    ] += ( err     ) / 16;
        }
        {
            long const err = 
                (PPM_GETB(oldpixel) - PPM_GETB(newpixel)) * FS_SCALE;
            fserrP->thisberr[col    ] += ( err * 7 ) / 16;
            fserrP->nextberr[col + 2] += ( err * 3 ) / 16;
            fserrP->nextberr[col + 1] += ( err * 5 ) / 16;
            fserrP->nextberr[col    ] += ( err     ) / 16;
        }
    }
}



static void
floydSwitchDir(struct fserr * const fserrP) {

    {
        long * const temperr = fserrP->thisrerr;
        fserrP->thisrerr = fserrP->nextrerr;
        fserrP->nextrerr = temperr;
    }
    {
        long * const temperr = fserrP->thisgerr;
        fserrP->thisgerr = fserrP->nextgerr;
        fserrP->nextgerr = temperr;
    }
    {
        long * const temperr = fserrP->thisberr;
        fserrP->thisberr = fserrP->nextberr;
        fserrP->nextberr = temperr;
        fserrP->fsForward = ! fserrP->fsForward;
    }
}



static void
searchColormap(colorhist_vector const colormap, int const newcolors,
               pixel const pixel, pixval const maxval,
               int * const colormapIndexP) {
/*----------------------------------------------------------------------------
   Search colormap 'colormap', which contains 'newcolors' colors (at
   least 1) for the color closest to that of pixel 'pixel'.  Return
   its index as *colormapIndexP.
-----------------------------------------------------------------------------*/
    int i, r1, g1, b1;
    unsigned long dist;
    /* The closest distance we've found so far between the color of
       pixel *pP and a color in the colormap.  This is measured as
       the square of the cartesian distance between the colors,
       except that if maxval > 255, it's divided by 16 so that it
       will fit in a 32 bit word.  Note that each sample can be up
       to 65535 (PPM_OVERALLMAXVAL).
    */
    r1 = PPM_GETR(pixel);
    g1 = PPM_GETG(pixel);
    b1 = PPM_GETB(pixel);
    dist = ULONG_MAX;  /* initial value */
    for (i = 0; i < newcolors; ++i) {
        unsigned long newdist;  /* candidate for new 'dist' value */
        int r2, g2, b2;
        r2 = PPM_GETR(colormap[i].color);
        g2 = PPM_GETG(colormap[i].color);
        b2 = PPM_GETB(colormap[i].color);
        if (maxval < 256)
            newdist = sqr(r1-r2) + sqr(g1-g2) + sqr(b1-b2);
        else
            newdist = 
                sqr(r1-r2) / 16 +
                sqr(g1-g2) / 16 +
                sqr(b1-b2) / 16;
        if (newdist < dist) {
            *colormapIndexP = i;
            dist = newdist;
        }
    }
    if (usehash) {
        if (ppm_addtocolorhash(cht, &pixel, *colormapIndexP) < 0) {
            pm_message("out of memory adding to hash table; "
                       "proceeding without it");
            usehash = FALSE;
        }
    }
}



static void
convertRow(pixel pixelrow[], int const cols, pixval const maxval, 
           colorhist_vector const colormap, int const newcolors,
           bool const floyd, struct fserr * const fserrP) {
/*----------------------------------------------------------------------------
   Replace the colors in row pixelrow[] with the new colors.

   Use and update the Floyd-Steinberg state *fserrP.
-----------------------------------------------------------------------------*/
    int col;
    int limitcol;
    int ind;
    
    if (floyd)
        floydInitRow(fserrP, cols);

    if ((!floyd) || fserrP->fsForward) {
        col = 0;
        limitcol = cols;
    } else {
        col = cols - 1;
        limitcol = -1;
    }
    do {
        if (floyd) 
            floydAdjustColor(&pixelrow[col], maxval, fserrP, col);

        /* Check hash table to see if we have already matched this color. */
        ind = ppm_lookupcolor(cht, &pixelrow[col]);
        if (ind == -1) 
            /* No, have to do a full lookup */
            searchColormap(colormap, newcolors, pixelrow[col], maxval, &ind);

        if (floyd)
            floydPropagateErr(fserrP, col, pixelrow[col], colormap[ind].color);

        pixelrow[col] = colormap[ind].color;

        if (floyd && !fserrP->fsForward) 
            --col;
        else
            ++col;
    } while (col != limitcol);

    if (floyd)
        floydSwitchDir(fserrP);
}



int
main(int argc, char * argv[] ) {

    struct cmdlineInfo cmdline;
    FILE* ifp;
    pixel** pixels;
    int rows, cols;
    pixval inputMaxval;
    pixval maxval;
    int newcolors;
    colorhist_vector colormap;
    struct fserr fserr;

    ppm_init(&argc, argv);
 
    parseCommandLine(argc, argv, &cmdline);

    ifp = pm_openr(cmdline.inputFilespec);

    pixels = ppm_readppm(ifp, &cols, &rows, &inputMaxval);

    pm_close(ifp);


    if (cmdline.mapFilespec) {
        FILE *mapfile;
        int mapcols, maprows;
        pixval mapmaxval;
        pixel ** mappixels;

        mapfile = pm_openr(cmdline.mapFilespec);
        mappixels = ppm_readppm(mapfile, &mapcols, &maprows, &mapmaxval);
        pm_close(mapfile);

        if (mapcols == 0 || maprows == 0)
            pm_error("colormap in file '%s' contains no pixels", 
                     cmdline.mapFilespec);

        computeColorMapFromMap(pixels, cols, rows, inputMaxval,
                               mappixels, mapcols, maprows, mapmaxval,
                               &colormap, &newcolors);
        maxval = mapmaxval;
        ppm_freearray(mappixels, maprows);
    } else { 
        newcolors = cmdline.newcolors;
        computeColorMapFromInput(pixels, cols, rows, inputMaxval, newcolors,
                                 &colormap, &maxval);
    }

    ppm_writeppminit(stdout, cols, rows, maxval, 0);

    pm_message( "mapping image to new colors..." );

    /* Initialize convertRow() */
    cht = ppm_alloccolorhash();
    usehash = TRUE;

    if (cmdline.floyd)
        initFserr(&fserr, cols);

    {
        int row;
        for (row = 0; row < rows; ++row) {
            convertRow(pixels[row], cols, maxval, colormap, newcolors, 
                       cmdline.floyd, &fserr);
            
            ppm_writeppmrow(stdout, pixels[row], cols, maxval, 0);
        }
    }
    ppm_freecolorhash(cht);

    pm_close(stdout);

    exit(0);
}

