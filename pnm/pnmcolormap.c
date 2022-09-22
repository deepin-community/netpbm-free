/******************************************************************************
                               pnmcolormap.c
*******************************************************************************

  Create a colormap file (a PPM image containing one pixel of each of a set
  of colors).  Base the set of colors on an input image.

  For PGM input, do the equivalent for grayscale and produce a PGM graymap.

  By Bryan Henderson, San Jose, CA 2001.12.17

  Derived from ppmquant, originally by Jef Poskanzer.

  Copyright (C) 1989, 1991 by Jef Poskanzer.
  Copyright (C) 2001 by Bryan Henderson.

  Permission to use, copy, modify, and distribute this software and its
  documentation for any purpose and without fee is hereby granted, provided
  that the above copyright notice appear in all copies and that both that
  copyright notice and this permission notice appear in supporting
  documentation.  This software is provided "as is" without express or
  implied warranty.

******************************************************************************/

#include <string.h>
#include <math.h>

#include "pam.h"
#include "pammap.h"

#define MAXCOLORS 32767u
#define MAXDEPTH 3

enum methodForLargest {LARGE_NORM, LARGE_LUM};

enum methodForRep {REP_CENTER_BOX, REP_AVERAGE_COLORS, REP_AVERAGE_PIXELS};

typedef struct box* boxVector;
struct box {
    int ind;
    int colors;
    int sum;
};

struct cmdlineInfo {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
    char *inputFilespec;  /* Filespec of input file */
    unsigned int allcolors;  /* boolean: select all colors from the input */
    unsigned int newcolors;    
        /* Number of colors argument; meaningless if allcolors true */
    enum methodForLargest methodForLargest; 
        /* -spreadintensity/-spreadluminosity options */
    enum methodForRep methodForRep;
        /* -center/-meancolor/-meanpixel options */
    unsigned int sort;
    unsigned int square;
    unsigned int verbose;
};



static void
parseCommandLine (int argc, char ** argv,
                  struct cmdlineInfo *cmdlineP) {
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

    unsigned int spreadbrightness, spreadluminosity;
    unsigned int center, meancolor, meanpixel;

    option_def_index = 0;   /* incremented by OPTENT3 */
    OPTENT3(0,   "spreadbrightness", OPT_FLAG,   
            NULL,                       &spreadbrightness, 0);
    OPTENT3(0,   "spreadluminosity", OPT_FLAG,   
            NULL,                       &spreadluminosity, 0);
    OPTENT3(0,   "center",           OPT_FLAG,   
            NULL,                       &center,           0);
    OPTENT3(0,   "meancolor",        OPT_FLAG,   
            NULL,                       &meancolor,        0);
    OPTENT3(0,   "meanpixel",        OPT_FLAG,   
            NULL,                       &meanpixel,        0);
    OPTENT3(0, "sort",     OPT_FLAG,   NULL,                  
            &cmdlineP->sort,       0 );
    OPTENT3(0, "square",   OPT_FLAG,   NULL,                  
            &cmdlineP->square,       0 );
    OPTENT3(0, "verbose",   OPT_FLAG,   NULL,                  
            &cmdlineP->verbose,       0 );

    opt.opt_table = option_def;
    opt.short_allowed = FALSE;  /* We have no short (old-fashioned) options */
    opt.allowNegNum = FALSE;  /* We have no parms that are negative numbers */

    pm_optParseOptions3( &argc, argv, opt, sizeof(opt), 0 );
        /* Uses and sets argc, argv, and some of *cmdline_p and others. */


    if (spreadbrightness && spreadluminosity) 
        pm_error("You cannot specify both -spreadbrightness and "
                 "spreadluminosity.");
    if (spreadluminosity)
        cmdlineP->methodForLargest = LARGE_LUM;
    else
        cmdlineP->methodForLargest = LARGE_NORM;

    if (center + meancolor + meanpixel > 1)
        pm_error("You can specify only one of -center, -meancolor, and "
                 "-meanpixel.");
    if (meancolor)
        cmdlineP->methodForRep = REP_AVERAGE_COLORS;
    else if (meanpixel) 
        cmdlineP->methodForRep = REP_AVERAGE_PIXELS;
    else
        cmdlineP->methodForRep = REP_CENTER_BOX;

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
                     "output as an argument.");
        else {
            if (strcmp(argv[1], "all") == 0)
                cmdlineP->allcolors = TRUE;
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
                else {
                    cmdlineP->newcolors = newcolors;
                    cmdlineP->allcolors = FALSE;
                }
            }
        }
    }
}
static int __inline__
compareplane(struct tupleint ** const comparandP, 
             struct tupleint ** const comparatorP,
             unsigned int       const plane) {
    return (*comparandP)->tuple[plane] - (*comparatorP)->tuple[plane];
}

static int
compareplane0(const void * const comparandP, const void * const comparatorP) {
    return compareplane((struct tupleint **)comparandP,
                        (struct tupleint **)comparatorP,
                        0);
}

static int
compareplane1(const void * const comparandP, const void * const comparatorP) {
    return compareplane((struct tupleint **)comparandP,
                        (struct tupleint **)comparatorP,
                        1);
}

static int
compareplane2(const void * const comparandP, const void * const comparatorP) {
    return compareplane((struct tupleint **)comparandP,
                        (struct tupleint **)comparatorP,
                        2);
}



typedef int qsort_comparison_fn(const void *, const void *);

/* This ought to be an array of functions, but the compiler complains at
   that for some reason.
*/
static struct {qsort_comparison_fn * fn;} samplecompare[MAXDEPTH];

static void
initSampleCompare(void) {
    samplecompare[0].fn = compareplane0;
    samplecompare[1].fn = compareplane1;
    samplecompare[2].fn = compareplane2;
}



static int
sumcompare(const void * const b1, const void * const b2) {
    return(((boxVector)b2)->sum - ((boxVector)b1)->sum);
}




/*
** Here is the fun part, the median-cut colormap generator.  This is based
** on Paul Heckbert's paper "Color Image Quantization for Frame Buffer
** Display", SIGGRAPH '82 Proceedings, page 297.
*/

static tupletable
newColorMap(const struct pam * const pamP, 
            unsigned int       const newcolors) {

    tupletable colormap;
    unsigned int i;

    colormap = pnm_alloctupletable(pamP, newcolors);

    for (i = 0; i < newcolors; ++i) {
        unsigned int plane;
        for (plane = 0; plane < pamP->depth; ++plane) 
            colormap[i]->tuple[plane] = 0;
    }
    return colormap;
}



static boxVector
newBoxVector(int const colors, int const sum, int const newcolors) {

    boxVector bv;
    bv = (boxVector) malloc2(sizeof(struct box), newcolors);
    if (bv == NULL)
        pm_error("out of memory allocating box vector table");

    /* Set up the initial box. */
    bv[0].ind = 0;
    bv[0].colors = colors;
    bv[0].sum = sum;

    return bv;
}



static void
findBoxBoundaries(struct pam * const pamP, 
                  tupletable   const colorfreqtable,
                  unsigned int const indx,
                  unsigned int const clrs,
                  sample             minval[],
                  sample             maxval[]) {
/*----------------------------------------------------------------------------
  Go through the box finding the minimum and maximum of each
  component - the boundaries of the box.
-----------------------------------------------------------------------------*/
    unsigned int plane;
    unsigned int i;

    for (plane = 0; plane < pamP->depth; ++plane) 
        minval[plane] = maxval[plane] = colorfreqtable[indx]->tuple[plane];
    
    for (i = 1; i < clrs; ++i) {
        unsigned int plane;
        for (plane = 0; plane < pamP->depth; ++plane) {
            sample const v = colorfreqtable[indx + i]->tuple[plane];
            if (v < minval[plane]) minval[plane] = v;
            if (v > maxval[plane]) maxval[plane] = v;
        } 
    }
}



static unsigned int
largestByNorm(struct pam * const pamP, sample minval[], sample maxval[]) {
    
    unsigned int largestDimension;
    unsigned int plane;

    sample largestSpreadSoFar = 0;  
    largestDimension = 0;
    for (plane = 0; plane < pamP->depth; ++plane) {
        sample const spread = maxval[plane]-minval[plane];
        if (spread > largestSpreadSoFar) {
            largestDimension = plane;
            largestSpreadSoFar = spread;
        }
    }
    return largestDimension;
}



static unsigned int 
largestByLuminosity(struct pam * const pamP, sample minval[], sample maxval[]) {
/*----------------------------------------------------------------------------
   This subroutine presumes that the tuple type is either 
   BLACKANDWHITE, GRAYSCALE, or RGB (which implies pamP->depth is 1 or 3).
   To save time, we don't actually check it.
-----------------------------------------------------------------------------*/
    unsigned int retval;

    if (pamP->depth == 1)
        retval = 0;
    else {
        /* An RGB tuple */
        unsigned int largestDimension;
        unsigned int plane;
        double largestSpreadSoFar;

        largestSpreadSoFar = 0.0;

        for (plane = 0; plane < 3; ++plane) {
            double const spread = 
                pnm_lumin_factor[plane] * (maxval[plane]-minval[plane]);
            if (spread > largestSpreadSoFar) {
                largestDimension = plane;
                largestSpreadSoFar = spread;
            }
        }
        retval = largestDimension;
    }
    return retval;
}



static void
centerBox(struct pam * const pamP, 
          int          const indx,
          int          const clrs,
          tupletable   const colorfreqtable, 
          tuple        const newTuple) {

    unsigned int plane;

    for (plane = 0; plane < pamP->depth; ++plane) {
        int minval, maxval;
        unsigned int i;

        minval = maxval = colorfreqtable[indx]->tuple[plane];
        
        for (i = 1; i < clrs; ++i) {
            int const v = colorfreqtable[indx + i]->tuple[plane];
            minval = min( minval, v);
            maxval = max( maxval, v);
        }
        newTuple[plane] = (minval + maxval) / 2;
    }
}



static void
averageColors(struct pam * const pamP, 
              int          const indx,
              int          const clrs,
              tupletable   const colorfreqtable, 
              tuple        const newTuple) {

    unsigned int plane;

    for (plane = 0; plane < pamP->depth; ++plane) {
        sample sum;
        int i;

        sum = 0;

        for (i = 0; i < clrs; ++i) 
            sum += colorfreqtable[indx+i]->tuple[plane];

        newTuple[plane] = sum / clrs;
    }
}



static void
averagePixels(struct pam * const pamP, 
              int          const indx,
              int          const clrs,
              tupletable   const colorfreqtable, 
              tuple        const newTuple) {

    unsigned int n;
        /* Number of tuples represented by the box */
    unsigned int plane;
    unsigned int i;

    /* Count the tuples in question */
    n = 0;  /* initial value */
    for (i = 0; i < clrs; ++i)
        n += colorfreqtable[indx + i]->value;


    for (plane = 0; plane < pamP->depth; ++plane) {
        sample sum;
        int i;

        sum = 0;

        for (i = 0; i < clrs; ++i) 
            sum += colorfreqtable[indx+i]->tuple[plane]
                * colorfreqtable[indx+i]->value;

        newTuple[plane] = sum / n;
    }
}



static tupletable
colormapFromBv(struct pam *      const pamP, 
               unsigned int      const newcolors,
               boxVector         const bv, 
               unsigned int      const boxes,
               tupletable        const colorfreqtable, 
               enum methodForRep const methodForRep) {
    /*
    ** Ok, we've got enough boxes.  Now choose a representative color for
    ** each box.  There are a number of possible ways to make this choice.
    ** One would be to choose the center of the box; this ignores any structure
    ** within the boxes.  Another method would be to average all the colors in
    ** the box - this is the method specified in Heckbert's paper.  A third
    ** method is to average all the pixels in the box. 
    */
    tupletable colormap;
    unsigned int bi;

    colormap = newColorMap(pamP, newcolors);

    for (bi = 0; bi < boxes; ++bi) {
        switch (methodForRep) {
        case REP_CENTER_BOX: 
            centerBox(pamP, bv[bi].ind, bv[bi].colors, colorfreqtable, 
                      colormap[bi]->tuple);
            break;
        case REP_AVERAGE_COLORS:
            averageColors(pamP, bv[bi].ind, bv[bi].colors, colorfreqtable,
                          colormap[bi]->tuple);
            break;
        case REP_AVERAGE_PIXELS:
            averagePixels(pamP, bv[bi].ind, bv[bi].colors, colorfreqtable,
                          colormap[bi]->tuple);
            break;
        default:
            pm_error("Internal error: invalid value of methodForRep: %d",
                     methodForRep);
        }
    }
    return colormap;
}



static tupletable
mediancut(struct pam * const pamP,
          tupletable   const colorfreqtable, 
          int          const colors, 
          int          const newcolors,
          enum methodForLargest const methodForLargest,
          enum methodForRep     const methodForRep) {
/*----------------------------------------------------------------------------
   If *pamP describes an image with 'colors' distinct tuple values in it,
   and colorfreqtable[] gives how many times each of those tuple values
   occurs, compute a set of only 'newcolors' colors that are close to the
   colors in colorfreqtable[].

   As a side effect, sort 'colorfreqtable'.
-----------------------------------------------------------------------------*/

    tupletable colormap;
    boxVector bv;
    int bi;
    int boxes;

    bv = newBoxVector(colors, pamP->width*pamP->height, newcolors);
    boxes = 1;


    /* Main loop: split boxes until we have enough. */
    while (boxes < newcolors) {
        unsigned int indx;
        unsigned int clrs;
        unsigned int sm;
        sample minval[MAXDEPTH];
        sample maxval[MAXDEPTH];
        unsigned int largestDimension;
            /* number of the plane with the largest spread */
        unsigned int medianIndex;
        int lowersum;
            /* Number of pixels whose value is "less than" the median */

        /* Find the first splittable box. */
        for (bi = 0; bi < boxes && bv[bi].colors < 2; ++bi);
        if (bi >= boxes)
            break;  /* ran out of colors! */
        indx = bv[bi].ind;
        clrs = bv[bi].colors;
        sm = bv[bi].sum;

        findBoxBoundaries(pamP, colorfreqtable, indx, clrs, minval, maxval);

        /*
        ** Find the largest dimension, and sort by that component.  I have
        ** included two methods for determining the "largest" dimension;
        ** first by simply comparing the range in RGB space, and second
        ** by transforming into luminosities before the comparison. 
        */
        switch (methodForLargest) {
        case LARGE_NORM: 
            largestDimension = largestByNorm(pamP, minval, maxval);
            break;
        case LARGE_LUM: 
            largestDimension = largestByLuminosity(pamP, minval, maxval);
            break;
        }
                                                    
        /* TODO: I think this sort should go after creating a box, not before
           splitting.  Because you need the sort to use the REP_CENTER_BOX
           method of choosing a color to represent the final boxes 
        */
        qsort((char*) &colorfreqtable[indx], clrs, 
              sizeof(colorfreqtable[indx]), 
              samplecompare[largestDimension].fn);

        {
            /* Now find the median based on the counts, so that about half the
               pixels (not colors, pixels) are in each subdivision.
            */
            unsigned int i;

            lowersum = colorfreqtable[indx]->value;  /* initial value */
            for (i = 1; i < clrs - 1 && lowersum < sm/2; ++i) {
                lowersum += colorfreqtable[indx + i]->value;
            }
            medianIndex = i;
        }
        /* Split the box, and sort to bring the biggest boxes to the top. */
        bv[bi].colors = medianIndex;
        bv[bi].sum = lowersum;
        bv[boxes].ind = indx + medianIndex;
        bv[boxes].colors = clrs - medianIndex;
        bv[boxes].sum = sm - lowersum;
        ++boxes;
        qsort((char*) bv, boxes, sizeof(struct box), sumcompare);
    }

    colormap = colormapFromBv(pamP, newcolors, bv, boxes, colorfreqtable, 
                              methodForRep);

    free(bv);
    return colormap;
}




static void
computeHistogram(struct pam *   const pamP, 
                 tuple **       const tuples,
                 tupletable *   const colorfreqtableP, 
                 unsigned int * const colorsP) {
/*----------------------------------------------------------------------------
  Attempt to make a histogram of the colors, unclustered.  If at first
  we don't succeed, lower maxval to increase color coherence and try
  again.  This will eventually terminate, with maxval at worst 15,
  since 32^3 is approximately MAXCOLORS.

  We actually change the maxval of the input 'tuples' and update the input
  *pamP to reflect the new maxval.
-----------------------------------------------------------------------------*/
    tupletable colorfreqtable;
    unsigned int colors;

    colorfreqtable = NULL;  /* initial value */
    
    while (colorfreqtable == NULL)
    {
        pm_message( "making histogram..." );
        colorfreqtable = pnm_computetuplefreqtable(pamP, tuples, 
                                                   MAXCOLORS, &colors);
        if (colorfreqtable == NULL) {
            sample const newmaxval = pamP->maxval / 2;
            int row;

            pm_message("too many colors!");
            pm_message("scaling colors from maxval=%ld to maxval=%ld "
                       "to improve clustering...",
                       pamP->maxval, newmaxval );
            for (row = 0; row < pamP->height; ++row) {
                int col;
                for (col = 0; col < pamP->width; ++col)
                    pnm_scaletuple(pamP, tuples[row][col], tuples[row][col], 
                                   newmaxval);
            }
            pamP->maxval = newmaxval;
        }
    }
    pm_message( "%d colors found", colors );
    *colorfreqtableP = colorfreqtable;
    *colorsP = colors;
}




static void
computeColorMapFromInput(struct pam * const pamP, tuple ** const tuples,
                         bool const allColors, int const reqColors, 
                         enum methodForLargest const methodForLargest,
                         enum methodForRep const methodForRep,
                         tupletable * const colormapP,
                         int * const colorsP) {
/*----------------------------------------------------------------------------
   Produce a colormap containing the colors that we will use in the output.
   Figure it out using the median cut technique, based on the colors (and
   their frequencies) in the input image described by *pamP and tuples[][].

   The output map will have 'reqcolors' or fewer colors in it, unless
   'allcolors' is true, in which case it will have all the colors that are
   in the input.

   Modify the maxval of 'tuples', and update *pamP to reflect it, if 
   necessary.

   Put the colormap in newly allocated storage as a tupletable 
   and return its address as *colormapP.
-----------------------------------------------------------------------------*/
    tupletable colorfreqtable;
    unsigned int colors;

    computeHistogram(pamP, tuples, &colorfreqtable, &colors);
       /* May modify tuples[][]/pamP to reduce the maxval */
    
    if (allColors) {
        *colormapP = colorfreqtable;
        *colorsP = colors;
    } else {
        if (colors <= reqColors) {
            pm_message("Image already has few enough colors (<=%d).  "
                       "Keeping same colors.", reqColors);
            *colormapP = colorfreqtable;
            *colorsP = colors;
        } else {
            pm_message("choosing %d colors...", reqColors);
            *colormapP = mediancut(pamP, colorfreqtable, colors, reqColors,
                                   methodForLargest, methodForRep);
            *colorsP = reqColors;
            pnm_freetupletable(pamP, colorfreqtable);
        }
    }
}



static void
sortColormap(tupletable   const colormap, 
             sample       const depth, 
             unsigned int const colormapSize) {
/*----------------------------------------------------------------------------
   Sort the colormap in place, in order of ascending Plane 0 value, 
   the Plane 1 value, etc.

   Use insertion sort.
-----------------------------------------------------------------------------*/
    int i;
    
    pm_message("Sorting %d colors...", colormapSize);

    for (i = 0; i < colormapSize; ++i) {
        int j;
        for (j = i+1; j < colormapSize; ++j) {
            unsigned int plane;
            bool iIsGreater, iIsLess;

            iIsGreater = FALSE; iIsLess = FALSE;
            for (plane = 0; 
                 plane < depth && !iIsGreater && !iIsLess; 
                 ++plane) {
                if (colormap[i]->tuple[plane] > colormap[j]->tuple[plane])
                    iIsGreater = TRUE;
                else if (colormap[i]->tuple[plane] < colormap[j]->tuple[plane])
                    iIsLess = TRUE;
            }            
            if (iIsGreater) {
                for (plane = 0; plane < depth; ++plane) {
                    sample const temp = colormap[i]->tuple[plane];
                    colormap[i]->tuple[plane] = colormap[j]->tuple[plane];
                    colormap[j]->tuple[plane] = temp;
                }
            }
        }    
    }
}



static void 
colormapToSquare(struct pam * const pamP,
                 tupletable   const colormap,
                 unsigned int const colormapSize, 
                 tuple ***    const outputRasterP) {
    {
        unsigned int const intsqrt = (int)sqrt((float)colormapSize);
        if (intsqrt * intsqrt == colormapSize) 
            pamP->width = intsqrt;
        else 
            pamP->width = intsqrt + 1;
    }
    {
        unsigned int const intQuotient = colormapSize / pamP->width;
        if (pamP->width * intQuotient == colormapSize)
            pamP->height = intQuotient;
        else
            pamP->height = intQuotient + 1;
    }
    {
        tuple ** outputRaster;
        unsigned int row;
        unsigned int colormapIndex;
        
        outputRaster = pnm_allocpamarray(pamP);

        colormapIndex = 0;  /* initial value */
        
        for (row = 0; row < pamP->height; ++row) {
            unsigned int col;
            for (col = 0; col < pamP->width; ++col) {
                unsigned int plane;
                for (plane = 0; plane < pamP->depth; ++plane) {
                    outputRaster[row][col][plane] = 
                        colormap[colormapIndex]->tuple[plane];
                }
                if (colormapIndex < colormapSize-1)
                    colormapIndex++;
            }
        }
        *outputRasterP = outputRaster;
    } 
}



static void 
colormapToSingleRow(struct pam * const pamP,
                    tupletable   const colormap,
                    unsigned int const colormapSize, 
                    tuple ***    const outputRasterP) {

    tuple ** outputRaster;
    unsigned int col;
    
    pamP->width = colormapSize;
    pamP->height = 1;
    
    outputRaster = pnm_allocpamarray(pamP);
    
    for (col = 0; col < pamP->width; ++col) {
        int plane;
        for (plane = 0; plane < pamP->depth; ++plane)
            outputRaster[0][col][plane] = colormap[col]->tuple[plane];
    }
    *outputRasterP = outputRaster;
}



static void
colormapToImage(struct pam * const inpamP, 
                struct pam * const outpamP, 
                tupletable   const colormap,
                unsigned int const colormapSize, 
                bool         const sort,
                bool         const square,
                tuple ***    const outputRasterP) {
/*----------------------------------------------------------------------------
   Create a tuple array and pam structure for an image which includes
   one pixel of each of the colors in the colormap 'colormap'.

   May rearrange the contents of 'colormap'.
-----------------------------------------------------------------------------*/
    outpamP->size = sizeof(*outpamP);
    outpamP->len = sizeof(*outpamP);
    outpamP->format = inpamP->format;
    outpamP->plainformat = FALSE;
    outpamP->depth = inpamP->depth;
    outpamP->maxval = inpamP->maxval;
    outpamP->bytes_per_sample = inpamP->bytes_per_sample;
    strcpy(outpamP->tuple_type, inpamP->tuple_type);

    if (sort)
        sortColormap(colormap, outpamP->depth, colormapSize);

    if (square) 
        colormapToSquare(outpamP, colormap, colormapSize, outputRasterP);
    else 
        colormapToSingleRow(outpamP, colormap, colormapSize, outputRasterP);
}



int
main(int argc, char * argv[] ) {

    struct cmdlineInfo cmdline;
    FILE* ifp;
    struct pam inpam, outpam;
    tuple** tuples;
    tuple** colormapRaster;
    int newcolors;
    tupletable colormap;

    pnm_init(&argc, argv);

    initSampleCompare();
 
    parseCommandLine(argc, argv, &cmdline);

    ifp = pm_openr(cmdline.inputFilespec);

    tuples = pnm_readpam(ifp, &inpam, sizeof(inpam));
    
    pm_close(ifp);

    computeColorMapFromInput(&inpam, tuples, 
                             cmdline.allcolors, cmdline.newcolors, 
                             cmdline.methodForLargest, 
                             cmdline.methodForRep,
                             &colormap, &newcolors);
        /* May modify maxval of 'tuples', inpam */

    pnm_freepamarray(tuples, &inpam);

    colormapToImage(&inpam, &outpam, colormap, newcolors, cmdline.sort,
                    cmdline.square, &colormapRaster);

    if (cmdline.verbose)
        pm_message("Generating %u x %u image", outpam.width, outpam.height);

    outpam.file = stdout;
    
    pnm_writepam(&outpam, colormapRaster);
    
    pnm_freepamarray(colormapRaster, &outpam);

    pm_close(stdout);

    exit(0);
}



